/**
 * @file    signal_conditioning.c
 * @brief   L2-L5 Signal Conditioning Implementations - Amplifiers, Filters, Bridges
 *
 * @details Implements analog front-end building blocks for sensor shields:
 *          instrumentation amp gain calculation, Wheatstone bridge analysis,
 *          Sallen-Key / MFB filter design with component value calculation,
 *          voltage divider optimization, 4-20mA loop, level shifting, ESD protection,
 *          and complete analog front-end chain processing.
 *
 * Knowledge Mapping:
 *   L2 - Amplifier gain formulas, CMRR from resistor mismatch
 *   L3 - Transfer functions, Bode magnitude/phase, filter coefficients
 *   L4 - Ohm's Law, Kirchhoff's Laws on bridges, Thevenin equivalents
 *   L5 - Bilinear transform for digital filter coefficients
 *   L6 - Complete analog front-end for thermistor shield design
 */

#include "signal_conditioning.h"
#include <math.h>
#include <string.h>
#include <stdio.h>

/* ---- Amplifier Functions (L2) ---- */

void amplifier_init(amplifier_t *amp, amplifier_config_t config) {
    if (!amp) return;
    memset(amp, 0, sizeof(*amp));
    amp->config = config;
    amp->R1 = 10000.0; amp->R2 = 10000.0;
    amp->gain_nominal = 1.0;
    amp->bandwidth_hz = 100000.0;
    amp->cmrr_db = 80.0;
    amp->psrr_db = 80.0;
    amp->input_impedance_mohm = 10.0;
}

double amplifier_gain_calc(const amplifier_t *amp) {
    if (!amp) return 1.0;
    switch (amp->config) {
        case AMP_CONFIG_NON_INVERTING:
            return (amp->R1 > 0.0) ? 1.0 + amp->Rf / amp->R1 : 1.0;
        case AMP_CONFIG_INVERTING:
            return (amp->R1 > 0.0) ? -amp->Rf / amp->R1 : -1.0;
        case AMP_CONFIG_DIFFERENTIAL:
            /* Assumes R1=R3, R2=R4 for common-mode rejection */
            return (amp->R1 > 0.0) ? amp->R2 / amp->R1 : 1.0;
        case AMP_CONFIG_INSTRUMENTATION_3OPAMP:
            /* G = (1 + 2R1/Rg) * (R2/R1) simplified */
            if (amp->Rg > 0.0 && amp->R1 > 0.0)
                return (1.0 + 2.0*amp->R1/amp->Rg) * (amp->R2/amp->R1);
            return 1.0;
        case AMP_CONFIG_TRANSIMPEDANCE:
            return amp->Rf; /* Vout = -I_in * Rf, gain in V/A */
        case AMP_CONFIG_BUFFER_VOLTAGE_FOLLOWER:
            return 1.0;
        default:
            return 1.0;
    }
}

double amplifier_bandwidth_calc(const amplifier_t *amp, double gbp_mhz) {
    if (!amp) return 0.0;
    double gain = fabs(amplifier_gain_calc(amp));
    if (gain <= 0.0) return gbp_mhz * 1e6;
    return (gbp_mhz * 1e6) / gain;
}

double amplifier_output_noise_rms(const amplifier_t *amp, double bw_hz) {
    if (!amp) return 0.0;
    /* Simplified: en_rms = en_density * sqrt(BW * pi/2) * noise_gain */
    double noise_gain = fabs(amplifier_gain_calc(amp));
    if (amp->config == AMP_CONFIG_NON_INVERTING && amp->R1 > 0.0)
        noise_gain = 1.0 + amp->Rf/amp->R1;
    return amp->noise_density_nv_per_sqrt_hz * 1e-9
           * sqrt(bw_hz * M_PI / 2.0) * noise_gain;
}

