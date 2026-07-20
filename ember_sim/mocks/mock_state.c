/* mock_state.c — Stateful GPIO simulation with value logging */
#include "mock_hal.h"
#include "trace_log.h"
#include <string.h>
#include <stdio.h>
#include <stdint.h>

#define MAX_GPIO_PORTS  8

static uint16_t gpio_pin_states[MAX_GPIO_PORTS];

#define GPIOA_BASE  ((uintptr_t)0x10000000U)
#define GPIOB_BASE  ((uintptr_t)0x10000400U)
#define GPIOC_BASE  ((uintptr_t)0x10000800U)
#define GPIOD_BASE  ((uintptr_t)0x10000C00U)
#define GPIOE_BASE  ((uintptr_t)0x10001000U)
#define GPIOF_BASE  ((uintptr_t)0x10001400U)
#define GPIOG_BASE  ((uintptr_t)0x10001800U)
#define GPIOH_BASE  ((uintptr_t)0x10001C00U)

static int gpio_port_index(uintptr_t base) {
    switch (base) {
        case GPIOA_BASE: return 0;
        case GPIOB_BASE: return 1;
        case GPIOC_BASE: return 2;
        case GPIOD_BASE: return 3;
        case GPIOE_BASE: return 4;
        case GPIOF_BASE: return 5;
        case GPIOG_BASE: return 6;
        case GPIOH_BASE: return 7;
        default:         return 0;
    }
}

void mock_gpio_set_input(uintptr_t port_base, uint16_t pin, uint8_t value) {
    int port = gpio_port_index(port_base);
    if (value) gpio_pin_states[port] |= pin;
    else       gpio_pin_states[port] &= ~pin;
}

void mock_gpio_init(void) {
    memset(gpio_pin_states, 0, sizeof(gpio_pin_states));
}

void HAL_GPIO_WritePin(GPIO_TypeDef* GPIOx, uint16_t GPIO_Pin, GPIO_PinState PinState) {
    int port = gpio_port_index((uintptr_t)GPIOx);
    if (PinState == GPIO_PIN_SET) gpio_pin_states[port] |= GPIO_Pin;
    else                           gpio_pin_states[port] &= ~GPIO_Pin;

    char payload[256];
    snprintf(payload, sizeof(payload),
             "\"port\":\"0x%08X\",\"pin\":%u,\"state\":\"%s\"",
             (uint32_t)(uintptr_t)GPIOx, GPIO_Pin,
             PinState == GPIO_PIN_SET ? "SET" : "RESET");
    trace_log("HAL_GPIO_WritePin", payload);
}

GPIO_PinState HAL_GPIO_ReadPin(GPIO_TypeDef* GPIOx, uint16_t GPIO_Pin) {
    int port = gpio_port_index((uintptr_t)GPIOx);
    GPIO_PinState state = (gpio_pin_states[port] & GPIO_Pin) ? GPIO_PIN_SET : GPIO_PIN_RESET;

    char payload[256];
    snprintf(payload, sizeof(payload),
             "\"port\":\"0x%08X\",\"pin\":%u,\"result\":\"%s\"",
             (uint32_t)(uintptr_t)GPIOx, GPIO_Pin,
             state == GPIO_PIN_SET ? "SET" : "RESET");
    trace_log("HAL_GPIO_ReadPin", payload);

    return state;
}

void HAL_GPIO_TogglePin(GPIO_TypeDef* GPIOx, uint16_t GPIO_Pin) {
    int port = gpio_port_index((uintptr_t)GPIOx);
    gpio_pin_states[port] ^= GPIO_Pin;

    GPIO_PinState new_state = (gpio_pin_states[port] & GPIO_Pin) ? GPIO_PIN_SET : GPIO_PIN_RESET;
    char payload[256];
    snprintf(payload, sizeof(payload),
             "\"port\":\"0x%08X\",\"pin\":%u,\"new_state\":\"%s\"",
             (uint32_t)(uintptr_t)GPIOx, GPIO_Pin,
             new_state == GPIO_PIN_SET ? "SET" : "RESET");
    trace_log("HAL_GPIO_TogglePin", payload);
}