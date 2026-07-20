/* Minimal STM32F4 HAL header for CI validation.
   In production, use the real STM32Cube HAL header from your project. */
#ifndef __STM32F4XX_HAL_H
#define __STM32F4XX_HAL_H

#include <stdint.h>

typedef enum {
    HAL_OK       = 0x00,
    HAL_ERROR    = 0x01,
    HAL_BUSY     = 0x02,
    HAL_TIMEOUT  = 0x03
} HAL_StatusTypeDef;

typedef struct {
    uint32_t Instance;
    uint32_t Init;
    uint8_t  *pTxBuffPtr;
    uint16_t TxXferSize;
    uint16_t TxXferCount;
    uint8_t  *pRxBuffPtr;
    uint16_t RxXferSize;
    uint16_t RxXferCount;
} UART_HandleTypeDef;

HAL_StatusTypeDef HAL_UART_Init(UART_HandleTypeDef *huart);
HAL_StatusTypeDef HAL_UART_Transmit(UART_HandleTypeDef *huart, uint8_t *pData, uint16_t Size, uint32_t Timeout);

#endif
