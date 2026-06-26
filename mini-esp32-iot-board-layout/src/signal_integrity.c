/**
 * @file    signal_integrity.c
 * @brief   Signal integrity: crosstalk, ground bounce, SSN, eye diagrams.
 *
 * Computes near-end (NEXT) and far-end (FEXT) crosstalk from coupled
 * transmission line parameters, ground bounce voltage, simultaneous
 * switching noise (SSN), impedance discontinuity reflections,
 * and eye diagram margin estimation.
 *
 * References:
 *   - Johnson & Graham, High-Speed Digital Design, 1993
 *   - Bogatin, Signal and Power Integrity Simplified, 3rd Ed., 2018
 * @author  mini-esp32-iot-board-layout
 * @date    2026-06-21
 */

#include "signal_integrity.h"
#include <math.h>
#include <string.h>

/* Reflection coefficient at impedance discontinuity.
 * Gamma = (Z2 - Z1) / (Z2 + Z1). */
double reflection_coefficient_Z(double Z1, double Z2)
{
    if (Z1 <= 0.0 || Z2 <= 0.0) return NAN;
    return (Z2 - Z1) / (Z2 + Z1);
}

/* Reflected voltage at an impedance discontinuity.
 * V_reflected = Gamma * V_incident. */
double reflected_voltage(double incident_v, double Z1, double Z2)
{
    double gamma = reflection_coefficient_Z(Z1, Z2);
    return gamma * incident_v;
}

/* Transmitted voltage across an impedance discontinuity.
 * V_transmitted = (1 + Gamma) * V_incident. */
double transmitted_voltage(double incident_v, double Z1, double Z2)
{
    double gamma = reflection_coefficient_Z(Z1, Z2);
    return (1.0 + gamma) * incident_v;
}

/* Near-end crosstalk (NEXT) in coupled transmission lines.
 * NEXT = V_in * (L_m/(2*L_s) + C_m/(2*C_s)) / 4.
 * NEXT coefficient is independent of line length for homogeneous lines.
 * Ref: Johnson & Graham, Ch.5 */
void near_end_crosstalk(const coupled_line_t *line, double input_v,
                         crosstalk_result_t *result)
{
    if (!line || !result) return;
    memset(result, 0, sizeof(*result));
    result->signal_amplitude_v = input_v;
    if (line->L_self_nH <= 0.0 || line->C_self_pF <= 0.0) return;
    double L_ratio = line->L_mutual_nH / line->L_self_nH;
    double C_ratio = line->C_mutual_pF / line->C_self_pF;
    result->NEXT_coefficient = (L_ratio + C_ratio) / 4.0;
    result->NEXT_mv = result->NEXT_coefficient * input_v * 1000.0;
}

/* Far-end crosstalk (FEXT) in coupled transmission lines.
 * FEXT = V_in * length * (C_m*Z0 - L_m/Z0) / (2 * t_rise * v_prop).
 * For homogeneous stripline: L_m/C_m = Z0^2, so FEXT ~ 0.
 * For microstrip (inhomogeneous): FEXT is non-zero.
 * Ref: Johnson & Graham, Ch.5 */
void far_end_crosstalk(const coupled_line_t *line, double input_v,
                        crosstalk_result_t *result)
{
    if (!line || !result) return;
    memset(result, 0, sizeof(*result));
    result->signal_amplitude_v = input_v;
    if (line->t_rise_ps <= 0.0 || line->v_prop_mm_per_ps <= 0.0 ||
        line->Z0_ohm <= 0.0) return;
    double L_per_mm = line->L_mutual_nH * 1e-9;
    double C_per_mm = line->C_mutual_pF * 1e-12;
    double FEXT_factor = (C_per_mm * line->Z0_ohm
                        - L_per_mm / line->Z0_ohm)
                       / (2.0 * line->t_rise_ps * 1e-12
                          * line->v_prop_mm_per_ps * 1e-3);
    result->FEXT_coefficient = FEXT_factor * line->length_mm;
    result->FEXT_mv = result->FEXT_coefficient * input_v * 1000.0;
}
