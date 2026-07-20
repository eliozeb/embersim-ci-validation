/* mock_spi.c — Professional SPI master mock with virtual device support,
   tick‑driven async completion, error injection, and detailed trace logging. */

   #include "mock_hal.h"
   #include "mock_spi.h"
   #include "trace_log.h"
   #include <string.h>
   #include <stdio.h>
   #include <stdbool.h>

   /* Forward declarations for HAL callbacks (weak defaults defined later) */
void HAL_SPI_TxCpltCallback(SPI_HandleTypeDef *hspi);
void HAL_SPI_RxCpltCallback(SPI_HandleTypeDef *hspi);
void HAL_SPI_TxRxCpltCallback(SPI_HandleTypeDef *hspi);
void HAL_SPI_ErrorCallback(SPI_HandleTypeDef *hspi);
   
   /* ----- internal configuration ----- */
   #define SPI_MAX_INSTANCES  3
   #define SPI_MAX_DEVICES    4
   #define SPI_BUF_SIZE       256
   
   /* ----- per‑instance state ----- */
   typedef enum {
       SPI_MODE_NONE,
       SPI_MODE_TX,
       SPI_MODE_RX,
       SPI_MODE_TXRX
   } SPI_Mode;
   
   typedef struct {
       VirtualSPIDevice *devs[SPI_MAX_DEVICES];
       uint8_t dev_count;
   
       SPI_Mode   mode;
       uint8_t    tx_buf[SPI_BUF_SIZE];
       uint16_t   tx_len;
       uint8_t    rx_buf[SPI_BUF_SIZE];
       uint16_t   rx_len;
       uint16_t   rx_idx;
   
       /* active transfer parameters */
       uint16_t   data_size;     /* SPI_DATASIZE_8BIT or SPI_DATASIZE_16BIT */
       uint8_t    *pTxData;
       uint16_t   TxXferCount;
       uint8_t    *pRxData;
       uint16_t   RxXferCount;
   
       /* error injection */
       uint32_t   error_flags;
       bool       inject_error;
   
       /* async scheduling */
       bool       xfer_pending;
       uint32_t   xfer_end_tick;
       SPI_HandleTypeDef *active_hspi;
   } SPI_Instance;
   
   static SPI_Instance spi_instances[SPI_MAX_INSTANCES];
   static uint32_t    s_tick = 0;
   
   /* ----- base‑address lookup ----- */
   static int spi_index(uintptr_t base) {
       switch (base) {
           case 0x40013000: return 0;   /* SPI1 */
           case 0x40003800: return 1;   /* SPI2 */
           case 0x40003C00: return 2;   /* SPI3 */
           default:         return 0;
       }
   }
   
   /* ----- public control API ----- */
   void mock_spi_init(void) {
       memset(spi_instances, 0, sizeof(spi_instances));
       s_tick = 0;
   }
   
   void mock_spi_add_device(uintptr_t spi_base, VirtualSPIDevice *dev) {
       int idx = spi_index(spi_base);
       SPI_Instance *inst = &spi_instances[idx];
       if (inst->dev_count >= SPI_MAX_DEVICES) return;
       inst->devs[inst->dev_count++] = dev;
   }
   
   /* YAML loader stub – will be implemented fully in a future day */
   void mock_spi_load_device_yaml(uintptr_t spi_base, const char *yaml_path) {
       (void)spi_base;
       (void)yaml_path;
       trace_log("mock_spi_load_device_yaml", "\"msg\":\"not yet implemented\"");
   }
   
   /* ----- tick function ----- */
   void ember_sim_spi_tick(void) {
       s_tick++;
       for (int i = 0; i < SPI_MAX_INSTANCES; i++) {
           SPI_Instance *inst = &spi_instances[i];
           if (inst->xfer_pending && s_tick >= inst->xfer_end_tick) {
               inst->xfer_pending = false;
               SPI_HandleTypeDef *hspi = inst->active_hspi;
               /* call completion callback based on mode */
               /* weak callbacks already declared in mock_hal.h */
               switch (inst->mode) {
                   case SPI_MODE_TX:
                       HAL_SPI_TxCpltCallback(hspi);
                       break;
                   case SPI_MODE_RX:
                       HAL_SPI_RxCpltCallback(hspi);
                       break;
                   case SPI_MODE_TXRX:
                       HAL_SPI_TxRxCpltCallback(hspi);
                       break;
                   default: break;
               }
               inst->mode = SPI_MODE_NONE;
               inst->active_hspi = NULL;
           }
       }
   }
   
   /* ----- helper: find active device by CS (simplified: first device always active) ----- */
   static VirtualSPIDevice *active_device(SPI_Instance *inst) {
       /* In a real implementation, CS pin state would select device.
          For now, return the first registered device (or NULL). */
       return (inst->dev_count > 0) ? inst->devs[0] : NULL;
   }
   
   /* ----- hex encoding helper ----- */
   static void hex_str(const uint8_t *data, uint16_t len, char *out, size_t out_sz) {
       out[0] = '\0';
       for (uint16_t i = 0; i < len && strlen(out) + 3 < out_sz; i++) {
           char tmp[4];
           snprintf(tmp, sizeof(tmp), "%02X", data[i]);
           strcat(out, tmp);
       }
   }
   
   /* ----- blocking transmit / receive / transmit‑receive ----- */
   HAL_StatusTypeDef HAL_SPI_Transmit(SPI_HandleTypeDef *hspi, uint8_t *pData, uint16_t Size, uint32_t Timeout) {
       (void)Timeout;
       int idx = spi_index((uintptr_t)hspi->Instance);
       SPI_Instance *inst = &spi_instances[idx];
   
       if (inst->inject_error) {
           hspi->ErrorCode = inst->error_flags;
           inst->inject_error = false;
           HAL_SPI_ErrorCallback(hspi);
           return HAL_ERROR;
       }
   
       VirtualSPIDevice *dev = active_device(inst);
       if (dev && dev->on_select) dev->on_select(dev);
   
       char hex[SPI_BUF_SIZE*2+1];
       hex_str(pData, Size, hex, sizeof(hex));
       char payload[512];
       snprintf(payload, sizeof(payload), "\"inst\":\"0x%08X\",\"data\":\"%s\",\"sz\":%u",
                (uint32_t)hspi->Instance, hex, Size);
       trace_log("HAL_SPI_Transmit", payload);
   
       if (dev && dev->on_deselect) dev->on_deselect(dev);
       return HAL_OK;
   }
   
   HAL_StatusTypeDef HAL_SPI_Receive(SPI_HandleTypeDef *hspi, uint8_t *pData, uint16_t Size, uint32_t Timeout) {
       (void)Timeout;
       int idx = spi_index((uintptr_t)hspi->Instance);
       SPI_Instance *inst = &spi_instances[idx];
   
       if (inst->inject_error) {
           hspi->ErrorCode = inst->error_flags;
           inst->inject_error = false;
           HAL_SPI_ErrorCallback(hspi);
           return HAL_ERROR;
       }
   
       VirtualSPIDevice *dev = active_device(inst);
       if (dev && dev->on_select) dev->on_select(dev);
   
       for (uint16_t i = 0; i < Size; i++) {
           uint8_t tx = 0xFF;  /* dummy byte to generate clock */
           pData[i] = dev ? dev->on_byte(dev, tx) : 0x00;
       }
   
       char hex[SPI_BUF_SIZE*2+1];
       hex_str(pData, Size, hex, sizeof(hex));
       char payload[512];
       snprintf(payload, sizeof(payload), "\"inst\":\"0x%08X\",\"data\":\"%s\",\"sz\":%u",
                (uint32_t)hspi->Instance, hex, Size);
       trace_log("HAL_SPI_Receive", payload);
   
       if (dev && dev->on_deselect) dev->on_deselect(dev);
       return HAL_OK;
   }
   
   HAL_StatusTypeDef HAL_SPI_TransmitReceive(SPI_HandleTypeDef *hspi, uint8_t *pTxData, uint8_t *pRxData, uint16_t Size, uint32_t Timeout) {
       (void)Timeout;
       int idx = spi_index((uintptr_t)hspi->Instance);
       SPI_Instance *inst = &spi_instances[idx];
   
       if (inst->inject_error) {
           hspi->ErrorCode = inst->error_flags;
           inst->inject_error = false;
           HAL_SPI_ErrorCallback(hspi);
           return HAL_ERROR;
       }
   
       VirtualSPIDevice *dev = active_device(inst);
       if (dev && dev->on_select) dev->on_select(dev);
   
       for (uint16_t i = 0; i < Size; i++) {
           pRxData[i] = dev ? dev->on_byte(dev, pTxData[i]) : 0x00;
       }
   
       char hex_tx[SPI_BUF_SIZE*2+1], hex_rx[SPI_BUF_SIZE*2+1];
       hex_str(pTxData, Size, hex_tx, sizeof(hex_tx));
       hex_str(pRxData, Size, hex_rx, sizeof(hex_rx));
       char payload[1024];
       snprintf(payload, sizeof(payload), "\"inst\":\"0x%08X\",\"tx\":\"%s\",\"rx\":\"%s\",\"sz\":%u",
                (uint32_t)hspi->Instance, hex_tx, hex_rx, Size);
       trace_log("HAL_SPI_TransmitReceive", payload);
   
       if (dev && dev->on_deselect) dev->on_deselect(dev);
       return HAL_OK;
   }
   
   /* ----- IT / DMA variants (schedule callback via tick) ----- */
   HAL_StatusTypeDef HAL_SPI_Transmit_IT(SPI_HandleTypeDef *hspi, uint8_t *pData, uint16_t Size) {
       int idx = spi_index((uintptr_t)hspi->Instance);
       SPI_Instance *inst = &spi_instances[idx];
       HAL_StatusTypeDef ret = HAL_SPI_Transmit(hspi, pData, Size, 100);
       if (ret == HAL_OK) {
           inst->mode = SPI_MODE_TX;
           inst->active_hspi = hspi;
           inst->xfer_pending = true;
           inst->xfer_end_tick = s_tick + 1;
       }
       return ret;
   }
   
   HAL_StatusTypeDef HAL_SPI_Receive_IT(SPI_HandleTypeDef *hspi, uint8_t *pData, uint16_t Size) {
       int idx = spi_index((uintptr_t)hspi->Instance);
       SPI_Instance *inst = &spi_instances[idx];
       HAL_StatusTypeDef ret = HAL_SPI_Receive(hspi, pData, Size, 100);
       if (ret == HAL_OK) {
           inst->mode = SPI_MODE_RX;
           inst->active_hspi = hspi;
           inst->xfer_pending = true;
           inst->xfer_end_tick = s_tick + 1;
       }
       return ret;
   }
   
   HAL_StatusTypeDef HAL_SPI_TransmitReceive_IT(SPI_HandleTypeDef *hspi, uint8_t *pTxData, uint8_t *pRxData, uint16_t Size) {
       int idx = spi_index((uintptr_t)hspi->Instance);
       SPI_Instance *inst = &spi_instances[idx];
       HAL_StatusTypeDef ret = HAL_SPI_TransmitReceive(hspi, pTxData, pRxData, Size, 100);
       if (ret == HAL_OK) {
           inst->mode = SPI_MODE_TXRX;
           inst->active_hspi = hspi;
           inst->xfer_pending = true;
           inst->xfer_end_tick = s_tick + 1;
       }
       return ret;
   }
   
   HAL_StatusTypeDef HAL_SPI_Transmit_DMA(SPI_HandleTypeDef *hspi, uint8_t *pData, uint16_t Size) {
       int idx = spi_index((uintptr_t)hspi->Instance);
       SPI_Instance *inst = &spi_instances[idx];
       HAL_StatusTypeDef ret = HAL_SPI_Transmit(hspi, pData, Size, 100);
       if (ret == HAL_OK) {
           inst->mode = SPI_MODE_TX;
           inst->active_hspi = hspi;
           inst->xfer_pending = true;
           inst->xfer_end_tick = s_tick; /* immediate */
       }
       return ret;
   }
   
   HAL_StatusTypeDef HAL_SPI_Receive_DMA(SPI_HandleTypeDef *hspi, uint8_t *pData, uint16_t Size) {
       int idx = spi_index((uintptr_t)hspi->Instance);
       SPI_Instance *inst = &spi_instances[idx];
       HAL_StatusTypeDef ret = HAL_SPI_Receive(hspi, pData, Size, 100);
       if (ret == HAL_OK) {
           inst->mode = SPI_MODE_RX;
           inst->active_hspi = hspi;
           inst->xfer_pending = true;
           inst->xfer_end_tick = s_tick;
       }
       return ret;
   }
   
   HAL_StatusTypeDef HAL_SPI_TransmitReceive_DMA(SPI_HandleTypeDef *hspi, uint8_t *pTxData, uint8_t *pRxData, uint16_t Size) {
       int idx = spi_index((uintptr_t)hspi->Instance);
       SPI_Instance *inst = &spi_instances[idx];
       HAL_StatusTypeDef ret = HAL_SPI_TransmitReceive(hspi, pTxData, pRxData, Size, 100);
       if (ret == HAL_OK) {
           inst->mode = SPI_MODE_TXRX;
           inst->active_hspi = hspi;
           inst->xfer_pending = true;
           inst->xfer_end_tick = s_tick;
       }
       return ret;
   }
   
   /* Error injection helper */
   void mock_spi_inject_error(uintptr_t spi_base, uint32_t error) {
       int idx = spi_index(spi_base);
       spi_instances[idx].error_flags = error;
       spi_instances[idx].inject_error = true;
   }
   
   /* Weak default callbacks */
   __attribute__((weak)) void HAL_SPI_TxCpltCallback(SPI_HandleTypeDef *hspi) {}
   __attribute__((weak)) void HAL_SPI_RxCpltCallback(SPI_HandleTypeDef *hspi) {}
   __attribute__((weak)) void HAL_SPI_TxRxCpltCallback(SPI_HandleTypeDef *hspi) {}
   __attribute__((weak)) void HAL_SPI_ErrorCallback(SPI_HandleTypeDef *hspi) {}