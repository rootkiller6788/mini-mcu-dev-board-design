/**
 * @file test_battery_life.c
 * @brief Tests for advanced battery life estimation and reliability
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <math.h>
#include "../include/battery_life.h"
#include "../include/coin_cell_battery.h"

#define EPSILON 1e-6
static int tests_run = 0, tests_passed = 0;
#define TEST(n) do { tests_run++; printf("  TEST: %s ... ", n); } while(0)
#define PASS() do { printf("PASSED\n"); tests_passed++; } while(0)

static void test_load_profile_avg(void) {
    TEST("load_profile_avg_current");
    LoadSegment segs[] = {{1.0, 100.0, 25.0, "active"}, {3.0, 10.0, 25.0, "sleep"}};
    LoadProfile prof = {segs, 2, 4.0, "test"};
    double Iavg = load_profile_avg_current(&prof);
    assert(fabs(Iavg - 32.5) < 1.0);
    PASS();
}

static void test_calendar_aging(void) {
    TEST("calendar_aging_loss");
    CalendarAgingModel cal = {0.001, 0.65, 298.15};
    double loss = calendar_aging_loss(&cal, 87600.0, 25.0); /* 10 years */
    assert(loss >= 0.0 && loss <= 1.0);
    PASS();
}

static void test_weibull(void) {
    TEST("weibull_reliability");
    WeibullParams wp = {1000.0, 2.0};
    ReliabilityMetrics m;
    weibull_reliability(&wp, 500.0, &m);
    assert(m.reliability > 0.0 && m.reliability < 1.0);
    assert(m.MTTF_hours > 0.0);
    PASS();

    TEST("weibull_fit");
    double lifetimes[] = {800.0, 900.0, 1000.0, 1100.0, 1200.0};
    WeibullParams fitted;
    weibull_fit(lifetimes, 5, &fitted);
    assert(fitted.eta > 0.0 && fitted.beta > 0.0);
    PASS();
}

static void test_arrhenius(void) {
    TEST("arrhenius_acceleration_factor");
    double af = arrhenius_acceleration_factor(0.65, 333.15, 298.15);
    assert(af > 1.0);
    PASS();

    TEST("arrhenius_project_lifetime");
    double proj = arrhenius_project_lifetime(1000.0, 0.65, 333.15, 298.15);
    assert(proj > 1000.0);
    PASS();
}

static void test_soh(void) {
    TEST("estimate_state_of_health");
    StateOfHealth soh;
    estimate_state_of_health(200.0, 225.0, 20.0, 15.0, &soh);
    assert(soh.SoH_combined_pct > 0.0 && soh.SoH_combined_pct < 100.0);
    PASS();
}

int main(void) {
    printf("\n=== Battery Life Tests ===\n\n");
    test_load_profile_avg();
    test_calendar_aging();
    test_weibull();
    test_arrhenius();
    test_soh();
    printf("\n=== Results: %d/%d tests passed ===\n", tests_passed, tests_run);
    return (tests_passed == tests_run) ? 0 : 1;
}
