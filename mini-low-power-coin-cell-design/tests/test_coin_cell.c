/**
 * @file test_coin_cell.c
 * @brief Assertion-based tests for coin cell battery models
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <math.h>
#include "../include/coin_cell_battery.h"

#define EPSILON 1e-9

static int tests_run = 0, tests_passed = 0;

#define TEST(name) do { tests_run++; printf("  TEST: %s ... ", name); } while(0)
#define PASS() do { printf("PASSED\n"); tests_passed++; } while(0)
#define FAIL(msg) do { printf("FAILED: %s\n", msg); } while(0)

static void test_params_lookup(void) {
    TEST("coin_cell_get_params");
    const CoinCellParams *p = coin_cell_get_params(CELL_CR2032);
    assert(p != NULL);
    assert(fabs(p->C_nominal_mAh - 225.0) < 1.0);
    assert(fabs(p->V_nominal_V - 3.0) < EPSILON);
    assert(fabs(p->V_cutoff_V - 2.0) < EPSILON);
    PASS();
}

static void test_shepherd_model(void) {
    TEST("shepherd_model_init + voltage");
    const CoinCellParams *p = coin_cell_get_params(CELL_CR2032);
    ShepherdModel sm;
    shepherd_model_init(p, &sm);
    double V_full = shepherd_voltage(&sm, 0.0, 0.0);
    assert(V_full > 2.9 && V_full < 3.3);
    double V_half = shepherd_voltage(&sm, sm.Q_Ah * 0.5, 0.001);
    assert(V_half > 2.5 && V_half < 3.2);
    double V_empty = shepherd_voltage(&sm, sm.Q_Ah, 0.0);
    assert(fabs(V_empty - sm.V_cutoff_V) < EPSILON);
    PASS();
}

static void test_peukert_law(void) {
    TEST("peukert_capacity k=1.0 (ideal)");
    double C = peukert_capacity(225.0, 0.2, 0.2, 1.0);
    assert(fabs(C - 225.0) < EPSILON);
    PASS();

    TEST("peukert_capacity k>1 (derating)");
    double C2 = peukert_capacity(225.0, 0.2, 2.0, 1.1);
    assert(C2 < 225.0);
    PASS();

    TEST("peukert_discharge_time");
    double t = peukert_discharge_time(225.0, 0.2, 0.1, 1.02);
    assert(t > 225.0 / 0.1); /* Longer than naive due to low-rate gain */
    PASS();
}

static void test_voltage_under_load(void) {
    TEST("terminal_voltage_under_load");
    double V = terminal_voltage_under_load(3.0, 10.0, 15.0);
    assert(fabs(V - 2.85) < 0.01);
    PASS();

    TEST("max_current_at_cutoff");
    double Imax = max_current_at_cutoff(3.0, 2.0, 15.0);
    assert(fabs(Imax - 66.667) < 0.1);
    PASS();
}

static void test_self_discharge(void) {
    TEST("self_discharge_remaining 12 months");
    double rem = self_discharge_remaining(225.0, 0.2, 12.0);
    assert(rem < 225.0 && rem > 215.0);
    PASS();

    TEST("arrhenius_self_discharge_rate");
    ArrheniusSelfDischarge arr = {0.65, 0.2, 298.15};
    double rate_60C = arrhenius_self_discharge_rate(&arr, 60.0);
    assert(rate_60C > 0.2); /* Higher at elevated temperature */
    PASS();
}

static void test_coulomb_counter(void) {
    TEST("coulomb_counter basic");
    CoulombCounter cc;
    coulomb_counter_init(&cc, 225.0);
    assert(fabs(coulomb_counter_get_soc(&cc) - 1.0) < EPSILON);
    coulomb_counter_update(&cc, 0.01, 3600.0); /* 10uA = 0.01mA for 1 hour */
    double soc = coulomb_counter_get_soc(&cc);
    assert(soc < 1.0 && soc > 0.999);
    PASS();

    TEST("coulomb_counter reset");
    coulomb_counter_reset(&cc, 100.0);
    assert(fabs(coulomb_counter_get_soc(&cc) - 1.0) < EPSILON);
    PASS();
}

static void test_ekf_soc(void) {
    TEST("ekf_soc init and get");
    EKF_SoCEstimator ekf;
    ekf_soc_init(&ekf, 0.225, 15.0, 1.0);
    assert(fabs(ekf_soc_get_soc(&ekf) - 1.0) < EPSILON);
    double unc = ekf_soc_get_uncertainty(&ekf);
    assert(unc > 0.0);
    PASS();
}

static void test_discharge_lut(void) {
    TEST("discharge_lut_interpolate");
    double soc_pts[] = {0.0, 0.5, 1.0};
    double ocv_pts[] = {2.0, 2.8, 3.0};
    double r_pts[]   = {60.0, 25.0, 15.0};
    DischargeLUT lut = {soc_pts, ocv_pts, r_pts, 3};
    InterpResult r = discharge_lut_interpolate(&lut, 0.75);
    assert(r.valid);
    assert(fabs(r.value - 2.9) < 0.01);
    PASS();
}

static void test_internal_resistance(void) {
    TEST("internal_resistance_vs_soc full");
    double R_full = internal_resistance_vs_soc(15.0, 1.0);
    assert(fabs(R_full - 15.0) < EPSILON);
    PASS();

    TEST("internal_resistance_vs_soc depleted");
    double R_empty = internal_resistance_vs_soc(15.0, 0.0);
    assert(R_empty > 15.0);
    PASS();

    TEST("internal_resistance_vs_temp cold");
    double R_cold = internal_resistance_vs_temp(15.0, 0.0, 0.3);
    assert(R_cold > 15.0);
    PASS();
}

int main(void) {
    printf("\n=== Coin Cell Battery Tests ===\n\n");
    test_params_lookup();
    test_shepherd_model();
    test_peukert_law();
    test_voltage_under_load();
    test_self_discharge();
    test_coulomb_counter();
    test_ekf_soc();
    test_discharge_lut();
    test_internal_resistance();
    printf("\n=== Results: %d/%d tests passed ===\n", tests_passed, tests_run);
    return (tests_passed == tests_run) ? 0 : 1;
}
