#include "clock_design.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

/* ===================================================================
 * L2 Core Concepts Implementation
 * =================================================================== */

void crystal_load_capacitor(const crystal_spec_t* crystal, double c_stray_pF, double* c_ext_pF)
{
    if (!crystal || !c_ext_pF) return;
    double cl = crystal->load_cap_pF;
    *c_ext_pF = 2.0 * (cl - c_stray_pF);
    if (*c_ext_pF < 0.0) *c_ext_pF = 0.0;
}

int barkhausen_check(const crystal_spec_t* crystal, double c_load_pF,
                     double gm_actual_S, pierce_oscillator_t* oscillator)
{
    if (!crystal || !oscillator) return -1;
    double omega = 2.0 * M_PI * crystal->nominal_freq_Hz;
    double cl_total = c_load_pF * 1e-12;
    double c0 = crystal->shunt_cap_pF * 1e-12;
    double gm_crit = 4.0 * crystal->esr_ohm * omega * omega * (cl_total + c0) * (cl_total + c0);
    oscillator->gm_critical_S = gm_crit;
    if (gm_crit <= 0.0) return -1;
    oscillator->gain_margin = gm_actual_S / gm_crit;
    oscillator->c_in_pF = c_load_pF;
    oscillator->c_out_pF = c_load_pF;
    oscillator->rf_ohm = 1e6;
    oscillator->rd_ohm = 0.0;
    if (oscillator->gain_margin < 5.0) {
        return -1;
    }
    return 0;
}

int pll_frequency_synthesis(double ref_freq_Hz, double target_freq_Hz,
                            double vco_min_Hz, double vco_max_Hz, pll_config_t* config)
{
    if (!config || ref_freq_Hz <= 0.0 || target_freq_Hz <= 0.0) return -1;
    int best_r = 1, best_n = 1;
    double best_error = 1e18;
    int found = 0;
    for (int r = 1; r <= 64; r++) {
        double f_pfd = ref_freq_Hz / r;
        if (f_pfd < 100e3 || f_pfd > 100e6) continue;
        double n_exact = target_freq_Hz / f_pfd;
        int n = (int)(n_exact + 0.5);
        if (n < 1) continue;
        double f_vco = f_pfd * n;
        if (f_vco < vco_min_Hz || f_vco > vco_max_Hz) continue;
        double error = fabs(f_vco - target_freq_Hz);
        if (error < best_error) {
            best_error = error;
            best_r = r;
            best_n = n;
            found = 1;
        }
    }
    if (!found) return -1;
    config->ref_freq_Hz = ref_freq_Hz;
    config->vco_freq_Hz = ref_freq_Hz * best_n / best_r;
    config->ref_div = best_r;
    config->feedback_div = best_n;
    config->loop_bandwidth_Hz = 100e3;
    config->phase_margin_deg = 60.0;
    return 0;
}

void clock_tree_jitter_accumulate(clock_tree_node_t* root)
{
    if (!root) return;
    for (int i = 0; i < root->num_children; i++) {
        clock_tree_node_t* child = root->children[i];
        if (!child) continue;
        double sigma_parent = root->accumulated_jitter.phase_jitter_ps_rms;
        double sigma_child = child->accumulated_jitter.phase_jitter_ps_rms;
        double total_rms = sqrt(sigma_parent * sigma_parent + sigma_child * sigma_child);
        child->accumulated_jitter.phase_jitter_ps_rms = total_rms;
        clock_tree_jitter_accumulate(child);
    }
}

void crystal_drive_check(const crystal_spec_t* crystal, double i_rms_uA, crystal_drive_level_t* result)
{
    if (!crystal || !result) return;
    double p_actual_uW = (i_rms_uA * 1e-6) * (i_rms_uA * 1e-6) * crystal->esr_ohm * 1e6;
    result->measured_power_uW = p_actual_uW;
    if (crystal->drive_level_uW > 0.0) {
        result->margin_percent = (1.0 - p_actual_uW / crystal->drive_level_uW) * 100.0;
        result->within_spec = (p_actual_uW < crystal->drive_level_uW) ? 1 : 0;
    } else {
        result->margin_percent = 0.0;
        result->within_spec = 1;
    }
}

