#ifndef MOCK_HAL_H
#define MOCK_HAL_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* HAL status */
typedef enum { HAL_OK = 0, HAL_ERROR = 1, HAL_BUSY = 2, HAL_TIMEOUT = 3 } HAL_StatusTypeDef;

/* GPIO types */
typedef struct { volatile uint32_t dummy; } GPIO_TypeDef;
typedef struct { uint32_t dummy; } GPIO_InitTypeDef;
typedef enum { GPIO_PIN_RESET = 0, GPIO_PIN_SET = 1 } GPIO_PinState;

/* UART state machine */
typedef enum { HAL_UART_STATE_RESET = 0x00, HAL_UART_STATE_READY = 0x20, HAL_UART_STATE_BUSY = 0x24, HAL_UART_STATE_BUSY_TX = 0x21, HAL_UART_STATE_BUSY_RX = 0x22, HAL_UART_STATE_BUSY_TX_RX = 0x23, HAL_UART_STATE_TIMEOUT = 0xA0, HAL_UART_STATE_ERROR = 0xE0 } HAL_UART_StateTypeDef;
#define HAL_UART_ERROR_PE  0x00000001U
#define HAL_UART_ERROR_NE  0x00000002U
#define HAL_UART_ERROR_FE  0x00000004U
#define HAL_UART_ERROR_ORE 0x00000008U
#define HAL_UART_ERROR_DMA 0x00000010U

/* UART handle — full struct for professional mock */
typedef struct { uint32_t Instance; uint32_t Init; uint8_t *pTxBuffPtr; uint16_t TxXferSize; uint16_t TxXferCount; uint8_t *pRxBuffPtr; uint16_t RxXferSize; uint16_t RxXferCount; HAL_UART_StateTypeDef gState; HAL_UART_StateTypeDef RxState; uint32_t ErrorCode; } UART_HandleTypeDef;


/* I2C error codes */
#define HAL_I2C_ERROR_NONE  0x00000000U
#define HAL_I2C_ERROR_BERR  0x00000001U
#define HAL_I2C_ERROR_ARLO  0x00000002U
#define HAL_I2C_ERROR_AF    0x00000004U
#define HAL_I2C_ERROR_OVR   0x00000008U
#define HAL_I2C_ERROR_DMA   0x00000010U
#define HAL_I2C_ERROR_TIMEOUT 0x00000020U
/* Other peripheral handles (minimal) */
/* I2C handle — full struct for professional mock */
typedef struct { uint32_t Instance; uint32_t Init; uint8_t *pBuffPtr; uint16_t XferSize; uint16_t XferCount; uint32_t ErrorCode; } I2C_HandleTypeDef;
HAL_StatusTypeDef HAL_UART_Init(UART_HandleTypeDef* huart);
HAL_StatusTypeDef HAL_UART_Transmit(UART_HandleTypeDef* huart, uint8_t* pData, uint16_t Size, uint32_t Timeout);

/* I2C master functions (always provided by EmberSim) */
HAL_StatusTypeDef HAL_I2C_Master_Transmit(I2C_HandleTypeDef* hi2c, uint16_t DevAddress, uint8_t* pData, uint16_t Size, uint32_t Timeout);
HAL_StatusTypeDef HAL_I2C_Master_Receive(I2C_HandleTypeDef* hi2c, uint16_t DevAddress, uint8_t* pData, uint16_t Size, uint32_t Timeout);
HAL_StatusTypeDef HAL_I2C_Master_Transmit_IT(I2C_HandleTypeDef* hi2c, uint16_t DevAddress, uint8_t* pData, uint16_t Size);
HAL_StatusTypeDef HAL_I2C_Master_Receive_IT(I2C_HandleTypeDef* hi2c, uint16_t DevAddress, uint8_t* pData, uint16_t Size);
HAL_StatusTypeDef HAL_I2C_Master_Transmit_DMA(I2C_HandleTypeDef* hi2c, uint16_t DevAddress, uint8_t* pData, uint16_t Size);
HAL_StatusTypeDef HAL_I2C_Master_Receive_DMA(I2C_HandleTypeDef* hi2c, uint16_t DevAddress, uint8_t* pData, uint16_t Size);
HAL_StatusTypeDef HAL_I2C_Mem_Write(I2C_HandleTypeDef* hi2c, uint16_t DevAddress, uint16_t MemAddress, uint16_t MemAddSize, uint8_t* pData, uint16_t Size, uint32_t Timeout);
HAL_StatusTypeDef HAL_I2C_Mem_Read(I2C_HandleTypeDef* hi2c, uint16_t DevAddress, uint16_t MemAddress, uint16_t MemAddSize, uint8_t* pData, uint16_t Size, uint32_t Timeout);
HAL_StatusTypeDef HAL_I2C_Mem_Write_IT(I2C_HandleTypeDef* hi2c, uint16_t DevAddress, uint16_t MemAddress, uint16_t MemAddSize, uint8_t* pData, uint16_t Size);
HAL_StatusTypeDef HAL_I2C_Mem_Read_IT(I2C_HandleTypeDef* hi2c, uint16_t DevAddress, uint16_t MemAddress, uint16_t MemAddSize, uint8_t* pData, uint16_t Size);
HAL_StatusTypeDef HAL_I2C_Mem_Write_DMA(I2C_HandleTypeDef* hi2c, uint16_t DevAddress, uint16_t MemAddress, uint16_t MemAddSize, uint8_t* pData, uint16_t Size);
HAL_StatusTypeDef HAL_I2C_Mem_Read_DMA(I2C_HandleTypeDef* hi2c, uint16_t DevAddress, uint16_t MemAddress, uint16_t MemAddSize, uint8_t* pData, uint16_t Size);
HAL_StatusTypeDef HAL_I2C_IsDeviceReady(I2C_HandleTypeDef* hi2c, uint16_t DevAddress, uint32_t Trials, uint32_t Timeout);