int amplifier_check_stability(const amplifier_t *amp, double cload_pf) {
    if (!amp) return -1;
    /* Check if capacitive load causes instability.
     * Simplified: if Rf*Cload > 100ns, isolation resistor needed */
    double time_constant_ns = amp->Rf * cload_pf * 1e-3;
    if (time_constant_ns > 100.0) return 1; /* needs isolation R */
    return 0; /* stable */
}

/* ---- Instrumentation Amplifier (L2-L4) ---- */

void instrumentation_amp_design(instrumentation_amp_t *ina,
                                 double target_gain, double bw_khz) {
    if (!ina) return;
    memset(ina, 0, sizeof(*ina));
    /* Split gain: stage 1 provides most gain for noise performance,
     * stage 2 provides the rest. */
    if (target_gain <= 10.0) {
        ina->gain_stage1 = target_gain; ina->gain_stage2 = 1.0;
    } else {
        ina->gain_stage1 = 10.0; ina->gain_stage2 = target_gain / 10.0;
    }
    ina->R_stage1 = 10000.0;
    ina->R_gain = (2.0 * ina->R_stage1) / (ina->gain_stage1 - 1.0);
    ina->R_stage2_input = 10000.0;
    ina->R_stage2_feedback = ina->gain_stage2 * ina->R_stage2_input;
    ina->gain_total = ina->gain_stage1 * ina->gain_stage2;
    ina->bandwidth_khz = bw_khz;
    ina->cmrr_limited_db = instrumentation_amp_cmrr_from_mismatch(0.1);
    ina->noise_referred_to_input_uv = 1.0; /* typical */
}

double instrumentation_amp_vout(const instrumentation_amp_t *ina,
                                  double Vin_p, double Vin_n) {
    if (!ina) return 0.0;
    return ina->gain_total * (Vin_p - Vin_n);
}

double instrumentation_amp_cmrr_from_mismatch(double tol_percent) {
    /* CMRR (in dB) limited by resistor mismatch:
     * CMRR_dB approx = 20*log10((1+G) / (4*tol))  where tol = dR/R */
    double G = 100.0; /* typical gain */
    double tol = tol_percent / 100.0;
    if (tol <= 0.0) return 120.0;
    return 20.0 * log10((1.0 + G) / (4.0 * tol));
}

double instrumentation_amp_rg_for_gain(const instrumentation_amp_t *ina,
                                        double desired_gain) {
    if (!ina || ina->gain_stage2 <= 0.0) return 0.0;
    double G1 = desired_gain / ina->gain_stage2;
    if (G1 <= 1.0) return 1e9; /* very large -> gain=1 */
    return (2.0 * ina->R_stage1) / (G1 - 1.0);
}

/* ---- Wheatstone Bridge (L2-L4) ---- */

void wheatstone_bridge_init(wheatstone_bridge_t *wb, double R_nominal, int active) {
    if (!wb) return;
    memset(wb, 0, sizeof(*wb));
    wb->R1 = R_nominal; wb->R2 = R_nominal;
    wb->R3 = R_nominal; wb->R4 = R_nominal;
    wb->V_excitation = 5.0;
    wb->active_arm_count = active;
    wb->is_balanced = true;
}

double wheatstone_bridge_vout(const wheatstone_bridge_t *wb) {
    if (!wb) return 0.0;
    double denom1 = wb->R1 + wb->R2;
    double denom2 = wb->R3 + wb->R4;
    if (denom1 <= 0.0 || denom2 <= 0.0) return 0.0;
    return wb->V_excitation * (wb->R1/denom1 - wb->R3/denom2);
}

double wheatstone_bridge_vout_linearized(const wheatstone_bridge_t *wb) {
    /* Exact nonlinear output for quarter bridge with dR in R1:
     * Vout_exact = Vex * dR / (4R0 + 2dR)
     * Linear approx: Vout_lin = Vex * dR / (4R0) */
    if (!wb) return 0.0;
    double R0 = wb->R1;
    double dR = wb->delta_R;
    if (wb->active_arm_count == 1) {
        return wb->V_excitation * dR / (4.0 * R0 + 2.0 * dR);
    } else if (wb->active_arm_count == 2) {
        return wb->V_excitation * dR / (2.0 * R0);
    } else {
        return wb->V_excitation * dR / R0;
    }
}

