/**
 * @file power_design.h
 * @brief MCU Development Board - Power Supply Design (L1-L8)
 *
 * Knowledge Coverage:
 *   L1: voltage regulator, dropout voltage, PSRR, load/line reg, efficiency, quiescent
 *   L2: LDO vs DC-DC topology selection, power tree architecture, PDN target impedance
 *   L3: thermal resistance network, volt-second balance, RLC decoupling models, LC resonance
 *   L4: Ohm's law, Kirchhoff's KCL/KVL in power tree, thermodynamics of regulation
 *   L5: PDN impedance synthesis, LDO stability analysis, anti-resonance detection
 *   L6: STM32 3.3V/1.8V/1.2V multi-rail, nRF52 power config, STM32 rail check
 *   L7: Arduino Nano power, ESP32 battery life, EV auxiliary power
 *   L8: GaN vs Si FET FOM, energy harvesting, PMBus digital power, loss decomposition
 *   L9: AI-driven DVS, sub-threshold voltage scaling (documented only)
 *
 * Reference: Erickson & Maksimovic (2001), Ott PDN Ch.11 (2009)
 */

#ifndef POWER_DESIGN_H
#define POWER_DESIGN_H
#include <stdint.h>
#include <stddef.h>
#include <math.h>
#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * L1 Definitions - 9 typedef struct blocks covering all core power concepts
 * ======================================================================== */

/** Voltage rail specification: defines one power rail in an MCU board.
 *  Parameters: nominal voltage, min/max tolerance, max current, ripple. */
typedef struct {
    double voltage_nominal, voltage_min, voltage_max, current_max, ripple_max;
    char   name[32];
} power_rail_spec_t;

/** LDO linear regulator: electrical specs, noise, thermal.
 *  dropout_voltage: minimum Vin-Vout for regulation (critical for battery designs).
 *  psrr_db: Power Supply Rejection Ratio (input ripple attenuation).
 *  output_noise_uvrms: Intrinsic output noise 10Hz-100kHz.
 *  thermal_resistance_ja: Junction-to-ambient for thermal analysis.
 *  i_q: quiescent current - critical for battery-powered designs. */
typedef struct {
    double v_in, v_out, dropout_voltage, i_q, psrr_db;
    double output_noise_uvrms, load_regulation, line_regulation;
    double thermal_resistance_ja, max_junction_temp;
} ldo_regulator_t;

/** DC-DC switching converter: buck/boost/buck-boost/SEPIC.
 *  switching_freq: fundamental for inductor/capacitor sizing and EMI.
 *  inductor_uH: critical for ripple current and transient response.
 *  cap_output_uF: determines output ripple and load-step response.
 *  efficiency: P_out/P_in (80-95% typical for well-designed converters). */
typedef struct {
    double v_in_min, v_in_max, v_out, i_out_max, switching_freq;
    double efficiency, inductor_uH, cap_output_uF, ripple_mVpp;
    char   topology[16];
} dcdc_converter_t;

/** Decoupling capacitor with series RLC equivalent model.
 *  esr_ohm: dominates at SRF, determines ripple current capability.
 *  esl_H: dominates above SRF, package-dependent (smaller = lower ESL).
 *  self_resonant_freq: f = 1/(2*pi*sqrt(ESL*C)), Z_min = ESR.
 *  dielectric: X7R (stable), X5R (cheaper), C0G/NP0 (best stability). */
typedef struct {
    double capacitance_F, esr_ohm, esl_H, self_resonant_freq, voltage_rating;
    char   dielectric[16], package[16];
} decoupling_cap_t;

/** PDN target impedance: FDTIM method by Smith/Bogatin.
 *  Z_target = V_rail * (ripple% / 100) / I_transient_max */
typedef struct {
    double target_impedance_ohm, frequency_start_Hz, frequency_end_Hz;
} pdn_target_t;

/** Power tree node: hierarchical power architecture.
 *  Each node = regulation stage or power rail. Root = primary source.
 *  Enables recursive efficiency, thermal, and KCL analysis. */
typedef struct power_tree_node {
    char   name[32];
    double voltage, current_draw, power_loss;
    int    num_children;
    struct power_tree_node* children[8];
} power_tree_node_t;

/** Power sequencing step: multi-rail controlled startup.
 *  Defines timing, inter-rail dependencies, and voltage thresholds.
 *  Reference: TI SLVA883 Power Sequencing Techniques. */
typedef struct {
    int    rail_index;
    double turn_on_delay_ms, rise_time_ms, min_voltage_before_next;
    int    depends_on_rail;
} power_sequence_step_t;

/** Buck converter small-signal control model.
 *  g_vd_dc: DC control-to-output gain.
 *  f_pole_Hz: dominant pole from output LC filter.
 *  f_esr_zero_Hz: zero from output capacitor ESR.
 *  q_factor: LC filter quality factor.
 *  f_rhpz_Hz: right-half-plane zero (non-zero for boost/buck-boost). */
