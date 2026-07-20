#include "mock_hal.h"
#include <stdio.h>
#include "trace_log.h"

__attribute__((weak)) HAL_StatusTypeDef HAL_UART_Init(UART_HandleTypeDef* huart)
{
    trace_log("HAL_UART_Init", "huart");
    return HAL_OK;
}

__attribute__((weak)) HAL_StatusTypeDef HAL_UART_Transmit(UART_HandleTypeDef* huart, uint8_t* pData, uint16_t Size, uint32_t Timeout)
{
    trace_log("HAL_UART_Transmit", "huart,pData,Size,Timeout");
    return HAL_OK;
}


/* ── I2C master weak stubs (always provided) ── */
__attribute__((weak)) HAL_StatusTypeDef HAL_I2C_Master_Transmit(I2C_HandleTypeDef* hi2c, uint16_t DevAddress, uint8_t* pData, uint16_t Size, uint32_t Timeout) { trace_log("HAL_I2C_Master_Transmit", ""); return HAL_OK; }
__attribute__((weak)) HAL_StatusTypeDef HAL_I2C_Master_Receive(I2C_HandleTypeDef* hi2c, uint16_t DevAddress, uint8_t* pData, uint16_t Size, uint32_t Timeout) { trace_log("HAL_I2C_Master_Receive", ""); return HAL_OK; }
__attribute__((weak)) HAL_StatusTypeDef HAL_I2C_Master_Transmit_IT(I2C_HandleTypeDef* hi2c, uint16_t DevAddress, uint8_t* pData, uint16_t Size) { trace_log("HAL_I2C_Master_Transmit_IT", ""); return HAL_OK; }
__attribute__((weak)) HAL_StatusTypeDef HAL_I2C_Master_Receive_IT(I2C_HandleTypeDef* hi2c, uint16_t DevAddress, uint8_t* pData, uint16_t Size) { trace_log("HAL_I2C_Master_Receive_IT", ""); return HAL_OK; }
__attribute__((weak)) HAL_StatusTypeDef HAL_I2C_Master_Transmit_DMA(I2C_HandleTypeDef* hi2c, uint16_t DevAddress, uint8_t* pData, uint16_t Size) { trace_log("HAL_I2C_Master_Transmit_DMA", ""); return HAL_OK; }
__attribute__((weak)) HAL_StatusTypeDef HAL_I2C_Master_Receive_DMA(I2C_HandleTypeDef* hi2c, uint16_t DevAddress, uint8_t* pData, uint16_t Size) { trace_log("HAL_I2C_Master_Receive_DMA", ""); return HAL_OK; }
__attribute__((weak)) HAL_StatusTypeDef HAL_I2C_Mem_Write(I2C_HandleTypeDef* hi2c, uint16_t DevAddress, uint16_t MemAddress, uint16_t MemAddSize, uint8_t* pData, uint16_t Size, uint32_t Timeout) { trace_log("HAL_I2C_Mem_Write", ""); return HAL_OK; }
__attribute__((weak)) HAL_StatusTypeDef HAL_I2C_Mem_Read(I2C_HandleTypeDef* hi2c, uint16_t DevAddress, uint16_t MemAddress, uint16_t MemAddSize, uint8_t* pData, uint16_t Size, uint32_t Timeout) { trace_log("HAL_I2C_Mem_Read", ""); return HAL_OK; }
__attribute__((weak)) HAL_StatusTypeDef HAL_I2C_Mem_Write_IT(I2C_HandleTypeDef* hi2c, uint16_t DevAddress, uint16_t MemAddress, uint16_t MemAddSize, uint8_t* pData, uint16_t Size) { trace_log("HAL_I2C_Mem_Write_IT", ""); return HAL_OK; }
__attribute__((weak)) HAL_StatusTypeDef HAL_I2C_Mem_Read_IT(I2C_HandleTypeDef* hi2c, uint16_t DevAddress, uint16_t MemAddress, uint16_t MemAddSize, uint8_t* pData, uint16_t Size) { trace_log("HAL_I2C_Mem_Read_IT", ""); return HAL_OK; }
__attribute__((weak)) HAL_StatusTypeDef HAL_I2C_IsDeviceReady(I2C_HandleTypeDef* hi2c, uint16_t DevAddress, uint32_t Trials, uint32_t Timeout) { trace_log("HAL_I2C_IsDeviceReady", ""); return HAL_OK; }

