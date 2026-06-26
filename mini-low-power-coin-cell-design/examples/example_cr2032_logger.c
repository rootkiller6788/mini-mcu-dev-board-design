/**
 * @file example_cr2032_logger.c
 * @brief L6 Canonical Problem: CR2032-powered temperature data logger design
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "../include/coin_cell_battery.h"
#include "../include/low_power_mcu.h"
#include "../include/power_budget.h"
#include "../include/voltage_regulation.h"

int main(void) {
    printf("\n==========================================================\n");
    printf("  CR2032 TEMPERATURE DATA LOGGER - BATTERY LIFE ANALYSIS\n");
    printf("==========================================================\n\n");

    const CoinCellParams *battery = coin_cell_get_params(CELL_CR2032);
    printf("Battery: %s\n", coin_cell_get_name(CELL_CR2032));
    printf("  Nominal Capacity: %.0f mAh\n", battery->C_nominal_mAh);
    printf("  Nominal Voltage:  %.2f V\n", battery->V_nominal_V);
    printf("  Cutoff Voltage:   %.2f V\n", battery->V_cutoff_V);
    printf("  Internal R:       %.0f Ohm\n\n", battery->R_internal_0_ohm);

    PowerBudget budget;
    power_budget_init(&budget, 3.0, 15.0);
    power_budget_add_consumer(&budget, CONSUMER_MCU_CORE, "STM32L0_STOP",
        5000.0, 1.5, 0.1, 10.0, 60.0);
    power_budget_add_consumer(&budget, CONSUMER_SENSOR, "TMP117",
        150.0, 0.1, 0.01, 1.0, 60.0);
    power_budget_add_consumer(&budget, CONSUMER_STORAGE, "Flash_Write",
        5000.0, 1.0, 0.01, 5.0, 60.0);
    power_budget_add_consumer(&budget, CONSUMER_MCU_PERIPH, "RTC",
        0.3, 0.3, 100.0, 0.0, 0.0);
    power_budget_compute(&budget);
    power_budget_print(&budget);

    CapacityDerating derating;
    compute_capacity_derating(battery, 20.0, 5.0, 2.0, 3.0, 1, &derating);
    printf("Capacity Derating Factors:\n");
    printf("  Temperature (20C):     %.3f\n", derating.temperature_derate);
    printf("  Rate (Peukert):        %.3f\n", derating.rate_derate);
    printf("  Cutoff Voltage (2.0V): %.3f\n", derating.cutoff_derate);
    printf("  Shelf (3 months):      %.3f\n", derating.self_discharge_derate);
    printf("  Overall Derate:        %.3f\n\n", derating.overall_derate);

    BatteryLifeEstimate est;
    battery_life_estimate_from_budget(&budget, battery, &derating, &est);

    printf("==========================================================\n");
    printf("  BATTERY LIFE ESTIMATE\n");
    printf("==========================================================\n");
    printf("  Usable Capacity:   %.0f mAh\n", est.usable_capacity_mAh);
    printf("  Average Current:   %.2f uA\n", est.avg_current_uA);
    printf("  Estimated Life:    %.0f hours\n", est.estimated_life_hours);
    printf("                     %.1f days\n", est.estimated_life_days);
    printf("                     %.1f months\n", est.estimated_life_months);
    printf("                     %.2f years\n", est.estimated_life_years);
    printf("  Voltage Limited:   %s\n", est.is_voltage_limited ? "YES" : "No");
    printf("  Pulse Limited:     %s\n", est.is_pulse_limited ? "YES" : "No");
    printf("==========================================================\n\n");

    printf("CONCLUSION: A CR2032-powered temperature logger sampling\n");
    printf("every 60 seconds can operate for approximately %.1f years.\n", est.estimated_life_years);
    printf("This exceeds the typical 1-year target for IoT sensors.\n\n");

    free(budget.entries);
    return 0;
}
