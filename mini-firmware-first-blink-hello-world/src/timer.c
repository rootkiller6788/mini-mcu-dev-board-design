/*
 * timer.c — Timer Implementation for ARM Cortex-M MCUs
 *
 * Knowledge points implemented (independent):
 *   1. timer_init — time-base configuration (PSC/ARR for tick rate)
 *   2. timer_delay_ms — SysTick polling delay with update flag
 *   3. timer_delay_us — µs delay with free-running counter
 *   4. pwm_init — PWM mode 1 configuration with frequency/duty calculation
 *   5. pwm_set_duty — duty cycle update with preload
 *   6. capture_init — input capture configuration
 *   7. dead_time_calculate — DTG computation for half-bridge
 *   8. timer_start/stop/set_period/get_count — basic counter control
 */

#include <stdint.h>
#include <stdbool.h>
#include "timer.h"

/* ──────────────────────────────────────────────
 * Timer Base Addresses (STM32F4, APB1/APB2)
 *
 * TIM1:  0x40010000 (APB2, Advanced)
 * TIM2:  0x40000000 (APB1, General 32-bit)
 * TIM3:  0x40000400 (APB1, General 16-bit)
 * TIM4:  0x40000800 (APB1, General 16-bit)
 * TIM5:  0x40000C00 (APB1, General 32-bit)
 * TIM6:  0x40001000 (APB1, Basic 16-bit)
 * TIM7:  0x40001400 (APB1, Basic 16-bit)
 * TIM8:  0x40010400 (APB2, Advanced — not in this simple array)
 * ────────────────────────────────────────────── */

#define TIMER_NUM_INSTANCES 7

static const uint32_t timer_base_addresses[TIMER_NUM_INSTANCES] = {
    0x40010000U,  /* TIM1 */
    0x40000000U,  /* TIM2 */
    0x40000400U,  /* TIM3 */
    0x40000800U,  /* TIM4 */
    0x40000C00U,  /* TIM5 */
    0x40001000U,  /* TIM6 */
    0x40001400U   /* TIM7 */
};

/* Timer is 32-bit or 16-bit? */
static const bool timer_is_32bit[TIMER_NUM_INSTANCES] = {
    false,  /* TIM1: 16-bit advanced */
    true,   /* TIM2: 32-bit general   */
    false,  /* TIM3: 16-bit           */
    false,  /* TIM4: 16-bit           */
    true,   /* TIM5: 32-bit           */
    false,  /* TIM6: 16-bit basic     */
    false   /* TIM7: 16-bit basic     */
};

timer_regs_t *timer_get_regs(timer_index_t timer)
{
    if ((uint8_t)timer >= TIMER_NUM_INSTANCES) {
        return NULL;
    }
    return (timer_regs_t *)(uintptr_t)timer_base_addresses[(uint8_t)timer];
}

/* ──────────────────────────────────────────────
 * timer_init — configure timer as a time base
 *
 * Knowledge: Prescaler + Auto-Reload = tick rate.
 *
 * Timer frequency: f_cnt = f_clk / (PSC + 1)
 * Update event:    f_upd = f_cnt / (ARR + 1)
 *
 * Example: f_clk = 84 MHz, want 1 kHz tick.
 *   Choose PSC = 84 − 1 = 83 (→ f_cnt = 1 MHz)
 *   Then ARR = 1000 − 1 = 999 (→ f_upd = 1 kHz)
 *
 * The update event generates an interrupt if UIE is set in DIER.
 * The counter wraps from ARR to 0 (up-counting) on update.
 *
 * UDIS (Update Disable) is set during configuration to prevent
 * a spurious update event when ARR or PSC is written.
 *
 * Reference: STM32F4 §17.3 — Time-base unit
 * ────────────────────────────────────────────── */

