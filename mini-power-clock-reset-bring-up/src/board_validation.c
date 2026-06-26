#include "board_validation.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

/* ===================================================================
 * L2 Core Concepts Implementation
 * =================================================================== */

int eye_mask_test(const eye_diagram_t* eye, double mask_hexagon[6][2], int* mask_hits)
{
    if (!eye || !mask_hexagon || !mask_hits) return -1;
    *mask_hits = 0;
    double eye_voltage = eye->eye_height_mV / 1000.0;
    double eye_time = eye->eye_width_ps * 1e-12;
    for (int i = 0; i < 6; i++) {
        double mv = mask_hexagon[i][1];
        double mt = mask_hexagon[i][0];
        if (eye_voltage < mv * 1.5 || eye_time < mt * 1.5) (*mask_hits)++;
    }
    return (*mask_hits == 0) ? 0 : -1;
}

double pdn_shunt_through_impedance(double s21_magnitude, double s21_phase_deg, double z0_ohm)
{
    double s21_cmplx_real = s21_magnitude * cos(s21_phase_deg * M_PI / 180.0);
    double s21_cmplx_imag = s21_magnitude * sin(s21_phase_deg * M_PI / 180.0);
    double denom_real = 2.0 - 2.0 * s21_cmplx_real;
    double denom_imag = -2.0 * s21_cmplx_imag;
    double z_real = z0_ohm * (s21_cmplx_real * denom_real + s21_cmplx_imag * denom_imag) /
                    (denom_real * denom_real + denom_imag * denom_imag);
    return fabs(z_real);
}

double timing_budget_margin(double clock_period_ns, double setup_ns, double hold_ns,
                             double skew_ns, double jitter_ns)
{
    double margin = clock_period_ns - setup_ns - hold_ns - skew_ns - jitter_ns;
    return margin;
}

double microstrip_impedance(double er, double h_mm, double w_mm, double t_mm)
{
    if (er <= 0.0 || h_mm <= 0.0 || w_mm <= 0.0) return 0.0;
    double z0 = 87.0 / sqrt(er + 1.41) * log(5.98 * h_mm / (0.8 * w_mm + t_mm));
    return z0;
}

/* ===================================================================
 * L3 Mathematical Structures Implementation
 * =================================================================== */

double tdr_impedance_from_reflection(double z0_ohm, double reflection_coefficient)
{
    if (reflection_coefficient >= 1.0 || reflection_coefficient <= -1.0) return z0_ohm;
    return z0_ohm * (1.0 + reflection_coefficient) / (1.0 - reflection_coefficient);
}

void crosstalk_coupling(double c_mutual_pF, double l_mutual_nH, double tr_rise_ns,
                         double v_driver_V, double z0_ohm, double* v_next_mV, double* v_fext_mV)
{
    if (tr_rise_ns <= 0.0 || z0_ohm <= 0.0) return;
    double dvdt = v_driver_V / (tr_rise_ns * 1e-9);
    double i_noise_cap = c_mutual_pF * 1e-12 * dvdt;
    double v_noise_ind = l_mutual_nH * 1e-9 * dvdt / (2.0 * z0_ohm);
    if (v_next_mV) *v_next_mV = (i_noise_cap * z0_ohm / 2.0 + v_noise_ind) * 1000.0;
    if (v_fext_mV) *v_fext_mV = (i_noise_cap * z0_ohm / 2.0 - v_noise_ind) * 1000.0;
}

double ber_from_qfactor(double q_factor)
{
    if (q_factor <= 0.0) return 0.5;
    double x = q_factor / sqrt(2.0);
    double t = 1.0 / (1.0 + 0.3275911 * fabs(x));
    double erfc_approx = (0.254829592 * t - 0.284496736 * t * t + 1.421413741 * t * t * t
                          - 1.453152027 * t * t * t * t + 1.061405429 * t * t * t * t * t)
                         * exp(-x * x);
    if (x < 0.0) erfc_approx = 2.0 - erfc_approx;
    return 0.5 * erfc_approx;
}

double skin_effect_resistance(double r_dc, double frequency_Hz, double thickness_m)
{
    if (frequency_Hz <= 0.0 || thickness_m <= 0.0) return r_dc;
    double rho_copper = 1.72e-8;
    double mu0 = 4.0 * M_PI * 1e-7;
    double delta = sqrt(rho_copper / (M_PI * frequency_Hz * mu0));
    if (thickness_m < 2.0 * delta) {
        return r_dc * (1.0 + thickness_m / (3.0 * delta));
    }
    return r_dc * (thickness_m / delta);
}

/* ===================================================================
 * L4 Fundamental Laws Implementation
 * =================================================================== */

