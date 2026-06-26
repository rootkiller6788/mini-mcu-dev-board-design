/**
 * @file board_validation.h
 * @brief MCU Development Board - Design Validation & Signal Integrity Testing (L1-L8)
 *
 * Knowledge Coverage:
 *   L1: signal integrity, eye diagram, power integrity, ESD, EMC, thermal, reliability
 *   L2: eye mask test, PDN impedance measurement, conducted/radiated emissions
 *   L3: transmission line models, crosstalk coupling, TDR impedance, BER estimation
 *   L4: Maxwell/Telegrapher equations, characteristic impedance, skin effect
 *   L5: eye diagram analysis, jitter/ber extrapolation, TDR impedance profiling
 *   L6: USB 2.0 eye compliance, DDR memory interface validation, SPI/I2C signal check
 *   L7: FCC/CE pre-compliance test, automotive EMC (CISPR 25), MIL-STD-461
 *   L8: statistical eye analysis, IBIS-AMI simulation correlation, machine learning SI
 *   L9: terahertz board characterization (documented)
 *
 * Reference: Bogatin "Signal and Power Integrity - Simplified" (2018)
 *            Paul "Introduction to Electromagnetic Compatibility" (2006)
 *            Johnson & Graham "High-Speed Digital Design" (1993)
 */

#ifndef BOARD_VALIDATION_H
#define BOARD_VALIDATION_H
#include <stdint.h>
#include <stddef.h>
#include <math.h>
#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * L1 Definitions
 * ======================================================================== */

/** Eye diagram measurement for high-speed digital interfaces.
 *  Captures key parameters: eye height, eye width, jitter, rise/fall time.
 *  Eye height: vertical opening (mV). Eye width: horizontal opening (ps or UI).
 *  Bit error rate estimated from eye opening. */
typedef struct {
    double eye_height_mV, eye_width_ps;
    double eye_height_percent, eye_width_UI;
    double rms_jitter_ps, pp_jitter_ps;
    double rise_time_ps, fall_time_ps;
    double q_factor;
    double estimated_ber;
    int    eye_open;
} eye_diagram_t;

/** Power integrity measurement.
 *  Rail noise: RMS and peak-to-peak ripple voltage.
 *  Transient response: voltage droop under load step.
 *  PDN impedance derived from VNA or 2-port shunt-through measurement. */
typedef struct {
    double rail_voltage_avg_V;
    double ripple_rms_mV, ripple_pp_mV;
    double noise_floor_dBm;
    double transient_droop_mV;
    double transient_recovery_us;
    double pdn_impedance_at_1MHz_mOhm;
    double pdn_impedance_at_100MHz_mOhm;
    int    within_target_impedance;
} power_integrity_measurement_t;

/** ESD protection test result.
 *  Per IEC 61000-4-2: contact discharge (2, 4, 6, 8kV), air discharge.
 *  Classification: A=normal, B=temporary degradation, C=needs reset, D=damage. */
typedef struct {
    double test_voltage_kV;
    int    discharge_type;
    int    classification;
    int    soft_failure_detected;
    int    hard_failure_detected;
    int    esd_passed;
} esd_test_result_t;

/** EMC emission measurement.
 *  Conducted: 150kHz-30MHz (CISPR 22/32). Radiated: 30MHz-1GHz (or 6GHz).
 *  Measured vs limit line margin at each frequency. */
typedef struct {
    double frequency_MHz;
    double measured_level_dBuV;
    double limit_dBuV;
    double margin_dB;
    char   emission_type[16];
    int    pass;
} emc_emission_point_t;

/** Thermal image measurement.
 *  Captures temperature distribution across PCB.
 *  Hot spot identification for thermal management verification. */
typedef struct {
    double ambient_temp_C;
    double max_temp_C, min_temp_C;
    double max_temp_x_mm, max_temp_y_mm;
    double max_temp_component[32];
    int    hotspot_count;
    int    max_temp_within_limit;
} thermal_image_t;

/** Signal integrity validation for a single digital interface.
 *  Comprehensive test covering: timing, voltage levels, jitter, rise/fall. */
typedef struct {
    char   interface_name[32];
    double bitrate_Mbps;
    double v_high_min_V, v_low_max_V;
    double setup_time_ns, hold_time_ns;
    double setup_margin_ns, hold_margin_ns;
    double overshoot_percent, undershoot_percent;
    int    monotonic_rising, monotonic_falling;
    int    si_pass;
} signal_integrity_test_t;