/* ===================================================================
 * L3 Mathematical Structures Implementation
 * =================================================================== */

double leeson_phase_noise(double f_m_Hz, double f0_Hz, double q_loaded,
                          double f_corner_Hz, double noise_factor, double p_signal_W)
{
    if (f_m_Hz <= 0.0 || f0_Hz <= 0.0 || q_loaded <= 0.0 || p_signal_W <= 0.0) return 0.0;
    double k = 1.380649e-23;
    double T = 300.0;
    double thermal_noise = noise_factor * k * T / (2.0 * p_signal_W);
    double flicker_term = 1.0 + f_corner_Hz / f_m_Hz;
    double resonator_term = 1.0 + pow(f0_Hz / (2.0 * q_loaded * f_m_Hz), 2.0);
    return 10.0 * log10(thermal_noise * flicker_term * resonator_term);
}

void crystal_resonance_frequencies(const crystal_spec_t* crystal, double* f_series_Hz, double* f_parallel_Hz)
{
    if (!crystal) return;
    double L1 = crystal->motional_ind_mH * 1e-3;
    double C1 = crystal->motional_cap_fF * 1e-15;
    double C0 = crystal->shunt_cap_pF * 1e-12;
    if (L1 > 0.0 && C1 > 0.0) {
        double fs = 1.0 / (2.0 * M_PI * sqrt(L1 * C1));
        if (f_series_Hz) *f_series_Hz = fs;
        if (f_parallel_Hz) *f_parallel_Hz = fs * sqrt(1.0 + C1 / C0);
    } else {
        if (f_series_Hz) *f_series_Hz = crystal->nominal_freq_Hz;
        if (f_parallel_Hz) *f_parallel_Hz = crystal->nominal_freq_Hz * 1.001;
    }
}

double pll_transfer_magnitude(double f_offset_Hz, double loop_bw_Hz, double phase_margin_deg)
{
    if (loop_bw_Hz <= 0.0) return 1.0;
    double omega_c = 2.0 * M_PI * loop_bw_Hz;
    double omega = 2.0 * M_PI * f_offset_Hz;
    double pm_rad = phase_margin_deg * M_PI / 180.0;
    double tau1 = (1.0 - sin(pm_rad)) / (omega_c * cos(pm_rad));
    double tau2 = 1.0 / (omega_c * omega_c * tau1);
    if (omega <= 0.0) return 1.0;
    double num = sqrt(1.0 + omega * omega * tau2 * tau2);
    double den = omega * tau1;
    if (den <= 0.0) return 1e9;
    return num / den;
}

double crystal_q_factor(const crystal_spec_t* crystal)
{
    if (!crystal || crystal->esr_ohm <= 0.0) return 0.0;
    double L1 = crystal->motional_ind_mH * 1e-3;
    double C1 = crystal->motional_cap_fF * 1e-15;
    return (1.0 / crystal->esr_ohm) * sqrt(L1 / C1);
}

/* ===================================================================
 * L4 Fundamental Laws Implementation
 * =================================================================== */

double adc_jitter_limit(int n_bits, double f_input_Hz)
{
    if (f_input_Hz <= 0.0 || n_bits <= 0) return 0.0;
    double max_jitter = 1.0 / (M_PI * f_input_Hz * pow(2.0, n_bits + 1));
    return max_jitter;
}

double phase_noise_to_jitter(const phase_noise_t* pn, double f_start_Hz, double f_stop_Hz)
{
    if (!pn || f_start_Hz >= f_stop_Hz || f_start_Hz <= 0.0) return 0.0;
    double offsets[6] = {100.0, 1000.0, 10000.0, 100000.0, 1000000.0, 10000000.0};
    double pn_values[6] = {pn->phase_noise_at_100Hz, pn->phase_noise_at_1kHz,
                           pn->phase_noise_at_10kHz, pn->phase_noise_at_100kHz,
                           pn->phase_noise_at_1MHz, pn->noise_floor_dbHz};
    double total_power = 0.0;
    for (int i = 0; i < 5; i++) {
        double f1 = offsets[i];
        double f2 = offsets[i+1];
        if (f2 <= f_start_Hz || f1 >= f_stop_Hz) continue;
        if (f1 < f_start_Hz) f1 = f_start_Hz;
        if (f2 > f_stop_Hz) f2 = f_stop_Hz;
        double pn_linear = pow(10.0, pn_values[i] / 10.0);
        total_power += pn_linear * (f2 - f1);
    }
    double jitter_squared = total_power / (2.0 * M_PI * M_PI * pn->carrier_freq_Hz * pn->carrier_freq_Hz);
    return sqrt(jitter_squared);
}

