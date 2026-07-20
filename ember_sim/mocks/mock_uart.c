/* mock_uart.c — UART peripheral using the simulation kernel and register model */
#include "mock_hal.h"
#include "ember_regs.h"
#include "ember_sim_kernel.h"
#include "trace_log.h"
#include <string.h>
#include <stdio.h>

/* Forward declarations for callbacks */
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart);
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart);
void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart);
void HAL_UART_IRQHandler(UART_HandleTypeDef *huart);

/* Private peripheral functions */
static void uart_tick(EmberPeripheral *p, uint64_t now_us);
static void uart_init_fn(EmberPeripheral *p);
static void uart_irq_handler(void);

/* Register indices */
enum {
    UART_SR, UART_DR, UART_CR1, UART_BRR,
    UART_REG_COUNT
};

/* Bit definitions (simplified) */
#define UART_SR_TXE   (1 << 7)
#define UART_SR_RXNE  (1 << 5)
#define UART_SR_TC    (1 << 6)

#define UART_CR1_UE   (1 << 0)
#define UART_CR1_TE   (1 << 3)
#define UART_CR1_RE   (1 << 2)
#define UART_CR1_TXEIE (1 << 7)
#define UART_CR1_RXNEIE (1 << 5)

/* Internal state */
static EmberRegister  uart_regs[UART_REG_COUNT];
static EmberRegMap    uart_map;
static EmberPeripheral uart_peripheral;

static uint8_t  tx_buf[256];
static uint16_t tx_len, tx_pos;
static uint8_t  rx_buf[256];
static uint16_t rx_len, rx_pos;
static bool     tx_active = false;

/* ----- Public API (for tests) ----- */
void mock_uart_init(void) {
    memset(uart_regs, 0, sizeof(uart_regs));
    uart_regs[UART_SR].name = "SR";   uart_regs[UART_SR].writable_mask = 0xFFFF;
    uart_regs[UART_DR].name = "DR";   uart_regs[UART_DR].writable_mask = 0x00FF;
    uart_regs[UART_CR1].name = "CR1"; uart_regs[UART_CR1].writable_mask = 0xFFFF;
    uart_regs[UART_BRR].name = "BRR"; uart_regs[UART_BRR].writable_mask = 0xFFFF;
    ember_regs_init(&uart_map, "USART2", 0x40004400, uart_regs, UART_REG_COUNT);

    uart_peripheral.name         = "USART2";
    uart_peripheral.base_address = 0x40004400;
    uart_peripheral.irq_number   = 38;
    uart_peripheral.state        = NULL;
    uart_peripheral.init         = uart_init_fn;
    uart_peripheral.tick         = uart_tick;
    uart_peripheral.handle_event = NULL;

    tx_len = tx_pos = 0;
    rx_len = rx_pos = 0;
    tx_active = false;
}

void mock_uart_set_rx(uintptr_t base, const uint8_t *bytes, uint16_t len) {
    (void)base;
    memcpy(rx_buf, bytes, len);
    rx_len = len;
    rx_pos = 0;
}

const uint8_t *mock_uart_get_tx(uintptr_t base, uint16_t *len) {
    (void)base;
    *len = tx_len;
    return tx_buf;
}

/* ----- HAL API functions ----- */
HAL_StatusTypeDef HAL_UART_Init(UART_HandleTypeDef *huart) {
    (void)huart;
    kernel_register_peripheral(&uart_peripheral);
    trace_log("HAL_UART_Init", "\"registered\"");
    return HAL_OK;
}

HAL_StatusTypeDef HAL_UART_Transmit(UART_HandleTypeDef *huart, uint8_t *pData, uint16_t Size, uint32_t Timeout) {
    (void)Timeout;
    memcpy(tx_buf, pData, Size);
    tx_len = Size;
    tx_pos = 0;
    tx_active = true;
    trace_log("HAL_UART_Transmit", "\"started\"");
    return HAL_OK;
}

