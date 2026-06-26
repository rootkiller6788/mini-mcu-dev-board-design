/*
 * timer.h — General-purpose and System Timer for ARM Cortex-M MCUs
 *
 * Knowledge Mapping (SKILL.md L1-L6):
 *   L1 Definitions: Timer, counter, prescaler, auto-reload,
 *                   period, duty cycle, dead-time
 *   L2 Concepts:    Up/down/center-aligned counting, input capture,
 *                   output compare, PWM generation, one-pulse mode
 *   L3 Math:        f_timer = f_clk / (PSC + 1)
 *                   T_period = (ARR + 1) / f_timer
 *                   Duty cycle (%) = (CCR / (ARR + 1)) * 100
 *                   Center-aligned: T = 2 × (ARR + 1) / f_timer
 *   L4 Laws:        Nyquist-Shannon applied to timer capture:
 *                   f_signal < f_timer / 2 for unambiguous edge detection
 *   L5 Algorithms:  PWM with dead-time insert (complementary outputs),
 *                   frequency measurement by input capture on two edges,
 *                   phase-correct PWM for motor control
 *   L6 Problems:    LED brightness control via PWM, servo motor
 *                   positioning, ultrasonic sensor ranging via echo pulse
 *
 * Course Mapping:
 *   MIT 6.302 Feedback Systems — timing loops
 *   Berkeley EE16B — timers as embedded peripherals
 *   Valvano "Embedded Systems" Ch.6 — Timer input capture/output compare
 *   Sedra & Smith §17 — Timer-based waveform generation
 *
 * Reference: STM32F4xx Reference Manual RM0090 §13–§17 Timers
 */

#ifndef TIMER_H
#define TIMER_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* ──────────────────────────────────────────────
 * Timer instance identifiers
 * ────────────────────────────────────────────── */
typedef enum {
    TIMER_TIM1   = 0,   /* Advanced 16-bit, motor control  */
    TIMER_TIM2   = 1,   /* General 32-bit                  */
    TIMER_TIM3   = 2,   /* General 16-bit                  */
    TIMER_TIM4   = 3,   /* General 16-bit                  */
    TIMER_TIM5   = 4,   /* General 32-bit                  */
    TIMER_TIM6   = 5,   /* Basic 16-bit (no I/O)           */
    TIMER_TIM7   = 6,   /* Basic 16-bit (no I/O)           */
    TIMER_NUM_TIMERS
} timer_index_t;

/* ──────────────────────────────────────────────
 * L1 Definitions: Timer counting modes
 *
 * Up:     CNT increments from 0 to ARR, then resets to 0.
 *         Good for periodic interrupts and PWM with rising edge at 0.
 *
 * Down:   CNT decrements from ARR to 0, then reloads ARR.
 *         Used when the PWM falling edge should be aligned.
 *
 * Center: CNT counts 0→ARR→0. Symmetric PWM, less EMI.
 *         T_center = 2 × T_up. Used in motor control for
 *         reduced harmonics and dead-time insertion.
 *         Reference: Erickson & Maksimovic §18 — PWM modulation schemes
 * ────────────────────────────────────────────── */
typedef enum {
    TIMER_MODE_UP       = 0x0,
    TIMER_MODE_DOWN     = 0x1,
    TIMER_MODE_CENTER1  = 0x2,  /* Center-aligned, interrupt on underflow */
    TIMER_MODE_CENTER2  = 0x3,  /* Center-aligned, interrupt on overflow  */
    TIMER_MODE_CENTER3  = 0x4   /* Center-aligned, interrupt on both      */
} timer_count_mode_t;

/* ──────────────────────────────────────────────
 * Timer Register Map (representative of STM32 TIM2-TIM5)
 *
 * Advanced timers (TIM1/TIM8) have additional registers
 * for complementary outputs, break input, and dead-time.
 * ────────────────────────────────────────────── */
