#include "power_design.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

/* ===================================================================
 * L2: Core Concepts Implementation
 * =================================================================== */

int power_topology_select(double v_in, double v_out, double i_load, int noise_sensitive)
{
    double vdrop = v_in - v_out;
    if (vdrop <= 0.0) return -1;

    if (noise_sensitive) {
        if (vdrop < 0.3 && i_load < 0.1) return 0;
        return 0;
    }
    if (vdrop < 0.5 && i_load < 0.5) return 0;
    if (vdrop > 1.0 && i_load > 0.1) return 1;
    if (i_load > 0.05) return 1;
    return 0;
}

static double tree_power_helper(const power_tree_node_t* node, double* p_out_total)
{
    double p_in = node->voltage * node->current_draw;
    double p_children_out = 0.0;
    for (int i = 0; i < node->num_children; i++) {
        const power_tree_node_t* child = node->children[i];
        if (child) p_children_out += tree_power_helper(child, p_out_total);
    }
    *p_out_total += p_children_out;
    return p_in;
}

double power_tree_efficiency(const power_tree_node_t* root)
{
    if (!root) return 0.0;
    double p_out_total = 0.0;
    double p_in = tree_power_helper(root, &p_out_total);
    if (p_in <= 0.0) return 0.0;
    return p_out_total / p_in;
}

double pdn_target_impedance(const power_rail_spec_t* rail, double i_transient_max)
{
    if (!rail || i_transient_max <= 0.0) return 0.0;
    double ripple_pct = ((rail->voltage_max - rail->voltage_min) / rail->voltage_nominal) * 100.0;
    return rail->voltage_nominal * (ripple_pct / 100.0) / i_transient_max;
}

double ipc2221_trace_width(double current_A, double temp_rise_C, int inner_layer)
{
    if (current_A <= 0.0 || temp_rise_C <= 0.0) return 0.0;
    double k = inner_layer ? 0.024 : 0.048;
    double area_sq_mils = pow(current_A / (k * pow(temp_rise_C, 0.44)), 1.0 / 0.725);
    double width_mm = area_sq_mils * 0.0254 * 0.0254 / 0.035;
    return width_mm;
}

double decoupling_cap_value(double target_impedance_ohm, double frequency_Hz)
{
    if (target_impedance_ohm <= 0.0 || frequency_Hz <= 0.0) return 0.0;
    return 1.0 / (2.0 * M_PI * frequency_Hz * target_impedance_ohm);
}

int power_seq_ready(const power_sequence_step_t* seq, double measured_voltage)
{
    if (!seq) return 0;
    return (measured_voltage >= seq->min_voltage_before_next) ? 1 : 0;
}

/* ===================================================================
 * L3: Mathematical Structures Implementation
 * =================================================================== */

double ldo_junction_temp(const ldo_regulator_t* ldo, double t_ambient, double i_load)
{
    if (!ldo) return t_ambient;
    double vdrop = ldo->v_in - ldo->v_out;
    if (vdrop < 0.0) vdrop = 0.0;
    double p_diss = vdrop * i_load + ldo->v_in * ldo->i_q;
    return t_ambient + p_diss * ldo->thermal_resistance_ja;
}

double buck_duty_cycle(const dcdc_converter_t* dcdc)
{
    if (!dcdc || dcdc->v_in_min <= 0.0) return -1.0;
    double vin = dcdc->v_in_min;
    if (dcdc->v_out > vin) return -1.0;
    if (dcdc->v_out <= 0.0 || vin <= 0.0) return -1.0;
    return dcdc->v_out / (vin * dcdc->efficiency);
}

double buck_inductor_ripple(const dcdc_converter_t* dcdc, double duty)
{
    if (!dcdc || dcdc->switching_freq <= 0.0 || dcdc->inductor_uH <= 0.0) return 0.0;
    if (duty <= 0.0 || duty >= 1.0) return 0.0;
    double vin = dcdc->v_in_min;
    double vout = dcdc->v_out;
    double L = dcdc->inductor_uH * 1e-6;
    double fsw = dcdc->switching_freq;
    return (vin - vout) * duty / (fsw * L);
}

