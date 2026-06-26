/**
 * @file decoupling.c
 * @brief Decoupling capacitor network implementation.
 */

#include "decoupling.h"
#include <math.h>
#include <string.h>
#include <stdio.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

void capacitor_impedance(const DecouplingCap *cap, double freq_hz,
                         double *imp_out, double *phase_out) {
    if (!cap) {
        if (imp_out) *imp_out = 0;
        if (phase_out) *phase_out = 0;
        return;
    }
    double omega = 2.0 * M_PI * freq_hz;
    double xl = omega * cap->esl;
    double xc = 1.0 / (omega * cap->c);
    double reactance = xl - xc;
    double z_mag = sqrt(cap->esr * cap->esr + reactance * reactance);
    double phi_rad = atan2(reactance, cap->esr);
    double phi_deg = phi_rad * 180.0 / M_PI;
    if (imp_out) *imp_out = z_mag;
    if (phase_out) *phase_out = phi_deg;
}

double cap_self_resonant_freq(double c, double esl) {
    if (c <= 0 || esl < 0) return 0.0;
    if (esl == 0) return 1e12;
    return 1.0 / (2.0 * M_PI * sqrt(esl * c));
}

void parallel_cap_impedance(const DecouplingNetwork *network, double freq_hz,
                            double *imp_out, double *phase_out) {
    if (!network || network->num_caps <= 0) {
        if (imp_out) *imp_out = 1e9;
        if (phase_out) *phase_out = 0;
        return;
    }
    double omega = 2.0 * M_PI * freq_hz;
    double y_real = 0.0, y_imag = 0.0;
    for (int i = 0; i < network->num_caps; i++) {
        const DecouplingCap *cap = &network->caps[i];
        double xl = omega * cap->esl;
        double xc = 1.0 / (omega * cap->c);
        double reactance = xl - xc;
        double denom = cap->esr * cap->esr + reactance * reactance;
        if (denom <= 0) continue;
        y_real += cap->esr / denom;
        y_imag += -reactance / denom;
    }
    double y_mag_sq = y_real * y_real + y_imag * y_imag;
    if (y_mag_sq <= 0) {
        if (imp_out) *imp_out = 1e9;
        if (phase_out) *phase_out = 0;
        return;
    }
    double z_mag = 1.0 / sqrt(y_mag_sq);
    double phi_deg = atan2(-y_imag, y_real) * 180.0 / M_PI;
    if (imp_out) *imp_out = z_mag;
    if (phase_out) *phase_out = phi_deg;
}

double compute_target_impedance(double vdd, double ripple_percent,
                                double transient_current) {
    if (transient_current <= 0) return 1e9;
    return vdd * (ripple_percent / 100.0) / transient_current;
}

int min_caps_for_impedance(const DecouplingCap *cap_model, double target_z_ohm,
                           double derating_factor) {
    if (!cap_model || target_z_ohm <= 0) return 0;
    if (derating_factor <= 0 || derating_factor > 1.0) return 0;
    double esr_eff = cap_model->esr;
    if (esr_eff <= target_z_ohm) return 1;
    double n = esr_eff / target_z_ohm;
    int n_int = (int)ceil(n);
    return n_int < 1 ? 1 : n_int;
}

void pdn_impedance_sweep(const DecouplingNetwork *network,
                         double f_start_hz, double f_end_hz,
                         int num_points, PDNImpedanceProfile *profile) {
    if (!network || !profile || num_points <= 0) return;
    if (f_start_hz <= 0 || f_end_hz <= f_start_hz) return;
    memset(profile, 0, sizeof(*profile));
    profile->num_points = num_points > 512 ? 512 : num_points;
    profile->freq_log_start = log10(f_start_hz);
    profile->freq_log_end = log10(f_end_hz);
    profile->peak_impedance = 0.0;
    double log_step = (profile->freq_log_end - profile->freq_log_start)
                     / (double)(profile->num_points - 1);
    for (int i = 0; i < profile->num_points; i++) {
        double freq = pow(10.0, profile->freq_log_start + log_step * (double)i);
        ImpedancePoint *pt = &profile->points[i];
        pt->frequency_hz = freq;
        double imp, phase;
        parallel_cap_impedance(network, freq, &imp, &phase);
        pt->impedance_ohm = imp;
        pt->phase_deg = phase;
        double omega = 2.0 * M_PI * freq;
        double xc = 0.0, xl = omega * network->total_esl;
        if (network->num_caps > 0) xc = 1.0 / (omega * network->caps[0].c);
        pt->is_capacitive = (xl < xc) ? 1 : 0;
        pt->is_inductive = (xl > xc) ? 1 : 0;
        if (imp > profile->peak_impedance) {
            profile->peak_impedance = imp;
            profile->peak_impedance_freq = freq;
        }
    }
}

