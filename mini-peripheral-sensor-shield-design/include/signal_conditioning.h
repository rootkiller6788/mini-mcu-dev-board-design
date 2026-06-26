/**
 * @file    signal_conditioning.h
 * @brief   L2-L5 Signal Conditioning - Amplifiers, Filters, Bridges, Level Shifting
 *
 * @details Complete analog front-end (AFE) for sensor signal conditioning:
 *          instrumentation amplifiers, Wheatstone bridge analysis, active filters
 *          (Sallen-Key, MFB, Twin-T), 4-20mA current loop, level shifting,
 *          ESD protection, anti-aliasing filters.
 *
 * Knowledge Mapping:
 *   L1 - Gain, CMRR, PSRR, filter order, cutoff frequency, Q factor
 *   L2 - Differential vs single-ended, common-mode rejection, active filtering
 *   L3 - Transfer functions H(s), Bode plots, pole-zero analysis, bilinear transform
 *   L4 - Ohm's Law (dividers), KCL/KVL (bridges), Thevenin/Norton equivalents
 *   L5 - Filter coefficient calculation (bilinear transform), bridge linearization
 *
 * Reference: Sedra & Smith "Microelectronic Circuits" (2020)
 *            Horowitz & Hill "The Art of Electronics" (3rd ed., 2015)
 *            Kester "Analog-Digital Conversion" (ADI, 2004)
 */

#ifndef SIGNAL_CONDITIONING_H
#define SIGNAL_CONDITIONING_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <math.h>
#include "sensor_types.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* ---- Amplifier Configurations (L1-L2) ---- */
typedef enum {
    AMP_CONFIG_NON_INVERTING = 0, AMP_CONFIG_INVERTING, AMP_CONFIG_DIFFERENTIAL,
    AMP_CONFIG_INSTRUMENTATION_3OPAMP, AMP_CONFIG_INSTRUMENTATION_2OPAMP,
    AMP_CONFIG_TRANSIMPEDANCE, AMP_CONFIG_CHARGE_AMPLIFIER,
    AMP_CONFIG_BUFFER_VOLTAGE_FOLLOWER, AMP_CONFIG_SUMMING_AMPLIFIER
} amplifier_config_t;

typedef struct {
    amplifier_config_t config;
    double R1, R2, R3, R4, Rf, Rg;
    double gain_nominal; double gain_tolerance_percent;
    double bandwidth_hz; double slew_rate_v_per_us;
    double input_offset_voltage_uv; double input_bias_current_na;
    double input_impedance_mohm; double cmrr_db; double psrr_db;
    double noise_density_nv_per_sqrt_hz;
    double supply_voltage_positive; double supply_voltage_negative;
    bool is_rail_to_rail_input; bool is_rail_to_rail_output;
} amplifier_t;

void amplifier_init(amplifier_t *amp, amplifier_config_t config);
double amplifier_gain_calc(const amplifier_t *amp);
double amplifier_bandwidth_calc(const amplifier_t *amp, double gbp_mhz);
double amplifier_output_noise_rms(const amplifier_t *amp, double bw_hz);
int amplifier_check_stability(const amplifier_t *amp, double cload_pf);

/* ---- Instrumentation Amplifier (3-op-amp) (L2-L4) ----
 * Stage1: G1 = 1 + 2*R1/Rg
 * Stage2: G2 = R5/R4
 * Total:  G  = (1 + 2*R1/Rg) * (R5/R4)
 * CMRR depends on resistor matching in stage 2 */
typedef struct {
    double R_stage1; double R_gain;
    double R_stage2_input; double R_stage2_feedback;
    double gain_stage1; double gain_stage2; double gain_total;
    double cmrr_limited_db; double bandwidth_khz;
    double noise_referred_to_input_uv;
} instrumentation_amp_t;

void instrumentation_amp_design(instrumentation_amp_t *ina, double target_gain, double bw_khz);
double instrumentation_amp_vout(const instrumentation_amp_t *ina, double Vin_p, double Vin_n);
double instrumentation_amp_cmrr_from_mismatch(double resistor_tolerance_percent);
double instrumentation_amp_rg_for_gain(const instrumentation_amp_t *ina, double desired_gain);

/* ---- Wheatstone Bridge Analysis (L2-L4) ----
 * Vout = Vex * (R1/(R1+R2) - R3/(R3+R4))
 * Quarter bridge: Vout ~= Vex * dR / (4*R0)  = Vex * GF * eps / 4
 * Half bridge:    Vout ~= Vex * dR / (2*R0)
 * Full bridge:    Vout = Vex * dR / R0 = Vex * GF * eps */
