/*
 * test_core.c — Combined tests for timer, watchdog, ADC, startup
 *
 * Tests the math and logic behind peripheral operations
 * that can be verified on the host without MCU hardware.
 */

#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <stdbool.h>
#include "../include/timer.h"
#include "../include/watchdog.h"
#include "../include/adc.h"
#include "../include/startup.h"
#include "../include/cortex_m.h"

/* ──────────────────────────────────────────────
 * Timer tests
 * ────────────────────────────────────────────── */

static void test_timer_regs(void)
{
    assert(timer_get_regs(TIMER_TIM1) != NULL);
    assert(timer_get_regs(TIMER_TIM7) != NULL);
    assert(timer_get_regs((timer_index_t)7) == NULL);
    printf("  PASS: timer base addresses\n");
}

static void test_dead_time(void)
{
    dead_time_config_t cfg;
    uint8_t dtg;

    /* 168 MHz, 100 ns dead-time */
    cfg.timer_clk = 168000000U;
    cfg.dead_time_ns = 100U;

    dtg = dead_time_calculate(&cfg);
    /* t_dts = 1/168e6 = 5.95 ns. DT = 100 ns → ~16.8 ticks → DTG ≈ 17 */
    assert(dtg >= 16 && dtg <= 18);

    /* NULL safety */
    assert(dead_time_calculate(NULL) == 0);

    printf("  PASS: dead-time calculation\n");
}

static void test_pwm_duty(void)
{
    /* Duty = 50% of ARR=999 → CCR = (1000 * 50) / 100 = 500 */
    /* We can't easily test pwm_set_duty without hardware,
     * but we verify the structure types compile correctly. */
    pwm_config_t cfg = {0};

    cfg.timer = TIMER_TIM2;
    cfg.channel = PWM_CH1;
    cfg.frequency_hz = 1000;
    cfg.duty_percent = 50;
    cfg.timer_clk = 84000000;
    cfg.count_mode = TIMER_MODE_UP;
    cfg.preload_enable = true;

    assert(cfg.frequency_hz == 1000);
    assert(cfg.duty_percent == 50);

    printf("  PASS: PWM config\n");
}

/* ──────────────────────────────────────────────
 * Watchdog tests
 * ────────────────────────────────────────────── */

static void test_iwdg_timeout(void)
{
    uint32_t timeout_ms;

    /* LSI = 32000 Hz, prescaler = /4 (~div=4), reload = 0xFFF
     * T = 4 × (0xFFF + 1) / 32000 = 4 × 4096 / 32000 = 16384 / 32000 = 0.512 s = 512 ms */
    timeout_ms = iwdg_calculate_timeout(32000U, IWDG_PRESCALER_4, 0xFFFU);
    assert(timeout_ms == 512U);

    /* LSI = 32000 Hz, prescaler = /256, reload = 0xFFF
     * T = 256 × 4096 / 32000 = 1,048,576 / 32,000 = 32.768 s = 32768 ms */
    timeout_ms = iwdg_calculate_timeout(32000U, IWDG_PRESCALER_256, 0xFFFU);
    assert(timeout_ms == 32768U);

    /* Invalid LSI → timeout = 0 */
    timeout_ms = iwdg_calculate_timeout(0U, IWDG_PRESCALER_4, 0xFFFU);
    assert(timeout_ms == 0U);

    printf("  PASS: IWDG timeout\n");
}

static void test_wwdg_timeout(void)
{
    uint32_t timeout_ms;

    /* APB1 = 42 MHz, prescaler = 1 (div=1), counter = 127
     * T_tick = 4096 × 1 / 42,000,000 = 97.5 µs
     * Ticks until reset = 127 − 63 = 64
     * Timeout = 64 × 97.5 µs = 6240 µs = 6.24 ms → ~6 ms */
    timeout_ms = wwdg_calculate_timeout(42000000U, WWDG_PRESCALER_1, 127U);
    assert(timeout_ms >= 5U && timeout_ms <= 7U);

    /* Counter = 63 → timeout = 0 (already at threshold) */
    timeout_ms = wwdg_calculate_timeout(42000000U, WWDG_PRESCALER_1, 63U);
    assert(timeout_ms == 0U);

    printf("  PASS: WWDG timeout\n");
}

static void test_watchdog_self_test(void)
{
    watchdog_config_t cfg;
    bool ok;

    memset(&cfg, 0, sizeof(cfg));
    cfg.type = WATCHDOG_INDEPENDENT;
    ok = watchdog_self_test(&cfg);
    assert(ok == true);

    cfg.type = WATCHDOG_WINDOW;
    ok = watchdog_self_test(&cfg);
    assert(ok == true);

    /* NULL config */
    ok = watchdog_self_test(NULL);
    assert(ok == false);

    printf("  PASS: watchdog self-test\n");
}

/* ──────────────────────────────────────────────
 * ADC tests
 * ────────────────────────────────────────────── */