int find_anti_resonance_peaks(const PDNImpedanceProfile *profile,
                              double z_target,
                              double peaks[], int max_peaks) {
    if (!profile || !peaks || max_peaks <= 0) return 0;
    if (profile->num_points < 3) return 0;
    int count = 0;
    for (int i = 1; i < profile->num_points - 1 && count < max_peaks; i++) {
        double z_prev = profile->points[i - 1].impedance_ohm;
        double z_curr = profile->points[i].impedance_ohm;
        double z_next = profile->points[i + 1].impedance_ohm;
        if (z_curr > z_prev && z_curr > z_next && z_curr > z_target) {
            peaks[count] = profile->points[i].frequency_hz;
            count++;
        }
    }
    return count;
}

double estimate_mounting_inductance(double via_length_mm,
                                    double via_diameter_mm,
                                    double trace_length_mm) {
    if (via_diameter_mm <= 0) return 0.0;
    double l_via_nh = 0.0;
    if (via_length_mm > 0 && via_diameter_mm > 0) {
        double ratio = (4.0 * via_length_mm) / via_diameter_mm;
        if (ratio > 0) l_via_nh = 5.08 * via_length_mm * (log(ratio) + 1.0);
    }
    double l_trace_nh = trace_length_mm * 0.8;
    return (l_via_nh + l_trace_nh) * 1e-9;
}

double dc_bias_derating(double c_nominal, double v_bias, double v_rated) {
    if (v_rated <= 0 || c_nominal <= 0) return c_nominal;
    if (v_bias <= 0) return c_nominal;
    double v_ratio = v_bias / v_rated;
    if (v_ratio >= 1.0) return 0.1 * c_nominal;
    double derating = 1.0 - 0.55 * v_ratio;
    if (derating < 0.15) derating = 0.15;
    return c_nominal * derating;
}

/* =========================================================================
 * Additional PDN functions — each implements an independent knowledge point
 * ========================================================================= */

/*
 * compute_capacitor_bank_optimization
 * L5: Greedy algorithm for selecting optimal capacitor mix.
 *
 * Given a target impedance profile over frequency, select the minimum
 * set of standard capacitor values (100nF, 1uF, 10uF, etc.) that
 * maintains impedance below Z_target from f_min to f_max.
 *
 * The algorithm:
 *   1. Start with one bulk capacitor (largest value)
 *   2. Check impedance at each frequency decade
 *   3. If |Z| > Z_target, add the next smaller capacitor value
 *   4. Repeat until all frequency points pass
 *
 * This is a simplified version of the "Big-V" method from
 * Smith & Bogatin, "Principles of Power Integrity for PDN Design".
 *
 * @param z_target      Target impedance (ohm)
 * @param f_min         Minimum frequency (Hz)
 * @param f_max         Maximum frequency (Hz)
 * @param max_caps      Maximum number of capacitors allowed
 * @param bank          Output: optimized capacitor bank
 * @return              Number of capacitors in bank, 0 if impossible
 */
int compute_capacitor_bank_optimization(double z_target, double f_min,
                                         double f_max, int max_caps,
                                         DecouplingNetwork *bank) {
    if (!bank || z_target <= 0 || f_min <= 0 || f_max <= f_min)
        return 0;

    /* Standard capacitor values and their typical parasitics */
    typedef struct { double c, esr, esl; } StdCap;
    StdCap std_caps[] = {
        {100e-6, 0.050, 2.0e-9},   /* 100uF electrolytic */
        {47e-6,  0.030, 1.5e-9},   /* 47uF */
        {22e-6,  0.020, 1.0e-9},   /* 22uF ceramic */
        {10e-6,  0.010, 0.8e-9},   /* 10uF MLCC */
        {4.7e-6, 0.008, 0.6e-9},   /* 4.7uF MLCC */
        {1.0e-6, 0.005, 0.5e-9},   /* 1uF MLCC */
        {0.1e-6, 0.003, 0.4e-9},   /* 100nF MLCC */
        {0.01e-6,0.002, 0.3e-9},   /* 10nF MLCC */
        {0.001e-6,0.001,0.2e-9},   /* 1nF MLCC */
    };
    int num_std = sizeof(std_caps) / sizeof(std_caps[0]);

    memset(bank, 0, sizeof(*bank));

    /* Start with largest cap as bulk */
    bank->caps[0].c = std_caps[0].c;
    bank->caps[0].esr = std_caps[0].esr;
    bank->caps[0].esl = std_caps[0].esl;
    bank->num_caps = 1;

    bank->total_c = std_caps[0].c;
    bank->total_esr = std_caps[0].esr;
    bank->total_esl = std_caps[0].esl;

    /* Check impedance at key frequencies */
    double check_freqs[] = {1e3, 10e3, 100e3, 1e6, 10e6, 100e6, 500e6};
    int num_checks = sizeof(check_freqs) / sizeof(check_freqs[0]);

    for (int si = 1; si < num_std && bank->num_caps < max_caps; si++) {
        int all_pass = 1;
        for (int fi = 0; fi < num_checks; fi++) {
            if (check_freqs[fi] < f_min || check_freqs[fi] > f_max)
                continue;
            double imp, phase;
            parallel_cap_impedance(bank, check_freqs[fi], &imp, &phase);
            if (imp > z_target) {
                all_pass = 0;
                break;
            }
        }
        if (all_pass) break;

        /* Add next smaller capacitor */
        int idx = bank->num_caps;
        bank->caps[idx].c = std_caps[si].c;
        bank->caps[idx].esr = std_caps[si].esr;
        bank->caps[idx].esl = std_caps[si].esl;
        bank->num_caps++;

        /* Update parallel totals */
        bank->total_c += std_caps[si].c;
        /* Parallel ESR: 1/R_total = sum(1/R_i) */
        bank->total_esr = 1.0 / (1.0 / bank->total_esr + 1.0 / std_caps[si].esr);
        bank->total_esl = 1.0 / (1.0 / bank->total_esl + 1.0 / std_caps[si].esl);
    }

    return bank->num_caps;
}

