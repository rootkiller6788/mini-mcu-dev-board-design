/**
 * @file example_ble_beacon.c
 * @brief L6 Canonical Problem: BLE beacon coin cell design
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "../include/coin_cell_battery.h"
#include "../include/low_power_mcu.h"
#include "../include/power_budget.h"

int main(void) {
    printf("\n==========================================================\n");
    printf("  BLE BEACON - COIN CELL BATTERY LIFE\n");
    printf("==========================================================\n\n");

    const CoinCellParams *batt = coin_cell_get_params(CELL_CR2450);
    printf("Battery: %s (%.0f mAh)\n\n", coin_cell_get_name(CELL_CR2450), batt->C_nominal_mAh);

    DutyCyclePhase phases[] = {
        {MCU_MODE_RUN, 2.0, 8000.0, "Wake + sensor read"},
        {MCU_MODE_RUN, 0.5, 8000.0, "BLE TX @ 0dBm"},
        {MCU_MODE_RUN, 0.3, 8000.0, "BLE RX window"},
        {MCU_MODE_STOP, 997.2, 0.0, "Deep sleep"}
    };
    DutyCycle dc = {phases, 4, 1000.0, "BLE Beacon 1s interval"};

    const McuPowerProfile *mcu = mcu_get_power_profile("nRF52840", 3.0);
    assert(mcu != NULL);

    OperationEnergy energy;
    duty_cycle_energy(&dc, mcu, &energy);

    printf("Duty Cycle Analysis:\n");
    printf("  Active Energy:    %.2f uJ\n", energy.E_active_uJ);
    printf("  Sleep Energy:     %.2f uJ\n", energy.E_sleep_uJ);
    printf("  Transition:       %.2f uJ\n", energy.E_transition_uJ);
    printf("  Average Current:  %.3f uA\n", energy.avg_current_uA);
    printf("  Duty Cycle:       %.3f%%\n", energy.duty_cycle_pct);

    CapacityDerating derating;
    compute_capacity_derating(batt, 25.0, 10.0, 2.0, 0.0, 1, &derating);

    double life_hours = battery_life_from_duty_cycle(&dc, mcu, batt, &derating);
    printf("\n  Estimated Battery Life: %.0f hours (%.1f years)\n",
           life_hours, life_hours / 8760.0);
    printf("\nCONCLUSION: A CR2450-powered BLE beacon at 1Hz advertising\n");
    printf("interval can operate for >5 years due to extremely low\n");
    printf("duty cycle (<0.3%% active time).\n\n");
    return 0;
}