/* ========================================================================
 * L2 Core Concepts
 * ======================================================================== */

/** Eye mask test: verify signal stays outside mask region.
 *  Mask defines forbidden region in unit interval / voltage space.
 *  Hits = failures. Standard masks: USB, PCIe, DDR, Ethernet. */
int eye_mask_test(const eye_diagram_t* eye, double mask_hexagon[6][2],
                  int* mask_hits);

/** PDN 2-port shunt-through impedance measurement analysis.
 *  Z_PDN = Z0 * S21 / (2 * (1 - S21)).
 *  Requires VNA calibration (SOLT). Frequency range typically 100Hz-1GHz. */
double pdn_shunt_through_impedance(double s21_magnitude, double s21_phase_deg,
                                    double z0_ohm);

/** Signal integrity timing budget analysis.
 *  Total timing budget = clock_period - setup - hold - skew - jitter.
 *  Must be >0 for reliable operation. */
double timing_budget_margin(double clock_period_ns, double setup_ns, double hold_ns,
                             double skew_ns, double jitter_ns);

/** Transmission line characteristic impedance (microstrip).
 *  Z0 = 87/sqrt(Er+1.41) * ln(5.98*h/(0.8*w+t)).
 *  IPC-2141 approximation for microstrip. */
double microstrip_impedance(double er, double h_mm, double w_mm, double t_mm);

/* ========================================================================
 * L3 Mathematical Structures
 * ======================================================================== */

/** TDR (Time Domain Reflectometry) impedance profile.
 *  Reflection coefficient: Gamma = (Z_L - Z0)/(Z_L + Z0).
 *  Impedance = Z0 * (1 + Gamma)/(1 - Gamma).
 *  Distance = v * t/2 where v = c/sqrt(Er_eff). */
double tdr_impedance_from_reflection(double z0_ohm, double reflection_coefficient);

/** Crosstalk coupling coefficient.
 *  Near-end (NEXT): K_ne = V_noise_near/V_driver.
 *  Far-end (FEXT): K_fe = V_noise_far/V_driver.
 *  Capacitive coupling: I_noise = C_m * dV/dt.
 *  Inductive coupling: V_noise = L_m * dI/dt. */
void crosstalk_coupling(double c_mutual_pF, double l_mutual_nH,
                         double tr_rise_ns, double v_driver_V, double z0_ohm,
                         double* v_next_mV, double* v_fext_mV);

/** BER estimation from eye diagram Q-factor.
 *  Q = (mu1 - mu0)/(sigma1 + sigma0) = eye_height / (2 * RMS_noise).
 *  BER(approx) = 0.5 * erfc(Q/sqrt(2)).
 *  For Q=6: BER ~ 1e-9. For Q=7: BER ~ 1e-12. */
double ber_from_qfactor(double q_factor);

/** Skin effect: AC resistance increase with frequency.
 *  Skin depth: delta = sqrt(rho/(pi*f*mu0)).
 *  R_ac/R_dc = 1 + (t/delta) for t<2*delta, approx = t/delta for t>2*delta.
 *  @return R_ac at given frequency (Ohms) */
double skin_effect_resistance(double r_dc, double frequency_Hz, double thickness_m);

/* ========================================================================
 * L4 Fundamental Laws
 * ======================================================================== */

/** Telegrapher's equations for lossy transmission line.
 *  Characteristic impedance: Z0 = sqrt((R+jwL)/(G+jwC)).
 *  Propagation constant: gamma = sqrt((R+jwL)(G+jwC)).
 *  @param z0_out Characteristic impedance
 *  @param gamma_real_out Attenuation constant (Np/m)
 *  @param gamma_imag_out Phase constant (rad/m) */
void telegrapher_equations(double r_per_m, double l_per_m, double g_per_m,
                            double c_per_m, double frequency_Hz,
                            double* z0_out, double* gamma_real_out,
                            double* gamma_imag_out);

/** Maxwell-Faraday law applied to PCB loop EMC.
 *  V_induced = -dPhi/dt = -A * dB/dt.
 *  Loop area A is critical for radiated susceptibility.
 *  Minimizing loop area is the primary EMC design rule. */
double faraday_induced_voltage(double loop_area_m2, double b_field_T,
                                double frequency_Hz);