double allan_deviation(double stability_1s, double tau_sec, int noise_type)
{
    if (tau_sec <= 0.0) return stability_1s;
    switch (noise_type) {
        case 0: return stability_1s / sqrt(tau_sec);
        case 1: return stability_1s;
        case 2: return stability_1s * sqrt(tau_sec);
        default: return stability_1s;
    }
}

/* ===================================================================
 * L5 Algorithms Implementation
 * =================================================================== */

void pll_loop_filter_design(const pll_config_t* pll, double* c1_out, double* r2_out, double* c2_out)
{
    if (!pll || !c1_out || !r2_out || !c2_out) return;
    double wc = 2.0 * M_PI * pll->loop_bandwidth_Hz;
    double pm_rad = pll->phase_margin_deg * M_PI / 180.0;
    double sin_pm = sin(pm_rad);
    double cos_pm = cos(pm_rad);
    double tau1 = (1.0 - sin_pm) / (wc * cos_pm);
    double tau2 = 1.0 / (wc * wc * tau1);
    double icp = pll->charge_pump_current_A;
    double kvco = pll->kvco_Hz_per_V;
    int N = pll->feedback_div;
    if (N <= 0) N = 1;
    double numerator = icp * kvco;
    double sqrt_term = sqrt((1.0 + wc * wc * tau2 * tau2) / (1.0 + wc * wc * tau1 * tau1));
    double c1 = (tau1 / tau2) * (numerator / (wc * wc * N)) * sqrt_term;
    double r2 = tau2 / c1;
    double c2 = c1 / 10.0;
    *c1_out = c1;
    *r2_out = r2;
    *c2_out = c2;
}

void jitter_decomposition(double rms_jitter_ps, double pp_jitter_ps,
                          double* rj_out_ps, double* dj_out_ps)
{
    double alpha = 14.069;
    double dj_pp = pp_jitter_ps - alpha * rms_jitter_ps;
    if (dj_pp < 0.0) dj_pp = 0.0;
    double dj_rms = dj_pp / alpha;
    double rj_var = rms_jitter_ps * rms_jitter_ps - dj_rms * dj_rms;
    if (rj_var < 0.0) rj_var = 0.0;
    if (rj_out_ps) *rj_out_ps = sqrt(rj_var);
    if (dj_out_ps) *dj_out_ps = dj_pp;
}

double sscg_profile(double f_nom_Hz, double delta_f_Hz, double f_mod_Hz, double t_sec, int down_spread)
{
    double period = 1.0 / f_mod_Hz;
    double phase = fmod(t_sec, period) / period;
    double triangle;
    if (phase < 0.25) triangle = phase * 4.0;
    else if (phase < 0.75) triangle = 2.0 - phase * 4.0;
    else triangle = phase * 4.0 - 4.0;
    if (down_spread) return f_nom_Hz - delta_f_Hz * (1.0 + triangle) / 2.0;
    else return f_nom_Hz + delta_f_Hz * triangle / 2.0;
}

/* ===================================================================
 * L6 Canonical Problems Implementation
 * =================================================================== */