typedef struct {
    volatile uint32_t CR1;    /* 0x00: Control Register 1           */
    volatile uint32_t CR2;    /* 0x04: Control Register 2           */
    volatile uint32_t SMCR;   /* 0x08: Slave Mode Control           */
    volatile uint32_t DIER;   /* 0x0C: DMA/Interrupt Enable         */
    volatile uint32_t SR;     /* 0x10: Status Register              */
    volatile uint32_t EGR;    /* 0x14: Event Generation Register    */
    volatile uint32_t CCMR1;  /* 0x18: Capture/Compare Mode 1       */
    volatile uint32_t CCMR2;  /* 0x1C: Capture/Compare Mode 2       */
    volatile uint32_t CCER;   /* 0x20: Capture/Compare Enable       */
    volatile uint32_t CNT;    /* 0x24: Counter Register             */
    volatile uint32_t PSC;    /* 0x28: Prescaler Register           */
    volatile uint32_t ARR;    /* 0x2C: Auto-Reload Register         */
    volatile uint32_t CCR[4]; /* 0x30–0x3C: Capture/Compare Reg 1-4*/
    volatile uint32_t DCR;    /* 0x48: DMA Control (some timers)    */
    volatile uint32_t DMAR;   /* 0x4C: DMA Address (some timers)   */
} timer_regs_t;

/* SR bits */
#define TIMER_SR_UIF   (1U << 0)   /* Update Interrupt Flag */
#define TIMER_SR_CC1IF (1U << 1)   /* Capture/Compare 1 interrupt */
#define TIMER_SR_CC2IF (1U << 2)
#define TIMER_SR_CC3IF (1U << 3)
#define TIMER_SR_CC4IF (1U << 4)

/* DIER bits */
#define TIMER_DIER_UIE (1U << 0)   /* Update interrupt enable */

/* CR1 bits */
#define TIMER_CR1_CEN  (1U << 0)   /* Counter enable */
#define TIMER_CR1_UDIS (1U << 1)   /* Update disable */
#define TIMER_CR1_URS  (1U << 2)   /* Update request source */
#define TIMER_CR1_OPM  (1U << 3)   /* One-pulse mode */
#define TIMER_CR1_ARPE (1U << 7)   /* Auto-reload preload enable */

/* ──────────────────────────────────────────────
 * PWM Channel Configuration
 *
 * L1 Definition: Pulse-Width Modulation
 *   Ton: time output is high
 *   Period: T = (ARR + 1) × (PSC + 1) / f_clk
 *   Duty cycle: D = CCR / (ARR + 1)
 *   Average voltage: V_avg = D × V_dd
 *
 * L2 Concept: Edge-aligned vs Center-aligned PWM
 *   Edge: one edge fixed (usually period start), other edge moves.
 *   Center: both edges symmetric about period center → less harmonics.
 *
 * L5 Algorithm: Phase-correct PWM updates ARR/CCR at period center
 *   to avoid glitch when duty cycle changes mid-cycle.
 * ────────────────────────────────────────────── */
typedef enum {
    PWM_CH1 = 0,
    PWM_CH2 = 1,
    PWM_CH3 = 2,
    PWM_CH4 = 3
} pwm_channel_t;

typedef struct {
    timer_index_t      timer;
    pwm_channel_t      channel;
    uint32_t           frequency_hz;   /* PWM frequency                      */
    uint32_t           duty_percent;   /* Duty cycle 0–100 (%)                */
    uint32_t           timer_clk;      /* Timer clock frequency (after APB)   */
    timer_count_mode_t count_mode;     /* Edge or center aligned              */
    bool               preload_enable; /* Buffer ARR/CCR for glitch-free update */
} pwm_config_t;

/* ──────────────────────────────────────────────
 * Delay / Time-base Configuration
 *
 * A timer configured as a simple time base: periodic interrupts
 * at a fixed rate, used for RTOS tick or software timers.
 *
 * L1 Definition: System Tick
 *   f_tick = f_clk / ((PSC + 1) × (ARR + 1))
 *   For 1 ms tick with 84 MHz clock: PSC=83, ARR=999
 *   (84e6 / (84 × 1000) = 1000 Hz tick rate)
 * ────────────────────────────────────────────── */
typedef struct {
    timer_index_t timer;
    uint32_t      tick_rate_hz;   /* Desired interrupt frequency (Hz)   */
    uint32_t      timer_clk;      /* Timer input clock frequency (Hz)   */
} tick_config_t;