typedef struct {
    double g_vd_dc, f_pole_Hz, f_esr_zero_Hz, q_factor, f_rhpz_Hz;
} buck_small_signal_t;

/** Thermal analysis result for a power stage.
 *  Captures junction temp, dissipation, heatsink requirements, safety margin. */
typedef struct {
    double junction_temp_C, power_dissipation_W;
    double required_heatsink_Rth_C_per_W, margin_to_max_C;
    int    thermal_ok;
} thermal_result_t;

/* ========================================================================
 * L2 Core Concepts - Topology Selection, Power Tree, PDN, Trace Sizing
 * ======================================================================== */

/** Select LDO vs DC-DC topology based on Vdrop, current, noise sensitivity.
 *  - Vdrop<0.5V & I<500mA -> LDO
 *  - Vdrop>1.0V & I>100mA -> DC-DC
 *  - Noise-critical (PLL/ADC/RF) -> LDO or DC-DC+LDO post-reg
 *  - Battery, I>50mA -> DC-DC
 *  @return 0=LDO, 1=DC-DC */
int power_topology_select(double v_in, double v_out, double i_load, int noise_sensitive);

/** Compute overall power tree efficiency recursively.
 *  eta_total = product of stage efficiencies = P_out_total / P_in_total.
 *  Recursive depth-first traversal with power accumulation.
 *  @return efficiency (0.0-1.0) */
double power_tree_efficiency(const power_tree_node_t* root);

/** PDN target impedance per FDTIM.
 *  Z_target = V_rail * (ripple_tol% / 100) / I_transient_max.
 *  Example: 3.3V, 5% tol, 0.5A step -> Z_target = 0.33 Ohm.
 *  For modern high-speed MCUs at 100MHz: Z_target < 0.01 Ohm. */
double pdn_target_impedance(const power_rail_spec_t* rail, double i_transient_max);

/** IPC-2221 minimum PCB trace width for given current.
 *  External: I=k*dT^0.44*A^0.725, k=0.048.
 *  Internal: I=k*dT^0.44*A^0.725, k=0.024.
 *  @param current_A DC current through trace
 *  @param temp_rise_C Allowed temp rise above ambient
 *  @param inner_layer Non-zero for internal layer
 *  @return minimum trace width (mm) */
double ipc2221_trace_width(double current_A, double temp_rise_C, int inner_layer);

/** Minimum decoupling capacitance for target impedance at frequency.
 *  For f < SRF: Z ~ 1/(2*pi*f*C) -> C_min = 1/(2*pi*f*Z_target)
 *  @return minimum capacitance (Farads) */
double decoupling_cap_value(double target_impedance_ohm, double frequency_Hz);

/** Power sequencing readiness: check if dependent rail meets min voltage.
 *  measured_voltage >= seq->min_voltage_before_next ?
 *  @return 1 if ready, 0 if waiting */
int power_seq_ready(const power_sequence_step_t* seq, double measured_voltage);

/* ========================================================================
 * L3 Mathematical Structures - Thermal, V-s Balance, RLC, LC Resonance
 * ======================================================================== */

/** LDO junction temperature (Fourier heat conduction analog).
 *  T_j = T_a + P_diss * theta_ja.
 *  P_diss = (Vin-Vout)*I_load + Vin*I_q. */
double ldo_junction_temp(const ldo_regulator_t* ldo, double t_ambient, double i_load);

/** Buck duty cycle from volt-second balance.
 *  (Vin-Vout)*t_on = Vout*t_off -> Vout/Vin = D.
 *  Returns -1.0 if Vout > Vin (impossible for buck). */
double buck_duty_cycle(const dcdc_converter_t* dcdc);

/** Buck inductor peak-to-peak ripple current.
 *  dI_L = (Vin-Vout)*D/(f_sw*L).
 *  Critical for: saturation margin, CCM/DCM boundary, output ripple.
 *  Design target: dI_L = 20-40% of I_out_max. */
double buck_inductor_ripple(const dcdc_converter_t* dcdc, double duty);

/** RC time constant: tau = R * C.
 *  Applications: power rail discharge, reset timing, soft-start.
 *  Time to 99.3% discharge: t = 5*tau. */
double rc_time_constant(double resistance_ohm, double capacitance_F);

/** Buck CCM/DCM boundary current.
 *  I_boundary = dI_L/2 = (Vin-Vout)*D/(2*f_sw*L).
 *  I_load > I_boundary -> CCM. I_load < I_boundary -> DCM. */
double buck_ccm_boundary(const dcdc_converter_t* dcdc, double duty);