int stm32f4_pll_config(double hse_freq_Hz, double target_sysclk,
                       int* pll_m, int* pll_n, int* pll_p, int* pll_q)
{
    int m, n, p, q;
    int found = 0;
    for (m = 2; m <= 63; m++) {
        double f_vco_in = hse_freq_Hz / m;
        if (f_vco_in < 0.95e6 || f_vco_in > 2.1e6) continue;
        for (n = 50; n <= 432; n++) {
            double f_vco = f_vco_in * n;
            if (f_vco < 100e6 || f_vco > 432e6) continue;
            int p_vals[] = {2, 4, 6, 8};
            for (int pi = 0; pi < 4; pi++) {
                p = p_vals[pi];
                double sysclk = f_vco / p;
                if (fabs(sysclk - target_sysclk) < 1e6) {
                    found = 1;
                    break;
                }
            }
            if (found) break;
        }
        if (found) {
            double f_vco_final = (hse_freq_Hz / m) * n;
            for (q = 2; q <= 15; q++) {
                double usb_clk = f_vco_final / q;
                if (fabs(usb_clk - 48e6) < 1e6) break;
            }
            if (pll_m) *pll_m = m;
            if (pll_n) *pll_n = n;
            if (pll_p) *pll_p = p;
            if (pll_q) *pll_q = q;
            return 0;
        }
    }
    return -1;
}

int esp32_clock_config(double xtal_freq_Hz, double target_cpu_Hz, int* pll_mult, int* cpu_div)
{
    if (xtal_freq_Hz <= 0.0) return -1;
    double valid_targets[] = {80e6, 160e6, 240e6};
    int valid = 0;
    for (int i = 0; i < 3; i++) {
        if (fabs(target_cpu_Hz - valid_targets[i]) < 1e6) { valid = 1; break; }
    }
    if (!valid) return -1;
    int mult = (int)(target_cpu_Hz / xtal_freq_Hz + 0.5);
    if (mult * xtal_freq_Hz != target_cpu_Hz) return -1;
    if (pll_mult) *pll_mult = mult;
    if (cpu_div) *cpu_div = 1;
    return 0;
}

int nrf52_clock_validate(double hfclk_Hz, double lfclk_Hz, double hfclk_tol_ppm)
{
    int ok = 1;
    if (fabs(hfclk_Hz - 32e6) > 0.1e6) ok = 0;
    if (fabs(lfclk_Hz - 32768.0) > 100.0) ok = 0;
    if (hfclk_tol_ppm > 60.0) ok = 0;
    return ok;
}

/* ===================================================================
 * L7 Applications Implementation
 * =================================================================== */

double gpsdo_accuracy(double ocxo_stability_1s, double gps_1pps_jitter_ns,
                      double pll_time_constant_s, double* locked_accuracy_ppb)
{
    double gps_accuracy = 4e-12;
    double tau = pll_time_constant_s;
    double locked_acc = gps_accuracy + ocxo_stability_1s / sqrt(tau / 1.0);
    double acc_ppb = locked_acc * 1e9;
    if (locked_accuracy_ppb) *locked_accuracy_ppb = acc_ppb;
    double lock_time = 5.0 * tau;
    (void)gps_1pps_jitter_ns;
    return lock_time;
}

int pcie_refclk_budget(double crystal_jitter_ps, double pll_jitter_ps,
                       double buffer_jitter_ps, double spec_limit_ps)
{
    double total_rms = sqrt(crystal_jitter_ps * crystal_jitter_ps +
                            pll_jitter_ps * pll_jitter_ps +
                            buffer_jitter_ps * buffer_jitter_ps);
    return (total_rms <= spec_limit_ps) ? 0 : -1;
}

/* ===================================================================
 * L8 Advanced Topics Implementation
 * =================================================================== */

double mems_vs_quartz_comparison(double mems_pn_1kHz, double mems_stability_ppm,
                                  double quartz_pn_1kHz, double quartz_stability_ppm)
{
    double pn_score = quartz_pn_1kHz - mems_pn_1kHz;
    double stab_score = quartz_stability_ppm - mems_stability_ppm;
    double total_score = pn_score * 0.6 + stab_score * (-0.4);
    return total_score;
}

double sscg_emi_reduction(double f_clock_Hz, double delta_percent, double rbw_Hz)
{
    double delta = delta_percent / 100.0;
    double reduction = 10.0 * log10(delta * f_clock_Hz / rbw_Hz);
    return reduction > 0.0 ? reduction : 0.0;
}