/*
 * compute_plane_capacitance
 * L3: Parallel-plate capacitance of power/ground plane pair.
 *
 * The power and ground planes in a PCB form a distributed
 * parallel-plate capacitor:
 *   C_plane = epsilon_0 * epsilon_r * A / d
 *
 * where:
 *   epsilon_0 = 8.854e-12 F/m
 *   epsilon_r = relative permittivity (FR-4 ~ 4.2-4.6)
 *   A = plane overlap area (m^2)
 *   d = dielectric thickness (m)
 *
 * This plane capacitance provides very low ESL decoupling
 * at high frequencies (100 MHz - 1 GHz), complementing
 * discrete capacitors which become inductive above their SRF.
 *
 * Example: 100x100mm plane on 0.2mm FR-4:
 *   C = 8.854e-12 * 4.4 * 0.01 / 0.0002 = ~1.95 nF
 *
 * While small, the ESL is extremely low (~50 pH), giving
 * an SRF in the hundreds of MHz range.
 *
 * Reference: Bogatin, "Signal and Power Integrity", Ch 11
 */
double compute_plane_capacitance(double area_mm2, double dielectric_thickness_mm,
                                 double epsilon_r) {
    if (area_mm2 <= 0 || dielectric_thickness_mm <= 0 || epsilon_r <= 0)
        return 0.0;

    double epsilon_0 = 8.854e-12;  /* F/m */
    double area_m2 = area_mm2 * 1e-6;  /* mm^2 -> m^2 */
    double d_m = dielectric_thickness_mm * 1e-3;  /* mm -> m */

    return epsilon_0 * epsilon_r * area_m2 / d_m;
}

/*
 * compute_esr_from_dissipation_factor
 * L3: Relationship between ESR, DF, and capacitance.
 *
 * The dissipation factor (DF or tan(delta)) relates ESR to
 * capacitive reactance:
 *   ESR = DF / (2*pi*f*C)
 *   DF = ESR * 2*pi*f*C = tan(delta)
 *
 * DF is often specified in datasheets at 120Hz or 1kHz.
 * For MLCCs (X7R): DF ~ 1.5-3.5% at 1kHz
 * For tantalum: DF ~ 4-8%
 * For aluminum electrolytic: DF ~ 10-25%
 *
 * This function converts DF to ESR at a given frequency.
 */
double compute_esr_from_dissipation_factor(double capacitance,
                                            double dissipation_factor,
                                            double freq_hz) {
    if (capacitance <= 0 || dissipation_factor < 0 || freq_hz <= 0)
        return 0.0;

    double omega = 2.0 * M_PI * freq_hz;
    return dissipation_factor / (omega * capacitance);
}

/*
 * compute_esl_from_srf
 * L3: Extract ESL from measured self-resonant frequency.
 *
 * Given a capacitor's measured SRF and capacitance, the
 * ESL can be computed:
 *   ESL = 1 / ((2*pi*f_SRF)^2 * C)
 *
 * This is the inverse of the SRF formula.
 */
double compute_esl_from_srf(double capacitance, double srf_hz) {
    if (capacitance <= 0 || srf_hz <= 0) return 0.0;
    double omega = 2.0 * M_PI * srf_hz;
    return 1.0 / (omega * omega * capacitance);
}