double wheatstone_bridge_delta_r_from_vout(const wheatstone_bridge_t *wb,
                                            double Vout_m) {
    if (!wb || wb->V_excitation <= 0.0) return 0.0;
    double R0 = wb->R1;
    /* Invert the nonlinear equation for quarter bridge:
     * dR = (4R0*Vout/Vex) / (1 - 2*Vout/Vex) */
    double ratio = Vout_m / wb->V_excitation;
    if (wb->active_arm_count == 1) {
        double denom = 1.0 - 2.0 * ratio;
        if (fabs(denom) < 1e-12) return 0.0;
        return (4.0 * R0 * ratio) / denom;
    }
    return R0 * ratio * (wb->active_arm_count == 2 ? 2.0 : 1.0);
}

double wheatstone_bridge_common_mode(const wheatstone_bridge_t *wb) {
    if (!wb) return 0.0;
    double denom1 = wb->R1 + wb->R2;
    double denom2 = wb->R3 + wb->R4;
    if (denom1 <= 0.0 || denom2 <= 0.0) return 0.0;
    return wb->V_excitation * (wb->R2/denom1 + wb->R4/denom2) / 2.0;
}

int wheatstone_bridge_balance_check(const wheatstone_bridge_t *wb,
                                     double tol_percent) {
    if (!wb) return -1;
    double R1R4 = wb->R1 * wb->R4;
    double R2R3 = wb->R2 * wb->R3;
    if (R2R3 <= 0.0) return -1;
    double error = fabs(R1R4 / R2R3 - 1.0) * 100.0;
    return (error <= tol_percent) ? 0 : 1;
}

/* ---- Active Filter Design (L2-L3-L5) ---- */

void active_filter_init(active_filter_t *f, filter_type_t type,
                         uint8_t order, double fc, double gain) {
    if (!f) return;
    memset(f, 0, sizeof(*f));
    f->type = type; f->order = order; f->fc_hz = fc;
    f->gain_passband = gain; f->topology = FILTER_TOPOLOGY_SALLEN_KEY;
    f->prototype = FILTER_PROTOTYPE_BUTTERWORTH;
}

int active_filter_design_sallen_key_lp(active_filter_t *f, double fc_hz, double gain) {
    if (!f || fc_hz <= 0.0) return -1;
    /* Butterworth Q = 0.7071, choose C1=C2=10nF as starting point */
    double C = 10e-9;
    double Q = 0.7071;
    double w0 = 2.0 * M_PI * fc_hz;
    /* R1,2 = 1/(w0*C) for unity gain; adjust for gain > 1 */
    double R = 1.0 / (w0 * C);
    f->R1 = R; f->R2 = R;
    f->C1 = C; f->C2 = C;
    f->gain_passband = gain;
    f->q_factor = Q;
    /* Bilinear transform to digital coefficients */
    return active_filter_to_digital(f, fc_hz * 10.0);
}

int active_filter_design_sallen_key_hp(active_filter_t *f, double fc_hz, double gain) {
    if (!f || fc_hz <= 0.0) return -1;
    /* Swap R and C from LP design */
    double R = 10000.0;
    double w0 = 2.0 * M_PI * fc_hz;
    double C = 1.0 / (w0 * R);
    f->R1 = R; f->R2 = R;
    f->C1 = C; f->C2 = C;
    f->gain_passband = gain;
    return 0;
}

