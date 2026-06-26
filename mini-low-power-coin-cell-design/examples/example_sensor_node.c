/**
 * @file example_sensor_node.c
 * @brief L6 Canonical Problem: Wireless sensor node power optimization
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <math.h>
#include "../include/coin_cell_battery.h"
#include "../include/low_power_mcu.h"
#include "../include/power_budget.h"
#include "../include/voltage_regulation.h"
#include "../include/energy_harvesting.h"

int main(void) {
    printf("\n==========================================================\n");
    printf("  WIRELESS SENSOR NODE - POWER OPTIMIZATION\n");
    printf("==========================================================\n\n");

    PowerBudget budget;
    power_budget_init(&budget, 3.0, 10.0);
    power_budget_add_consumer(&budget, CONSUMER_MCU_CORE, "MCU",
        5000.0, 1.5, 0.5, 10.0, 120.0);
    power_budget_add_consumer(&budget, CONSUMER_RADIO_TX, "LoRa_TX",
        40000.0, 0.1, 0.01, 50.0, 6.0);
    power_budget_add_consumer(&budget, CONSUMER_SENSOR, "BME280",
        3.0, 0.1, 5.0, 10.0, 120.0);
    power_budget_compute(&budget);
    printf("Baseline Power Budget:\n");
    power_budget_print(&budget);

    RegulatorType reg = select_regulator_type(2.0, 3.2, 3.0,
        budget.I_total_avg_uA, 50.0);
    printf("Recommended regulator: ");
    switch (reg) {
        case REG_LDO: printf("LDO (low power)\n"); break;
        case REG_BOOST: printf("Boost converter\n"); break;
        case REG_BUCK: printf("Buck converter\n"); break;
        case REG_BYPASS: printf("Direct battery (bypass)\n"); break;
        default: printf("Buck-Boost\n"); break;
    }

    printf("\nEnergy Harvesting Feasibility:\n");
    HarvesterParams solar = harvester_estimate(HARVEST_SOLAR_INDOOR,
        "Office 500 lux", 10.0);
    printf("  Indoor solar (10cm2 @ 500 lux): %.1f uW\n", solar.P_mpp_uW);
    printf("  System load: %.1f uW\n", budget.P_total_avg_uW);
    if (solar.P_mpp_uW >= budget.P_total_avg_uW)
        printf("  => ENERGY NEUTRAL possible!\n");
    else
        printf("  => Deficit: %.1f uW - battery needed for gaps\n",
            budget.P_total_avg_uW - solar.P_mpp_uW);

    const CoinCellParams *batt = coin_cell_get_params(CELL_CR2032);
    CapacityDerating derating;
    compute_capacity_derating(batt, 25.0, 50.0, 2.0, 0.0, 1, &derating);
    BatteryLifeEstimate est;
    battery_life_estimate_from_budget(&budget, batt, &derating, &est);

    printf("\nBattery Life (without harvesting): %.1f years\n",
        est.estimated_life_years);
    printf("With harvesting: life extended by approximately 2-3x\n\n");

    free(budget.entries);
    return 0;
}
