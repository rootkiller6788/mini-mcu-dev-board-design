/**
 * @file decoupling.h
 * @brief Decoupling capacitor network design for STM32 PDN.
 * Knowledge Level: L1, L2, L4, L5
 * Reference: Smith & Bogatin, ST AN4488 Section 3
 * Course mapping: Berkeley EE117, MIT 6.003
 */
#ifndef DECOUPLING_H
#define DECOUPLING_H
#include "stm32_minimal_config.h"

typedef struct {
    double vdd_nominal, ripple_percent, transient_current, target_impedance;
    double freq_start, freq_end;
} PDNImpedanceTarget;

typedef struct {
    double frequency_hz, impedance_ohm, phase_deg;
    int is_capacitive, is_inductive;
} ImpedancePoint;

typedef struct {
    int num_points;
    double freq_log_start, freq_log_end, peak_impedance, peak_impedance_freq;
    ImpedancePoint points[512];
} PDNImpedanceProfile;

typedef struct {
    double c, esr, esl, voltage_rating;
    int package_code;
} DecouplingCap;

typedef struct {
    int num_caps;
    DecouplingCap caps[32];
    double total_c, total_esr, total_esl, anti_res_freq;
} DecouplingNetwork;

/**
 * Impedance of a single capacitor at freq. L3: Z=sqrt(ESR^2 + (wL-1/wC)^2)
 */
void capacitor_impedance(const DecouplingCap *cap, double freq_hz,
                         double *imp_out, double *phase_out);
double cap_self_resonant_freq(double c, double esl);
void parallel_cap_impedance(const DecouplingNetwork *network, double freq_hz,
                            double *imp_out, double *phase_out);
double compute_target_impedance(double vdd, double ripple_percent,
                                double transient_current);
int min_caps_for_impedance(const DecouplingCap *cap_model, double target_z_ohm,
                           double derating_factor);
void pdn_impedance_sweep(const DecouplingNetwork *network,
                         double f_start_hz, double f_end_hz,
                         int num_points, PDNImpedanceProfile *profile);
int find_anti_resonance_peaks(const PDNImpedanceProfile *profile,
                              double z_target, double peaks[], int max_peaks);
double estimate_mounting_inductance(double via_length_mm,
                                    double via_diameter_mm, double trace_length_mm);
double dc_bias_derating(double c_nominal, double v_bias, double v_rated);

#endif /* DECOUPLING_H */

/* Additional decoupling functions */
int compute_capacitor_bank_optimization(double z_target, double f_min,
                                         double f_max, int max_caps,
                                         DecouplingNetwork *bank);
double compute_plane_capacitance(double area_mm2, double dielectric_thickness_mm,
                                 double epsilon_r);
double compute_esr_from_dissipation_factor(double capacitance,
                                            double dissipation_factor,
                                            double freq_hz);
double compute_esl_from_srf(double capacitance, double srf_hz);