/** Boost converter duty cycle: D = 1 - Vin/Vout (ideal). */
double boost_duty_cycle(double v_in, double v_out);

/** Real capacitor impedance magnitude: |Z| = sqrt(ESR^2 + (w*ESL - 1/(w*C))^2).
 *  At SRF: |Z| = ESR (minimum). Below SRF: capacitive. Above SRF: inductive. */
double capacitor_impedance(double capacitance_F, double esr_ohm, double esl_H, double freq_Hz);

/** Inductor energy storage: E = 0.5*L*I^2. Magnetic field energy. */
double inductor_energy(double inductance_H, double current_A);

/** LC filter resonant frequency: f_res = 1/(2*pi*sqrt(L*C)).
 *  Design rule: f_res < f_sw/10 for effective output filtering. */
double lc_resonant_frequency(double inductance_H, double capacitance_F);

/* ========================================================================
 * L4 Fundamental Laws - Ohm, Kirchhoff, Thermodynamics
 * ======================================================================== */

/** Ohm's Law: V_drop = I_load * R_trace.
 *  R = rho*L/(W*t), rho_copper = 1.72e-8 Ohm*m at 25C (IPC-2221).
 *  For 100mm, 0.5mm wide, 1oz, 1A: R=0.098 Ohm, V_drop=98mV. */
double trace_voltage_drop(double length_m, double width_m, double thickness_m, double current_a);

/** KCL check at power tree node. I_parent - sum(I_children) ~= 0.
 *  Non-zero residual = possible leakage or unmodeled load. */
double kcl_power_node(const power_tree_node_t* node);

/** Regulator power loss (First Law of Thermodynamics).
 *  P_loss = P_in - P_out = V_out*I_out*(1/eta - 1).
 *  For LDO: P_loss = (Vin-Vout)*I_out + Vin*I_q. */
double regulator_power_loss(double v_in, double v_out, double i_out, double efficiency);

/** Joule heating: P = I^2 * R. PCB trace/connector/fuse thermal analysis. */
double joule_heating(double current_A, double resistance_ohm);

/** DC power: P = V * I. Most fundamental power electronics equation. */
double dc_power(double voltage_V, double current_A);

/** Capacitor stored energy: E = 0.5*C*V^2.
 *  Hold-up time: t_hold = C*(V_init^2-V_min^2)/(2*P_load). */
double capacitor_energy(double capacitance_F, double voltage_V);

/** KVL verification: V_source - sum(V_drops) - V_load ~= 0. */
double kvl_voltage_check(double v_source, const double* v_drops, int num_drops, double v_load);

/** Full power tree KCL validation (recursive). Returns max residual across all nodes. */
double power_tree_kcl_verify(const power_tree_node_t* root);

/* ========================================================================
 * L5 Algorithms - PDN Synthesis, Stability, Optimization
 * ======================================================================== */

/** Decoupling capacitor bank impedance frequency sweep.
 *  For N caps: Z_total(f) = 1 / sum(1/Z_n(f)).
 *  Z_n(f) = ESR_n + j(w*ESL_n - 1/(w*C_n)).
 *  Complexity: O(N*M). Revels anti-resonance peaks and Z_target violations. */
void decoupling_impedance_sweep(const decoupling_cap_t* caps, int num_caps,
                                const double* frequencies, int num_freqs, double* z_out);

/** LDO phase margin from dominant-pole + ESR zero model.
 *  PM = 90 - atan(fc/fp) - atan(fc/f_esr).
 *  PM>45 stable, PM>60 good transient. */
double ldo_phase_margin(double f_pole, double f_crossover, double f_esr_zero);

/** Buck output capacitance for load transient.
 *  C_out_min = dI * t_response / dV_max.
 *  t_response ~ 3/f_crossover for well-damped system. */
double buck_output_capacitance(double delta_i_load, double t_response, double delta_v_max);

/** Inrush current limiting resistor sizing.
 *  R_limit = V_max / I_inrush_max.
 *  E_diss = 0.5*C*V^2 (independent of R).
 *  Options: fixed R, NTC thermistor, active MOSFET. */
double inrush_limiter_resistance(double v_in_max, double i_inrush_max,
                                 double total_cap_F, double* energy_j_out);

/** Find anti-resonance peaks in PDN impedance sweep.
 *  Scans for local maxima where Z[i] > Z[i-1] and Z[i] > Z[i+1].
 *  These peaks can violate Z_target. */
int pdn_anti_resonance_find(const double* z_impedance, const double* frequencies,
                            int num_points, double* peaks_freq, int max_peaks);

/** Power tree loss allocation (proportional method).
 *  Loss_node = Total_loss * (I_node / I_total).
 *  Complexity: O(N). Enables per-stage thermal budgeting. */
void power_loss_allocation(power_tree_node_t* root, double total_input_power);