double rc_time_constant(double resistance_ohm, double capacitance_F)
{
    if (resistance_ohm < 0.0 || capacitance_F < 0.0) return 0.0;
    return resistance_ohm * capacitance_F;
}

double buck_ccm_boundary(const dcdc_converter_t* dcdc, double duty)
{
    if (!dcdc || dcdc->switching_freq <= 0.0 || dcdc->inductor_uH <= 0.0) return 0.0;
    double vin = dcdc->v_in_min;
    double vout = dcdc->v_out;
    double L = dcdc->inductor_uH * 1e-6;
    double fsw = dcdc->switching_freq;
    return (vin - vout) * duty / (2.0 * fsw * L);
}

double boost_duty_cycle(double v_in, double v_out)
{
    if (v_in <= 0.0 || v_out <= v_in) return 0.0;
    return 1.0 - v_in / v_out;
}

double capacitor_impedance(double capacitance_F, double esr_ohm, double esl_H, double freq_Hz)
{
    if (capacitance_F <= 0.0 || freq_Hz <= 0.0) return 0.0;
    double omega = 2.0 * M_PI * freq_Hz;
    double xc = -1.0 / (omega * capacitance_F);
    double xl = omega * esl_H;
    double reactance = xl + xc;
    return sqrt(esr_ohm * esr_ohm + reactance * reactance);
}

double inductor_energy(double inductance_H, double current_A)
{
    if (inductance_H < 0.0 || current_A < 0.0) return 0.0;
    return 0.5 * inductance_H * current_A * current_A;
}

double lc_resonant_frequency(double inductance_H, double capacitance_F)
{
    if (inductance_H <= 0.0 || capacitance_F <= 0.0) return 0.0;
    return 1.0 / (2.0 * M_PI * sqrt(inductance_H * capacitance_F));
}

/* ===================================================================
 * L4: Fundamental Laws Implementation
 * =================================================================== */

double trace_voltage_drop(double length_m, double width_m, double thickness_m, double current_a)
{
    if (width_m <= 0.0 || thickness_m <= 0.0) return 0.0;
    double rho_copper = 1.72e-8;
    double resistance = rho_copper * length_m / (width_m * thickness_m);
    return current_a * resistance;
}

static double kcl_helper(const power_tree_node_t* node, double* max_residual)
{
    double children_sum = 0.0;
    for (int i = 0; i < node->num_children; i++) {
        if (node->children[i]) children_sum += kcl_helper(node->children[i], max_residual);
    }
    double residual = node->current_draw - children_sum;
    double abs_residual = fabs(residual);
    if (abs_residual > *max_residual) *max_residual = abs_residual;
    return node->current_draw;
}

double kcl_power_node(const power_tree_node_t* node)
{
    if (!node) return 0.0;
    double children_sum = 0.0;
    for (int i = 0; i < node->num_children; i++) {
        if (node->children[i]) children_sum += node->children[i]->current_draw;
    }
    return node->current_draw - children_sum;
}

double power_tree_kcl_verify(const power_tree_node_t* root)
{
    if (!root) return 0.0;
    double max_residual = 0.0;
    kcl_helper(root, &max_residual);
    return max_residual;
}

double regulator_power_loss(double v_in, double v_out, double i_out, double efficiency)
{
    if (efficiency <= 0.0 || efficiency > 1.0) return 0.0;
    return v_out * i_out * (1.0 / efficiency - 1.0);
}

double joule_heating(double current_A, double resistance_ohm)
{
    if (current_A < 0.0 || resistance_ohm < 0.0) return 0.0;
    return current_A * current_A * resistance_ohm;
}

double dc_power(double voltage_V, double current_A)
{
    return voltage_V * current_A;
}

double capacitor_energy(double capacitance_F, double voltage_V)
{
    if (capacitance_F < 0.0 || voltage_V < 0.0) return 0.0;
    return 0.5 * capacitance_F * voltage_V * voltage_V;
}

double kvl_voltage_check(double v_source, const double* v_drops, int num_drops, double v_load)
{
    double sum_drops = 0.0;
    for (int i = 0; i < num_drops; i++) sum_drops += v_drops[i];
    return v_source - sum_drops - v_load;
}