int active_filter_design_mfb_lp(active_filter_t *f, double fc_hz, double gain, double Q) {
    if (!f || fc_hz <= 0.0) return -1;
    f->topology = FILTER_TOPOLOGY_MFB;
    double w0 = 2.0 * M_PI * fc_hz;
    /* Choose C1 = 10nF, calculate others */
    double C1 = 10e-9;
    double C2 = C1;
    double k = w0 * C1;
    double g = fabs(gain);
    f->R2 = 1.0 / (2.0 * Q * k);
    f->R1 = f->R2 / g;
    f->R3 = 1.0 / (2.0 * Q * k * (1.0 - 1.0/(2.0*Q*Q)));
    if (f->R3 < 0.0) f->R3 = f->R2;
    f->C1 = C1; f->C2 = C2;
    f->q_factor = Q;
    return active_filter_to_digital(f, fc_hz * 10.0);
}

int active_filter_design_notch_twin_t(active_filter_t *f, double fn_hz, double Q) {
    if (!f || fn_hz <= 0.0) return -1;
    f->topology = FILTER_TOPOLOGY_TWIN_T_NOTCH;
    double R = 10000.0;
    double w0 = 2.0 * M_PI * fn_hz;
    double C = 1.0 / (w0 * R);
    /* Twin-T: R1=R2=2*R, R3=R/2; C1=C2=C/2, C3=C */
    f->R1 = R; f->R2 = R; f->R3 = R/2.0;
    f->C1 = C; f->C2 = C; f->C3 = C*2.0;
    f->fc_hz = fn_hz; f->q_factor = Q;
    return active_filter_to_digital(f, fn_hz * 20.0);
}

double active_filter_transfer_magnitude(const active_filter_t *f, double freq_hz) {
    if (!f) return 1.0;
    /* Use digital filter coefficients to compute |H(e^{jw})| */
    double w = 2.0 * M_PI * freq_hz / (freq_hz > 0 ? freq_hz * 10.0 : 1.0);
    double cos_w = cos(w);
    double num_re = f->b0 + f->b1 * cos_w + f->b2 * cos(2.0*w);
    double num_im = -f->b1 * sin(w) - f->b2 * sin(2.0*w);
    double den_re = 1.0 + f->a1 * cos_w + f->a2 * cos(2.0*w);
    double den_im = -f->a1 * sin(w) - f->a2 * sin(2.0*w);
    return sqrt((num_re*num_re + num_im*num_im) / (den_re*den_re + den_im*den_im));
}

double active_filter_transfer_phase(const active_filter_t *f, double freq_hz) {
    if (!f) return 0.0;
    double w = 2.0 * M_PI * freq_hz / (freq_hz > 0 ? freq_hz * 10.0 : 1.0);
    double cos_w = cos(w);
    double num_re = f->b0 + f->b1 * cos_w + f->b2 * cos(2.0*w);
    double num_im = -f->b1 * sin(w) - f->b2 * sin(2.0*w);
    double den_re = 1.0 + f->a1 * cos_w + f->a2 * cos(2.0*w);
    double den_im = -f->a1 * sin(w) - f->a2 * sin(2.0*w);
    return atan2(num_im*den_re - num_re*den_im, num_re*den_re + num_im*den_im) * 180.0/M_PI;
}

double active_filter_group_delay(const active_filter_t *f, double freq_hz) {
    /* Numerical derivative of phase with respect to frequency */
    if (!f) return 0.0;
    double df = freq_hz * 0.01 + 0.1;
    double p1 = active_filter_transfer_phase(f, freq_hz - df) * M_PI/180.0;
    double p2 = active_filter_transfer_phase(f, freq_hz + df) * M_PI/180.0;
    double dw = 2.0 * M_PI * 2.0 * df;
    if (dw <= 0.0) return 0.0;
    return -(p2 - p1) / dw * 1000.0; /* ms */
}