/** Buck small-signal model: state-space averaging.
 *  G_vd(s) = G_d0 / (1 + s/(Q*w0) + s^2/w0^2).
 *  G_d0=V_in/D, w0=1/sqrt(L*C), Q=R*sqrt(C/L), f_esr=1/(2*pi*ESR*C). */
void buck_small_signal_model(const dcdc_converter_t* dcdc, double load_resistance_ohm,
                             double esr_ohm, buck_small_signal_t* model_out);

/* ========================================================================
 * L6 Canonical Problems - MCU Power Architecture
 * ======================================================================== */

/** 3.3V->1.8V->1.2V cascaded MCU power budget (STM32H7/RT10xx pattern).
 *  5V USB -> 3.3V LDO -> 1.8V LDO -> 1.2V DC-DC (core).
 *  Returns total input power. Outputs per-stage efficiency. */
double mcu_power_budget(double i_3v3, double i_1v8, double i_1v2,
                        double* eff_3v3_out, double* eff_1v8_out, double* eff_1v2_out);

/** STM32 power rail check against datasheet limits.
 *  VDD: 1.8-3.6V, VDDA: 1.8-3.6V, VBAT: 1.65-3.6V, VREF+: 1.8-VDDA.
 *  @return bitmask: bit0=VDD, bit1=VDDA, bit2=VBAT, bit3=VREF+ */
int stm32_power_rail_check(int vdd_mV, int vdda_mV, int vbat_mV, int vref_mV);

/** nRF52840 power supply configuration validation.
 *  LDO mode: 1.7-5.5V. DC-DC mode: 1.8-3.6V, ~30% lower current. */
int nrf52_power_config(double v_supply, int enable_dcdc, double* current_est_uA_out);

/* ========================================================================
 * L7 Applications - Real Board Power Designs
 * ======================================================================== */

/** Arduino Nano power architecture: USB 5V or VIN 7-12V -> 5V -> 3.3V.
 *  AMS1117-5.0: Imax=1A, Vdrop=1.1V. LP2985-3.3: Imax=150mA, Vdrop=0.28V. */
int arduino_nano_power_model(double vin, int usb_5v, double i_5v_load,
                             double i_3v3_load, double* total_power_w);

/** ESP32 battery life with low-power modes.
 *  Active: 160-260mA, Modem-sleep: 3-20mA, Light-sleep: 0.8mA, Deep-sleep: 5-10uA.
 *  I_avg = sum(I_mode * duty_mode). Life = Capacity / I_avg. */
double esp32_battery_life(double capacity_mAh, double active_current_mA,
                          double active_duty, double sleep_current_uA, double sleep_duty);

/** EV/Tesla auxiliary power: 400/800V traction -> 12V DC-DC -> 5V/3.3V POL.
 *  Overall efficiency = eta_12V_DCDC * eta_POL. */
double ev_aux_power_efficiency(double hv_voltage, double load_12v_w,
                               double dcdc_efficiency, double pol_efficiency);

/* ========================================================================
 * L8 Advanced Topics - GaN, Energy Harvesting, Digital Power
 * ======================================================================== */

/** GaN vs Si FET figure-of-merit: FOM = Rds(on)*Qg, HS-FOM = Rds(on)*Qgd.
 *  GaN: 5-20 mOhm*nC. Si: 50-200 mOhm*nC. GaN has zero Q_rr. */
void gan_vs_si_fom(double rds_on_mohm, double qg_nc, double qgd_nc,
                   int is_gan, double* fom_out, double* hs_fom_out);

/** Energy harvesting PMIC cold-start feasibility.
 *  P_max = V_oc^2/(4*R_source). ADP5091 starts from 380mV, BQ25570 from 330mV. */
int energy_harvesting_feasibility(double v_source_mV, double r_source_ohm,
                                  double v_startup_min_mV, double* p_available_uW);

/** Buck loss decomposition: P_cond + P_sw + P_core + P_gate_drive.
 *  P_cond=I^2*Rds(on), P_sw=0.5*V*I*t_tr*f, P_gate=Qg*Vgs*f, P_core=Steinmetz. */
double buck_loss_decomposition(const dcdc_converter_t* dcdc, double duty,
                               double rds_on_ohm, double qg_total_nC, double core_loss_factor);

/** PMBus VOUT_COMMAND: V_out = VOUT_COMMAND * VREF / (2^N). Digital power. */
int pmbus_vout_command(double v_out, double v_ref, int resolution_bits);

/** Power stage thermal analysis: T_j, P_diss, required heatsink, margin. */
void power_thermal_analysis(const ldo_regulator_t* ldo, double t_ambient_C,
                            double i_load_A, double heatsink_rth, thermal_result_t* result_out);

#ifdef __cplusplus
}
#endif
#endif /* POWER_DESIGN_H */