/* ===================================================================
 * L5: Algorithms Implementation
 * =================================================================== */

void decoupling_impedance_sweep(const decoupling_cap_t* caps, int num_caps,
                                const double* frequencies, int num_freqs, double* z_out)
{
    if (!caps || !frequencies || !z_out || num_caps <= 0 || num_freqs <= 0) return;
    for (int fi = 0; fi < num_freqs; fi++) {
        double sum_admittance_real = 0.0, sum_admittance_imag = 0.0;
        for (int ci = 0; ci < num_caps; ci++) {
            double omega = 2.0 * M_PI * frequencies[fi];
            double c = caps[ci].capacitance_F;
            double esr = caps[ci].esr_ohm;
            double esl = caps[ci].esl_H;
            if (c <= 0.0) continue;
            double x = omega * esl - 1.0 / (omega * c);
            double denom = esr * esr + x * x;
            if (denom <= 0.0) continue;
            sum_admittance_real += esr / denom;
            sum_admittance_imag += -x / denom;
        }
        double y_mag = sqrt(sum_admittance_real * sum_admittance_real +
                            sum_admittance_imag * sum_admittance_imag);
        z_out[fi] = (y_mag > 1e-15) ? (1.0 / y_mag) : 1e15;
    }
}

double ldo_phase_margin(double f_pole, double f_crossover, double f_esr_zero)
{
    if (f_pole <= 0.0 || f_crossover <= 0.0) return 0.0;
    double pm = 90.0;
    pm -= atan(f_crossover / f_pole) * 180.0 / M_PI;
    if (f_esr_zero > 0.0) pm += atan(f_crossover / f_esr_zero) * 180.0 / M_PI;
    return pm;
}

double buck_output_capacitance(double delta_i_load, double t_response, double delta_v_max)
{
    if (delta_v_max <= 0.0 || t_response <= 0.0) return 0.0;
    return delta_i_load * t_response / delta_v_max;
}

double inrush_limiter_resistance(double v_in_max, double i_inrush_max,
                                 double total_cap_F, double* energy_j_out)
{
    if (i_inrush_max <= 0.0) return 0.0;
    double r_limit = v_in_max / i_inrush_max;
    if (energy_j_out) *energy_j_out = 0.5 * total_cap_F * v_in_max * v_in_max;
    return r_limit;
}

int pdn_anti_resonance_find(const double* z_impedance, const double* frequencies,
                            int num_points, double* peaks_freq, int max_peaks)
{
    if (!z_impedance || !frequencies || !peaks_freq || num_points < 3 || max_peaks <= 0) return 0;
    int found = 0;
    for (int i = 1; i < num_points - 1 && found < max_peaks; i++) {
        if (z_impedance[i] > z_impedance[i-1] && z_impedance[i] > z_impedance[i+1]) {
            peaks_freq[found++] = frequencies[i];
        }
    }
    return found;
}

void power_loss_allocation(power_tree_node_t* root, double total_input_power)
{
    if (!root || total_input_power <= 0.0) return;
    double total_current = root->current_draw;
    if (total_current <= 0.0) return;
    root->power_loss = total_input_power;
    for (int i = 0; i < root->num_children; i++) {
        power_tree_node_t* child = root->children[i];
        if (!child) continue;
        double child_share = total_input_power * (child->current_draw / total_current);
        power_loss_allocation(child, child_share);
    }
}

void buck_small_signal_model(const dcdc_converter_t* dcdc, double load_resistance_ohm,
                             double esr_ohm, buck_small_signal_t* model_out)
{
    if (!dcdc || !model_out || load_resistance_ohm <= 0.0) return;
    double L = dcdc->inductor_uH * 1e-6;
    double C = dcdc->cap_output_uF * 1e-6;
    double vin = dcdc->v_in_min;
    double duty = buck_duty_cycle(dcdc);
    if (duty <= 0.0) duty = 0.5;
    model_out->g_vd_dc = vin / duty;
    double w0 = 1.0 / sqrt(L * C);
    model_out->f_pole_Hz = w0 / (2.0 * M_PI);
    if (esr_ohm > 0.0) model_out->f_esr_zero_Hz = 1.0 / (2.0 * M_PI * esr_ohm * C);
    else model_out->f_esr_zero_Hz = 1e9;
    model_out->q_factor = load_resistance_ohm * sqrt(C / L);
    model_out->f_rhpz_Hz = 0.0;
}

