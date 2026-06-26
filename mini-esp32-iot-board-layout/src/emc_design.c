/**
 * @file    emc_design.c
 * @brief   EMC/EMI design considerations for ESP32 IoT board layout.
 *
 * Covers EMI filter design, common-mode/differential-mode filtering,
 * ferrite bead selection, shielding effectiveness, and FCC/CE
 * regulatory considerations for 2.4 GHz IoT devices.
 *
 * Knowledge mapping:
 *   L1 - Definitions:    EMI, EMC, differential-mode, common-mode noise
 *   L2 - Core Concepts:  conducted/radiated emissions, shielding effectiveness
 *   L3 - Math:           filter transfer functions, insertion loss
 *   L4 - Fundamental:    Maxwell equations (EM radiation), Faraday shielding
 *   L5 - Algorithms:     EMI filter design, ferrite bead selection
 *   L7 - Applications:   FCC Part 15 compliance, CE EMC directive
 *
 * References:
 *   - Paul, C.R. Introduction to Electromagnetic Compatibility, 2006
 *   - Ott, H.W. Electromagnetic Compatibility Engineering, 2009
 *   - FCC Part 15.247 for 2.4 GHz intentional radiators
 * @author  mini-esp32-iot-board-layout
 * @date    2026-06-21
 */

#include "board_geometry.h"
#include "transmission_line.h"
#include <math.h>
#include <string.h>

/* --- EMI Filter Types --------------------------------------------- */

typedef enum {
    FILTER_LOWPASS_LC,
    FILTER_LOWPASS_PI,
    FILTER_LOWPASS_T,
    FILTER_COMMON_MODE_CHOKE,
    FILTER_FERRITE_BEAD
} emi_filter_type_t;

typedef struct {
    emi_filter_type_t type;
    double  fc_hz;
    double  L_value_h;
    double  C_value_f;
    double  insertion_loss_at_freq_db;
    double  target_freq_hz;
} emi_filter_t;

/* Design a simple LC lowpass EMI filter.
 * fc = 1/(2*pi*sqrt(L*C)).
 * Choose C first based on practical values, then compute L.
 * Returns filter parameters. */
emi_filter_t emi_lc_lowpass_design(double fc_hz, double Z0_ohm)
{
    emi_filter_t f;
    memset(&f, 0, sizeof(f));
    f.type = FILTER_LOWPASS_LC;
    f.fc_hz = fc_hz;
    if (fc_hz <= 0.0 || Z0_ohm <= 0.0) return f;
    double wc = 2.0 * M_PI * fc_hz;
    f.C_value_f = 1.0 / (wc * Z0_ohm);
    f.L_value_h = Z0_ohm / wc;
    f.target_freq_hz = fc_hz;
    return f;
}

/* Compute insertion loss of a single-stage LC filter at target frequency.
 * IL_dB = 20*log10(|1/(1 - w^2*L*C + j*w*L/Z0)|) for series-L, shunt-C.
 * Simplifies to: IL ~ 40*log10(f_target/fc) for f >> fc. */
double emi_filter_insertion_loss(const emi_filter_t *filter, double freq_hz)
{
    if (!filter || filter->fc_hz <= 0.0 || freq_hz <= 0.0) return 0.0;
    double ratio = freq_hz / filter->fc_hz;
    if (ratio <= 1.0) return 0.0;
    return 40.0 * log10(ratio);
}

/* --- Common-Mode Choke Design ------------------------------------- */

/* Compute common-mode impedance of a choke at target frequency.
 * Z_cm = j*w*L_cm, where L_cm is the common-mode inductance.
 * Typical SMD common-mode chokes: 30-1000 ohm at 100 MHz.
 * Returns impedance magnitude in ohms. */
double common_mode_impedance(double L_cm_h, double freq_hz)
{
    if (L_cm_h <= 0.0 || freq_hz <= 0.0) return 0.0;
    return 2.0 * M_PI * freq_hz * L_cm_h;
}

/* --- Ferrite Bead Selection --------------------------------------- */

/* Ferrite bead impedance model (simplified R-L parallel + C).
 * At low freq: inductive (Z ~ j*w*L).
 * At high freq: resistive (Z ~ R, losses).
 * At very high freq: capacitive (Z ~ 1/(j*w*C)).
 * Returns impedance magnitude at given frequency.
 * Ref: Ott, EMC Engineering, Ch.12 */
double ferrite_bead_impedance(double L_h, double R_ohm,
                               double C_parasitic_f, double freq_hz)
{
    if (freq_hz <= 0.0) return 0.0;
    double w = 2.0 * M_PI * freq_hz;
    /* Parallel R-L, then series C */
    double Z_rl_real = R_ohm * w*w * L_h*L_h
                     / (R_ohm*R_ohm + w*w * L_h*L_h);
    double Z_rl_imag = R_ohm*R_ohm * w * L_h
                     / (R_ohm*R_ohm + w*w * L_h*L_h);
    double Z_imag = Z_rl_imag - 1.0 / (w * C_parasitic_f);
    return sqrt(Z_rl_real*Z_rl_real + Z_imag*Z_imag);
}

/* --- Shielding Effectiveness -------------------------------------- */

/* Plane-wave shielding effectiveness (Schellkunoff theory).
 * SE_dB = A + R + B
 * A = absorption loss ~ 131.4 * t * sqrt(f * mu_r * sigma_r) [dB]
 * R = reflection loss ~ 168 - 10*log10(mu_r*f/sigma_r) [dB]
 * B = multiple reflection correction (neglected for A > 10dB).
 * t = thickness in mm, f = frequency in Hz.
 * mu_r = relative permeability, sigma_r = conductivity relative to copper.
 * Ref: Paul, Introduction to EMC, Ch.8 */
double shielding_effectiveness_db(double thickness_mm, double freq_hz,
                                   double mu_r, double sigma_r)
{
    if (thickness_mm <= 0.0 || freq_hz <= 0.0 || sigma_r <= 0.0)
        return 0.0;
    double A = 131.4 * thickness_mm * 1e-3
             * sqrt(freq_hz * mu_r * sigma_r);
    double R = 168.0 - 10.0 * log10(mu_r * freq_hz / sigma_r);
    /* B is negligible if A > 10 dB */
    double B = (A > 10.0) ? 0.0
             : 20.0 * log10(fabs(1.0 - exp(-2.0 * A / 8.686)));
    return A + R + B;
}