typedef struct {
    double R1, R2, R3, R4; double V_excitation;
    double V_output; double V_output_common_mode; double delta_R;
    int active_arm_count; bool is_balanced;
    double sensitivity_mv_per_v_per_unit; double nonlinearity_percent;
} wheatstone_bridge_t;

void wheatstone_bridge_init(wheatstone_bridge_t *wb, double R_nominal, int active_arms);
double wheatstone_bridge_vout(const wheatstone_bridge_t *wb);
double wheatstone_bridge_vout_linearized(const wheatstone_bridge_t *wb);
double wheatstone_bridge_delta_r_from_vout(const wheatstone_bridge_t *wb, double Vout_m);
double wheatstone_bridge_common_mode(const wheatstone_bridge_t *wb);
int wheatstone_bridge_balance_check(const wheatstone_bridge_t *wb, double tol_percent);

/* ---- Active Filter Design (L2-L3-L5) ----
 * Sallen-Key LP: H(s) = K / (s^2 + s/(R1*C1) + 1/(R1*R2*C1*C2))
 * MFB LP: H(s) = -(R2/R1) / (1 + s*C1*(R2+R3+R2*R3/R1) + s^2*C1*C2*R2*R3)
 * Twin-T Notch: f_notch = 1/(2*pi*R*C), R1=R2=2*R3, C1=C2=C3/2
 * Bilinear Transform (L5): s = (2/T)*(1-z^{-1})/(1+z^{-1}) */
typedef enum {
    FILTER_LOWPASS = 0, FILTER_HIGHPASS, FILTER_BANDPASS, FILTER_BANDSTOP,
    FILTER_NOTCH_50HZ, FILTER_NOTCH_60HZ, FILTER_ANTI_ALIASING
} filter_type_t;

typedef enum {
    FILTER_TOPOLOGY_SALLEN_KEY = 0, FILTER_TOPOLOGY_MFB,
    FILTER_TOPOLOGY_TWIN_T_NOTCH, FILTER_TOPOLOGY_FLIEGE_NOTCH, FILTER_TOPOLOGY_BIQUAD
} filter_topology_t;

typedef enum {
    FILTER_PROTOTYPE_BUTTERWORTH = 0, FILTER_PROTOTYPE_CHEBYSHEV,
    FILTER_PROTOTYPE_BESSEL, FILTER_PROTOTYPE_ELLIPTIC
} filter_prototype_t;

typedef struct {
    filter_type_t type; filter_topology_t topology; filter_prototype_t prototype;
    uint8_t order; double fc_hz; double fc_low_hz; double fc_high_hz;
    double gain_passband; double q_factor; double ripple_db;
    double R1, R2, R3, R4; double C1, C2, C3;
    double b0, b1, b2; double a1, a2;
} active_filter_t;

void active_filter_init(active_filter_t *f, filter_type_t type, uint8_t order, double fc, double gain);
int active_filter_design_sallen_key_lp(active_filter_t *f, double fc_hz, double gain);
int active_filter_design_sallen_key_hp(active_filter_t *f, double fc_hz, double gain);
int active_filter_design_mfb_lp(active_filter_t *f, double fc_hz, double gain, double q);
int active_filter_design_notch_twin_t(active_filter_t *f, double f_notch_hz, double q);
double active_filter_transfer_magnitude(const active_filter_t *f, double freq_hz);
double active_filter_transfer_phase(const active_filter_t *f, double freq_hz);
double active_filter_group_delay(const active_filter_t *f, double freq_hz);
int active_filter_to_digital(active_filter_t *f, double fs_hz);

/* ---- Voltage Divider Design (L2) ----
 * Vout = Vin * R2/(R1+R2)
 * For thermistor: optimal R_fixed = R_therm(T0) at temperature of interest */
typedef struct {
    double R_top; double R_bottom; double Vin; double Vout;
    double sensitivity_v_per_ohm; double power_dissipation_mw; double optimal_R_for_sensor;
} voltage_divider_t;

void voltage_divider_init(voltage_divider_t *vd, double R_top, double R_bottom, double Vin);
double voltage_divider_vout(const voltage_divider_t *vd);
double voltage_divider_r_from_vout(const voltage_divider_t *vd, double Vout, bool is_r_top);
double voltage_divider_optimal_r_fixed(double R_sensor, double V_supply, double max_power_mw);

