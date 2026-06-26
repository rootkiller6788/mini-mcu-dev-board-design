/**
 * @file clock_system.c
 * @brief Clock system implementation.
 */

#include "clock_system.h"
#include <math.h>
#include <stdio.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

int compute_load_capacitors(const CrystalSpec *crystal, double c_stray,
                            double *cl1_out, double *cl2_out) {
    if (!crystal || !cl1_out || !cl2_out) return -1;
    if (c_stray >= crystal->load_capacitance) {
        *cl1_out = 0; *cl2_out = 0;
        return -1;
    }
    double cl_net = crystal->load_capacitance - c_stray;
    *cl1_out = 2.0 * cl_net;
    *cl2_out = 2.0 * cl_net;
    if (*cl1_out < 5e-12 || *cl1_out > 30e-12) return -1;
    return 0;
}

double compute_gain_margin(const CrystalSpec *crystal, double gm_osc) {
    if (!crystal) return 0.0;
    if (gm_osc <= 0) return 0.0;
    double omega = 2.0 * M_PI * crystal->nominal_freq;
    double c_total = crystal->shunt_capacitance + crystal->load_capacitance;
    double gm_crit = 4.0 * crystal->esr_max * omega * omega * c_total * c_total;
    if (gm_crit <= 0) return 1e6;
    return gm_osc / gm_crit;
}

double compute_drive_level(const CrystalSpec *crystal, double vpp_across) {
    if (!crystal) return 0.0;
    double omega = 2.0 * M_PI * crystal->nominal_freq;
    double i_rms = (omega * vpp_across * crystal->load_capacitance) / (2.0 * sqrt(2.0));
    return crystal->esr_max * i_rms * i_rms;
}

double compute_rext_limit(const CrystalSpec *crystal, double target_drive_w) {
    if (!crystal || target_drive_w <= 0) return 0.0;
    double omega = 2.0 * M_PI * crystal->nominal_freq;
    double z_cl = 1.0 / (omega * crystal->load_capacitance);
    if (crystal->drive_level_max <= target_drive_w) return 0.0;
    double v_ratio = sqrt(target_drive_w / crystal->drive_level_max);
    if (v_ratio >= 1.0 || v_ratio <= 0.0) return 0.0;
    double rext = z_cl * (1.0 / v_ratio - 1.0);
    if (rext > 5.0 * z_cl) rext = 5.0 * z_cl;
    return rext;
}

double estimate_startup_time(const CrystalSpec *crystal, double gain_margin) {
    if (!crystal) return 0.0;
    if (crystal->nominal_freq <= 0) return 0.0;
    if (gain_margin <= 0) return 1e9;
    double omega = 2.0 * M_PI * crystal->nominal_freq;
    double q_l = 1.0 / (omega * crystal->load_capacitance * crystal->esr_max);
    return (2.0 * q_l) / (M_PI * crystal->nominal_freq * gain_margin);
}

int compute_pll_frequencies(double input_freq_hz, int pll_m, int pll_n,
                            int pll_p, int pll_q,
                            double *vco_out, double *sys_clk, double *usb_clk) {
    if (input_freq_hz <= 0) return -1;
    if (pll_m < 1 || pll_m > 63) return -1;
    if (pll_n < 1 || pll_n > 432) return -1;
    if (pll_p != 1 && pll_p != 2 && pll_p != 4 && pll_p != 6 && pll_p != 8) return -1;
    if (pll_q < 1 || pll_q > 15) return -1;
    double vco_in = input_freq_hz / (double)pll_m;
    if (vco_in < 500000.0 || vco_in > 50000000.0) return -1;
    double vco = vco_in * (double)pll_n;
    if (vco < 16000000.0 || vco > 960000000.0) return -1;
    if (vco_out) *vco_out = vco;
    if (sys_clk) *sys_clk = vco / (double)pll_p;
    if (usb_clk) *usb_clk = vco / (double)pll_q;
    return 0;
}

int find_pll_config(double input_freq_hz, double target_sysclk,
                    double vco_min, double vco_max, int need_usb_48mhz,
                    int *pll_m_out, int *pll_n_out,
                    int *pll_p_out, int *pll_q_out) {
    if (input_freq_hz <= 0 || target_sysclk <= 0) return -1;
    if (!pll_m_out || !pll_n_out || !pll_p_out || !pll_q_out) return -1;
    int best_m = 0, best_n = 0, best_p = 0, best_q = 0;
    double best_error = 1e12;
    int found = 0;
    int p_values[] = {2, 4, 6, 8};
    double usb_target = 48e6;
    for (int m = 2; m <= 63; m++) {
        double vco_in = input_freq_hz / (double)m;
        if (vco_in < 500000.0 || vco_in > 50000000.0) continue;
        for (int n = 50; n <= 432; n++) {
            double vco = vco_in * (double)n;
            if (vco < vco_min || vco > vco_max) continue;
            for (int pi = 0; pi < 4; pi++) {
                int p = p_values[pi];
                double sys_clk = vco / (double)p;
                if (target_sysclk > 0) {
                    double err = fabs(sys_clk - target_sysclk);
                    if (err > target_sysclk * 0.02) continue;
                    if (need_usb_48mhz) {
                        for (int q = 2; q <= 15; q++) {
                            double usb_clk = vco / (double)q;
                            double usb_err = fabs(usb_clk - usb_target);
                            if (usb_err <= usb_target * 0.0025 && err < best_error) {
                                best_error = err;
                                best_m = m; best_n = n; best_p = p; best_q = q;
                                found = 1;
                            }
                        }
                    } else {
                        if (err < best_error) {
                            best_error = err;
                            best_m = m; best_n = n; best_p = p;
                            best_q = (int)(vco / 48e6);
                            if (best_q < 2) best_q = 2;
                            if (best_q > 15) best_q = 15;
                            found = 1;
                        }
                    }
                }
            }
        }
    }
    if (!found) return -1;
    *pll_m_out = best_m; *pll_n_out = best_n;
    *pll_p_out = best_p; *pll_q_out = best_q;
    return 0;
}