/* ===================================================================
 * L6: Canonical Problems Implementation
 * =================================================================== */

double mcu_power_budget(double i_3v3, double i_1v8, double i_1v2,
                        double* eff_3v3_out, double* eff_1v8_out, double* eff_1v2_out)
{
    double p_1v2 = 1.2 * i_1v2;
    double eff_1v2 = 0.88;
    double p_in_1v2 = p_1v2 / eff_1v2;
    double p_1v8 = 1.8 * i_1v8;
    double eff_1v8 = 0.75;
    double p_in_1v8 = p_1v8 / eff_1v8;
    double p_3v3 = 3.3 * i_3v3 + p_in_1v8 + p_in_1v2;
    double eff_3v3 = 0.70;
    double p_in_total = p_3v3 / eff_3v3;
    if (eff_3v3_out) *eff_3v3_out = eff_3v3;
    if (eff_1v8_out) *eff_1v8_out = eff_1v8;
    if (eff_1v2_out) *eff_1v2_out = eff_1v2;
    return p_in_total;
}

int stm32_power_rail_check(int vdd_mV, int vdda_mV, int vbat_mV, int vref_mV)
{
    int faults = 0;
    if (vdd_mV < 1800 || vdd_mV > 3600) faults |= (1 << 0);
    if (vdda_mV < 1800 || vdda_mV > 3600) faults |= (1 << 1);
    if (vbat_mV < 1650 || vbat_mV > 3600) faults |= (1 << 2);
    if (vref_mV < 1800 || vref_mV > vdda_mV) faults |= (1 << 3);
    return faults;
}

int nrf52_power_config(double v_supply, int enable_dcdc, double* current_est_uA_out)
{
    if (v_supply < 1.7 || v_supply > 5.5) return -1;
    double base_current_uA = 5000.0;
    if (enable_dcdc) {
        base_current_uA *= 0.7;
        if (v_supply < 1.8 || v_supply > 3.6) return -1;
    }
    if (current_est_uA_out) *current_est_uA_out = base_current_uA;
    return 0;
}

/* ===================================================================
 * L7: Applications Implementation
 * =================================================================== */

int arduino_nano_power_model(double vin, int usb_5v, double i_5v_load,
                             double i_3v3_load, double* total_power_w)
{
    double p_5v, i_vin_ldo;
    double v_usb = 5.0;
    double eff_ams1117 = 0.65;
    double eff_lp2985 = 0.70;
    if (vin > 0.0 && usb_5v) {
        double v_usb_eff = (vin > v_usb) ? vin : v_usb;
        p_5v = v_usb_eff * i_5v_load;
    } else if (vin > 7.0) {
        double i_vin_total = i_5v_load + i_3v3_load * (3.3 / 5.0);
        i_vin_ldo = i_vin_total;
        p_5v = vin * i_vin_ldo / eff_ams1117;
        double vdrop = vin - 5.0;
        double p_ams1117_loss = vdrop * i_vin_total + vin * 0.005;
        if (p_ams1117_loss > 1.5) return -1;
    } else if (usb_5v) {
        p_5v = 5.0 * (i_5v_load + i_3v3_load * 3.3 / 5.0 / eff_lp2985);
    } else {
        return -1;
    }
    double p_3v3 = 3.3 * i_3v3_load;
    double p_lp2985_loss = (5.0 - 3.3) * i_3v3_load + 5.0 * 0.0001;
    p_5v += p_3v3 / eff_lp2985;
    if (i_3v3_load > 0.15) {
        if (total_power_w) *total_power_w = p_5v;
        return -1;
    }
    if (total_power_w) *total_power_w = p_5v;
    return 0;
}