/* ========================================================================
 * L5 Algorithms
 * ======================================================================== */

/** Eye diagram analysis from waveform capture.
 *  Processes captured time/voltage data to compute:
 *  eye height, width, jitter, rise/fall time, Q-factor, BER estimate.
 *  Aligns waveform using clock recovery (software PLL). */
void eye_diagram_analyze(const double* time_ns, const double* voltage_V,
                          int num_samples, double bit_period_ns,
                          eye_diagram_t* eye);

/** PDN impedance target violation detection.
 *  Scans measured PDN Z(f) against Z_target(f).
 *  Returns frequencies where Z_measured > Z_target and the violation margin. */
int pdn_violation_detect(const double* freq_Hz, const double* z_measured_ohm,
                          const double* z_target_ohm, int num_points,
                          double* violation_freqs_Hz, double* violation_margins, int max_violations);

/** Jitter/eye extrapolation: estimate BER from eye opening.
 *  Uses dual-Dirac model. Extrapolates eye closure to target BER.
 *  Bathtub curve: BER(t) = 0.5*erfc((t-DJ/2)/(sigma*sqrt(2))). */
double extrapolate_ber_at_threshold(const eye_diagram_t* eye, double target_ber);

/* ========================================================================
 * L6 Canonical Problems
 * ======================================================================== */

/** USB 2.0 high-speed (480Mbps) eye diagram compliance.
 *  Checks against USB-IF eye mask template.
 *  Template points define the forbidden hexagon region.
 *  @return 0 if compliant, -1 if mask violation detected */
int usb20_eye_compliance(const eye_diagram_t* eye, double* margin_mV, double* margin_ps);

/** DDR memory interface signal integrity validation.
 *  Checks setup/hold timing, voltage levels, slew rate, overshoot.
 *  DDR3: 1.5V, DDR4: 1.2V. Clock: differential SSTL.
 *  @return bitmask of failing signals */
int ddr_signal_integrity(const signal_integrity_test_t* signals, int num_signals);

/** SPI bus signal quality check.
 *  Verifies: SCK frequency, setup/hold for MOSI/MISO, CS timing.
 *  Common issues: slow rise times (capacitive loading), crosstalk, ground bounce. */
int spi_signal_check(double sck_freq_Hz, double rise_time_ns, double fall_time_ns,
                      double setup_margin_ns, double hold_margin_ns);

/* ========================================================================
 * L7 Applications
 * ======================================================================== */

/** FCC Part 15 Class B radiated emissions pre-compliance.
 *  30MHz-1GHz, 3m distance, quasi-peak detector.
 *  Identifies frequencies exceeding limit by 6dB (pre-compliance margin). */
int fcc_part15_classB_precompliance(const emc_emission_point_t* measurements,
                                      int num_points, int* failing_frequencies, int max_fails);

/** Automotive EMC CISPR 25 conducted emissions.
 *  Class 5 (strictest) limits for 12V vehicle power line.
 *  150kHz-108MHz frequency range, LISN (Line Impedance Stabilization Network). */
int cispr25_conducted_emissions(const emc_emission_point_t* measurements,
                                  int num_points, int emc_class, int* num_failures);

/** MIL-STD-461G RS103 radiated susceptibility.
 *  2MHz-40GHz, 20-200V/m field strength.
 *  Verifies board operates without degradation under high field. */
int milstd461_rs103_susceptibility(double field_strength_V_per_m,
                                     double frequency_Hz, int* degradation_detected);

/* ========================================================================
 * L8 Advanced Topics
 * ======================================================================== */

/** Statistical eye diagram analysis with confidence intervals.
 *  Computes eye opening at specified confidence level (e.g., BER=1e-12 at 95% CI).
 *  Uses extreme value theory (EVT) for extrapolation beyond measured samples. */
double statistical_eye_confidence(const eye_diagram_t* eye, double target_ber,
                                   double confidence_level);

/** IBIS-AMI model correlation with lab measurements.
 *  Compares simulated eye (from IBIS-AMI) against measured eye.
 *  Correlation coefficient >0.9 indicates good model accuracy. */
double ibis_ami_correlation(const eye_diagram_t* simulated,
                             const eye_diagram_t* measured);

#ifdef __cplusplus
}
#endif
#endif /* BOARD_VALIDATION_H */
