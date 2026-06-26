#include "../include/power_design.h"
#include <stdio.h>
#include <math.h>
int main(void) {
    printf("=== STM32H7 Power Tree Analysis ===\n");
    double i_3v3 = 0.100, i_1v8 = 0.050, i_1v2 = 0.300;
    double e1, e2, e3;
    double p_in = mcu_power_budget(i_3v3, i_1v8, i_1v2, &e1, &e2, &e3);
    printf("Load: 3.3V@%.0fmA 1.8V@%.0fmA 1.2V@%.0fmA\n", i_3v3*1000, i_1v8*1000, i_1v2*1000);
    printf("Eff: 3.3V=%.0f%% 1.8V=%.0f%% 1.2V=%.0f%%\n", e1*100, e2*100, e3*100);
    printf("Total input: %.3f W\n", p_in);
    double loss = regulator_power_loss(5.0, 3.3, i_3v3+i_1v8+i_1v2, e1);
    printf("Power loss: %.0f mW\n", loss*1000);
    int faults = stm32_power_rail_check(3300, 3300, 2200, 3000);
    printf("STM32 rails: %s\n", faults==0 ? "OK" : "FAULT");
    double life = esp32_battery_life(3000.0, 200.0, 0.01, 10.0, 0.99);
    printf("ESP32 3000mAh: %.0f hours\n", life);
    double fom, hs_fom;
    gan_vs_si_fom(10.0, 5.0, 1.0, 1, &fom, &hs_fom);
    printf("GaN FOM: %.1f\n", fom);
    printf("Example complete.\n");
    return 0;
}
