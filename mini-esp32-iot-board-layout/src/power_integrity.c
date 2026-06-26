/**
 * @file    power_integrity.c
 * @brief   Power Delivery Network (PDN) analysis for ESP32 IoT boards.
 *
 * Target impedance method, plane capacitance, decoupling capacitor
 * impedance models, via inductance, IR drop, and PDN sweep analysis.
 *
 * Key references:
 *   - Smith, L.D., IEEE EMC Symposium, 1999
 *   - Swaminathan & Engin, Power Integrity Modeling and Design, 2007
 *   - Bogatin, Signal and Power Integrity Simplified, 3rd Ed., 2018
 * @author  mini-esp32-iot-board-layout
 * @date    2026-06-21
 */

#include "power_integrity.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

/* Compute PDN target impedance.
 * Z_target = Vdd * (ripple_pct/100) / I_transient
 * This is the maximum allowable PDN impedance to keep voltage
 * ripple within specification during load transients.
 * Ref: Smith, IEEE EMC, 1999 */
double pdn_target_impedance(const pdn_spec_t *spec)
{
    if (!spec || spec->I_transient_a <= 0.0 || spec->Vdd <= 0.0)
        return -1.0;
    return spec->Vdd * (spec->ripple_pct / 100.0) / spec->I_transient_a;
}

/* Impedance magnitude of a real capacitor model (RLC series).
 * Z(f) = sqrt(ESR^2 + (2*pi*f*ESL - 1/(2*pi*f*C))^2).
 * Returns magnitude in ohms. */
double decap_impedance(const decap_model_t *cap, double freq_hz)
{
    if (!cap || cap->capacitance_f <= 0.0 || freq_hz <= 0.0)
        return -1.0;
    double w = 2.0 * M_PI * freq_hz;
    double Xc = -1.0 / (w * cap->capacitance_f);
    double Xl = w * cap->esl_h;
    double X = Xl + Xc;
    return sqrt(cap->esr_ohm * cap->esr_ohm + X * X);
}

/* Self-resonant frequency (SRF) of a capacitor.
 * f_srf = 1 / (2*pi*sqrt(L*C)).
 * At this frequency, capacitor impedance is minimum (=ESR). */
double decap_srf(const decap_model_t *cap)
{
    if (!cap || cap->capacitance_f <= 0.0 || cap->esl_h < 0.0)
        return -1.0;
    if (cap->esl_h == 0.0) return INFINITY;
    return 1.0 / (2.0 * M_PI * sqrt(cap->esl_h * cap->capacitance_f));
}

/* Combined impedance of n identical capacitors in parallel.
 * Effective C = n*C, ESL = ESL/n, ESR = ESR/n.
 * Handles n <= 0 gracefully. */
double decap_bank_impedance(const decap_model_t *cap, int n, double freq_hz)
{
    if (!cap || n <= 0 || freq_hz <= 0.0) return -1.0;
    decap_model_t bank;
    bank.capacitance_f = cap->capacitance_f * n;
    bank.esl_h = cap->esl_h / n;
    bank.esr_ohm = cap->esr_ohm / n;
    bank.voltage_rating = cap->voltage_rating;
    bank.package = cap->package;
    bank.dielectric = cap->dielectric;
    return decap_impedance(&bank, freq_hz);
}

/* Anti-resonance frequency between two different capacitor values.
 * At f_anti, the parallel combination of C1 and C2 creates a
 * high-impedance peak that can violate PDN impedance targets.
 * f_anti = 1/(2*pi) * sqrt((C1+C2)/(C1*C2*(ESL1+ESL2))).
 * Returns frequency in Hz. */
double decap_anti_resonance_freq(const decap_model_t *cap1,
                                  const decap_model_t *cap2)
{
    if (!cap1 || !cap2) return -1.0;
    double C1 = cap1->capacitance_f;
    double C2 = cap2->capacitance_f;
    double L1 = cap1->esl_h;
    double L2 = cap2->esl_h;
    if (C1 <= 0.0 || C2 <= 0.0) return -1.0;
    double denom = C1 * C2 * (L1 + L2);
    if (denom <= 0.0) return -1.0;
    return 1.0 / (2.0 * M_PI) * sqrt((C1 + C2) / denom);
}
