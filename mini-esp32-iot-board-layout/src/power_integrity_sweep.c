/**
 * @file    power_integrity_sweep.c
 * @brief   PDN frequency sweep, IR drop, and specification checking.
 * @author  mini-esp32-iot-board-layout
 * @date    2026-06-21
 */

#include "power_integrity.h"
#include <math.h>

/* Evaluate PDN impedance across logarithmic frequency sweep.
 * Computes the combined impedance of multiple capacitor stages
 * (each with potentially different values and quantities),
 * finds the maximum impedance point and corresponding frequency.
 * Uses log10-spaced frequency points.
 * Complexity: O(n_points * n_stages). */
void pdn_impedance_sweep(const decap_model_t *stages,
                          const int *quantities, int num_stages,
                          double f_start_hz, double f_stop_hz,
                          int points_per_decade,
                          double *max_z, double *f_max_z)
{
    if (!stages || !quantities || num_stages <= 0 || !max_z || !f_max_z)
        return;
    if (f_start_hz <= 0.0 || f_stop_hz <= f_start_hz ||
        points_per_decade <= 0) {
        *max_z = -1.0;
        *f_max_z = -1.0;
        return;
    }
    double decades = log10(f_stop_hz / f_start_hz);
    int n_points = (int)(decades * points_per_decade) + 1;
    if (n_points < 2) n_points = 2;

    *max_z = 0.0;
    *f_max_z = f_start_hz;

    for (int i = 0; i < n_points; i++) {
        double frac = (double)i / (n_points - 1);
        double f = f_start_hz * pow(10.0, frac * decades);

        /* Parallel combination of all capacitor banks */
        double Y_real = 0.0, Y_imag = 0.0;
        for (int s = 0; s < num_stages; s++) {
            if (quantities[s] <= 0) continue;
            double C = stages[s].capacitance_f * quantities[s];
            double L = stages[s].esl_h / quantities[s];
            double R = stages[s].esr_ohm / quantities[s];
            double w = 2.0 * M_PI * f;
            double X = w * L - 1.0 / (w * C);
            double denom = R*R + X*X;
            if (denom > 0.0) {
                Y_real += R / denom;
                Y_imag += -X / denom;
            }
        }
        double Y_mag_sq = Y_real*Y_real + Y_imag*Y_imag;
        double Z = (Y_mag_sq > 0.0) ? 1.0 / sqrt(Y_mag_sq) : 1e6;
        if (Z > *max_z) {
            *max_z = Z;
            *f_max_z = f;
        }
    }
}

/* Compute IR drop for a given trace/pour geometry.
 * V_drop = I * rho * L / (W * t).
 * Uses SI units internally. Returns voltage drop in volts. */
double ir_drop(double current_a, double resistivity_ohm_m,
               double length_mm, double width_mm, double thickness_um)
{
    if (current_a <= 0.0 || length_mm <= 0.0 || width_mm <= 0.0 ||
        thickness_um <= 0.0)
        return -1.0;
    double R = resistivity_ohm_m * length_mm * 1e-3
             / (width_mm * 1e-3 * thickness_um * 1e-6);
    return current_a * R;
}

/* Check if a PDN design meets target impedance specifications.
 * Compares max PDN impedance against target with margin.
 * Returns 1 if design passes, 0 if fails. */
int pdn_meets_spec(const pdn_result_t *result, const pdn_spec_t *spec)
{
    if (!result || !spec) return 0;
    double target_z = pdn_target_impedance(spec);
    if (target_z <= 0.0) return 0;
    /* Design passes if max_z < target_z (with 3dB margin recommended) */
    double margin = 20.0 * log10(target_z / result->max_z_ohm);
    return (margin >= 3.0) ? 1 : 0;
}