/* ── SPI master weak stubs (always provided) ── */
__attribute__((weak)) HAL_StatusTypeDef HAL_SPI_Transmit(SPI_HandleTypeDef* hspi, uint8_t* pData, uint16_t Size, uint32_t Timeout) { trace_log("HAL_SPI_Transmit", ""); return HAL_OK; }
__attribute__((weak)) HAL_StatusTypeDef HAL_SPI_Receive(SPI_HandleTypeDef* hspi, uint8_t* pData, uint16_t Size, uint32_t Timeout) { trace_log("HAL_SPI_Receive", ""); return HAL_OK; }
__attribute__((weak)) HAL_StatusTypeDef HAL_SPI_TransmitReceive(SPI_HandleTypeDef* hspi, uint8_t* pTxData, uint8_t* pRxData, uint16_t Size, uint32_t Timeout) { trace_log("HAL_SPI_TransmitReceive", ""); return HAL_OK; }
__attribute__((weak)) HAL_StatusTypeDef HAL_SPI_Transmit_IT(SPI_HandleTypeDef* hspi, uint8_t* pData, uint16_t Size) { trace_log("HAL_SPI_Transmit_IT", ""); return HAL_OK; }
__attribute__((weak)) HAL_StatusTypeDef HAL_SPI_Receive_IT(SPI_HandleTypeDef* hspi, uint8_t* pData, uint16_t Size) { trace_log("HAL_SPI_Receive_IT", ""); return HAL_OK; }
__attribute__((weak)) HAL_StatusTypeDef HAL_SPI_TransmitReceive_IT(SPI_HandleTypeDef* hspi, uint8_t* pTxData, uint8_t* pRxData, uint16_t Size) { trace_log("HAL_SPI_TransmitReceive_IT", ""); return HAL_OK; }
__attribute__((weak)) HAL_StatusTypeDef HAL_SPI_Transmit_DMA(SPI_HandleTypeDef* hspi, uint8_t* pData, uint16_t Size) { trace_log("HAL_SPI_Transmit_DMA", ""); return HAL_OK; }
__attribute__((weak)) HAL_StatusTypeDef HAL_SPI_Receive_DMA(SPI_HandleTypeDef* hspi, uint8_t* pData, uint16_t Size) { trace_log("HAL_SPI_Receive_DMA", ""); return HAL_OK; }
__attribute__((weak)) HAL_StatusTypeDef HAL_SPI_TransmitReceive_DMA(SPI_HandleTypeDef* hspi, uint8_t* pTxData, uint8_t* pRxData, uint16_t Size) { trace_log("HAL_SPI_TransmitReceive_DMA", ""); return HAL_OK; }

/* ── TIM weak stubs (always provided) ── */
__attribute__((weak)) HAL_StatusTypeDef HAL_TIM_Base_Init(TIM_HandleTypeDef* htim) { trace_log("HAL_TIM_Base_Init", ""); return HAL_OK; }
__attribute__((weak)) HAL_StatusTypeDef HAL_TIM_Base_Start(TIM_HandleTypeDef* htim) { trace_log("HAL_TIM_Base_Start", ""); return HAL_OK; }
__attribute__((weak)) HAL_StatusTypeDef HAL_TIM_Base_Stop(TIM_HandleTypeDef* htim) { trace_log("HAL_TIM_Base_Stop", ""); return HAL_OK; }
__attribute__((weak)) HAL_StatusTypeDef HAL_TIM_Base_Start_IT(TIM_HandleTypeDef* htim) { trace_log("HAL_TIM_Base_Start_IT", ""); return HAL_OK; }
__attribute__((weak)) HAL_StatusTypeDef HAL_TIM_Base_Stop_IT(TIM_HandleTypeDef* htim) { trace_log("HAL_TIM_Base_Stop_IT", ""); return HAL_OK; }
__attribute__((weak)) HAL_StatusTypeDef HAL_TIM_PWM_Init(TIM_HandleTypeDef* htim) { trace_log("HAL_TIM_PWM_Init", ""); return HAL_OK; }
__attribute__((weak)) HAL_StatusTypeDef HAL_TIM_PWM_Start(TIM_HandleTypeDef* htim, uint32_t Channel) { trace_log("HAL_TIM_PWM_Start", ""); return HAL_OK; }
__attribute__((weak)) HAL_StatusTypeDef HAL_TIM_PWM_Stop(TIM_HandleTypeDef* htim, uint32_t Channel) { trace_log("HAL_TIM_PWM_Stop", ""); return HAL_OK; }
__attribute__((weak)) HAL_StatusTypeDef HAL_TIM_IC_Init(TIM_HandleTypeDef* htim) { trace_log("HAL_TIM_IC_Init", ""); return HAL_OK; }
__attribute__((weak)) HAL_StatusTypeDef HAL_TIM_IC_Start_IT(TIM_HandleTypeDef* htim, uint32_t Channel) { trace_log("HAL_TIM_IC_Start_IT", ""); return HAL_OK; }