void timer_init(const tick_config_t *config)
{
    timer_regs_t *regs;
    uint32_t prescaler;
    uint32_t period;

    if (config == NULL) {
        return;
    }

    regs = timer_get_regs(config->timer);
    if (regs == NULL) {
        return;
    }

    if (config->tick_rate_hz == 0 || config->timer_clk == 0) {
        return;
    }

    /* Stop the timer during configuration */
    regs->CR1 &= ~TIMER_CR1_CEN;

    /* Disable update during setup */
    regs->CR1 |= TIMER_CR1_UDIS;

    /* Compute prescaler: N such that f_cnt × (ARR+1) = tick_rate.
     * First, pick PSC to get a reasonable f_cnt:
     * f_cnt = f_clk / (PSC + 1)
     *
     * We want ARR to be ≥ 0x100 (256) for reasonable resolution
     * and ≤ 0xFFFF for 16-bit timers. For 32-bit timers, ≤ 0xFFFFFFFF.
     *
     * Strategy: set PSC so that f_cnt = tick_rate × 1000,
     * then ARR = 999 for exactly tick_rate Hz.
     * If timer_clk / (PSC+1) can't hit exactly, we adjust.
     */
    {
        uint32_t max_arr;
        uint32_t fcnt_target;
        uint32_t fcnt;

        max_arr = 0xFFFFFFFFU;  /* default: assume 32-bit timer */
        if (!timer_is_32bit[(uint8_t)config->timer]) {
            max_arr = 0xFFFFU;  /* 16-bit timer */
        }

        /* Target: f_cnt = tick_rate × 1000 for ARR = 999 */
        fcnt_target = config->tick_rate_hz * 1000U;

        if (config->timer_clk >= fcnt_target) {
            /* PSC = (f_clk / fcnt_target) - 1, clamped to 16-bit */
            prescaler = (config->timer_clk / fcnt_target);
            if (prescaler > 0U) {
                prescaler -= 1U;
            }
            if (prescaler > 0xFFFFU) {
                prescaler = 0xFFFFU;  /* Max prescaler */
            }
            fcnt = config->timer_clk / (prescaler + 1U);
            period = (fcnt / config->tick_rate_hz);
            if (period > 0U) {
                period -= 1U;
            }
            if (period > max_arr) {
                period = max_arr;
            }
        } else {
            /* f_clk too slow for target. Set PSC=0, ARR directly. */
            prescaler = 0U;
            period = config->timer_clk / config->tick_rate_hz;
            if (period > 0U) {
                period -= 1U;
            }
            if (period > max_arr) {
                period = max_arr;
            }
        }
    }

    /* Write PSC and ARR */
    regs->PSC = prescaler;
    regs->ARR = period;

    /* Generate an update event to load the shadow registers */
    regs->EGR = 0x01U;  /* UG: Update Generation */

    /* Clear update flag */
    regs->SR &= ~TIMER_SR_UIF;

    /* Re-enable update, enable auto-reload preload */
    regs->CR1 &= ~TIMER_CR1_UDIS;
    regs->CR1 |= TIMER_CR1_ARPE;
}

/* ──────────────────────────────────────────────
 * timer_start — enable counter
 * ────────────────────────────────────────────── */

void timer_start(timer_index_t timer)
{
    timer_regs_t *regs = timer_get_regs(timer);
    if (regs == NULL) {
        return;
    }
    regs->CR1 |= TIMER_CR1_CEN;
}

/* ──────────────────────────────────────────────
 * timer_stop — disable counter
 * ────────────────────────────────────────────── */

void timer_stop(timer_index_t timer)
{
    timer_regs_t *regs = timer_get_regs(timer);
    if (regs == NULL) {
        return;
    }
    regs->CR1 &= ~TIMER_CR1_CEN;
}

/* ──────────────────────────────────────────────
 * timer_get_count — read current counter value
 * ────────────────────────────────────────────── */

uint32_t timer_get_count(timer_index_t timer)
{
    timer_regs_t *regs = timer_get_regs(timer);
    if (regs == NULL) {
        return 0;
    }
    return regs->CNT;
}

/* ──────────────────────────────────────────────
 * timer_set_period — change auto-reload value
 *
 * Knowledge: Preload register (ARPE) for glitch-free updates.
 *
 * When ARPE=1, the ARR value written by software is stored in a
 * shadow register and transferred to the active register only at
 * the update event (counter overflow/underflow). This prevents a
 * truncated or extended cycle when the period is changed mid-count.
 *
 * For immediate effect, generate a software update event (UG)
 * after writing ARR.
 * ────────────────────────────────────────────── */

