/**
 * @file test_power_budget.c
 * @brief Tests for power budget analysis and battery life estimation
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <math.h>
#include "../include/power_budget.h"
#include "../include/coin_cell_battery.h"

#define EPSILON 1e-6
static int tests_run = 0, tests_passed = 0;
#define TEST(n) do { tests_run++; printf("  TEST: %s ... ", n); } while(0)
#define PASS() do { printf("PASSED\n"); tests_passed++; } while(0)

static void test_power_budget_basic(void) {
    TEST("power budget basic compute");
    PowerBudget budget;
    power_budget_init(&budget, 3.0, 10.0);
    power_budget_add_consumer(&budget, CONSUMER_MCU_CORE, "MCU_RUN", 10000.0, 2.0, 1.0, 10.0, 3600.0);
    power_budget_add_consumer(&budget, CONSUMER_SENSOR, "Temperature", 50.0, 0.1, 5.0, 100.0, 60.0);
    power_budget_compute(&budget);
    assert(budget.I_total_avg_uA > 0.0);
    PASS();
    free(budget.entries);
}

static void test_simple_battery_life(void) {
    TEST("simple_battery_life_hours");
    double life = simple_battery_life_hours(225.0, 10.0);
    assert(fabs(life - 22500.0) < 1.0);
    PASS();
}

static void test_capacity_derating(void) {
    TEST("capacity derating at 25C");
    const CoinCellParams *batt = coin_cell_get_params(CELL_CR2032);
    CapacityDerating derating;
    compute_capacity_derating(batt, 25.0, 5.0, 2.3, 0.0, 1, &derating);
    assert(derating.overall_derate > 0.5 && derating.overall_derate <= 1.0);
    PASS();
}

static void test_battery_life_estimate(void) {
    TEST("battery_life_estimate_from_budget");
    PowerBudget budget;
    power_budget_init(&budget, 3.0, 10.0);
    power_budget_add_consumer(&budget, CONSUMER_MCU_CORE, "MCU", 5000.0, 2.0, 2.0, 5.0, 720.0);
    power_budget_compute(&budget);
    const CoinCellParams *batt = coin_cell_get_params(CELL_CR2032);
    CapacityDerating derating;
    compute_capacity_derating(batt, 25.0, 5.0, 2.3, 0.0, 1, &derating);
    BatteryLifeEstimate est;
    battery_life_estimate_from_budget(&budget, batt, &derating, &est);
    assert(est.estimated_life_days > 0.0);
    PASS();
    free(budget.entries);
}

static void test_energy_stats(void) {
    TEST("compute_energy_statistics");
    PowerSample samples[] = {
        {0.0, 100.0, 3.0}, {1.0, 200.0, 2.9}, {2.0, 150.0, 2.95}
    };
    EnergyStatistics stats;
    compute_energy_statistics(samples, 3, &stats);
    assert(stats.total_energy_uJ > 0.0);
    assert(stats.min_current_uA <= stats.max_current_uA);
    PASS();
}

int main(void) {
    printf("\n=== Power Budget Tests ===\n\n");
    test_power_budget_basic();
    test_simple_battery_life();
    test_capacity_derating();
    test_battery_life_estimate();
    test_energy_stats();
    printf("\n=== Results: %d/%d tests passed ===\n", tests_passed, tests_run);
    return (tests_passed == tests_run) ? 0 : 1;
}