HAL_StatusTypeDef HAL_UART_Receive(UART_HandleTypeDef *huart, uint8_t *pData, uint16_t Size, uint32_t Timeout) {
    (void)Timeout;
    uint16_t avail = rx_len - rx_pos;
    if (Size > avail) {
        memcpy(pData, rx_buf + rx_pos, avail);
        rx_pos = rx_len;
        return HAL_TIMEOUT;
    }
    memcpy(pData, rx_buf + rx_pos, Size);
    rx_pos += Size;
    return HAL_OK;
}

HAL_StatusTypeDef HAL_UART_Transmit_IT(UART_HandleTypeDef *huart, uint8_t *pData, uint16_t Size) {
    ember_reg_set_bits(&uart_map, UART_CR1, UART_CR1_TXEIE, "enable TXEIE", 0);
    HAL_UART_Transmit(huart, pData, Size, 0);
    return HAL_OK;
}

HAL_StatusTypeDef HAL_UART_Receive_IT(UART_HandleTypeDef *huart, uint8_t *pData, uint16_t Size) {
    (void)pData; (void)Size;
    ember_reg_set_bits(&uart_map, UART_CR1, UART_CR1_RXNEIE, "enable RXNEIE", 0);
    return HAL_OK;
}

/* ----- Peripheral callbacks ----- */
static void uart_tick(EmberPeripheral *p, uint64_t now_us) {
    (void)now_us;

    /* Transmit: one byte per tick (simplified) */
    if (tx_active && (tx_pos < tx_len)) {
        tx_pos++;
        if (tx_pos >= tx_len) {
            tx_active = false;
            ember_reg_set_bits(&uart_map, UART_SR, UART_SR_TXE | UART_SR_TC, "TX complete", 0);
            uint32_t cr1 = ember_reg_read(&uart_map, UART_CR1);
            if (cr1 & UART_CR1_TXEIE) {
                ember_bus_publish(BUS_EVT_UART_TX_DONE, p->base_address, 0, NULL);
            }
        }
    }

    /* Receive: if data available, set RXNE */
    if (rx_pos < rx_len) {
        ember_reg_set_bits(&uart_map, UART_SR, UART_SR_RXNE, "RX byte ready", 0);
        uint32_t cr1 = ember_reg_read(&uart_map, UART_CR1);
        if (cr1 & UART_CR1_RXNEIE) {
            ember_bus_publish(BUS_EVT_UART_RX_DONE, p->base_address, 0, NULL);
        }
    }
}

static void uart_init_fn(EmberPeripheral *p) {
    (void)p;
    nvic_register_handler(38, uart_irq_handler);
}

static void uart_irq_handler(void) {
    UART_HandleTypeDef huart = { .Instance = 0x40004400, .ErrorCode = 0 };
    HAL_UART_IRQHandler(&huart);
}

/* ----- HAL IRQ Handler (called via NVIC dispatcher) ----- */
void HAL_UART_IRQHandler(UART_HandleTypeDef *huart) {
    uint32_t sr  = ember_reg_read(&uart_map, UART_SR);
    uint32_t cr1 = ember_reg_read(&uart_map, UART_CR1);

    if ((sr & UART_SR_TXE) && (cr1 & UART_CR1_TXEIE)) {
        ember_reg_clear_bits(&uart_map, UART_SR, UART_SR_TXE, "clear TXE", 0);
        HAL_UART_TxCpltCallback(huart);
    }
    if (sr & UART_SR_RXNE) {
        ember_reg_clear_bits(&uart_map, UART_SR, UART_SR_RXNE, "clear RXNE", 0);
        if (rx_pos < rx_len) rx_pos++;  /* consume byte (simulates DR read) */
        HAL_UART_RxCpltCallback(huart);
    }
    if (sr & UART_SR_TC) {
        ember_reg_clear_bits(&uart_map, UART_SR, UART_SR_TC, "clear TC", 0);
    }
}

/* Weak default callbacks (overridden by test) */
__attribute__((weak)) void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart) { (void)huart; }
__attribute__((weak)) void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) { (void)huart; }
__attribute__((weak)) void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart) { (void)huart; }