/* ──────────────────────────────────────────────
 * Input Capture Configuration
 *
 * L1 Definition: Input Capture
 *   Records the counter value (timestamp) when a specified edge
 *   occurs on the input pin. Two consecutive captures yield
 *   period = |CNT2 − CNT1| × (PSC + 1) / f_clk.
 *
 * L5 Algorithm: Frequency measurement
 *   Method 1 (direct): f_signal = 1 / period
 *   Method 2 (reciprocal): count pulses in fixed gate time
 *     → better for high frequencies.
 *
 * L6 Problem: Ultrasonic ranging HC-SR04
 *   Trigger: 10 µs pulse. Echo: width ∝ distance.
 *   d = (echo_width_µs) / 58  (cm), since sound travels
 *   343 m/s = 34.3 cm/ms = 0.0343 cm/µs, round trip → /58.
 * ────────────────────────────────────────────── */
typedef enum {
    CAPTURE_EDGE_RISING  = 0x0,
    CAPTURE_EDGE_FALLING = 0x1,
    CAPTURE_EDGE_BOTH    = 0x2
} capture_edge_t;

typedef struct {
    timer_index_t  timer;
    pwm_channel_t  channel;
    capture_edge_t edge;
    uint32_t       prescaler;     /* Typically 0 for µs resolution    */
} capture_config_t;

/* ──────────────────────────────────────────────
 * Dead-Time Insertion (Advanced Timer, L8)
 *
 * Preventing shoot-through in half-bridge/H-bridge circuits:
 * when one transistor turns off, the complementary one must wait
 * for the dead-time before turning on. Otherwise both conduct
 * simultaneously, creating a short from V_dd to V_ss.
 *
 * Dead-time = DTG[7:0] × t_dts
 *   t_dts = t_CK_INT (when CKD=0) or 2×t_CK_INT (CKD=1) or 4×t_CK_INT
 *
 * Reference: Mohan, Undeland & Robbins §22 — Gate drive and dead-time
 * ────────────────────────────────────────────── */
typedef struct {
    uint32_t dead_time_ns;   /* Dead-time in nanoseconds    */
    uint32_t timer_clk;      /* Timer clock frequency (Hz)  */
} dead_time_config_t;

/* ──────────────────────────────────────────────
 * Timer API
 * ────────────────────────────────────────────── */

/*
 * timer_init — configure a timer as a basic time base
 *
 * Sets PSC and ARR for the desired tick rate.
 * UDIS=1 during setup to prevent spurious update events.
 * Enables update interrupt (UIE) in DIER.
 *
 * @param config: tick configuration
 *
 * Complexity: O(1)
 * Reference: STM32F4 §17.3 — Time-base unit
 */
void timer_init(const tick_config_t *config);

/*
 * timer_start — enable the counter
 *
 * Sets the CEN bit in CR1. Counter begins counting from 0.
 *
 * @param timer: timer instance
 */
void timer_start(timer_index_t timer);

/*
 * timer_stop — disable the counter
 *
 * Clears CEN bit. Counter holds its current value.
 *
 * @param timer: timer instance
 */
void timer_stop(timer_index_t timer);

/*
 * timer_get_count — read the current counter value
 *
 * @param timer: timer instance
 * @return current CNT value
 *
 * Note: For a 16-bit timer, the value is 0–65535.
 *       For a 32-bit timer (TIM2/5), value may exceed 16 bits.
 */
uint32_t timer_get_count(timer_index_t timer);

/*
 * timer_set_period — change the auto-reload value
 *
 * If ARPE=1 (preload enabled), the new value takes effect at the
 * next update event, avoiding glitches in PWM or tick timing.
 * For immediate effect with ARPE=1, generate an UG event.
 *
 * @param timer:  timer instance
 * @param period: new period (ARR value)
 *
 * Complexity: O(1)
 */
void timer_set_period(timer_index_t timer, uint32_t period);

