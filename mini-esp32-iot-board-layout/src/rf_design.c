/**
 * @file    rf_design.c
 * @brief   RF design: reflection, VSWR, return loss, matching, and link budget.
 *
 * Computes reflection coefficient, VSWR, return loss, mismatch loss,
 * impedance-admittance conversion, L/Pi/T match network synthesis,
 * stub matching, quarter-wave transformers, and Friis link budget.
 *
 * Key references:
 *   - Pozar, D.M. Microwave Engineering, 4th Ed., 2012
 *   - Bowick, C. RF Circuit Design, 2nd Ed., 2008
 *   - Ludwig & Bogdanov, RF Circuit Design, 2nd Ed., 2009
 * @author  mini-esp32-iot-board-layout
 * @date    2026-06-21
 */

#include "rf_design.h"
#include "transmission_line.h"
#include <math.h>
#include <string.h>

/* --- Reflection & VSWR -------------------------------------------- */

/* Reflection coefficient: Gamma = (ZL - Z0) / (ZL + Z0).
 * Returns complex reflection coefficient. */
complex_z_t reflection_coefficient(complex_z_t ZL, double Z0)
{
    if (Z0 <= 0.0) {
        complex_z_t nan = {NAN, NAN};
        return nan;
    }
    double denom_real = ZL.real + Z0;
    double denom_imag = ZL.imag;
    double denom_mag_sq = denom_real*denom_real + denom_imag*denom_imag;
    if (denom_mag_sq <= 0.0) {
        complex_z_t zero = {0.0, 0.0};
        return zero;
    }
    double num_real = ZL.real - Z0;
    double num_imag = ZL.imag;
    complex_z_t gamma;
    gamma.real = (num_real*denom_real + num_imag*denom_imag) / denom_mag_sq;
    gamma.imag = (num_imag*denom_real - num_real*denom_imag) / denom_mag_sq;
    return gamma;
}

/* VSWR from reflection coefficient magnitude.
 * VSWR = (1 + |Gamma|) / (1 - |Gamma|). */
double vswr(complex_z_t gamma)
{
    double mag = sqrt(gamma.real*gamma.real + gamma.imag*gamma.imag);
    if (mag >= 1.0) return INFINITY;
    return (1.0 + mag) / (1.0 - mag);
}

/* Return loss: RL = -20*log10(|Gamma|) [dB].
 * Higher return loss = better matching. */
double return_loss_db(complex_z_t gamma)
{
    double mag = sqrt(gamma.real*gamma.real + gamma.imag*gamma.imag);
    if (mag <= 0.0) return INFINITY;
    if (mag >= 1.0) return 0.0;
    return -20.0 * log10(mag);
}

/* Mismatch loss: ML = -10*log10(1 - |Gamma|^2) [dB].
 * Power lost due to impedance mismatch. */
double mismatch_loss_db(complex_z_t gamma)
{
    double mag_sq = gamma.real*gamma.real + gamma.imag*gamma.imag;
    if (mag_sq >= 1.0) return INFINITY;
    if (mag_sq <= 0.0) return 0.0;
    return -10.0 * log10(1.0 - mag_sq);
}

/* Insertion loss from S11: IL = -20*log10(|S21|) ~ -20*log10(sqrt(1-|S11|^2)).
 * Approximate for lossless symmetric network. */
double insertion_loss_db_s11(complex_z_t s11)
{
    double mag_sq = s11.real*s11.real + s11.imag*s11.imag;
    if (mag_sq >= 1.0) return INFINITY;
    return -10.0 * log10(1.0 - mag_sq);
}

/* Convert reflection coefficient back to impedance.
 * Z = Z0 * (1 + Gamma) / (1 - Gamma). */
complex_z_t impedance_from_gamma(complex_z_t gamma, double Z0)
{
    double one_m_real = 1.0 - gamma.real;
    double one_m_imag = -gamma.imag;
    double denom_mag_sq = one_m_real*one_m_real + one_m_imag*one_m_imag;
    if (denom_mag_sq <= 0.0) {
        complex_z_t inf = {INFINITY, INFINITY};
        return inf;
    }
    complex_z_t Z;
    double num_real = 1.0 + gamma.real;
    double num_imag = gamma.imag;
    Z.real = Z0 * (num_real*one_m_real + num_imag*one_m_imag) / denom_mag_sq;
    Z.imag = Z0 * (num_imag*one_m_real - num_real*one_m_imag) / denom_mag_sq;
    return Z;
}

/* Convert impedance to admittance.
 * Y = 1/Z = G + jB, G = R/(R^2+X^2), B = -X/(R^2+X^2). */
complex_z_t admittance_from_impedance(complex_z_t Z)
{
    double mag_sq = Z.real*Z.real + Z.imag*Z.imag;
    if (mag_sq <= 0.0) {
        complex_z_t inf = {INFINITY, INFINITY};
        return inf;
    }
    complex_z_t Y;
    Y.real = Z.real / mag_sq;
    Y.imag = -Z.imag / mag_sq;
    return Y;
}
