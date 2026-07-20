/* Minimal firmware for CI validation — TIM2 periodic + UART transmit */
#include "mock_hal.h"
#include "mock_tim.h"
#include <string.h>

extern void mock_uart_init(void);

static TIM_HandleTypeDef htim2;
static UART_HandleTypeDef huart2;
static int ticks = 0;

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim) {
    if (htim->Instance == 0x40000400) {
        ticks++;
        if (ticks % 10 == 0) {
            uint8_t msg[] = "OK\n";
            HAL_UART_Transmit(&huart2, msg, 3, 100);
        }
    }
}

void app_init(void) {
    mock_tim_init();
    mock_tim_set_period_ticks(0x40000400, 1000);
    htim2.Instance = 0x40000400;
    HAL_TIM_Base_Init(&htim2);
    HAL_TIM_Base_Start_IT(&htim2);

    mock_uart_init();
    huart2.Instance = 0x40004400;
    HAL_UART_Init(&huart2);
}

void app_run(void) {}