/* ---- 4-20mA Current Loop (L2) ----
 * 4mA = live zero (fault detection: 0mA = broken wire)
 * Receiver: V = I_loop * R_sense (typically 250 ohm for 1-5V)
 * HART overlay: FSK +/-0.5mA at 1200/2200 Hz */
typedef struct {
    double loop_current_min_ma; double loop_current_max_ma;
    double sense_resistance; double V_sense_min; double V_sense_max;
    double loop_voltage_min; double loop_voltage_max; double compliance_voltage;
    double accuracy_percent; bool is_loop_powered; bool supports_hart;
} current_loop_t;

void current_loop_init(current_loop_t *cl, double R_sense, double I_min, double I_max);
double current_loop_ma_from_voltage(const current_loop_t *cl, double V_sense);
double current_loop_percent_range(const current_loop_t *cl, double I_ma);
double current_loop_process_value(const current_loop_t *cl, double I_ma, double pmin, double pmax);
bool current_loop_fault_detect(const current_loop_t *cl, double I_ma);

/* ---- Level Shifting (L2) ----
 * 3.3V <-> 5V interfaces: resistive divider, MOSFET bidirectional, op-amp */
typedef enum {
    LEVEL_SHIFT_RESISTIVE_DIVIDER = 0, LEVEL_SHIFT_MOSFET_BIDIR,
    LEVEL_SHIFT_OPAMP_GAIN_OFFSET, LEVEL_SHIFT_DIODE_CLAMP, LEVEL_SHIFT_ZENER_CLAMP
} level_shift_type_t;

typedef struct {
    level_shift_type_t type; double V_in_low; double V_in_high;
    double V_out_low; double V_out_high; double R_top; double R_bottom;
    double V_ref; double max_frequency_hz; bool is_bidirectional;
} level_shifter_t;

void level_shifter_init(level_shifter_t *ls, level_shift_type_t type, double Vin_h, double Vout_h);
int level_shifter_design_divider(level_shifter_t *ls);
double level_shifter_translate(const level_shifter_t *ls, double Vin);
double level_shifter_power_consumption(const level_shifter_t *ls);

/* ---- ESD Protection & Input Conditioning (L2) ----
 * TVS: response <1ns; V_RWM < V_operating < V_BR < V_C
 * RC anti-aliasing: fc = 1/(2*pi*R*C), fc < fs/10 for practical ADC */
typedef struct {
    double tvs_standoff_voltage; double tvs_breakdown_voltage;
    double tvs_clamping_voltage; double tvs_peak_pulse_current;
    double tvs_capacitance_pf; double esd_rating_kv;
    bool has_rc_filter; double R_series; double C_to_gnd; double filter_cutoff_hz;
    double pull_up_resistance; double pull_down_resistance;
} input_protection_t;

void input_protection_init(input_protection_t *ip, double V_operating);
double input_protection_cutoff_freq(const input_protection_t *ip);
double input_protection_settling_time(const input_protection_t *ip, double adc_bits);
double input_protection_impedance_at_freq(const input_protection_t *ip, double freq_hz);
int input_protection_check_esd_margin(const input_protection_t *ip, double V_max_expected);

/* ---- Analog Front-End Complete Configuration (L2-L3) ----
 * Sensor -> Protection -> Amplifier -> Filter -> ADC Buffer -> MCU */
#define AFE_MAX_STAGES 6
typedef struct {
    char channel_name[32]; uint8_t sensor_index;
    input_protection_t protection; amplifier_t amplifier; active_filter_t filter;
    voltage_divider_t divider; level_shifter_t level_shift; current_loop_t current_loop;
    double V_out_expected_min; double V_out_expected_max; double adc_vref;
    uint8_t adc_resolution_bits; double adc_lsb_voltage;
    bool uses_external_vref; bool has_auto_zero;
} analog_frontend_t;

void analog_frontend_init(analog_frontend_t *afe, const char *name);
double analog_frontend_process(analog_frontend_t *afe, double V_sensor);
uint32_t analog_frontend_to_adc_counts(const analog_frontend_t *afe, double V_cond);
double analog_frontend_from_adc_counts(const analog_frontend_t *afe, uint32_t counts);
double analog_frontend_total_gain(const analog_frontend_t *afe);
double analog_frontend_total_noise_rti(const analog_frontend_t *afe);
int analog_frontend_validate(const analog_frontend_t *afe);

#endif /* SIGNAL_CONDITIONING_H */