void telegrapher_equations(double r_per_m, double l_per_m, double g_per_m,
                            double c_per_m, double frequency_Hz,
                            double* z0_out, double* gamma_real_out, double* gamma_imag_out)
{
    double omega = 2.0 * M_PI * frequency_Hz;
    double z_real = r_per_m;
    double z_imag = omega * l_per_m;
    double y_real = g_per_m;
    double y_imag = omega * c_per_m;
    double zy_real = z_real * y_real - z_imag * y_imag;
    double zy_imag = z_real * y_imag + z_imag * y_real;
    double gamma_mag = sqrt(sqrt(zy_real * zy_real + zy_imag * zy_imag));
    double gamma_phase = atan2(zy_imag, zy_real) / 2.0;
    if (gamma_real_out) *gamma_real_out = gamma_mag * cos(gamma_phase);
    if (gamma_imag_out) *gamma_imag_out = gamma_mag * sin(gamma_phase);
    if (z0_out) {
        double z_ratio_real = z_real * y_real + z_imag * y_imag;
        double z_ratio_imag = z_imag * y_real - z_real * y_imag;
        double denom = y_real * y_real + y_imag * y_imag;
        double z0_real = sqrt((z_ratio_real / denom + sqrt(z_ratio_real * z_ratio_real /
                              (denom * denom) + z_ratio_imag * z_ratio_imag / (denom * denom))) / 2.0);
        *z0_out = fabs(z0_real);
    }
}

double faraday_induced_voltage(double loop_area_m2, double b_field_T, double frequency_Hz)
{
    double omega = 2.0 * M_PI * frequency_Hz;
    return omega * loop_area_m2 * b_field_T;
}

/* ===================================================================
 * L5 Algorithms Implementation
 * =================================================================== */

void eye_diagram_analyze(const double* time_ns, const double* voltage_V,
                          int num_samples, double bit_period_ns, eye_diagram_t* eye)
{
    if (!time_ns || !voltage_V || !eye || num_samples < 10 || bit_period_ns <= 0.0) return;
    memset(eye, 0, sizeof(*eye));
    double v_min = 1e9, v_max = -1e9;
    double v_high_sum = 0.0, v_low_sum = 0.0;
    int v_high_count = 0, v_low_count = 0;
    for (int i = 0; i < num_samples; i++) {
        if (voltage_V[i] < v_min) v_min = voltage_V[i];
        if (voltage_V[i] > v_max) v_max = voltage_V[i];
    }
    double v_mid = (v_min + v_max) / 2.0;
    for (int i = 0; i < num_samples; i++) {
        if (voltage_V[i] > v_mid) { v_high_sum += voltage_V[i]; v_high_count++; }
        else { v_low_sum += voltage_V[i]; v_low_count++; }
    }
    double v_high_avg = v_high_count > 0 ? v_high_sum / v_high_count : v_max;
    double v_low_avg = v_low_count > 0 ? v_low_sum / v_low_count : v_min;
    eye->eye_height_mV = (v_high_avg - v_low_avg) * 1000.0;
    eye->eye_height_percent = (v_high_avg - v_low_avg) / (v_max - v_min) * 100.0;
    eye->eye_width_UI = 0.7;
    eye->eye_width_ps = bit_period_ns * 1000.0 * eye->eye_width_UI;
    eye->q_factor = (v_high_avg - v_low_avg) / ((v_max - v_min) * 0.1);
    eye->estimated_ber = ber_from_qfactor(eye->q_factor);
    eye->eye_open = (eye->eye_height_mV > 10.0) ? 1 : 0;
}

int pdn_violation_detect(const double* freq_Hz, const double* z_measured_ohm,
                          const double* z_target_ohm, int num_points,
                          double* violation_freqs_Hz, double* violation_margins, int max_violations)
{
    if (!freq_Hz || !z_measured_ohm || !z_target_ohm || num_points <= 0 || max_violations <= 0) return 0;
    int count = 0;
    for (int i = 0; i < num_points && count < max_violations; i++) {
        if (z_measured_ohm[i] > z_target_ohm[i]) {
            if (violation_freqs_Hz) violation_freqs_Hz[count] = freq_Hz[i];
            if (violation_margins) violation_margins[count] = z_measured_ohm[i] - z_target_ohm[i];
            count++;
        }
    }
    return count;
}

double extrapolate_ber_at_threshold(const eye_diagram_t* eye, double target_ber)
{
    if (!eye || eye->estimated_ber <= 0.0 || target_ber <= 0.0) return -1.0;
    return log10(target_ber / eye->estimated_ber);
}

/* ===================================================================
 * L6 Canonical Problems Implementation
 * =================================================================== */

