/*
 * pwm_led.c — L6 Canonical Problem: PWM LED Brightness Control
 *
 * Demonstrates LED brightness control via PWM:
 *   - Breathing effect: sinusoidal brightness sweep
 *   - Discrete levels: 0%, 25%, 50%, 75%, 100% duty
 *   - Center-aligned vs edge-aligned PWM comparison
 *
 * On real hardware (STM32F4 Discovery):
 *   - TIM4_CH1 → PD12 (green LED), AF2
 *   - TIM4_CH2 → PD13 (orange LED), AF2
 *
 * Knowledge:
 *   L1: PWM duty cycle, period, frequency
 *   L2: Edge-aligned vs center-aligned PWM, CCR shadow register
 *   L3: f_PWM = f_clk / ((PSC+1) × (ARR+1))
 *       Duty = CCR / (ARR+1) × 100%
 *   L4: Nyquist for visible flicker: f_PWM > 60 Hz (persistence of vision)
 *       LED luminance: L ∝ I_fwd ≈ I_fwd_rated × duty
 *   L5: Sinusoidal brightness sweep via lookup table
 *   L6: Breathing LED — complete demonstration
 *
 * Course Mapping:
 *   Valvano Ch.6 — PWM for LED
 *   MIT 6.302 — PWM modulation
 *   Erickson & Maksimovic §18 — PWM modulation schemes
 */

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <math.h>
#include "../include/timer.h"
#include "../include/gpio.h"

/* ──────────────────────────────────────────────
 * Brightness lookup table (L5 Algorithm)
 *
 * Precomputed sine wave for breathing effect.
 * sin_table[i] ∈ [0, 100] maps to 0%–100% duty cycle.
 * 256 entries → one full breath cycle.
 * ────────────────────────────────────────────── */

#define BREATH_TABLE_SIZE 256

static uint32_t sin_brightness_table[BREATH_TABLE_SIZE];

/*
 * precompute_breath_table — generate sine lookup
 *
 * sin_table[i] = 50 × (1 + sin(2π × i / N − π/2))
 * This produces a smooth 0→100→0 brightness curve,
 * offset so it starts at 50% (mid-brightness) and
 * breaths up then down.
 */
static void precompute_breath_table(void)
{
    int i;
    double phase;

    for (i = 0; i < BREATH_TABLE_SIZE; i++) {
        phase = (2.0 * 3.141592653589793 * (double)i) / (double)BREATH_TABLE_SIZE
              - 3.141592653589793 / 2.0;
        /* 50 * (1 + sin(phase)) → range [0, 100] */
        double val = 50.0 * (1.0 + sin(phase));
        if (val < 0.0) val = 0.0;
        if (val > 100.0) val = 100.0;
        sin_brightness_table[i] = (uint32_t)(val + 0.5);
    }
}

/* ──────────────────────────────────────────────
 * PWM frequency analysis (L3 Math)
 *
 * Given f_clk = 84 MHz, desired f_PWM = 1 kHz:
 *   Choose PSC = 83 → f_cnt = 84,000,000 / 84 = 1,000,000 Hz = 1 MHz
 *   ARR = f_cnt / f_PWM − 1 = 1,000,000 / 1,000 − 1 = 999
 *
 * Duty cycle resolution: steps = ARR + 1 = 1000
 *   → 0.1% duty resolution. More than enough for LED.
 *
 * Center-aligned: f_PWM = f_cnt / (2 × (ARR + 1))
 *   ARR = 499 for same 1 kHz output.
 * ────────────────────────────────────────────── */

static void pwm_frequency_analysis(void)
{
    uint32_t f_clk = 84000000U;
    uint32_t f_target = 1000U;
    uint32_t psc = 84U - 1U;   /* → 1 MHz counter */
    uint32_t arr_edge = 1000U - 1U;
    uint32_t arr_center = 500U - 1U;

    printf("\n--- PWM Frequency Analysis ---\n");
    printf("f_clk = %u Hz, f_target = %u Hz\n",
           (unsigned)f_clk, (unsigned)f_target);
    printf("PSC = %u → f_cnt = %u Hz\n",
           (unsigned)psc, (unsigned)(f_clk / (psc + 1)));

    printf("Edge-aligned:  ARR = %u → f_PWM = %u Hz\n",
           (unsigned)arr_edge,
           (unsigned)(f_clk / ((psc + 1) * (arr_edge + 1))));
    printf("               Resolution = %u steps (%.1f%% per step)\n",
           (unsigned)(arr_edge + 1),
           100.0 / (double)(arr_edge + 1));

    printf("Center-aligned: ARR = %u → f_PWM = %u Hz\n",
           (unsigned)arr_center,
           (unsigned)(f_clk / (2U * (psc + 1) * (arr_center + 1))));
    printf("                Resolution = %u steps (%.2f%% per step)\n",
           (unsigned)(arr_center + 1),
           100.0 / (double)(arr_center + 1));
}

/* ──────────────────────────────────────────────
 * LED brightness vs perceived brightness
 *
 * Human perception of brightness is logarithmic
 * (Weber-Fechner law). The perceived brightness L*
 * relates to duty cycle D approximately as:
 *   L* ∝ D^γ  where γ ≈ 0.5 for LED (simplified)
 *
 * So a 50% duty cycle LED appears about 70% as bright
 * as a 100% duty cycle LED. Gamma correction is sometimes
 * applied to the duty lookup table to achieve linear
 * perceived fade.
 * ────────────────────────────────────────────── */

