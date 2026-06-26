/**
 * @file    example_4layer_stackup.c
 * @brief   Example: Design a 4-layer ESP32 IoT board stackup.
 *
 * Demonstrates creating a 4-layer board stackup, computing trace
 * impedance, checking board area, and estimating thermal via needs.
 *
 * Usage: ./examples/example_4layer_stackup.ex
 */

#include "board_geometry.h"
#include "transmission_line.h"
#include <stdio.h>
#include <math.h>

int main(void)
{
    printf("=== ESP32 4-Layer IoT Board Stackup Design ===\n\n");

    /* Create a standard 4-layer stackup: 1.6mm, 1oz outer, 0.5oz inner */
    board_stackup_t stackup = stackup_4layer_standard(1.6,
        CU_WEIGHT_1OZ, CU_WEIGHT_0_5OZ);

    printf("Stackup: %s\n", stackup.name);
    printf("Total thickness: %.1f mm\n", stackup.total_thickness);
    printf("Layers: %d\n\n", stackup.num_layers);

    /* Print layer details */
    for (int i = 0; i < stackup.num_layers; i++) {
        printf("  Layer %d: %s (%.0f um copper, %s)\n",
               stackup.layers[i].layer_num,
               stackup.layers[i].name,
               stackup.layers[i].copper.thickness_um,
               stackup.layers[i].is_outer ? "outer" : "inner");
    }
    printf("\n");

    /* Print dielectric details */
    for (int i = 0; i < stackup.num_layers - 1; i++) {
        printf("  Dielectric %d-%d: %s, %.3f mm (Er=%.1f)\n",
               stackup.dielectrics[i].between_layers,
               stackup.dielectrics[i].between_layers + 1,
               stackup.dielectrics[i].material,
               stackup.dielectrics[i].thickness_mm,
               stackup.dielectrics[i].er);
    }
    printf("\n");

    /* Compute microstrip impedance for a typical 50-ohm trace */
    double prepreg_thk = stackup.dielectrics[0].thickness_mm;
    double er = stackup.dielectrics[0].er;
    double w_50ohm = 0.35; /* Typical 50-ohm width on 0.2mm prepreg */

    double z0 = microstrip_z0(er, prepreg_thk, w_50ohm, 0.035);
    printf("Microstrip Z0 (w=%.2f mm, h=%.3f mm, Er=%.1f): %.1f ohm\n",
           w_50ohm, prepreg_thk, er, z0);

    /* Compute wavelength at 2.45 GHz for antenna design */
    double ereff = microstrip_ereff(er, prepreg_thk, w_50ohm, 0.035);
    double lambda = wavelength_in_substrate(2.45e9, ereff);
    printf("Wavelength at 2.45 GHz in substrate: %.1f mm\n", lambda);
    printf("Quarter-wave at 2.45 GHz: %.1f mm\n", lambda / 4.0);

    /* Board outline: 50x30 mm IoT board */
    double vx[] = {0, 50, 50, 0};
    double vy[] = {0, 0, 30, 30};
    board_outline_t outline = {50, 30, 1.6, 4, 4, vx, vy};
    printf("\nBoard area: %.0f mm^2\n", board_area(&outline));

    /* Minimum board area estimate for ESP32-WROOM (18x25.5mm) */
    double min_area = min_board_area_estimate(18.0, 25.5, 3, 2);
    printf("Min recommended board area: %.0f mm^2\n", min_area);
    printf("Board fits: %s\n", (board_area(&outline) >= min_area)
           ? "YES" : "NO - need larger board");

    /* Thermal via count for ESP32 (typical 0.5W dissipation) */
    int n_vias = thermal_via_count(0.5, 0.3, 1.6, 25.0, 30.0);
    printf("\nThermal vias needed (0.5W, 30C rise): %d\n", n_vias);

    board_stackup_free(&stackup);
    return 0;
}
