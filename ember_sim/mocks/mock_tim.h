#ifndef MOCK_TIM_H
#define MOCK_TIM_H

#include <stdint.h>

/* Initialize the TIM2 peripheral model. Must be called before any HAL_TIM_* functions. */
void mock_tim_init(void);

/* Set the update period (in ember ticks) for a timer instance.
   One tick = 1 µs unless changed by the scheduler. */
void mock_tim_set_period_ticks(uintptr_t tim_base, uint32_t ticks);

/* Set the channel duty cycle for PWM (0 = 0%, period = 100%) */
void mock_tim_set_pwm_duty(uintptr_t tim_base, uint32_t channel, uint32_t duty);

/* Inject a captured value for a channel (input capture) */
void mock_tim_inject_capture(uintptr_t tim_base, uint32_t channel, uint32_t value);

/* Call this from the host loop to advance timer state */
void ember_sim_tim_tick(void);

uint32_t mock_tim_get_sr(uintptr_t base);

#endif