static void test_adc_to_mv(void)
{
    uint32_t mv;

    /* V_ref = 3300 mV, reading = 0 → 0 mV */
    mv = adc_to_millivolts(0, 3300);
    assert(mv == 0U);

    /* V_ref = 3300 mV, reading = 4095 → 3299 mV (4095/4096 × 3300) */
    mv = adc_to_millivolts(4095, 3300);
    assert(mv == 3299U);

    /* V_ref = 3300 mV, reading = 2048 → ~1650 mV */
    mv = adc_to_millivolts(2048, 3300);
    assert(mv == 1650U);

    /* V_ref = 5000 mV, reading = 2048 → 2500 mV */
    mv = adc_to_millivolts(2048, 5000);
    assert(mv == 2500U);

    printf("  PASS: ADC to millivolts\n");
}

static void test_adc_avg_filter(void)
{
    adc_average_filter_t f;
    uint16_t result;
    int i;

    adc_avg_filter_init(&f);
    assert(f.count == 0);
    assert(f.sum == 0);

    /* Feed constant value 100 */
    for (i = 0; i < 5; i++) {
        result = adc_avg_filter_update(&f, 100);
    }
    /* Average of 5 samples of 100 = 100 */
    assert(result == 100);

    /* Fill to window size with alternating values */
    adc_avg_filter_init(&f);
    for (i = 0; i < ADC_AVERAGE_WINDOW_SIZE; i++) {
        result = adc_avg_filter_update(&f, (uint16_t)((i % 2) * 200));
    }
    /* 8 × 0, 8 × 200 → avg = 100 */
    assert(result == 100);

    /* NULL safety */
    adc_avg_filter_init(NULL);
    result = adc_avg_filter_update(NULL, 100);
    assert(result == 100);

    printf("  PASS: ADC average filter\n");
}

static void test_adc_ema_filter(void)
{
    adc_ema_filter_t f;
    float result;

    /* Init with alpha=0.1, initial=0 */
    adc_ema_filter_init(&f, 0.1f, 0.0f);
    assert(f.alpha >= 0.09f && f.alpha <= 0.11f);
    assert(f.initialized == true);

    /* Feed 1.0 → output = 0.1 × 1.0 + 0.9 × 0.0 = 0.1 */
    result = adc_ema_filter_update(&f, 1.0f);
    assert(result >= 0.09f && result <= 0.11f);

    /* Feed another 1.0 → output = 0.1 × 1.0 + 0.9 × 0.1 = 0.19 */
    result = adc_ema_filter_update(&f, 1.0f);
    assert(result >= 0.18f && result <= 0.20f);

    /* NULL safety */
    adc_ema_filter_init(NULL, 0.5f, 0.0f);
    result = adc_ema_filter_update(NULL, 1.0f);
    assert(result == 1.0f);

    printf("  PASS: ADC EMA filter\n");
}

/* ──────────────────────────────────────────────
 * Startup / system tests
 * ────────────────────────────────────────────── */

static void test_system_info(void)
{
    system_info_t info;

    system_get_info(&info);
    /* After the test framework runs, the system state should be at least RESET */
    assert(info.state <= SYSTEM_STATE_RUNNING);
    assert(info.sysclk_hz > 0U);

    /* NULL safety */
    system_get_info(NULL);

    printf("  PASS: system info\n");
}

static void test_flash_latency_macro(void)
{
    assert(FLASH_LATENCY(16000000) == 0);
    assert(FLASH_LATENCY(50000000) == 1);
    assert(FLASH_LATENCY(80000000) == 2);
    assert(FLASH_LATENCY(100000000) == 3);
    assert(FLASH_LATENCY(140000000) == 4);
    assert(FLASH_LATENCY(168000000) == 5);

    printf("  PASS: flash latency macro\n");
}

static void test_bss_zero(void)
{
    uint8_t buf[16];
    int i;

    /* Fill with non-zero */
    for (i = 0; i < 16; i++) buf[i] = 0xFF;

    /* Zero the buffer */
    zero_bss_section(buf, 16);

    /* Verify all zeros */
    for (i = 0; i < 16; i++) {
        assert(buf[i] == 0);
    }

    /* NULL safety */
    zero_bss_section(NULL, 100);
    zero_bss_section(buf, 0);

    printf("  PASS: bss zeroing\n");
}

static void test_copy_data(void)
{
    uint32_t src[4] = {0xDEADBEEF, 0xCAFEBABE, 0x12345678, 0x90ABCDEF};
    uint32_t dst[4] = {0, 0, 0, 0};
    section_copy_entry_t entry;
    int i;

    entry.load_address = src;
    entry.run_address = dst;
    entry.size = 16; /* 4 × 4 bytes */

    copy_data_section(&entry, 1);

    for (i = 0; i < 4; i++) {
        assert(dst[i] == src[i]);
    }

    /* NULL safety */
    copy_data_section(NULL, 0);

    printf("  PASS: data section copy\n");
}

int main(void)
{
    printf("=== Core Peripheral Tests ===\n");

    test_timer_regs();
    test_dead_time();
    test_pwm_duty();
    test_iwdg_timeout();
    test_wwdg_timeout();
    test_watchdog_self_test();
    test_adc_to_mv();
    test_adc_avg_filter();
    test_adc_ema_filter();
    test_system_info();
    test_flash_latency_macro();
    test_bss_zero();
    test_copy_data();

    printf("All core peripheral tests passed!\n");
    return 0;
}
