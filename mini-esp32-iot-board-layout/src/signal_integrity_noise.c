/**
 * @file    signal_integrity_noise.c
 * @brief   Ground bounce, SSN, eye diagrams, and via effects.
 * @author  mini-esp32-iot-board-layout
 * @date    2026-06-21
 */

#include "signal_integrity.h"
#include <math.h>
#include <string.h>

/* Ground bounce voltage due to package inductance.
 * V_gb = L_gnd * di/dt.
 * This voltage shift on the internal ground reference causes
 * false switching and signal integrity degradation.
 * Ref: Johnson & Graham, Ch.7 */
double ground_bounce_voltage(double L_gnd_nH, double di_dt_A_per_ns)
{
    if (L_gnd_nH < 0.0 || di_dt_A_per_ns < 0.0) return -1.0;
    return L_gnd_nH * 1e-9 * di_dt_A_per_ns * 1e9;
}

/* Simultaneous switching noise (SSN) voltage.
 * V_ssn = N * L_pkg * di/dt.
 * N drivers switching simultaneously multiply the effect.
 * Returns voltage in volts. */
double ssn_voltage(const ssn_params_t *params)
{
    if (!params || params->num_switching <= 0) return -1.0;
    double L_eff = params->L_package_nH + params->L_gnd_nH;
    return params->num_switching * L_eff * 1e-9
         * params->di_dt_A_per_ns * 1e9;
}

/* Quiet-low noise from SSN (simplified).
 * V_ql = L_pkg * N * di/dt.
 * This is the noise induced on a quiet output when
 * neighboring drivers switch. */
double ssn_quiet_low_noise(double L_pkg_nH, int N, double di_dt_A_per_ns)
{
    if (L_pkg_nH < 0.0 || N <= 0 || di_dt_A_per_ns < 0.0) return -1.0;
    return L_pkg_nH * 1e-9 * N * di_dt_A_per_ns * 1e9;
}

/* Reflection magnitude at impedance discontinuity.
 * Returns |Gamma|. */
double impedance_discontinuity_reflection(double Z1, double Z2)
{
    double gamma = reflection_coefficient_Z(Z1, Z2);
    return fabs(gamma);
}

/* TDR impedance from reflection coefficient.
 * Z_TDR = Z0 * (1 + Gamma) / (1 - Gamma). */
double tdr_impedance_from_reflection(double Z0, double reflection_coeff)
{
    if (Z0 <= 0.0 || reflection_coeff <= -1.0 || reflection_coeff >= 1.0)
        return -1.0;
    return Z0 * (1.0 + reflection_coeff) / (1.0 - reflection_coeff);
}