static void brightness_perception_demo(void)
{
    int d;
    double perceived;

    printf("\n--- Brightness Perception (Weber-Fechner) ---\n");
    printf("Duty%%  | Perceived%%  | Notes\n");
    printf("-------+-------------+-------\n");

    for (d = 0; d <= 100; d += 10) {
        if (d == 0) {
            perceived = 0.0;
        } else {
            /* Simplified gamma: perceived = 100 * (duty/100)^0.45 */
            perceived = 100.0 * pow((double)d / 100.0, 0.45);
        }
        printf(" %3d%%  |    %3.0f%%    |%s\n",
               d, perceived,
               d == 0 ? " Off" :
               d == 50 ? " Appears ~70%" :
               d == 100 ? " Full brightness" : "");
    }

    printf("\nGamma correction note: for linear perceived fade,\n");
    printf("  use duty_table[i] = (i/255)^(1/gamma) × 100%%\n");
    printf("  where gamma ≈ 2.2 for display LEDs.\n");
}

/* ──────────────────────────────────────────────
 * Simulated PWM output for host-side demo
 * ────────────────────────────────────────────── */

static void print_brightness_bar(uint32_t duty)
{
    int filled = (int)(duty * 50 / 100);  /* 50 chars max width */
    int i;

    printf("  [");
    for (i = 0; i < 50; i++) {
        if (i < filled) {
            printf("█");
        } else {
            printf("░");
        }
    }
    printf("] %3u%%\n", (unsigned)duty);
}

/* ──────────────────────────────────────────────
 * Main: PWM LED demo
 * ────────────────────────────────────────────── */
int main(void)
{
    pwm_config_t cfg;
    int i, cycle;

    printf("╔══════════════════════════════════════════╗\n");
    printf("║  PWM LED Brightness Control              ║\n");
    printf("║  Breathing + Discrete Levels             ║\n");
    printf("╚══════════════════════════════════════════╝\n");

    /* Precompute breath table */
    precompute_breath_table();

    /* 1. PWM configuration */
    memset(&cfg, 0, sizeof(cfg));
    cfg.timer = TIMER_TIM4;
    cfg.channel = PWM_CH1;
    cfg.frequency_hz = 1000U;
    cfg.duty_percent = 50U;
    cfg.timer_clk = 84000000U;
    cfg.count_mode = TIMER_MODE_UP;
    cfg.preload_enable = true;

    printf("\n[PWM] Timer %d, Channel %d, %u Hz, %u%% initial duty\n",
           (int)cfg.timer, (int)(cfg.channel + 1),
           (unsigned)cfg.frequency_hz, (unsigned)cfg.duty_percent);

    pwm_init(&cfg);
    printf("[PWM] Initialized. Preload = enabled (glitch-free updates)\n");

    /* 2. Frequency analysis */
    pwm_frequency_analysis();

    /* 3. Discrete duty levels demo */
    printf("\n--- Discrete Duty Cycle Levels ---\n");
    {
        uint32_t levels[] = {0, 25, 50, 75, 100};
        for (i = 0; i < 5; i++) {
            pwm_set_duty(cfg.timer, cfg.channel, levels[i], 999U);
            printf("  Set duty to %u%%: ", (unsigned)levels[i]);
            print_brightness_bar(levels[i]);
        }
    }

    /* 4. Breathing effect (2 full cycles, step every 20 ms simulated) */
    printf("\n--- Breathing Effect (sinusoidal, 2 cycles) ---\n");

    for (cycle = 0; cycle < 2; cycle++) {
        printf("\n  Cycle %d:\n", cycle + 1);
        /* Step through 16 evenly-spaced samples for display */
        for (i = 0; i < BREATH_TABLE_SIZE; i += 16) {
            uint32_t duty = sin_brightness_table[i];
            pwm_set_duty(cfg.timer, cfg.channel, duty, 999U);
            print_brightness_bar(duty);
        }
    }

    /* 5. Brightness perception discussion */
    brightness_perception_demo();

    /* 6. Center-aligned PWM comparison */
    printf("\n--- Center-Aligned PWM — Motor/EMI Consideration ---\n");
    cfg.count_mode = TIMER_MODE_CENTER1;
    printf("Switching to center-aligned mode:\n");
    printf("  - Current ripple: ~50%% reduction vs edge-aligned\n");
    printf("  - EMI: reduced harmonics (symmetric switching)\n");
    printf("  - Effective frequency: doubled for same ARR\n");
    printf("  - Dead-time insertion: available (TIM1/TIM8 advanced)\n");

    /* 7. Dead-time example for motor control */
    {
        dead_time_config_t dt_cfg;
        uint8_t dtg;

        dt_cfg.dead_time_ns = 200U;   /* 200 ns dead-time */
        dt_cfg.timer_clk = 168000000U; /* TIM1 at 168 MHz */

        dtg = dead_time_calculate(&dt_cfg);
        printf("\n--- Dead-Time Calculation (H-Bridge Motor Drive) ---\n");
        printf("  f_clk = 168 MHz, Desired DT = 200 ns\n");
        printf("  DTG register = %u\n", (unsigned)dtg);
        printf("  Prevents shoot-through in half-bridge MOSFET drivers.\n");
        printf("  Reference: Mohan, Undeland & Robbins §22.3\n");
    }

    /* 8. Stop PWM */
    pwm_stop(cfg.timer);
    printf("\n[PWM] Stopped. Demo complete.\n");

    return 0;
}