double esp32_battery_life(double capacity_mAh, double active_current_mA,
                          double active_duty, double sleep_current_uA, double sleep_duty)
{
    if (capacity_mAh <= 0.0) return 0.0;
    double i_avg_mA = active_current_mA * active_duty + (sleep_current_uA / 1000.0) * sleep_duty;
    if (i_avg_mA <= 0.0) return 1e9;
    return capacity_mAh / i_avg_mA;
}

double ev_aux_power_efficiency(double hv_voltage, double load_12v_w,
                               double dcdc_efficiency, double pol_efficiency)
{
    if (dcdc_efficiency <= 0.0 || pol_efficiency <= 0.0) return 0.0;
    (void)hv_voltage; (void)load_12v_w;
    return dcdc_efficiency * pol_efficiency;
}

/* ===================================================================
 * L8: Advanced Topics Implementation
 * =================================================================== */

void gan_vs_si_fom(double rds_on_mohm, double qg_nc, double qgd_nc,
                   int is_gan, double* fom_out, double* hs_fom_out)
{
    double fom = rds_on_mohm * qg_nc;
    double hs_fom = rds_on_mohm * qgd_nc;
    if (is_gan) {
        fom *= 0.3;
        hs_fom *= 0.2;
    }
    if (fom_out) *fom_out = fom;
    if (hs_fom_out) *hs_fom_out = hs_fom;
}

int energy_harvesting_feasibility(double v_source_mV, double r_source_ohm,
                                  double v_startup_min_mV, double* p_available_uW)
{
    if (v_source_mV < v_startup_min_mV) {
        if (p_available_uW) *p_available_uW = 0.0;
        return -1;
    }
    double v_src = v_source_mV * 1e-3;
    double p_max = (v_src * v_src) / (4.0 * r_source_ohm);
    double p_uW = p_max * 1e6;
    if (p_available_uW) *p_available_uW = p_uW;
    if (p_uW < 10.0) return -2;
    return 0;
}

double buck_loss_decomposition(const dcdc_converter_t* dcdc, double duty,
                               double rds_on_ohm, double qg_total_nC, double core_loss_factor)
{
    if (!dcdc) return 0.0;
    double i_rms = dcdc->i_out_max * sqrt(duty);
    double p_cond = i_rms * i_rms * rds_on_ohm;
    double p_sw = 0.5 * dcdc->v_in_min * dcdc->i_out_max * 20e-9 * dcdc->switching_freq;
    double p_gate = qg_total_nC * 1e-9 * 5.0 * dcdc->switching_freq;
    double p_core = core_loss_factor * pow(dcdc->switching_freq, 1.5);
    return p_cond + p_sw + p_gate + p_core;
}

int pmbus_vout_command(double v_out, double v_ref, int resolution_bits)
{
    if (resolution_bits <= 0 || resolution_bits > 16) return -1;
    if (v_ref <= 0.0) return -1;
    int max_val = (1 << resolution_bits) - 1;
    int cmd = (int)(v_out * max_val / v_ref + 0.5);
    if (cmd < 0) cmd = 0;
    if (cmd > max_val) cmd = max_val;
    return cmd;
}

void power_thermal_analysis(const ldo_regulator_t* ldo, double t_ambient_C,
                            double i_load_A, double heatsink_rth, thermal_result_t* result_out)
{
    if (!ldo || !result_out) return;
    double vdrop = ldo->v_in - ldo->v_out;
    if (vdrop < 0.0) vdrop = 0.0;
    double p_diss = vdrop * i_load_A + ldo->v_in * ldo->i_q;
    result_out->power_dissipation_W = p_diss;
    double total_rth = ldo->thermal_resistance_ja + heatsink_rth;
    result_out->junction_temp_C = t_ambient_C + p_diss * total_rth;
    result_out->margin_to_max_C = ldo->max_junction_temp - result_out->junction_temp_C;
    double max_allowed_rth = (ldo->max_junction_temp - t_ambient_C) / p_diss - ldo->thermal_resistance_ja;
    result_out->required_heatsink_Rth_C_per_W = (max_allowed_rth > 0.0) ? max_allowed_rth : 0.0;
    result_out->thermal_ok = (result_out->junction_temp_C <= ldo->max_junction_temp) ? 1 : 0;
}