#endif /* MOCK_HAL_H */

/* SPI error codes */
#define HAL_SPI_ERROR_NONE  0x00000000U
#define HAL_SPI_ERROR_CRC   0x00000001U
#define HAL_SPI_ERROR_OVR   0x00000002U
#define HAL_SPI_ERROR_DMA   0x00000010U
/* SPI handle — full struct for professional mock */
typedef struct { uint32_t Instance; uint32_t Init; uint8_t *pTxBuffPtr; uint16_t TxXferSize; uint16_t TxXferCount; uint8_t *pRxBuffPtr; uint16_t RxXferSize; uint16_t RxXferCount; uint32_t ErrorCode; } SPI_HandleTypeDef;

/* SPI master functions (always provided by EmberSim) */
HAL_StatusTypeDef HAL_SPI_Transmit(SPI_HandleTypeDef* hspi, uint8_t* pData, uint16_t Size, uint32_t Timeout);
HAL_StatusTypeDef HAL_SPI_Receive(SPI_HandleTypeDef* hspi, uint8_t* pData, uint16_t Size, uint32_t Timeout);
HAL_StatusTypeDef HAL_SPI_TransmitReceive(SPI_HandleTypeDef* hspi, uint8_t* pTxData, uint8_t* pRxData, uint16_t Size, uint32_t Timeout);
HAL_StatusTypeDef HAL_SPI_Transmit_IT(SPI_HandleTypeDef* hspi, uint8_t* pData, uint16_t Size);
HAL_StatusTypeDef HAL_SPI_Receive_IT(SPI_HandleTypeDef* hspi, uint8_t* pData, uint16_t Size);
HAL_StatusTypeDef HAL_SPI_TransmitReceive_IT(SPI_HandleTypeDef* hspi, uint8_t* pTxData, uint8_t* pRxData, uint16_t Size);
HAL_StatusTypeDef HAL_SPI_Transmit_DMA(SPI_HandleTypeDef* hspi, uint8_t* pData, uint16_t Size);
HAL_StatusTypeDef HAL_SPI_Receive_DMA(SPI_HandleTypeDef* hspi, uint8_t* pData, uint16_t Size);
HAL_StatusTypeDef HAL_SPI_TransmitReceive_DMA(SPI_HandleTypeDef* hspi, uint8_t* pTxData, uint8_t* pRxData, uint16_t Size);

/* TIM error codes */
#define HAL_TIM_ERROR_NONE  0x00000000U
#define HAL_TIM_ERROR_TIMEOUT 0x00000001U
/* TIM handle — full struct for professional mock */
typedef struct { uint32_t Instance; uint32_t Init; uint32_t Channel; uint32_t Prescaler; uint32_t Period; uint32_t ErrorCode; } TIM_HandleTypeDef;

/* TIM functions (always provided by EmberSim) */
HAL_StatusTypeDef HAL_TIM_Base_Init(TIM_HandleTypeDef* htim);
HAL_StatusTypeDef HAL_TIM_Base_Start(TIM_HandleTypeDef* htim);
HAL_StatusTypeDef HAL_TIM_Base_Stop(TIM_HandleTypeDef* htim);
HAL_StatusTypeDef HAL_TIM_Base_Start_IT(TIM_HandleTypeDef* htim);
HAL_StatusTypeDef HAL_TIM_Base_Stop_IT(TIM_HandleTypeDef* htim);
HAL_StatusTypeDef HAL_TIM_PWM_Init(TIM_HandleTypeDef* htim);
HAL_StatusTypeDef HAL_TIM_PWM_Start(TIM_HandleTypeDef* htim, uint32_t Channel);
HAL_StatusTypeDef HAL_TIM_PWM_Stop(TIM_HandleTypeDef* htim, uint32_t Channel);
HAL_StatusTypeDef HAL_TIM_IC_Init(TIM_HandleTypeDef* htim);
HAL_StatusTypeDef HAL_TIM_IC_Start_IT(TIM_HandleTypeDef* htim, uint32_t Channel);
