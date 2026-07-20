/* mock_tim.c — Timer peripheral using the simulation kernel and register model */
#include "mock_hal.h"
#include "mock_tim.h"
#include "ember_regs.h"
#include "ember_sim_kernel.h"
#include "trace_log.h"
#include <string.h>
#include <stdio.h>

/* Forward declarations for callbacks (used by HAL_TIM_IRQHandler) */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim);
void HAL_TIM_PWM_PulseFinishedCallback(TIM_HandleTypeDef *htim);
void HAL_TIM_IC_CaptureCallback(TIM_HandleTypeDef *htim);
void HAL_TIM_IRQHandler(TIM_HandleTypeDef *htim);

/* Private peripheral functions */
static void tim_tick(EmberPeripheral *p, uint64_t now_us);
static void tim_init_fn(EmberPeripheral *p);
static void tim_irq_handler(void);

/* Register indices */
enum {
    IDX_CR1, IDX_SR, IDX_CNT, IDX_PSC, IDX_ARR, IDX_DIER,
    IDX_CCR1, IDX_CCR2, IDX_CCR3, IDX_CCR4,
    REG_COUNT
};

#define TIM_DIER_UIE   (1 << 0)
#define TIM_DIER_CC1IE (1 << 1)
#define TIM_DIER_CC2IE (1 << 2)
#define TIM_DIER_CC3IE (1 << 3)
#define TIM_DIER_CC4IE (1 << 4)

#define TIM_SR_UIF    (1 << 0)
#define TIM_SR_CC1IF  (1 << 1)
#define TIM_SR_CC2IF  (1 << 2)
#define TIM_SR_CC3IF  (1 << 3)
#define TIM_SR_CC4IF  (1 << 4)

static EmberRegister  tim_regs[REG_COUNT];
static EmberRegMap    tim_map;
static EmberPeripheral tim_peripheral;
static bool           pwm_active = false;

/* ----- Public API (for tests) ----- */
void mock_tim_init(void) {
    memset(tim_regs, 0, sizeof(tim_regs));
    tim_regs[IDX_CR1].name = "CR1"; tim_regs[IDX_CR1].writable_mask = 0x0001;
    tim_regs[IDX_SR].name  = "SR";  tim_regs[IDX_SR].writable_mask  = 0x001F;
    tim_regs[IDX_CNT].name = "CNT"; tim_regs[IDX_CNT].writable_mask = 0xFFFF;
    tim_regs[IDX_PSC].name = "PSC"; tim_regs[IDX_PSC].writable_mask = 0xFFFF;
    tim_regs[IDX_ARR].name = "ARR"; tim_regs[IDX_ARR].writable_mask = 0xFFFF;
    tim_regs[IDX_DIER].name = "DIER"; tim_regs[IDX_DIER].writable_mask = 0xFFFF;
    ember_regs_init(&tim_map, "TIM2", 0x40000400, tim_regs, REG_COUNT);

    tim_peripheral.name         = "TIM2";
    tim_peripheral.base_address = 0x40000400;
    tim_peripheral.irq_number   = 28;
    tim_peripheral.state        = NULL;
    tim_peripheral.init         = tim_init_fn;
    tim_peripheral.tick         = tim_tick;
    tim_peripheral.handle_event = NULL;   /* NVIC drives callbacks directly */
}

void mock_tim_set_period_ticks(uintptr_t base, uint32_t us) {
    ember_reg_write(&tim_map, IDX_ARR, us, "set period", 0);
}

void mock_tim_set_pwm_duty(uintptr_t base, uint32_t channel, uint32_t duty) {
    if (channel <= 4) ember_reg_write(&tim_map, IDX_CCR1 + channel - 1, duty, "set duty", 0);
}

void mock_tim_inject_capture(uintptr_t base, uint32_t channel, uint32_t value) {
    if (channel <= 4) {
        ember_reg_write(&tim_map, IDX_CCR1 + channel - 1, value, "capture inject", 0);
        ember_reg_set_bits(&tim_map, IDX_SR, 0x0002 << (channel-1), "capture", 0);
        /* Trigger NVIC via bus event */
        ember_bus_publish(BUS_EVT_TIMER_UPDATE, base, 0, NULL);
    }
}

uint32_t mock_tim_get_sr(uintptr_t base) {
    (void)base;
    return ember_reg_read(&tim_map, IDX_SR);
}

/* ----- HAL API functions ----- */
HAL_StatusTypeDef HAL_TIM_Base_Init(TIM_HandleTypeDef *htim) {
    (void)htim;
    if (tim_peripheral.base_address == 0) mock_tim_init();
    kernel_register_peripheral(&tim_peripheral);
    trace_log("HAL_TIM_Base_Init", "\"registered\"");
    return HAL_OK;
}

HAL_StatusTypeDef HAL_TIM_Base_Start(TIM_HandleTypeDef *htim) {
    ember_reg_set_bits(&tim_map, IDX_CR1, 0x0001, "start", 0);
    char payload[128];
    snprintf(payload, sizeof(payload), "\"inst\":\"0x%08X\"", (uint32_t)htim->Instance);
    trace_log("HAL_TIM_Base_Start", payload);
    return HAL_OK;
}

