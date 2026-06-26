/**
 * @file clock_design.h
 * @brief MCU Development Board - Clock & Crystal Oscillator Design (L1-L8)
 *
 * Knowledge Coverage:
 *   L1: crystal, load capacitance, frequency tolerance, PLL, jitter types, phase noise
 *   L2: Pierce oscillator, PLL synthesis, clock tree distribution, SSCG
 *   L3: Barkhausen criterion, crystal equivalent circuit, Leeson equation, PLL H(s)
 *   L4: Nyquist for ADC clock jitter, Parseval (PN->jitter), Allan deviation
 *   L5: PLL loop filter design, jitter decomposition (Rj+Dj), SSCG profile generation
 *   L6: STM32F4 HSE+PLL->168MHz, ESP32 40MHz->240MHz, nRF52 dual clock
 *   L7: GPSDO accuracy, PCIe REFCLK jitter budget
 *   L8: MEMS vs quartz comparison, SSCG EMI reduction estimation
 *   L9: optoelectronic clock distribution, quantum clock sync (documented)
 *
 * Reference: Vittoz "Low-Power Crystal Oscillator Design" (2010)
 *            Gardner "Phaselock Techniques" (2005), JEDEC JESD65B
 */

#ifndef CLOCK_DESIGN_H
#define CLOCK_DESIGN_H
#include <stdint.h>
#include <stddef.h>
#include <math.h>
#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * L1 Definitions - Core Data Types
 * ======================================================================== */

/** Crystal resonator equivalent circuit model.
 *  C0: shunt capacitance (pF). C1: motional capacitance (fF).
 *  L1: motional inductance (mH). ESR: equivalent series resistance (Ohm).
 *  drive_level_uW: max safe drive level to avoid crystal damage. */
typedef struct {
    double nominal_freq_Hz, freq_tolerance_ppm, freq_stability_ppm;
    double load_cap_pF, shunt_cap_pF, motional_cap_fF, motional_ind_mH;
    double esr_ohm, drive_level_uW;
} crystal_spec_t;

/** Pierce oscillator external component values and startup criteria.
 *  c_in_pF/c_out_pF: external load capacitors. rf_ohm: feedback R (~1M).
 *  rd_ohm: current-limiting R (0-1k). gm_critical_S: critical gm for startup.
 *  gain_margin: must be >=5 for reliable startup. */
typedef struct {
    double c_in_pF, c_out_pF, rf_ohm, rd_ohm, gm_critical_S, gain_margin;
} pierce_oscillator_t;

/** Clock jitter per JEDEC JESD65B.
 *  period_jitter: variation of one period. c2c_jitter: cycle-to-cycle variation.
 *  phase_jitter: integrated 12kHz-20MHz. long_term: N=10^6 cycles. */
typedef struct {
    double period_jitter_ps_rms, period_jitter_ps_pp;
    double c2c_jitter_ps_rms, c2c_jitter_ps_pp;
    double phase_jitter_ps_rms, long_term_jitter_ps;
} clock_jitter_t;

/** PLL configuration: ref, VCO, dividers, loop filter, charge pump.
 *  f_vco = f_ref * N / R, f_out = f_vco / post_div. */
typedef struct {
    double ref_freq_Hz, vco_freq_Hz;
    int    ref_div, feedback_div;
    double loop_bandwidth_Hz, phase_margin_deg;
    double charge_pump_current_A, kvco_Hz_per_V;
} pll_config_t;

/** Phase noise specification at standard offset frequencies.
 *  carrier_freq_Hz: carrier. PN at 100Hz/1k/10k/100k/1MHz offsets (dBc/Hz).
 *  noise_floor_dbHz: ultimate noise floor. */
typedef struct {
    double carrier_freq_Hz;
    double phase_noise_at_100Hz, phase_noise_at_1kHz, phase_noise_at_10kHz;
    double phase_noise_at_100kHz, phase_noise_at_1MHz, noise_floor_dbHz;
} phase_noise_t;

/** Clock tree node for hierarchical clock distribution modeling.
 *  Tracks frequency, jitter accumulation, duty cycle through the tree. */
typedef struct clock_tree_node {
    char   name[32];
    double frequency_Hz;
    int    is_source;
    double duty_cycle;
    clock_jitter_t accumulated_jitter;
    struct clock_tree_node* parent;
    struct clock_tree_node* children[4];
    int    num_children;
} clock_tree_node_t;

/** Crystal drive level measurement result.
 *  Actual_power_uW = I_rms^2 * ESR. Must be < drive_level spec. */