int validate_clock_tree(const ClockTree *tree, STM32Series series) {
    if (!tree) return -1;
    double max_sysclk, max_apb1, max_apb2;
    switch (series) {
        case STM32_SERIES_F0: max_sysclk = 48e6;  max_apb1 = 48e6;  max_apb2 = 48e6;  break;
        case STM32_SERIES_F1: max_sysclk = 72e6;  max_apb1 = 36e6;  max_apb2 = 72e6;  break;
        case STM32_SERIES_F2: max_sysclk = 120e6; max_apb1 = 30e6;  max_apb2 = 60e6;  break;
        case STM32_SERIES_F3: max_sysclk = 72e6;  max_apb1 = 36e6;  max_apb2 = 72e6;  break;
        case STM32_SERIES_F4: max_sysclk = 168e6; max_apb1 = 42e6;  max_apb2 = 84e6;  break;
        case STM32_SERIES_F7: max_sysclk = 216e6; max_apb1 = 54e6;  max_apb2 = 108e6; break;
        case STM32_SERIES_H7: max_sysclk = 480e6; max_apb1 = 120e6; max_apb2 = 120e6; break;
        case STM32_SERIES_G0: max_sysclk = 64e6;  max_apb1 = 64e6;  max_apb2 = 64e6;  break;
        case STM32_SERIES_G4: max_sysclk = 170e6; max_apb1 = 170e6; max_apb2 = 170e6; break;
        case STM32_SERIES_L4: max_sysclk = 80e6;  max_apb1 = 80e6;  max_apb2 = 80e6;  break;
        default:              max_sysclk = 216e6; max_apb1 = 54e6;  max_apb2 = 108e6; break;
    }
    if (tree->sysclk > max_sysclk || tree->sysclk <= 0) return -1;
    if (tree->ahb_prescaler < 1 || tree->ahb_prescaler > 512) return -1;
    if ((tree->ahb_prescaler & (tree->ahb_prescaler - 1)) != 0) return -1;
    double expected_hclk = tree->sysclk / (double)tree->ahb_prescaler;
    if (fabs(expected_hclk - tree->hclk) > expected_hclk * 0.01) return -1;
    if (tree->apb1_prescaler < 1 || tree->apb1_prescaler > 16) return -1;
    if ((tree->apb1_prescaler & (tree->apb1_prescaler - 1)) != 0) return -1;
    if (tree->pclk1 > max_apb1) return -1;
    if (tree->apb2_prescaler < 1 || tree->apb2_prescaler > 16) return -1;
    if ((tree->apb2_prescaler & (tree->apb2_prescaler - 1)) != 0) return -1;
    if (tree->pclk2 > max_apb2) return -1;
    return 0;
}


/* Additional clock functions */

double compute_pll_jitter_estimate(double vco_freq, double loop_bandwidth_hz,
                                    double phase_noise_floor_dbc) {
    if (vco_freq <= 0 || loop_bandwidth_hz <= 0) return 0.0;
    double jitter_rms = sqrt(phase_noise_floor_dbc) * vco_freq
                       / (2.0 * M_PI * loop_bandwidth_hz);
    return jitter_rms;
}

int check_pll_lock_range(double vco_freq, double vco_min, double vco_max) {
    return (vco_freq >= vco_min && vco_freq <= vco_max) ? 1 : 0;
}

double compute_hse_bypass_resistor(double oscillator_vpp, double mcu_vih_min) {
    if (oscillator_vpp <= 0 || mcu_vih_min <= 0) return 0.0;
    return 1000.0;
}

double compute_lse_sensitivity(double esr, double cl, double freq) {
    if (esr <= 0 || cl <= 0 || freq <= 0) return 0.0;
    double omega = 2.0 * M_PI * freq;
    double q = 1.0 / (omega * cl * esr);
    return q;
}

int compute_pll_divider_prescaler_chain(double target_freq,
                                         double input_freq,
                                         int *prescaler_out) {
    if (input_freq <= 0 || target_freq <= 0 || !prescaler_out) return -1;
    int prescalers[] = {1, 2, 4, 8, 16, 32, 64, 128, 256, 512};
    int n = sizeof(prescalers) / sizeof(prescalers[0]);
    for (int i = 0; i < n; i++) {
        double result = input_freq / (double)prescalers[i];
        if (fabs(result - target_freq) < target_freq * 0.05) {
            *prescaler_out = prescalers[i];
            return 0;
        }
    }
    return -1;
}