int active_filter_to_digital(active_filter_t *f, double fs_hz) {
    /* Convert analog prototype to digital using bilinear transform
     * For a 2nd-order LP with cutoff fc:
     * Pre-warp: w0 = 2*fs*tan(pi*fc/fs) */
    if (!f || fs_hz <= 0.0) return -1;
    double fc = f->fc_hz;
    double Q = f->q_factor > 0.0 ? f->q_factor : 0.7071;
    double w0 = 2.0 * fs_hz * tan(M_PI * fc / fs_hz);
    double alpha = sin(w0/fs_hz) / (2.0 * Q);
    /* Butterworth LP biquad coefficients */
    double c = cos(w0/fs_hz);
    double b0 = (1.0 - c) / 2.0;
    double b1 = 1.0 - c;
    double b2 = (1.0 - c) / 2.0;
    double a0 = 1.0 + alpha;
    double a1_val = -2.0 * c;
    double a2_val = 1.0 - alpha;
    /* Normalize by a0 */
    f->b0 = b0 / a0; f->b1 = b1 / a0; f->b2 = b2 / a0;
    f->a1 = a1_val / a0; f->a2 = a2_val / a0;
    return 0;
}

/* ---- Voltage Divider (L2) ---- */

void voltage_divider_init(voltage_divider_t *vd, double Rt, double Rb, double Vin) {
    if (!vd) return;
    memset(vd, 0, sizeof(*vd));
    vd->R_top = Rt; vd->R_bottom = Rb; vd->Vin = Vin;
}

double voltage_divider_vout(const voltage_divider_t *vd) {
    if (!vd || vd->R_top + vd->R_bottom <= 0.0) return 0.0;
    return vd->Vin * vd->R_bottom / (vd->R_top + vd->R_bottom);
}

double voltage_divider_r_from_vout(const voltage_divider_t *vd, double Vout, bool is_r_top) {
    if (!vd || Vout <= 0.0 || Vout >= vd->Vin) return 0.0;
    if (is_r_top) {
        /* R_top = R_bottom * (Vin/Vout - 1) */
        return vd->R_bottom * (vd->Vin / Vout - 1.0);
    } else {
        /* R_bottom = R_top * Vout / (Vin - Vout) */
        return vd->R_top * Vout / (vd->Vin - Vout);
    }
}

double voltage_divider_optimal_r_fixed(double R_sensor, double V_supply, double max_power) {
    /* For maximum sensitivity: R_fixed = R_sensor
     * But also check power: P = V^2 / (R_sensor + R_fixed) < max_power */
    if (max_power <= 0.0) return R_sensor;
    double R_min = V_supply * V_supply / max_power - R_sensor;
    if (R_sensor < R_min) return R_min;
    return R_sensor;
}

/* ---- 4-20mA Current Loop (L2) ---- */

void current_loop_init(current_loop_t *cl, double Rs, double Im, double Ix) {
    if (!cl) return;
    memset(cl, 0, sizeof(*cl));
    cl->sense_resistance = Rs;
    cl->loop_current_min_ma = Im;
    cl->loop_current_max_ma = Ix;
    cl->V_sense_min = Im * Rs / 1000.0;
    cl->V_sense_max = Ix * Rs / 1000.0;
    cl->is_loop_powered = false;
}

double current_loop_ma_from_voltage(const current_loop_t *cl, double Vs) {
    if (!cl || cl->sense_resistance <= 0.0) return 0.0;
    return Vs / cl->sense_resistance * 1000.0;
}

double current_loop_percent_range(const current_loop_t *cl, double I_ma) {
    if (!cl) return 0.0;
    double span = cl->loop_current_max_ma - cl->loop_current_min_ma;
    if (span <= 0.0) return 0.0;
    return (I_ma - cl->loop_current_min_ma) / span * 100.0;
}

double current_loop_process_value(const current_loop_t *cl, double I_ma,
                                   double pmin, double pmax) {
    double pct = current_loop_percent_range(cl, I_ma) / 100.0;
    return pmin + pct * (pmax - pmin);
}