typedef struct {
    double measured_power_uW;
    double margin_percent;
    int    within_spec;
} crystal_drive_level_t;

/* ========================================================================
 * L2 Core Concepts - Pierce, PLL Synthesis, Clock Tree
 * ======================================================================== */

/** Crystal external load capacitor calculation.
 *  C_ext = 2*(C_L - C_stray) for symmetric design (C_in = C_out).
 *  C_stray typically 3-5pF (PCB trace + pin capacitance). */
void crystal_load_capacitor(const crystal_spec_t* crystal, double c_stray_pF, double* c_ext_pF);

/** Barkhausen criterion check for Pierce oscillator startup.
 *  Loop gain |A*beta| >= 1 AND loop phase = 0 deg.
 *  gm_crit = 4*ESR*(2*pi*f)^2*(C_L + C0)^2.
 *  Gain margin = gm_actual / gm_crit >= 5 for reliability. */
int barkhausen_check(const crystal_spec_t* crystal, double c_load_pF,
                     double gm_actual_S, pierce_oscillator_t* oscillator);

/** PLL integer-N frequency synthesis.
 *  f_vco = f_ref * N / R. Searches for valid N,R within VCO range.
 *  @return 0 success, -1 no solution, -2 VCO range exceeded */
int pll_frequency_synthesis(double ref_freq_Hz, double target_freq_Hz,
                            double vco_min_Hz, double vco_max_Hz, pll_config_t* config);

/** Clock tree jitter accumulation analysis.
 *  Each stage adds jitter: sigma_total^2 = sum(sigma_i^2) for uncorrelated sources.
 *  Traverses tree and accumulates per JEDEC JESD65B model. */
void clock_tree_jitter_accumulate(clock_tree_node_t* root);

/** Crystal drive level measurement and safety check.
 *  Measures actual power dissipation in crystal and verifies margin.
 *  Excessive drive can damage crystal or cause frequency shifts. */
void crystal_drive_check(const crystal_spec_t* crystal, double i_rms_uA, crystal_drive_level_t* result);

/* ========================================================================
 * L3 Mathematical Structures - Leeson, Resonance, PLL Transfer
 * ======================================================================== */

/** Leeson's equation for oscillator phase noise.
 *  L(fm)=10*log10[(F*k*T/(2*Ps))*(1+fc/fm)*(1+(f0/(2*QL*fm))^2)]
 *  fm: offset freq, f0: carrier, QL: loaded Q, fc: flicker corner,
 *  F: noise factor, Ps: signal power. */
double leeson_phase_noise(double f_m_Hz, double f0_Hz, double q_loaded,
                          double f_corner_Hz, double noise_factor, double p_signal_W);

/** Crystal series and parallel resonance frequencies.
 *  f_s = 1/(2*pi*sqrt(L1*C1)). f_p = f_s * sqrt(1 + C1/C0).
 *  f_p > f_s. Crystal operates between these for inductive reactance. */
void crystal_resonance_frequencies(const crystal_spec_t* crystal, double* f_series_Hz, double* f_parallel_Hz);

/** PLL closed-loop transfer magnitude at offset frequency.
 *  Type-2 second-order: |H(jw)| at given offset from loop BW and PM. */
double pll_transfer_magnitude(double f_offset_Hz, double loop_bw_Hz, double phase_margin_deg);

/** Crystal motional arm Q factor.
 *  Q = (1/ESR) * sqrt(L1/C1). Typical: 10,000-100,000 for quartz. */
double crystal_q_factor(const crystal_spec_t* crystal);

/* ========================================================================
 * L4 Fundamental Laws - Nyquist, Parseval, Allan
 * ======================================================================== */

/** ADC clock jitter limit per Nyquist.
 *  For N-bit ADC at f_in: t_jitter < 1/(pi*f_in*2^(N+1)).
 *  To preserve 12-bit ENOB at 1MHz: t_jitter < 38.8ps RMS. */
double adc_jitter_limit(int n_bits, double f_input_Hz);

/** Phase noise to RMS jitter conversion (Parseval).
 *  RMS_jitter^2 = (1/(2*pi*fc)^2) * integral(2*10^(L(f)/10) df).
 *  Piece-wise linear integration of phase noise profile. */
double phase_noise_to_jitter(const phase_noise_t* pn, double f_start_Hz, double f_stop_Hz);

/** Allan deviation for clock frequency stability.
 *  Quantifies frequency instability over observation interval tau.
 *  TCXO: ~1e-9 at tau=1s. OCXO: ~1e-11 at tau=1s.
 *  noise_type: 0=white FM, 1=flicker FM, 2=random walk FM. */
