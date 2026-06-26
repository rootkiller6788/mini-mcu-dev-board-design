/**
 * @file test_power.c
 * @brief Tests for power system module.
 */
#include "power_system.h"
#include "stm32_minimal_config.h"
#include <stdio.h>
#include <math.h>
#include <assert.h>

static int tests_run = 0;
static int tests_passed = 0;
#define TEST(name) do { tests_run++; printf("  TEST %s... ", name); } while(0)
#define PASS() do { printf("PASS\n"); tests_passed++; } while(0)
#define FAIL(msg) do { printf("FAIL: %s\n", msg); } while(0)
#define ASSERT_EQ_DBL(a, b, eps) do { if (fabs((a)-(b)) > (eps)) { FAIL("value mismatch"); return; } } while(0)
#define ASSERT_TRUE(cond, msg) do { if (!(cond)) { FAIL(msg); return; } } while(0)

static void test_validate_power_spec(void) {
    TEST("validate_power_spec F103");
    PowerSpec spec = {VOLTAGE_DOMAIN_VDD, 3.3, 2.0, 3.6, 0.15, 0.05, 0};
    ASSERT_TRUE(validate_power_spec(&spec, STM32_SERIES_F1), "F103 VDD should pass");
    PASS();

    TEST("validate_power_spec F103 undervoltage");
    spec.nominal_voltage = 1.5;
    ASSERT_TRUE(!validate_power_spec(&spec, STM32_SERIES_F1), "Undervoltage should fail");
    PASS();

    TEST("validate_power_spec F407");
    spec.nominal_voltage = 3.3;
    spec.min_voltage = 1.8;
    spec.max_voltage = 3.6;
    ASSERT_TRUE(validate_power_spec(&spec, STM32_SERIES_F4), "F407 VDD should pass");
    PASS();

    TEST("validate_power_spec H7");
    spec.min_voltage = 1.7;
    ASSERT_TRUE(validate_power_spec(&spec, STM32_SERIES_H7), "H7 VDD should pass");
    PASS();

    TEST("validate_power_spec VDDA");
    spec.domain = VOLTAGE_DOMAIN_VDDA;
    spec.requires_filtering = 1;
    spec.ripple_tolerance = 0.010;
    ASSERT_TRUE(validate_power_spec(&spec, STM32_SERIES_F4), "VDDA with filter should pass");
    PASS();

    TEST("validate_power_spec null");
    ASSERT_TRUE(!validate_power_spec(NULL, STM32_SERIES_F1), "NULL should fail");
    PASS();
}

static void test_estimate_mcu_power(void) {
    TEST("estimate_mcu_power F103@72MHz");
    double p = estimate_mcu_power(3.3, 72e6, 5);
    ASSERT_TRUE(p > 0.05 && p < 0.5, "Power should be reasonable");
    ASSERT_EQ_DBL(p, 3.3 * (0.005 + 0.00028*72 + 5*0.0003), 0.01);
    PASS();

    TEST("estimate_mcu_power zero freq");
    ASSERT_EQ_DBL(estimate_mcu_power(3.3, 0, 0), 0.0, 0.001);
    PASS();
}

static void test_size_bulk_capacitance(void) {
    TEST("size_bulk_capacitance 100mA step");
    double c = size_bulk_capacitance(0.1, 50, 50);
    ASSERT_TRUE(c > 0, "Capacitance should be positive");
    ASSERT_TRUE(c < 0.01, "Capacitance should be reasonable");
    PASS();
}

static void test_compute_psrr_attenuation(void) {
    TEST("compute_psrr_attenuation");
    double atten = compute_psrr_attenuation(60, 1e-6, 10, 100000);
    ASSERT_TRUE(atten < 60, "Filter should add attenuation");
    PASS();
}

static void test_ldo_headroom_check(void) {
    TEST("ldo_headroom_check sufficient");
    LDOSpec ldo = {5.0, 3.3, 0.3, 1.0, 0.0001, 60, 30, 0};
    ASSERT_TRUE(ldo_headroom_check(&ldo, 4.0), "4V input sufficient for 3.3V output");
    PASS();

    TEST("ldo_headroom_check insufficient");
    ASSERT_TRUE(!ldo_headroom_check(&ldo, 3.5), "3.5V insufficient for 3.3V+0.3V");
    PASS();
}

static void test_nrst_rc_delay(void) {
    TEST("nrst_rc_delay basic");
    double t = nrst_rc_delay(10000, 100e-9, 3.3, 1.65);
    ASSERT_TRUE(t > 0, "Delay should be positive");
    ASSERT_TRUE(t < 0.01, "Delay should be reasonable ms range");
    PASS();
}

int main(void) {
    printf("Running power system tests...\n");
    test_validate_power_spec();
    test_estimate_mcu_power();
    test_size_bulk_capacitance();
    test_compute_psrr_attenuation();
    test_ldo_headroom_check();
    test_nrst_rc_delay();
    printf("Results: %d/%d passed\n", tests_passed, tests_run);
    return (tests_passed == tests_run) ? 0 : 1;
}