HAL_StatusTypeDef HAL_TIM_Base_Stop(TIM_HandleTypeDef *htim) {
    ember_reg_clear_bits(&tim_map, IDX_CR1, 0x0001, "stop", 0);
    char payload[128];
    snprintf(payload, sizeof(payload), "\"inst\":\"0x%08X\"", (uint32_t)htim->Instance);
    trace_log("HAL_TIM_Base_Stop", payload);
    return HAL_OK;
}

HAL_StatusTypeDef HAL_TIM_Base_Start_IT(TIM_HandleTypeDef *htim) {
    ember_reg_set_bits(&tim_map, IDX_CR1, 0x0001, "start IT", 0);
    ember_reg_set_bits(&tim_map, IDX_DIER, TIM_DIER_UIE, "UIE enable", 0);
    return HAL_OK;
}

HAL_StatusTypeDef HAL_TIM_Base_Stop_IT(TIM_HandleTypeDef *htim) {
    ember_reg_clear_bits(&tim_map, IDX_DIER, TIM_DIER_UIE, "UIE disable", 0);
    pwm_active = false;   // also reset PWM state
    return HAL_TIM_Base_Stop(htim);
}

HAL_StatusTypeDef HAL_TIM_PWM_Start(TIM_HandleTypeDef *htim, uint32_t Channel) {
    (void)Channel;
    pwm_active = true;
    return HAL_TIM_Base_Start(htim);
}

HAL_StatusTypeDef HAL_TIM_PWM_Stop(TIM_HandleTypeDef *htim, uint32_t Channel) {
    (void)Channel;
    pwm_active = false;
    return HAL_TIM_Base_Stop(htim);
}

HAL_StatusTypeDef HAL_TIM_IC_Start_IT(TIM_HandleTypeDef *htim, uint32_t Channel) {
    ember_reg_set_bits(&tim_map, IDX_DIER, (TIM_DIER_CC1IE << (Channel - 1)), "IC enable", 0);
    return HAL_TIM_Base_Start(htim);
}

/* ----- Peripheral callbacks (tick + init) ----- */
static void tim_tick(EmberPeripheral *p, uint64_t now_us) {
    (void)now_us;
    uint32_t cr1 = ember_reg_read(&tim_map, IDX_CR1);
    if (!(cr1 & 1)) return;   /* counter disabled */

    uint32_t cnt = ember_reg_read(&tim_map, IDX_CNT);
    uint32_t arr = ember_reg_read(&tim_map, IDX_ARR);
    cnt++;

    if (cnt >= arr) {
        /* --- overflow --- */
        cnt = 0;
        ember_reg_write(&tim_map, IDX_CNT, cnt, "overflow", 0);
        ember_reg_set_bits(&tim_map, IDX_SR, TIM_SR_UIF, "UIF set", 0);

        /* Publish bus event ONLY if the update interrupt is enabled */
        uint32_t dier = ember_reg_read(&tim_map, IDX_DIER);
        if (dier & TIM_DIER_UIE) {
            ember_bus_publish(BUS_EVT_TIMER_UPDATE, p->base_address, 0, NULL);
        }
    } else {
        /* --- normal increment --- */
        ember_reg_write(&tim_map, IDX_CNT, cnt, "increment", 0);
    }
}

static void tim_init_fn(EmberPeripheral *p) {
    (void)p;
    nvic_register_handler(28, tim_irq_handler);
}

static void tim_irq_handler(void) {
    TIM_HandleTypeDef htim = { .Instance = 0x40000400, .ErrorCode = 0 };
    HAL_TIM_IRQHandler(&htim);
}

/* ----- HAL IRQ Handler (called via NVIC dispatcher) ----- */
void HAL_TIM_IRQHandler(TIM_HandleTypeDef *htim) {
    char details[128];
    snprintf(details, sizeof(details),
             "{\"instance\":\"TIM2\",\"address\":\"0x%08X\"}", (uint32_t)htim->Instance);
    trace_software_event("TIM", "irq_handler", details);

    uint32_t sr   = ember_reg_read(&tim_map, IDX_SR);
    uint32_t dier = ember_reg_read(&tim_map, IDX_DIER);

    /* Update interrupt */
    if ((sr & 0x0001) && (dier & TIM_DIER_UIE)) {
        ember_reg_clear_bits(&tim_map, IDX_SR, 0x0001, "HAL cleared UIF", 0);
        HAL_TIM_PeriodElapsedCallback(htim);
        if (pwm_active) {
            HAL_TIM_PWM_PulseFinishedCallback(htim);
        }
    }

    /* Capture/compare interrupts */
    for (int ch = 0; ch < 4; ch++) {
        uint32_t flag = TIM_SR_CC1IF << ch;
        if ((sr & flag) && (dier & (TIM_DIER_CC1IE << ch))) {
            ember_reg_clear_bits(&tim_map, IDX_SR, flag, "HAL cleared CCxIF", 0);
            if (!pwm_active) {
                HAL_TIM_IC_CaptureCallback(htim);
            }
        }
    }
}

/* Weak default callbacks (overridden by test) */
__attribute__((weak)) void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim) { (void)htim; }
__attribute__((weak)) void HAL_TIM_PWM_PulseFinishedCallback(TIM_HandleTypeDef *htim) { (void)htim; }
__attribute__((weak)) void HAL_TIM_IC_CaptureCallback(TIM_HandleTypeDef *htim) { (void)htim; }