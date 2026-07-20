/* mock_i2c.c — Professional I2C master mock with virtual devices, tick-driven
   async completion, error injection, and detailed trace logging. */

   #include "mock_hal.h"
   #include "trace_log.h"
   #include <string.h>
   #include <stdio.h>
   #include <stdbool.h>
   
   /* ----- forward declarations for callbacks (weak defaults later) ----- */
   void HAL_I2C_MasterTxCpltCallback(I2C_HandleTypeDef *hi2c);
   void HAL_I2C_MasterRxCpltCallback(I2C_HandleTypeDef *hi2c);
   void HAL_I2C_MemTxCpltCallback(I2C_HandleTypeDef *hi2c);
   void HAL_I2C_MemRxCpltCallback(I2C_HandleTypeDef *hi2c);
   void HAL_I2C_ErrorCallback(I2C_HandleTypeDef *hi2c);
   
   /* ----- internal configuration ----- */
   #define I2C_MAX_INSTANCES  3
   #define I2C_MAX_DEVICES    8
   #define I2C_MEM_SIZE       256
   
   /* ----- per‑instance state ----- */
   typedef enum {
       I2C_MODE_NONE,
       I2C_MODE_MASTER_TX,
       I2C_MODE_MASTER_RX,
       I2C_MODE_MEM_TX,
       I2C_MODE_MEM_RX
   } I2C_Mode;
   
   typedef struct {
       uint8_t  addr;               /* 7‑bit device address */
       uint8_t  mem[I2C_MEM_SIZE];  /* register file */
       uint16_t mem_size;
       uint8_t  current_reg;        /* last register addressed */
   } I2C_Device;
   
   typedef struct {
       I2C_Device devs[I2C_MAX_DEVICES];
       uint8_t dev_count;
   
       /* active transfer info */
       I2C_Mode   mode;
       uint8_t   *tx_buf;
       uint16_t   tx_len;
       uint8_t   *rx_buf;
       uint16_t   rx_len;
       uint8_t    target_addr;
       uint16_t   mem_addr;          /* register address for Mem operations */
       uint16_t   mem_addr_size;     /* 1 or 2 bytes */
   
       /* error injection */
       uint32_t   error_flags;       /* HAL_I2C_ERROR_xxx */
       bool       inject_error;
   
       /* async completion */
       bool       xfer_pending;
       uint32_t   xfer_end_tick;
       I2C_HandleTypeDef *active_hi2c;
   } I2C_Instance;
   
   static I2C_Instance i2c_instances[I2C_MAX_INSTANCES];
   static uint32_t    s_tick = 0;
   
   /* ----- base‑address lookup ----- */
   static int i2c_index(uintptr_t base) {
       switch (base) {
           case 0x40005400: return 0;   /* I2C1 */
           case 0x40005800: return 1;   /* I2C2 */
           case 0x40005C00: return 2;   /* I2C3 */
           default:         return 0;
       }
   }
   
   /* ----- public control API ----- */
   void mock_i2c_init(void) {
       memset(i2c_instances, 0, sizeof(i2c_instances));
       s_tick = 0;
   }
   
   /* Add a virtual device with a fully pre‑loaded register file.
      `reg_data` points to a buffer of `reg_size` bytes that will be
      copied into the device’s internal memory starting at address 0. */
   void mock_i2c_add_device(uintptr_t i2c_base, uint8_t dev_addr,
                            const uint8_t *reg_data, uint16_t reg_size) {
       int idx = i2c_index(i2c_base);
       I2C_Instance *inst = &i2c_instances[idx];
       if (inst->dev_count >= I2C_MAX_DEVICES) return;
       I2C_Device *d = &inst->devs[inst->dev_count++];
       d->addr = dev_addr;
       d->mem_size = reg_size < I2C_MEM_SIZE ? reg_size : I2C_MEM_SIZE;
       memcpy(d->mem, reg_data, d->mem_size);
       d->current_reg = 0;
   }
   
   /* Retrieve a pointer to the device’s memory (for later inspection). */
   const uint8_t *mock_i2c_get_device_mem(uintptr_t i2c_base, uint8_t dev_addr,
                                          uint16_t *size) {
       int idx = i2c_index(i2c_base);
       I2C_Instance *inst = &i2c_instances[idx];
       for (int i = 0; i < inst->dev_count; i++) {
           if (inst->devs[i].addr == dev_addr) {
               *size = inst->devs[i].mem_size;
               return inst->devs[i].mem;
           }
       }
       *size = 0;
       return NULL;
   }
   
   /* Inject an error on the next transfer. */
   void mock_i2c_inject_error(uintptr_t i2c_base, uint32_t error) {
       int idx = i2c_index(i2c_base);
       i2c_instances[idx].error_flags = error;
       i2c_instances[idx].inject_error = true;
   }
   
   /* ----- tick function (must be called periodically) ----- */
   void ember_sim_i2c_tick(void) {
       s_tick++;
       for (int i = 0; i < I2C_MAX_INSTANCES; i++) {
           I2C_Instance *inst = &i2c_instances[i];
           if (inst->xfer_pending && s_tick >= inst->xfer_end_tick) {
               inst->xfer_pending = false;
               I2C_HandleTypeDef *hi2c = inst->active_hi2c;
               switch (inst->mode) {
                   case I2C_MODE_MASTER_TX:
                       HAL_I2C_MasterTxCpltCallback(hi2c);
                       break;
                   case I2C_MODE_MASTER_RX:
                       HAL_I2C_MasterRxCpltCallback(hi2c);
                       break;
                   case I2C_MODE_MEM_TX:
                       HAL_I2C_MemTxCpltCallback(hi2c);
                       break;
                   case I2C_MODE_MEM_RX:
                       HAL_I2C_MemRxCpltCallback(hi2c);
                       break;
                   default: break;
               }
               inst->mode = I2C_MODE_NONE;
               inst->active_hi2c = NULL;
           }
       }
   }
   
   /* ----- helper: locate device in instance ----- */
   static I2C_Device *find_device(I2C_Instance *inst, uint8_t addr) {
       for (int i = 0; i < inst->dev_count; i++) {
           if (inst->devs[i].addr == addr) return &inst->devs[i];
       }
       return NULL;
   }
   
   /* ----- trace helper (hex encode) ----- */
   static void hex_str(const uint8_t *data, uint16_t len, char *out, size_t out_sz) {
       out[0] = '\0';
       for (uint16_t i = 0; i < len && strlen(out) + 3 < out_sz; i++) {
           char tmp[4];
           snprintf(tmp, sizeof(tmp), "%02X", data[i]);
           strcat(out, tmp);
       }
   }
   
   /* =================================================================
      HAL I2C Functions
      ================================================================= */
   
   /* ----- Master Transmit / Receive (no register) ----- */
   HAL_StatusTypeDef HAL_I2C_Master_Transmit(I2C_HandleTypeDef *hi2c,
                                             uint16_t DevAddress, uint8_t *pData,
                                             uint16_t Size, uint32_t Timeout) {
       (void)Timeout;
       int idx = i2c_index((uintptr_t)hi2c->Instance);
       I2C_Instance *inst = &i2c_instances[idx];
   
       /* check for error injection */
       if (inst->inject_error) {
           hi2c->ErrorCode = inst->error_flags;
           inst->inject_error = false;
           HAL_I2C_ErrorCallback(hi2c);
           return HAL_ERROR;
       }
   
       /* just log the data; no device model for raw transfers */
       char hex[I2C_MEM_SIZE*2+1];
       hex_str(pData, Size, hex, sizeof(hex));
       char payload[512];
       snprintf(payload, sizeof(payload),
                "\"inst\":\"0x%08X\",\"dev\":\"0x%02X\",\"data\":\"%s\",\"sz\":%u",
                (uint32_t)hi2c->Instance, DevAddress, hex, Size);
       trace_log("HAL_I2C_Master_Transmit", payload);
       return HAL_OK;
   }
   
   HAL_StatusTypeDef HAL_I2C_Master_Receive(I2C_HandleTypeDef *hi2c,
                                            uint16_t DevAddress, uint8_t *pData,
                                            uint16_t Size, uint32_t Timeout) {
       (void)Timeout;
       int idx = i2c_index((uintptr_t)hi2c->Instance);
       I2C_Instance *inst = &i2c_instances[idx];
   
       if (inst->inject_error) {
           hi2c->ErrorCode = inst->error_flags;
           inst->inject_error = false;
           HAL_I2C_ErrorCallback(hi2c);
           return HAL_ERROR;
       }
   
       /* For raw receive without a register address, we return zeros. */
       memset(pData, 0, Size);
       char payload[256];
       snprintf(payload, sizeof(payload),
                "\"inst\":\"0x%08X\",\"dev\":\"0x%02X\",\"sz\":%u",
                (uint32_t)hi2c->Instance, DevAddress, Size);
       trace_log("HAL_I2C_Master_Receive", payload);
       return HAL_OK;
   }
   
   /* ----- Memory Write / Read (most common for sensors) ----- */
   HAL_StatusTypeDef HAL_I2C_Mem_Write(I2C_HandleTypeDef *hi2c,
                                       uint16_t DevAddress, uint16_t MemAddress,
                                       uint16_t MemAddSize, uint8_t *pData,
                                       uint16_t Size, uint32_t Timeout) {
       (void)Timeout;
       int idx = i2c_index((uintptr_t)hi2c->Instance);
       I2C_Instance *inst = &i2c_instances[idx];
   
       if (inst->inject_error) {
           hi2c->ErrorCode = inst->error_flags;
           inst->inject_error = false;
           HAL_I2C_ErrorCallback(hi2c);
           return HAL_ERROR;
       }
   
       I2C_Device *dev = find_device(inst, DevAddress);
       if (!dev) {
           hi2c->ErrorCode = 0x00000004;  /* HAL_I2C_ERROR_AF (acknowledge failure) */
           HAL_I2C_ErrorCallback(hi2c);
           return HAL_ERROR;
       }
   
       /* Write into device memory */
       if (MemAddress + Size <= dev->mem_size) {
           memcpy(&dev->mem[MemAddress], pData, Size);
       } else {
           /* partial write or overflow → return error */
           hi2c->ErrorCode = 0x00000004;
           HAL_I2C_ErrorCallback(hi2c);
           return HAL_ERROR;
       }
   
       char hex[I2C_MEM_SIZE*2+1];
       hex_str(pData, Size, hex, sizeof(hex));
       char payload[512];
       snprintf(payload, sizeof(payload),
                "\"inst\":\"0x%08X\",\"dev\":\"0x%02X\",\"reg\":\"0x%04X\",\"data\":\"%s\",\"sz\":%u",
                (uint32_t)hi2c->Instance, DevAddress, MemAddress, hex, Size);
       trace_log("HAL_I2C_Mem_Write", payload);
       return HAL_OK;
   }
   
   HAL_StatusTypeDef HAL_I2C_Mem_Read(I2C_HandleTypeDef *hi2c,
                                      uint16_t DevAddress, uint16_t MemAddress,
                                      uint16_t MemAddSize, uint8_t *pData,
                                      uint16_t Size, uint32_t Timeout) {
       (void)Timeout;
       int idx = i2c_index((uintptr_t)hi2c->Instance);
       I2C_Instance *inst = &i2c_instances[idx];
   
       if (inst->inject_error) {
           hi2c->ErrorCode = inst->error_flags;
           inst->inject_error = false;
           HAL_I2C_ErrorCallback(hi2c);
           return HAL_ERROR;
       }
   
       I2C_Device *dev = find_device(inst, DevAddress);
       if (!dev) {
           hi2c->ErrorCode = 0x00000004;
           HAL_I2C_ErrorCallback(hi2c);
           return HAL_ERROR;
       }
   
       if (MemAddress + Size <= dev->mem_size) {
           memcpy(pData, &dev->mem[MemAddress], Size);
       } else {
           memset(pData, 0, Size);
           hi2c->ErrorCode = 0x00000004;
           HAL_I2C_ErrorCallback(hi2c);
           return HAL_ERROR;
       }
   
       char hex[I2C_MEM_SIZE*2+1];
       hex_str(pData, Size, hex, sizeof(hex));
       char payload[512];
       snprintf(payload, sizeof(payload),
                "\"inst\":\"0x%08X\",\"dev\":\"0x%02X\",\"reg\":\"0x%04X\",\"data\":\"%s\",\"sz\":%u",
                (uint32_t)hi2c->Instance, DevAddress, MemAddress, hex, Size);
       trace_log("HAL_I2C_Mem_Read", payload);
       return HAL_OK;
   }
   
   /* ----- Interrupt / DMA variants (schedule callback via tick) ----- */
   HAL_StatusTypeDef HAL_I2C_Master_Transmit_IT(I2C_HandleTypeDef *hi2c,
       uint16_t DevAddress, uint8_t *pData, uint16_t Size) {
       int idx = i2c_index((uintptr_t)hi2c->Instance);
       I2C_Instance *inst = &i2c_instances[idx];
       inst->mode = I2C_MODE_MASTER_TX;
       inst->tx_buf = pData;
       inst->tx_len = Size;
       inst->active_hi2c = hi2c;
       inst->xfer_pending = true;
       inst->xfer_end_tick = s_tick + 1; // complete after 1 tick
       /* log the call */
       char hex[I2C_MEM_SIZE*2+1];
       hex_str(pData, Size, hex, sizeof(hex));
       char payload[512];
       snprintf(payload, sizeof(payload),
                "\"inst\":\"0x%08X\",\"dev\":\"0x%02X\",\"data\":\"%s\",\"sz\":%u,\"mode\":\"IT\"",
                (uint32_t)hi2c->Instance, DevAddress, hex, Size);
       trace_log("HAL_I2C_Master_Transmit_IT", payload);
       return HAL_OK;
   }
   
   HAL_StatusTypeDef HAL_I2C_Master_Receive_IT(I2C_HandleTypeDef *hi2c,
       uint16_t DevAddress, uint8_t *pData, uint16_t Size) {
       int idx = i2c_index((uintptr_t)hi2c->Instance);
       I2C_Instance *inst = &i2c_instances[idx];
       inst->mode = I2C_MODE_MASTER_RX;
       inst->rx_buf = pData;
       inst->rx_len = Size;
       inst->active_hi2c = hi2c;
       inst->xfer_pending = true;
       inst->xfer_end_tick = s_tick + 1;
       /* Fill with zeros for now (no pre‑loaded raw data) */
       memset(pData, 0, Size);
       char payload[256];
       snprintf(payload, sizeof(payload),
                "\"inst\":\"0x%08X\",\"dev\":\"0x%02X\",\"sz\":%u,\"mode\":\"IT\"",
                (uint32_t)hi2c->Instance, DevAddress, Size);
       trace_log("HAL_I2C_Master_Receive_IT", payload);
       return HAL_OK;
   }
   
   HAL_StatusTypeDef HAL_I2C_Mem_Write_IT(I2C_HandleTypeDef *hi2c,
       uint16_t DevAddress, uint16_t MemAddress, uint16_t MemAddSize,
       uint8_t *pData, uint16_t Size) {
       /* For simplicity, call blocking version then schedule callback */
       HAL_StatusTypeDef ret = HAL_I2C_Mem_Write(hi2c, DevAddress, MemAddress,
                                                 MemAddSize, pData, Size, 100);
       if (ret == HAL_OK) {
           int idx = i2c_index((uintptr_t)hi2c->Instance);
           I2C_Instance *inst = &i2c_instances[idx];
           inst->mode = I2C_MODE_MEM_TX;
           inst->active_hi2c = hi2c;
           inst->xfer_pending = true;
           inst->xfer_end_tick = s_tick + 1;
       }
       return ret;
   }
   
   HAL_StatusTypeDef HAL_I2C_Mem_Read_IT(I2C_HandleTypeDef *hi2c,
       uint16_t DevAddress, uint16_t MemAddress, uint16_t MemAddSize,
       uint8_t *pData, uint16_t Size) {
       HAL_StatusTypeDef ret = HAL_I2C_Mem_Read(hi2c, DevAddress, MemAddress,
                                                MemAddSize, pData, Size, 100);
       if (ret == HAL_OK) {
           int idx = i2c_index((uintptr_t)hi2c->Instance);
           I2C_Instance *inst = &i2c_instances[idx];
           inst->mode = I2C_MODE_MEM_RX;
           inst->active_hi2c = hi2c;
           inst->xfer_pending = true;
           inst->xfer_end_tick = s_tick + 1;
       }
       return ret;
   }
   
   /* DMA versions: same as IT, just different mode for trace */
   HAL_StatusTypeDef HAL_I2C_Master_Transmit_DMA(I2C_HandleTypeDef *hi2c,
       uint16_t DevAddress, uint8_t *pData, uint16_t Size) {
       HAL_StatusTypeDef ret = HAL_I2C_Master_Transmit(hi2c, DevAddress, pData, Size, 100);
       if (ret == HAL_OK) {
           int idx = i2c_index((uintptr_t)hi2c->Instance);
           I2C_Instance *inst = &i2c_instances[idx];
           inst->mode = I2C_MODE_MASTER_TX;
           inst->active_hi2c = hi2c;
           inst->xfer_pending = true;
           inst->xfer_end_tick = s_tick; // immediate
       }
       return ret;
   }
   
   HAL_StatusTypeDef HAL_I2C_Master_Receive_DMA(I2C_HandleTypeDef *hi2c,
       uint16_t DevAddress, uint8_t *pData, uint16_t Size) {
       HAL_StatusTypeDef ret = HAL_I2C_Master_Receive(hi2c, DevAddress, pData, Size, 100);
       if (ret == HAL_OK) {
           int idx = i2c_index((uintptr_t)hi2c->Instance);
           I2C_Instance *inst = &i2c_instances[idx];
           inst->mode = I2C_MODE_MASTER_RX;
           inst->active_hi2c = hi2c;
           inst->xfer_pending = true;
           inst->xfer_end_tick = s_tick;
       }
       return ret;
   }
   
   /* ----- Weak default callbacks (overridden by user) ----- */
   __attribute__((weak)) void HAL_I2C_MasterTxCpltCallback(I2C_HandleTypeDef *hi2c) {}
   __attribute__((weak)) void HAL_I2C_MasterRxCpltCallback(I2C_HandleTypeDef *hi2c) {}
   __attribute__((weak)) void HAL_I2C_MemTxCpltCallback(I2C_HandleTypeDef *hi2c) {}
   __attribute__((weak)) void HAL_I2C_MemRxCpltCallback(I2C_HandleTypeDef *hi2c) {}
   __attribute__((weak)) void HAL_I2C_ErrorCallback(I2C_HandleTypeDef *hi2c) {}