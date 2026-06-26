/**
 * @file    signal_integrity_eye.c
 * @brief   Eye diagram estimation, bandwidth, via effects, and BER.
 * @author  mini-esp32-iot-board-layout
 * @date    2026-06-21
 */

#include "signal_integrity.h"
#include <math.h>
#include <string.h>

/* Estimate eye diagram opening from noise and jitter.
 * eye_height = V_swing - 2*noise_rms.
 * eye_width = bit_period - 2*jitter_rms.
 * BER ~ Q(eye_height/(2*noise_rms)) for Gaussian noise.
 * Ref: Hall & Heck, Advanced Signal Integrity, 2009 */
void eye_diagram_estimate(double bit_period_ps, double v_swing,
                           double noise_rms, double jitter_rms,
                           eye_diagram_t *eye)
{
    if (!eye) return;
    memset(eye, 0, sizeof(*eye));
    if (bit_period_ps <= 0.0) return;
    eye->eye_height_v = v_swing - 2.0 * noise_rms;
    if (eye->eye_height_v < 0.0) eye->eye_height_v = 0.0;
    eye->eye_width_ps = bit_period_ps - 2.0 * jitter_rms;
    if (eye->eye_width_ps < 0.0) eye->eye_width_ps = 0.0;
    eye->noise_v_rms = noise_rms;
    eye->jitter_ps_rms = jitter_rms;
    /* Q-factor approximation */
    double Q = (noise_rms > 0.0) ? (v_swing / (2.0 * noise_rms)) : INFINITY;
    eye->BER_estimate = bit_error_rate_q(Q);
}

/* Signal bandwidth from rise time (knee frequency).
 * BW_GHz = 0.35 / t_rise_ns.
 * This is the -3dB bandwidth needed to preserve the rise time.
 * Ref: Johnson & Graham, Ch.1 */
double rise_time_bandwidth(double t_rise_ps)
{
    if (t_rise_ps <= 0.0) return -1.0;
    return 0.35 / (t_rise_ps * 1e-3);
}

/* Via impedance discontinuity estimate.
 * A via creates a capacitive discontinuity proportional to
 * pad/antipad geometry. Approximate model:
 * C_via ~ 1.41*er*d_pad*t / (d_antipad - d_pad) [pF],
 * then Z_discontinuity ~ Z0 * sqrt(1/(1+j*w*C_via*Z0)).
 * Returns impedance deviation from Z0 in percent. */
double via_impedance_discontinuity(const via_transition_t *via,
                                    double freq_hz)
{
    if (!via || via->pad_diam_mm <= via->via_diam_mm ||
        via->antipad_diam_mm <= via->pad_diam_mm ||
        via->board_thickness_mm <= 0.0 || freq_hz <= 0.0)
        return -1.0;
    double er = via->er;
    if (er <= 1.0) er = FR4_ER_1GHZ;
    double d1 = via->pad_diam_mm;
    double d2 = via->antipad_diam_mm;
    double t = via->board_thickness_mm;
    double C_via_pf = 1.41 * er * d1 * t / (d2 - d1);
    /* Assume 50 ohm system */
    double Z0 = 50.0;
    double w = 2.0 * M_PI * freq_hz;
    double Z_via = Z0 / sqrt(1.0 + pow(w * C_via_pf * 1e-12 * Z0, 2.0));
    return (Z0 - Z_via) / Z0 * 100.0;
}

/* Maximum stub length before significant reflection.
 * Rule of thumb: stub < t_rise / (2 * tpd).
 * Longer stubs cause reflections that degrade signal integrity.
 * Returns maximum stub length in mm. */
double maximum_stub_length(double t_rise_ps, double er)
{
    if (t_rise_ps <= 0.0 || er <= 1.0) return -1.0;
    double tpd_ps_per_mm = sqrt(er) / C_LIGHT * 1e12;
    return t_rise_ps / (2.0 * tpd_ps_per_mm);
}

/* Bit error rate from Q-factor (Gaussian approximation).
 * BER = 0.5 * erfc(Q/sqrt(2)).
 * Uses approximation: erfc(x) ~ exp(-x^2)/(x*sqrt(pi)).
 * Returns BER estimate. */
double bit_error_rate_q(double Q_factor)
{
    if (Q_factor <= 0.0) return 0.5;
    if (Q_factor > 7.0) return 1e-12;
    double x = Q_factor / sqrt(2.0);
    double erfc_approx = exp(-x*x) / (x * sqrt(M_PI));
    return 0.5 * erfc_approx;
}
