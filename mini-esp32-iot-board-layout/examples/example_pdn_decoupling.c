/**
 * @file    example_pdn_decoupling.c
 * @brief   Example: Design PDN decoupling for ESP32 power rails.
 *
 * Demonstrates target impedance calculation, decoupling capacitor
 * selection, anti-resonance checking, and PDN impedance sweep.
 *
 * Usage: ./examples/example_pdn_decoupling.ex
 */

#include "power_integrity.h"
#include <stdio.h>
#include <math.h>

int main(void)
{
    printf("=== ESP32 PDN Decoupling Design ===\n\n");

    /* ESP32 3.3V rail: 500mA max, 100mA transients, 5% ripple */
    pdn_spec_t spec = {
        .Vdd = 3.3,
        .ripple_pct = 5.0,
        .I_max_a = 0.5,
        .I_transient_a = 0.1,
        .t_rise_ns = 5.0,
        .f_min_hz = 100.0,
        .f_max_hz = 1.0e9
    };

    double Z_target = pdn_target_impedance(&spec);
    printf("Target impedance: %.3f ohm\n", Z_target);
    printf("  (3.3V, 5%% ripple, 100mA transient)\n\n");

    /* Decoupling strategy: 3 stages (bulk, mid, high freq) */
    decap_model_t stages[3];
    int quantities[3];

    /* Stage 1: 10uF bulk (near regulator) */
    stages[0] = DECAP_10UF_0603;
    quantities[0] = 2;

    /* Stage 2: 1uF mid-frequency */
    stages[1] = DECAP_1UF_0402;
    quantities[1] = 4;

    /* Stage 3: 100nF high-frequency (near each power pin) */
    stages[2] = DECAP_100NF_0402;
    quantities[2] = 8;

    printf("Decoupling stages:\n");
    for (int i = 0; i < 3; i++) {
        double fsrf = decap_srf(&stages[i]);
        printf("  Stage %d: %d x %.0f nF %s (SRF=%.1f MHz)\n",
               i+1, quantities[i],
               stages[i].capacitance_f * 1e9,
               stages[i].package,
               fsrf / 1e6);
    }
    printf("\n");

    /* Check anti-resonance between stages */
    printf("Anti-resonance checks:\n");
    double f12 = decap_anti_resonance_freq(&stages[0], &stages[1]);
    double f23 = decap_anti_resonance_freq(&stages[1], &stages[2]);
    printf("  Stage 1-2 anti-resonance: %.1f MHz\n", f12 / 1e6);
    printf("  Stage 2-3 anti-resonance: %.1f MHz\n", f23 / 1e6);
    printf("\n");

    /* PDN impedance sweep */
    double max_z, f_max_z;
    pdn_impedance_sweep(stages, quantities, 3,
                         1e3, 1e9, 100,
                         &max_z, &f_max_z);

    printf("PDN sweep (1 kHz - 1 GHz):\n");
    printf("  Max impedance: %.4f ohm at %.1f MHz\n",
           max_z, f_max_z / 1e6);
    printf("  Target: %.4f ohm\n", Z_target);

    pdn_result_t result = {
        .num_stages = 3,
        .stages = stages,
        .quantities = quantities,
        .max_z_ohm = max_z,
        .target_z_ohm = Z_target,
        .margin_db = 20.0 * log10(Z_target / max_z)
    };

    printf("  Margin: %.1f dB\n", result.margin_db);
    printf("  Status: %s\n",
           pdn_meets_spec(&result, &spec) ? "PASS" : "FAIL");

    /* Plane capacitance from power/ground plane pair */
    plane_params_t pp = {
        .width_mm = 30.0,
        .length_mm = 50.0,
        .separation_mm = 0.2,
        .er = 4.2,
        .tan_delta = 0.02
    };
    double C_plane = plane_capacitance(&pp);
    printf("\nPlane capacitance (30x50mm, 0.2mm gap): %.1f nF\n",
           C_plane * 1e9);

    /* Via inductance check */
    double L_via = via_inductance(1.6, 0.3);
    printf("Via inductance (1.6mm length, 0.3mm dia): %.2f nH\n",
           L_via * 1e9);

    return 0;
}