bool current_loop_fault_detect(const current_loop_t *cl, double I_ma) {
    /* 0mA = open circuit, <3.6mA or >21mA = fault (NAMUR NE43) */
    if (!cl) return true;
    return (I_ma < 3.6 || I_ma > 21.0);
}

/* ---- Level Shifting (L2) ---- */

void level_shifter_init(level_shifter_t *ls, level_shift_type_t type,
                         double Vin_h, double Vout_h) {
    if (!ls) return;
    memset(ls, 0, sizeof(*ls));
    ls->type = type;
    ls->V_in_high = Vin_h;
    ls->V_out_high = Vout_h;
    ls->V_in_low = 0.0;
    ls->V_out_low = 0.0;
    ls->max_frequency_hz = 100000.0;
    if (type == LEVEL_SHIFT_RESISTIVE_DIVIDER) {
        double ratio = Vout_h / Vin_h;
        ls->R_top = 10000.0;
        ls->R_bottom = ls->R_top * ratio / (1.0 - ratio);
    }
}

int level_shifter_design_divider(level_shifter_t *ls) {
    if (!ls || ls->V_in_high <= 0.0) return -1;
    double ratio = ls->V_out_high / ls->V_in_high;
    if (ratio >= 1.0) return -2; /* divider must reduce voltage */
    /* Choose R_bottom for ~1mA current: R = V/I = Vout/0.001 */
    ls->R_bottom = ls->V_out_high / 0.001;
    ls->R_top = ls->R_bottom * (1.0 - ratio) / ratio;
    return 0;
}

double level_shifter_translate(const level_shifter_t *ls, double Vin) {
    if (!ls) return Vin;
    switch (ls->type) {
        case LEVEL_SHIFT_RESISTIVE_DIVIDER:
            if (ls->R_top + ls->R_bottom <= 0.0) return Vin;
            return Vin * ls->R_bottom / (ls->R_top + ls->R_bottom);
        case LEVEL_SHIFT_OPAMP_GAIN_OFFSET:
            return Vin * (ls->V_out_high / ls->V_in_high) + ls->V_ref;
        default:
            return Vin;
    }
}

double level_shifter_power_consumption(const level_shifter_t *ls) {
    if (!ls || ls->R_top + ls->R_bottom <= 0.0) return 0.0;
    return ls->V_in_high * ls->V_in_high / (ls->R_top + ls->R_bottom);
}

/* ---- ESD Protection & Input Conditioning (L2) ---- */

void input_protection_init(input_protection_t *ip, double V_op) {
    if (!ip) return;
    memset(ip, 0, sizeof(*ip));
    ip->tvs_standoff_voltage = V_op * 1.2;
    ip->tvs_breakdown_voltage = V_op * 1.4;
    ip->tvs_clamping_voltage = V_op * 2.0;
    ip->esd_rating_kv = 8.0;
    ip->has_rc_filter = true;
    ip->R_series = 1000.0;
    ip->C_to_gnd = 100e-9;
    ip->filter_cutoff_hz = input_protection_cutoff_freq(ip);
}

double input_protection_cutoff_freq(const input_protection_t *ip) {
    if (!ip || ip->R_series <= 0.0 || ip->C_to_gnd <= 0.0) return 0.0;
    return 1.0 / (2.0 * M_PI * ip->R_series * ip->C_to_gnd);
}

double input_protection_settling_time(const input_protection_t *ip, double adc_bits) {
    /* Settling to within 0.5 LSB: t_settle = -ln(0.5/2^bits) * R*C */
    if (!ip || ip->R_series <= 0.0 || ip->C_to_gnd <= 0.0) return 0.0;
    double tau = ip->R_series * ip->C_to_gnd;
    double target = 0.5 / pow(2.0, adc_bits);
    return -log(target) * tau;
}