/*
 * timer_delay_ms — blocking delay using timer polling
 *
 * Polls the timer's update flag (UIF). Must be configured as a
 * time base first. Assumes 1 ms tick rate.
 *
 * @param timer: timer instance configured for 1 ms ticks
 * @param ms:    delay duration in milliseconds
 *
 * Complexity: O(ms)
 * Implementation: Polls SR.UIF, decrements counter, clears flag each tick.
 *
 * L2 Concept: Busy-wait vs interrupt-driven delay.
 *   Busy-wait: simple, deterministic, but wastes CPU.
 *   Interrupt: allows CPU to sleep, but adds jitter from ISR latency.
 */
void timer_delay_ms(timer_index_t timer, uint32_t ms);

/*
 * timer_delay_us — microsecond delay using counter polling
 *
 * Uses a timer configured at 1 MHz (1 tick = 1 µs).
 * Polls CNT register; wraps correctly with 16-bit counter.
 *
 * @param timer: timer instance at 1 MHz
 * @param us:    delay in microseconds
 *
 * Complexity: O(us)
 *
 * Mathematical basis:
 *   PSC = (f_clk / 1e6) - 1  →  1 MHz tick
 *   ARR = 0xFFFF (free-running 16-bit)
 *   For 84 MHz clock: PSC = 84 - 1 = 83
 */
void timer_delay_us(timer_index_t timer, uint32_t us);

/*
 * pwm_init — configure a PWM output channel
 *
 * Sets CCMR for PWM mode 1 (active when CNT < CCR, default)
 * or PWM mode 2 (active when CNT > CCR, inverted).
 * Configures ARR from frequency, CCR from duty cycle percent.
 *
 * PWM Mode 1 (edge-aligned, up-counting):
 *   Output high when CNT < CCR, low otherwise.
 *   Period = (ARR + 1) × (PSC + 1) / f_clk
 *
 * @param config: PWM configuration
 *
 * Complexity: O(1)
 */
void pwm_init(const pwm_config_t *config);

/*
 * pwm_set_duty — change PWM duty cycle at runtime
 *
 * Writes a new CCR value. If preload is enabled, the change
 * takes effect at the next update event (period boundary),
 * preventing truncated pulses.
 *
 * @param timer:       timer instance
 * @param channel:     channel (1–4)
 * @param duty_percent: duty cycle 0–100 (%)
 * @param arr_value:   current ARR value (to compute CCR)
 */
void pwm_set_duty(timer_index_t timer, pwm_channel_t channel,
                  uint32_t duty_percent, uint32_t arr_value);

/*
 * pwm_start — enable PWM outputs
 *
 * Sets CCER to enable the capture/compare output,
 * then starts the counter.
 *
 * @param timer: timer instance
 */
void pwm_start(timer_index_t timer);

/*
 * pwm_stop — disable PWM outputs
 *
 * Clears CCER bits and optionally stops the counter.
 *
 * @param timer: timer instance
 */
void pwm_stop(timer_index_t timer);

/*
 * capture_init — configure input capture
 *
 * Initializes the timer for input capture on the specified channel.
 * The CCER and CCMR registers are programmed for the chosen edge.
 *
 * @param config: capture configuration
 *
 * Complexity: O(1)
 */
void capture_init(const capture_config_t *config);

/*
 * capture_get_value — read the captured counter value
 *
 * Returns the CCR value latched at the last capture event.
 * For pulse width measurement: capture rising→read→switch to falling→capture→read.
 *   width = (CNT_falling - CNT_rising) × timer_period
 *
 * @param timer:   timer instance
 * @param channel: capture/compare channel
 * @return latched counter value
 *
 * Complexity: O(1)
 */
uint32_t capture_get_value(timer_index_t timer, pwm_channel_t channel);

/*
 * dead_time_calculate — compute the DTG register value
 *
 * Given a desired dead-time and the timer clock frequency,
 * returns the 8-bit DTG value to program into the BDTR register.
 *
 * @param config: dead-time configuration
 * @return 8-bit DTG value (0–255)
 *
 * Complexity: O(1)
 * Reference: STM32F4 §14.4.18 — BDTR register
 */
uint8_t dead_time_calculate(const dead_time_config_t *config);

/*
 * timer_get_regs — return pointer to timer register map
 *
 * @param timer: timer instance
 * @return pointer to timer_regs_t or NULL if invalid
 */
timer_regs_t *timer_get_regs(timer_index_t timer);

#endif /* TIMER_H */
