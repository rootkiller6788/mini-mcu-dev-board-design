/**
 * @file    example_thermal_design.c
 * @brief   Example: Thermal analysis for ESP32 IoT board.
 *
 * Demonstrates junction temperature calculation, heatsink sizing,
 * copper pour cooling, and thermal via array optimization.
 *
 * Usage: ./examples/example_thermal_design.ex
 */

#include "thermal_design.h"
#include <stdio.h>
#include <math.h>

int main(void)
{
    printf("=== ESP32 Thermal Analysis ===\n\n");

    /* ESP32 thermal parameters (from datasheet) */
    double T_ambient = 40.0;    /* Worst-case ambient: 40C */
    double P_diss = 0.5;        /* Typical active power: 500mW */
    double Rth_ja = 50.0;       /* Junction-to-ambient: 50 K/W */
    double Tj_max = 125.0;      /* Maximum junction temp */

    double T_junction = junction_temperature(T_ambient, P_diss, Rth_ja);
    printf("Junction temperature: %.1f C (max: %.0f C)\n",
           T_junction, Tj_max);
    printf("Temperature margin: %.1f C\n\n", Tj_max - T_junction);

    /* Check if heatsink is needed */
    double Rth_jc = 5.0;        /* Junction-to-case */
    double Rth_cs = 1.0;        /* Case-to-sink (thermal paste) */
    double Rth_sa_req = heatsink_thermal_resistance(
        Tj_max, T_ambient, P_diss, Rth_jc, Rth_cs);

    printf("Required heatsink Rth_sa: %.1f K/W\n", Rth_sa_req);
    if (Rth_sa_req <= 0.0) {
        printf("  -> No heatsink required (package alone suffices)\n");
    } else {
        printf("  -> Heatsink needed (Rth_sa <= %.1f K/W)\n", Rth_sa_req);
    }
    printf("\n");

    /* Copper pour cooling area */
    double area_nat = copper_area_for_cooling(P_diss, 30.0, 35.0, 0);
    double area_int = copper_area_for_cooling(P_diss, 30.0, 35.0, 1);
    printf("Copper area for %.1fW, 30C rise:\n", P_diss);
    printf("  External layer: %.0f mm^2\n", area_nat);
    printf("  Internal layer: %.0f mm^2\n", area_int);
    printf("\n");

    /* Thermal via array design */
    thermal_via_array_t vias = {
        .num_vias = 9,
        .via_diam_mm = 0.3,
        .via_pitch_mm = 1.0,
        .pad_size_mm = 0.6,
        .cu_plating_um = 25.0,
        .board_thickness_mm = 1.6
    };
    double Rth_vias = thermal_via_resistance(&vias);
    printf("Thermal via array (3x3, 0.3mm dia):\n");
    printf("  Effective Rth: %.1f K/W\n", Rth_vias);
    printf("  With 9 vias: Rth_total = %.1f K/W + %.1f (board)\n",
           Rth_vias, Rth_ja);

    /* Cooling with natural convection */
    double P_conv = natural_convection_power(1000.0, 60.0, 25.0);
    printf("\nNatural convection from 1000 mm^2 @ 60C: %.2f W\n",
           P_conv);

    /* Maximum power budget */
    double Rth_total = Rth_jc + Rth_cs + 20.0; /* with heatsink */
    double P_max = max_power_budget(Tj_max, T_ambient, Rth_total);
    printf("Max power budget (with heatsink): %.2f W\n", P_max);

    return 0;
}