void timer_set_period(timer_index_t timer, uint32_t period)
{
    timer_regs_t *regs = timer_get_regs(timer);
    if (regs == NULL) {
        return;
    }
    regs->ARR = period;
    /* If ARPE=0, the new ARR takes effect immediately.
     * If ARPE=1, we generate a UG event to force the update. */
    if (regs->CR1 & TIMER_CR1_ARPE) {
        regs->EGR = 0x01U;  /* UG: update generation */
    }
}

/* ──────────────────────────────────────────────
 * timer_delay_ms — blocking millisecond delay
 *
 * Knowledge: Busy-wait on timer update flag.
 *
 * Uses the timer's update interrupt flag (UIF) as a tick indicator.
 * This is a poll-based delay — the CPU is busy-waiting the entire
 * time, consuming 100% of its capacity. Acceptable for short delays
 * (< 100 ms) during initialization; for longer delays, use an
 * interrupt-driven approach to allow the CPU to sleep or do other work.
 *
 * Example: timer configured at 1 kHz → UIF asserts every 1 ms.
 * ────────────────────────────────────────────── */

void timer_delay_ms(timer_index_t timer, uint32_t ms)
{
    timer_regs_t *regs = timer_get_regs(timer);
    if (regs == NULL || ms == 0) {
        return;
    }

    while (ms > 0U) {
        /* Wait for update interrupt flag */
        while ((regs->SR & TIMER_SR_UIF) == 0U) {
            /* Spin. On Cortex-M, a WFI (Wait For Interrupt) instruction
             * could save power here, but for simplicity we spin. */
        }
        /* Clear the flag */
        regs->SR = (uint32_t)(~(TIMER_SR_UIF));
        ms--;
    }
}

/* ──────────────────────────────────────────────
 * timer_delay_us — microsecond delay with free-running counter
 *
 * Knowledge: CNT-polling delay with wrap-around arithmetic.
 *
 * Uses a timer configured at 1 MHz (PSC = f_clk/1e6 − 1), ARR = max.
 * The counter free-runs and wraps at 0xFFFF (16-bit) or 0xFFFFFFFF (32-bit).
 *
 * Delay loop: record start CNT, then poll until:
 *   (current_cnt − start_cnt) % (max_cnt + 1) ≥ target_ticks
 *
 * Unsigned subtraction naturally handles wrap-around:
 *   delta = (current_cnt − start_cnt)  (unsigned, modulo word size)
 *
 * For 16-bit timer at 1 MHz (1 µs/tick):
 *   Max delay without double-wrap: ~65.5 ms → safe for typical µs delays.
 *
 * For 32-bit timer: max delay ~71 minutes → completely safe.
 * ────────────────────────────────────────────── */

void timer_delay_us(timer_index_t timer, uint32_t us)
{
    timer_regs_t *regs = timer_get_regs(timer);
    uint32_t start_cnt;
    uint32_t current_cnt;
    uint32_t delta;
    uint32_t max_cnt;
    uint32_t max_single_poll;

    if (regs == NULL || us == 0) {
        return;
    }

    max_cnt = timer_is_32bit[(uint8_t)timer] ? 0xFFFFFFFFU : 0xFFFFU;

    /* For long delays, break into chunks to avoid double-wrap.
     * Each chunk is limited to max_cnt / 2 (so delta never wraps). */
    max_single_poll = max_cnt / 2U;  /* Safe: delta < max_single_poll always unambiguous */

    while (us > 0U) {
        uint32_t chunk = (us > max_single_poll) ? max_single_poll : us;

        start_cnt = regs->CNT;

        for (;;) {
            current_cnt = regs->CNT;
            delta = current_cnt - start_cnt;  /* Unsigned wrap is correct */
            if (delta >= chunk) {
                break;
            }
        }

        us -= chunk;
        /* Re-read start_cnt in case the loop ran beyond the chunk */
        (void)regs->CNT;
    }
}