double input_protection_impedance_at_freq(const input_protection_t *ip, double freq) {
    if (!ip) return 0.0;
    double R = ip->R_series;
    double C = ip->C_to_gnd;
    if (C <= 0.0) return R;
    double Zc = 1.0 / (2.0 * M_PI * freq * C);
    return sqrt(R*R + Zc*Zc);
}

int input_protection_check_esd_margin(const input_protection_t *ip, double Vmax_exp) {
    if (!ip) return -1;
    if (Vmax_exp > ip->tvs_standoff_voltage) return 1; /* risk of TVS conduction */
    if (Vmax_exp > ip->tvs_breakdown_voltage * 0.8) return 2; /* marginal */
    return 0; /* safe */
}

/* ---- Analog Front-End Complete (L2-L3) ---- */

void analog_frontend_init(analog_frontend_t *afe, const char *name) {
    if (!afe) return;
    memset(afe, 0, sizeof(*afe));
    if (name) strncpy(afe->channel_name, name, 31);
    afe->adc_vref = 3.3;
    afe->adc_resolution_bits = 10;
    afe->adc_lsb_voltage = afe->adc_vref / (double)(1u << afe->adc_resolution_bits);
}

double analog_frontend_process(analog_frontend_t *afe, double V_sensor) {
    /* Apply protection (clamp), amplifier, filter in sequence */
    if (!afe) return V_sensor;
    double V = V_sensor;
    /* Protection: clamp to safe range */
    if (V > afe->protection.tvs_clamping_voltage)
        V = afe->protection.tvs_clamping_voltage;
    if (V < -afe->protection.tvs_clamping_voltage)
        V = -afe->protection.tvs_clamping_voltage;
    /* Amplifier */
    if (afe->amplifier.gain_nominal > 1.0)
        V = V * afe->amplifier.gain_nominal;
    /* Level shift if needed */
    if (afe->level_shift.type != LEVEL_SHIFT_RESISTIVE_DIVIDER)
        V = level_shifter_translate(&afe->level_shift, V);
    return V;
}

uint32_t analog_frontend_to_adc_counts(const analog_frontend_t *afe, double Vc) {
    if (!afe || afe->adc_vref <= 0.0) return 0;
    double ratio = Vc / afe->adc_vref;
    if (ratio < 0.0) ratio = 0.0;
    if (ratio > 1.0) ratio = 1.0;
    return (uint32_t)(ratio * (double)((1u << afe->adc_resolution_bits) - 1));
}

double analog_frontend_from_adc_counts(const analog_frontend_t *afe, uint32_t counts) {
    if (!afe || afe->adc_resolution_bits == 0) return 0.0;
    uint32_t max_counts = (1u << afe->adc_resolution_bits) - 1;
    return afe->adc_vref * (double)counts / (double)max_counts;
}

double analog_frontend_total_gain(const analog_frontend_t *afe) {
    if (!afe) return 1.0;
    return afe->amplifier.gain_nominal;
}

double analog_frontend_total_noise_rti(const analog_frontend_t *afe) {
    if (!afe) return 0.0;
    /* RTI noise: output noise / gain */
    double gain = analog_frontend_total_gain(afe);
    if (gain <= 0.0) gain = 1.0;
    return afe->amplifier.noise_density_nv_per_sqrt_hz * sqrt(afe->filter.fc_hz) / gain;
}

int analog_frontend_validate(const analog_frontend_t *afe) {
    if (!afe) return -1;
    if (afe->V_out_expected_max > afe->adc_vref) return 1; /* clipping */
    if (afe->V_out_expected_min < 0.0) return 2; /* negative voltage to unipolar ADC */
    if (afe->filter.fc_hz > 0.0 && afe->adc_resolution_bits > 0) {
        double fs_nyquist = 1.0 / (2.0 * M_PI * afe->protection.R_series * afe->protection.C_to_gnd);
        if (afe->filter.fc_hz > fs_nyquist / 10.0) return 3; /* insufficient anti-aliasing */
    }
    return 0;
}