double allan_deviation(double stability_1s, double tau_sec, int noise_type);

/* ========================================================================
 * L5 Algorithms - PLL Design, Jitter Decomposition, SSCG
 * ======================================================================== */

/** Type-II second-order PLL loop filter component calculation.
 *  Computes C1, R2, C2 for desired BW and PM.
 *  tau1 = (1-sin(PM))/(wc*cos(PM)). tau2 = 1/(wc^2*tau1). */
void pll_loop_filter_design(const pll_config_t* pll, double* c1_out, double* r2_out, double* c2_out);

/** Jitter decomposition: separate Rj (Gaussian) from Dj (bounded).
 *  Dual-Dirac model: Dj_pp = PP_jitter - 14.069*RMS_jitter (for BER 10^-12).
 *  Rj = sqrt(RMS_jitter^2 - (Dj_pp/14.069)^2). */
void jitter_decomposition(double rms_jitter_ps, double pp_jitter_ps, double* rj_out_ps, double* dj_out_ps);

/** SSCG modulation profile: triangle wave frequency modulation.
 *  f(t) = f_nom + df * triangle(f_mod * t).
 *  Center-spread: varies +-df/2. Down-spread: varies from f_nom to f_nom-df. */
double sscg_profile(double f_nom_Hz, double delta_f_Hz, double f_mod_Hz, double t_sec, int down_spread);

/* ========================================================================
 * L6 Canonical Problems - MCU Clock Configuration
 * ======================================================================== */

/** STM32F4 HSE+PLL->168MHz SYSCLK configuration.
 *  8MHz HSE -> PLL(x336, /8, /2) -> 168MHz SYSCLK.
 *  PLL_VCO=336MHz, USB_48MHz via PLLQ. */
int stm32f4_pll_config(double hse_freq_Hz, double target_sysclk,
                       int* pll_m, int* pll_n, int* pll_p, int* pll_q);

/** ESP32 40MHz XTAL+PLL->CPU clock configuration.
 *  40MHz XTAL -> PLL -> 80/160/240MHz CPU clock. */
int esp32_clock_config(double xtal_freq_Hz, double target_cpu_Hz, int* pll_mult, int* cpu_div);

/** nRF52 32MHz+32kHz dual clock system validation.
 *  32MHz HFCLK for CPU+radio, 32.768kHz LFCLK for RTC/BLE timing. */
int nrf52_clock_validate(double hfclk_Hz, double lfclk_Hz, double hfclk_tol_ppm);

/* ========================================================================
 * L7 Applications - GPSDO, PCIe REFCLK
 * ======================================================================== */

/** GPSDO frequency accuracy analysis.
 *  GPS 1PPS (~10ns RMS jitter) disciplines local OCXO via long-tau PLL.
 *  Locked accuracy < 1e-12. Lock time depends on PLL time constant. */
double gpsdo_accuracy(double ocxo_stability_1s, double gps_1pps_jitter_ns,
                      double pll_time_constant_s, double* locked_accuracy_ppb);

/** PCIe reference clock jitter budget.
 *  Total = sqrt(sum(source^2)). PCIe Gen3: 100MHz REFCLK, <1ps RMS (12k-20M).
 *  Returns 0 if meets spec. */
int pcie_refclk_budget(double crystal_jitter_ps, double pll_jitter_ps,
                       double buffer_jitter_ps, double spec_limit_ps);

/* ========================================================================
 * L8 Advanced Topics - MEMS vs Quartz, SSCG EMI
 * ======================================================================== */

/** MEMS oscillator vs quartz crystal comparison.
 *  MEMS: smaller, faster startup, higher shock resistance.
 *  Quartz: lower phase noise, better temperature stability.
 *  Returns quality score: >0 quartz preferred, <0 MEMS preferred. */
double mems_vs_quartz_comparison(double mems_pn_1kHz, double mems_stability_ppm,
                                  double quartz_pn_1kHz, double quartz_stability_ppm);

/** SSCG EMI reduction estimation.
 *  Reduction(dB) ~ 10*log10(delta * fc / RBW).
 *  Typical: delta=0.5%, fc=100MHz, RBW=120kHz -> 7-15dB reduction. */
double sscg_emi_reduction(double f_clock_Hz, double delta_percent, double rbw_Hz);

#ifdef __cplusplus
}
#endif
#endif /* CLOCK_DESIGN_H */