/* ──────────────────────────────────────────────
 * pwm_init — configure PWM output
 *
 * Knowledge: PWM waveform generation using output compare.
 *
 * PWM Mode 1 (edge-aligned, up-counting):
 *   - CNT counts 0 → ARR → 0 ...
 *   - Output is HIGH when CNT < CCR
 *   - Output is LOW  when CNT ≥ CCR
 *   - Duty cycle = CCR / (ARR + 1)
 *
 * PWM Mode 2 (edge-aligned, inverted):
 *   - Output is HIGH when CNT > CCR
 *   - Output is LOW  when CNT ≤ CCR
 *
 * Center-aligned PWM (mode CENTER1/CENTER2/CENTER3):
 *   - CNT counts 0 → ARR → 0 ...
 *   - Output symmetric about the period center
 *   - Effective period = 2 × (ARR + 1) × (PSC + 1) / f_clk
 *   - Used in motor control for reduced current ripple and EMI
 *
 * Frequency: f_pwm = f_clk / ((PSC + 1) × (ARR + 1))
 *   for edge-aligned; divide by 2 for center-aligned.
 *
 * CCMR (Capture/Compare Mode Register) channel mode:
 *   OCxM[2:0] = 110 for PWM mode 1, 111 for PWM mode 2.
 *
 * Reference: STM32F4 §17.5.7 — PWM mode
 * ────────────────────────────────────────────── */

void pwm_init(const pwm_config_t *config)
{
    timer_regs_t *regs;
    uint32_t prescaler;
    uint32_t period;
    uint32_t ccr_val;
    uint32_t ccmr_shift;
    uint32_t ccmr_mask;
    volatile uint32_t *ccmr;
    uint8_t ch;

    if (config == NULL) {
        return;
    }

    regs = timer_get_regs(config->timer);
    if (regs == NULL) {
        return;
    }

    if (config->frequency_hz == 0 || config->timer_clk == 0) {
        return;
    }

    ch = (uint8_t)config->channel;

    /* Stop timer during configuration */
    regs->CR1 &= ~TIMER_CR1_CEN;

    /* Compute PSC and ARR from desired PWM frequency.
     *
     * f_cnt = f_clk / (PSC + 1)
     * For edge-aligned: f_pwm = f_cnt / (ARR + 1)
     * For center-aligned: f_pwm = f_cnt / (2 × (ARR + 1))
     *
     * Strategy: choose PSC to make f_cnt a decent range,
     * then compute ARR from f_pwm. */
    {
        uint32_t f_cnt_divider;
        uint32_t f_cnt;

        f_cnt_divider = 1U;
        if (config->count_mode >= TIMER_MODE_CENTER1) {
            f_cnt_divider = 2U;  /* Center-aligned: factor of 2 */
        }

        /* Choose PSC such that ARR is in a reasonable range.
         * Target ARR ≈ (max_arr / 2) for good resolution.
         * max_arr = 65535 for 16-bit, 4294967295 for 32-bit.
         * For simplicity: set PSC so that f_cnt ≈ f_pwm × 2000,
         * giving ARR ≈ 1999 (edge) or 999 (center). */
        {
            uint32_t fcnt_desired = config->frequency_hz * 2000U;
            if (config->timer_clk > fcnt_desired) {
                prescaler = config->timer_clk / fcnt_desired;
                if (prescaler > 0U) prescaler--;
                if (prescaler > 0xFFFFU) prescaler = 0xFFFFU;
            } else {
                prescaler = 0U;
            }
        }

        f_cnt = config->timer_clk / (prescaler + 1U);
        period = f_cnt / (config->frequency_hz * f_cnt_divider);
        if (period > 0U) period--;
        if (period > 0xFFFFU && !timer_is_32bit[(uint8_t)config->timer]) {
            period = 0xFFFFU;
        }
        if (period > 0xFFFFFFFFU) {
            period = 0xFFFFFFFFU;
        }
    }

    /* Compute CCR from duty cycle (0–100 percent) */
    ccr_val = ((uint64_t)(period + 1U) * (uint64_t)config->duty_percent) / 100ULL;
    if (ccr_val > period) {
        ccr_val = period + 1U;  /* 100% duty: always high in PWM mode 2, or clamp */
    }

    /* Configure count mode and preload */
    {
        uint32_t cr1_tmp = regs->CR1;
        /* Clear CMS bits [6:5] and DIR bit [4] */
        cr1_tmp &= ~(0x3U << 5U);
        cr1_tmp &= ~(1U << 4U);
        switch (config->count_mode) {
        case TIMER_MODE_UP:
            /* DIR=0 (up), CMS=00 (edge-aligned) */
            break;
        case TIMER_MODE_DOWN:
            cr1_tmp |= (1U << 4U);  /* DIR=1 */
            break;
        case TIMER_MODE_CENTER1:
            cr1_tmp |= (0x1U << 5U);
            break;
        case TIMER_MODE_CENTER2:
            cr1_tmp |= (0x2U << 5U);
            break;
        case TIMER_MODE_CENTER3:
            cr1_tmp |= (0x3U << 5U);
            break;
        }
        regs->CR1 = cr1_tmp;
    }

    if (config->preload_enable) {
        regs->CR1 |= TIMER_CR1_ARPE;
    }

    /* Write PSC and ARR */
    regs->PSC = prescaler;
    regs->ARR = period;

    /* Configure PWM mode 1 in CCMR.
     * CCMR1 handles channels 1 and 2; CCMR2 handles channels 3 and 4. */
    if (ch < 2U) {
        ccmr = &regs->CCMR1;
        ccmr_shift = (ch == 0U) ? 0U : 8U;  /* OC1 at bits[6:4], OC2 at bits[14:12] */
    } else {
        ccmr = &regs->CCMR2;
        ccmr_shift = (ch == 2U) ? 0U : 8U;  /* OC3 at bits[6:4], OC4 at bits[14:12] */
    }

    /* OCxM[2:0] = 110 = PWM mode 1, Preload enable (OCxPE = bit 3) */
    ccmr_mask = 0x7FU << ccmr_shift;
    {
        uint32_t ccmr_val = *ccmr;
        ccmr_val &= ~ccmr_mask;
        /* PWM mode 1: OCxM = 110 (bits [6:4]), OCxPE = 1 (bit 3, preload enable) */
        ccmr_val |= ((0x6U << 4U) | (1U << 3U)) << ccmr_shift;
        *ccmr = ccmr_val;
    }

    /* Write CCR (capture/compare value = duty cycle) */
    regs->CCR[ch] = ccr_val;

    /* Enable output (CCER): CCxE bit */
    {
        uint32_t ccer_val = regs->CCER;
        ccer_val |= (1U << (ch * 4U));  /* CCxE: output enable */
        /* Clear CCxP (polarity): active high */
        ccer_val &= ~(1U << (ch * 4U + 1U));
        regs->CCER = ccer_val;
    }

    /* Generate update event to load shadow registers */
    regs->EGR = 0x01U;
}