int usb20_eye_compliance(const eye_diagram_t* eye, double* margin_mV, double* margin_ps)
{
    if (!eye) return -1;
    double required_height_mV = 300.0;
    double required_width_ps = 1100.0;
    double margin_v = eye->eye_height_mV - required_height_mV;
    double margin_t = eye->eye_width_ps - required_width_ps;
    if (margin_mV) *margin_mV = margin_v;
    if (margin_ps) *margin_ps = margin_t;
    if (margin_v < 0.0 || margin_t < 0.0) return -1;
    return 0;
}

int ddr_signal_integrity(const signal_integrity_test_t* signals, int num_signals)
{
    if (!signals || num_signals <= 0) return -1;
    int fail_mask = 0;
    for (int i = 0; i < num_signals; i++) {
        if (signals[i].setup_margin_ns < 0.0) fail_mask |= (1 << (i * 2));
        if (signals[i].hold_margin_ns < 0.0) fail_mask |= (1 << (i * 2 + 1));
        if (signals[i].overshoot_percent > 10.0) fail_mask |= (1 << (i * 2));
        if (!signals[i].monotonic_rising) fail_mask |= (1 << (i * 2 + 1));
    }
    return fail_mask;
}

int spi_signal_check(double sck_freq_Hz, double rise_time_ns, double fall_time_ns,
                      double setup_margin_ns, double hold_margin_ns)
{
    int issues = 0;
    if (sck_freq_Hz > 50e6) issues |= (1 << 0);
    if (rise_time_ns > 10.0) issues |= (1 << 1);
    if (fall_time_ns > 10.0) issues |= (1 << 2);
    if (setup_margin_ns < 5.0) issues |= (1 << 3);
    if (hold_margin_ns < 5.0) issues |= (1 << 4);
    return issues;
}

/* ===================================================================
 * L7 Applications Implementation
 * =================================================================== */

int fcc_part15_classB_precompliance(const emc_emission_point_t* measurements,
                                      int num_points, int* failing_frequencies, int max_fails)
{
    if (!measurements || !failing_frequencies || num_points <= 0 || max_fails <= 0) return 0;
    int fail_count = 0;
    for (int i = 0; i < num_points && fail_count < max_fails; i++) {
        double margin = measurements[i].margin_dB;
        if (margin < 6.0) {
            failing_frequencies[fail_count++] = i;
        }
    }
    return fail_count;
}

int cispr25_conducted_emissions(const emc_emission_point_t* measurements,
                                  int num_points, int emc_class, int* num_failures)
{
    if (!measurements || !num_failures) return -1;
    int fail_count = 0;
    double class_margin[] = {12.0, 10.0, 6.0, 3.0, 0.0};
    double margin_dB = (emc_class >= 1 && emc_class <= 5) ? class_margin[emc_class - 1] : 6.0;
    for (int i = 0; i < num_points; i++) {
        if (measurements[i].margin_dB < margin_dB) fail_count++;
    }
    *num_failures = fail_count;
    return fail_count;
}

int milstd461_rs103_susceptibility(double field_strength_V_per_m, double frequency_Hz,
                                     int* degradation_detected)
{
    if (!degradation_detected) return -1;
    double max_field = 200.0;
    if (frequency_Hz < 2e6 || frequency_Hz > 40e9) return -1;
    *degradation_detected = (field_strength_V_per_m > max_field) ? 1 : 0;
    return *degradation_detected;
}

/* ===================================================================
 * L8 Advanced Topics Implementation
 * =================================================================== */

double statistical_eye_confidence(const eye_diagram_t* eye, double target_ber,
                                   double confidence_level)
{
    if (!eye || eye->estimated_ber <= 0.0 || target_ber <= 0.0) return -1.0;
    double z_score;
    if (confidence_level >= 0.99) z_score = 2.576;
    else if (confidence_level >= 0.95) z_score = 1.96;
    else if (confidence_level >= 0.90) z_score = 1.645;
    else z_score = 1.0;
    double margin = log10(target_ber / eye->estimated_ber);
    return margin - z_score * 0.5;
}

double ibis_ami_correlation(const eye_diagram_t* simulated, const eye_diagram_t* measured)
{
    if (!simulated || !measured) return 0.0;
    double dh = fabs(simulated->eye_height_mV - measured->eye_height_mV);
    double dw = fabs(simulated->eye_width_ps - measured->eye_width_ps);
    double h_norm = dh / (measured->eye_height_mV + 1e-9);
    double w_norm = dw / (measured->eye_width_ps + 1e-9);
    return 1.0 - (h_norm * 0.5 + w_norm * 0.5);
}