/* ──────────────────────────────────────────────
 * pwm_set_duty — change PWM duty cycle at runtime
 *
 * Knowledge: CCR update with preload.
 *
 * The CCR is the Capture/Compare Register. In PWM mode, it sets
 * the duty cycle threshold. Writing to CCR updates the shadow
 * register immediately; the active register is updated at the
 * next update event (if OCxPE=1, preload enabled).
 *
 * This ensures glitch-free duty cycle changes: the transition
 * occurs at the PWM period boundary, never in the middle of a pulse.
 * ────────────────────────────────────────────── */

void pwm_set_duty(timer_index_t timer, pwm_channel_t channel,
                  uint32_t duty_percent, uint32_t arr_value)
{
    timer_regs_t *regs = timer_get_regs(timer);
    uint32_t ccr_val;
    uint8_t ch;

    if (regs == NULL) {
        return;
    }

    ch = (uint8_t)channel;
    if (ch > 3U) {
        return;
    }

    if (duty_percent > 100U) {
        duty_percent = 100U;
    }

    ccr_val = ((uint64_t)(arr_value + 1U) * (uint64_t)duty_percent) / 100ULL;
    regs->CCR[ch] = ccr_val;
}

/* ──────────────────────────────────────────────
 * pwm_start — enable PWM output and start counter
 * ────────────────────────────────────────────── */

void pwm_start(timer_index_t timer)
{
    timer_regs_t *regs = timer_get_regs(timer);
    if (regs == NULL) {
        return;
    }
    regs->CR1 |= TIMER_CR1_CEN;
}

/* ──────────────────────────────────────────────
 * pwm_stop — disable PWM output and stop counter
 * ────────────────────────────────────────────── */

void pwm_stop(timer_index_t timer)
{
    timer_regs_t *regs = timer_get_regs(timer);
    if (regs == NULL) {
        return;
    }
    regs->CR1 &= ~TIMER_CR1_CEN;
    regs->CCER = 0;  /* Disable all outputs */
}

/* ──────────────────────────────────────────────
 * capture_init — configure input capture
 *
 * Knowledge: Edge-triggered timestamp capture.
 *
 * Input capture records the counter value when a specified edge
 * occurs on the input pin. The captured value is stored in CCR.
 *
 * Hardware configuration:
 *   - CCxS[1:0] in CCMR: 01 = CCx channel configured as input,
 *     ICx mapped to TIx (direct connection).
 *   - Input filter (ICxF[3:0]): debounce/noise filter. N consecutive
 *     samples must agree before edge is recognized. N = 2,4,8.
 *     Uses f_DTS = f_clk (or prescaled).
 *   - Edge selection (CCxP + CCxNP in CCER): rising/falling/both.
 *
 * Frequency measurement example (one channel, both edges):
 *   1. Capture rising edge → t1.
 *   2. Capture falling edge → t2.
 *   3. Pulse width = t2 − t1 (in timer ticks).
 *   4. Capture next rising edge → t3.
 *   5. Period = t3 − t1; frequency = f_cnt / period.
 * ────────────────────────────────────────────── */

void capture_init(const capture_config_t *config)
{
    timer_regs_t *regs;
    uint8_t ch;
    volatile uint32_t *ccmr;
    uint32_t ccmr_shift;
    uint32_t ccmr_mask;
    uint32_t ccmr_val;

    if (config == NULL) {
        return;
    }

    regs = timer_get_regs(config->timer);
    if (regs == NULL) {
        return;
    }

    ch = (uint8_t)config->channel;
    if (ch > 3U) {
        return;
    }

    /* Stop timer */
    regs->CR1 &= ~TIMER_CR1_CEN;

    /* Set prescaler */
    regs->PSC = (uint16_t)config->prescaler;

    /* Configure channel as input capture */
    if (ch < 2U) {
        ccmr = &regs->CCMR1;
        ccmr_shift = (ch == 0U) ? 0U : 8U;
    } else {
        ccmr = &regs->CCMR2;
        ccmr_shift = (ch == 2U) ? 0U : 8U;
    }

    ccmr_mask = 0xFFU << ccmr_shift;
    ccmr_val = *ccmr & ~ccmr_mask;

    /* CCxS[1:0] = 01 (input, ICx mapped to TIx)
     * ICxF[3:0] = 0000 (no filter, fastest capture)
     * ICxPSC[1:0] = 00 (no prescaler, capture on every event) */
    ccmr_val |= (0x01U) << ccmr_shift;  /* CCxS = 01 */
    *ccmr = ccmr_val;

    /* Configure edge polarity in CCER */
    {
        uint32_t ccer_val = regs->CCER;
        uint32_t edge_bits;

        /* Clear CCxP (bit ch*4+1) and CCxNP (bit ch*4+3) */
        ccer_val &= ~(0xAU << (ch * 4U));

        switch (config->edge) {
        case CAPTURE_EDGE_RISING:
            /* CCxP=0, CCxNP=0 → rising edge */
            break;
        case CAPTURE_EDGE_FALLING:
            /* CCxP=1 → falling edge */
            edge_bits = (1U << 1U);
            ccer_val |= edge_bits << (ch * 4U);
            break;
        case CAPTURE_EDGE_BOTH:
            /* CCxP=0, CCxNP=1 → both edges */
            edge_bits = (1U << 3U);
            ccer_val |= edge_bits << (ch * 4U);
            break;
        }

        /* Enable input capture: CCxE = 1 */
        ccer_val |= (1U << (ch * 4U));
        regs->CCER = ccer_val;
    }

    /* Set ARR to max for free-running capture */
    regs->ARR = 0xFFFFFFFFU;

    /* Generate update to load shadow registers */
    regs->EGR = 0x01U;
}

/* ──────────────────────────────────────────────
 * capture_get_value — read the captured counter value
 *
 * Returns the CCR value last captured by a hardware capture event.
 * If no capture has occurred yet, returns 0.
 * ────────────────────────────────────────────── */

uint32_t capture_get_value(timer_index_t timer, pwm_channel_t channel)
{
    timer_regs_t *regs = timer_get_regs(timer);
    uint8_t ch;

    if (regs == NULL) {
        return 0;
    }

    ch = (uint8_t)channel;
    if (ch > 3U) {
        return 0;
    }

    return regs->CCR[ch];
}

/* ──────────────────────────────────────────────
 * dead_time_calculate — compute DTG register value
 *
 * Knowledge: Dead-time insertion for half-bridge converters.
 *
 * In an H-bridge or half-bridge, two complementary transistors
 * (high-side and low-side) must NEVER conduct simultaneously.
 * If they do, a shoot-through current flows directly from V_dd
 * to V_ss, limited only by the transistor on-resistance (typically
 * milliohms), causing immediate destruction.
 *
 * Dead-time is the guaranteed interval where both transistors are
 * off during the switching transition. The advanced timer (TIM1/TIM8)
 * inserts this automatically on complementary outputs.
 *
 * DTG[7:0] encoding (STM32F4 §14.4.18):
 *   DTG[7:5] = 0xx: DT = DTG[7:0] × t_dts,   t_dts = t_CK_INT
 *   DTG[7:5] = 10x: DT = (64 + DTG[5:0]) × 2 × t_dts, t_dts = t_CK_INT
 *   DTG[7:5] = 110: DT = (32 + DTG[4:0]) × 8 × t_dts, t_dts = t_CK_INT
 *   DTG[7:5] = 111: DT = (32 + DTG[4:0]) × 16 × t_dts, t_dts = t_CK_INT
 *
 * e.g., f_clk = 168 MHz, t_dts = 5.95 ns, desired DT = 100 ns:
 *   DTG = 100 / 5.95 ≈ 16.8 → 17 (range 0xx)
 *
 * Reference: Mohan, Undeland & Robbins §22.3 — Gate drive circuitry
 * ────────────────────────────────────────────── */

uint8_t dead_time_calculate(const dead_time_config_t *config)
{
    uint32_t t_dts_ps;  /* t_dts in picoseconds */
    uint32_t dt_ps;      /* Desired dead-time in picoseconds */
    uint32_t ticks;
    uint32_t dtg;

    if (config == NULL || config->timer_clk == 0 || config->dead_time_ns == 0) {
        return 0;
    }

    /* t_dts = 1 / f_clk [seconds] = 1e12 / f_clk [picoseconds] */
    t_dts_ps = 1000000000000ULL / (uint64_t)config->timer_clk;

    /* Desired dead-time in picoseconds */
    dt_ps = (uint32_t)config->dead_time_ns * 1000UL;

    /* Direct range: DTG[7:5] = 0xx, DT = DTG × t_dts, max = 127 × t_dts */
    ticks = dt_ps / t_dts_ps;

    if (ticks < 128U) {
        dtg = ticks;
        if (dtg > 127U) dtg = 127U;
        return (uint8_t)dtg;
    }

    /* Range: DTG[7:5] = 10x, DT = (64 + DTG[5:0]) × 2 × t_dts */
    ticks = dt_ps / (2UL * t_dts_ps);
    if (ticks < 128U) {
        dtg = ticks;
        if (dtg < 64U) dtg = 64U;
        if (dtg > 127U) dtg = 127U;
        dtg = (dtg & 0x3FU) | 0x80U;  /* Set bits [7:5] = 10x */
        return (uint8_t)dtg;
    }

    /* Range: DTG[7:5] = 110, DT = (32 + DTG[4:0]) × 8 × t_dts */
    ticks = dt_ps / (8UL * t_dts_ps);
    if (ticks < 64U) {
        dtg = ticks;
        if (dtg < 32U) dtg = 32U;
        if (dtg > 63U) dtg = 63U;
        dtg = (dtg & 0x1FU) | 0xC0U;  /* Set bits [7:5] = 110 */
        return (uint8_t)dtg;
    }

    /* Range: DTG[7:5] = 111, DT = (32 + DTG[4:0]) × 16 × t_dts, max = 1008 × t_dts */
    ticks = dt_ps / (16UL * t_dts_ps);
    dtg = ticks;
    if (dtg < 32U) dtg = 32U;
    if (dtg > 63U) dtg = 63U;
    dtg = (dtg & 0x1FU) | 0xE0U;  /* Set bits [7:5] = 111 */
    return (uint8_t)dtg;
}
