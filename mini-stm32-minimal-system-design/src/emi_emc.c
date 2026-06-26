#include "stm32_minimal_config.h"
#include <math.h>
#include <stdio.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* compute_loop_antenna_radiation
 * L4: Maxwell's equations — radiation from small loop.
 * |E| = (120 * pi^2 * I * A) / (r * lambda^2)
 * Reference: Paul, "Introduction to EMC", Section 8.2 */
double compute_loop_antenna_radiation(double loop_current_a,
                                       double loop_area_m2,
                                       double distance_m,
                                       double freq_hz) {
    if (loop_current_a <= 0 || loop_area_m2 <= 0
        || distance_m <= 0 || freq_hz <= 0) return 0.0;
    double c = 2.998e8;
    double lambda = c / freq_hz;
    if (lambda <= 0) return 0.0;
    return (120.0 * M_PI * M_PI * loop_current_a * loop_area_m2)
           / (distance_m * lambda * lambda);
}

/* compute_common_mode_radiation
 * L4: CM current on cables — dipole radiation.
 * |E| = (1.26e-6 * I_cm * L * f) / r
 * Reference: Ott, "EMC Engineering", Ch 11 */
double compute_common_mode_radiation(double cm_current_a,
                                      double cable_length_m,
                                      double freq_hz,
                                      double distance_m) {
    if (cm_current_a <= 0 || cable_length_m <= 0
        || freq_hz <= 0 || distance_m <= 0) return 0.0;
    return (1.26e-6 * cm_current_a * cable_length_m * freq_hz) / distance_m;
}

/* compute_ferrite_bead_impedance
 * L5: Z(f) ~ Z_100MHz * sqrt(f/100MHz)
 * Reference: TDK/Murata ferrite bead app notes */
double compute_ferrite_bead_impedance(double impedance_at_100mhz,
                                       double target_freq_hz) {
    if (impedance_at_100mhz <= 0 || target_freq_hz <= 0) return 0.0;
    return impedance_at_100mhz * sqrt(target_freq_hz / 100e6);
}

/* compute_shielding_effectiveness
 * L5: SE = R + A + B (dB)
 * R = reflection, A = absorption (8.69*t/delta), B = multiple reflection
 * Reference: Paul, "Introduction to EMC", Ch 10 */
double compute_shielding_effectiveness(double freq_hz,
                                        double thickness_mm,
                                        double conductivity_s_per_m,
                                        double permeability) {
    if (freq_hz <= 0 || thickness_mm <= 0
        || conductivity_s_per_m <= 0 || permeability <= 0) return 0.0;
    double mu_0 = 1.2566e-6;
    double mu = mu_0 * permeability;
    double omega = 2.0 * M_PI * freq_hz;
    double delta = sqrt(2.0 / (omega * mu * conductivity_s_per_m));
    double t_m = thickness_mm * 0.001;
    double a_db = 8.69 * t_m / delta;
    double z_w = 377.0;
    double z_s = sqrt(omega * mu / conductivity_s_per_m);
    double r_db = 20.0 * log10(z_w / (4.0 * z_s));
    if (r_db < 0) r_db = 0;
    double se_db = r_db + a_db;
    if (a_db < 10.0) {
        double b_db = 20.0 * log10(fabs(1.0 - exp(-2.0 * t_m / delta)));
        se_db += b_db;
    }
    return se_db;
}

/* check_fcc_part15_compliance
 * L2: FCC Part 15 Class B limits at 3m
 * 30-88MHz: 100uV/m, 88-216: 150uV/m, 216-960: 200uV/m, >960: 500uV/m */
int check_fcc_part15_compliance(double e_field_uv_per_m, double freq_hz) {
    if (e_field_uv_per_m < 0) return 0;
    double limit_uv_per_m;
    if (freq_hz >= 30e6 && freq_hz < 88e6) limit_uv_per_m = 100.0;
    else if (freq_hz >= 88e6 && freq_hz < 216e6) limit_uv_per_m = 150.0;
    else if (freq_hz >= 216e6 && freq_hz < 960e6) limit_uv_per_m = 200.0;
    else if (freq_hz >= 960e6) limit_uv_per_m = 500.0;
    else return 1;
    return (e_field_uv_per_m <= limit_uv_per_m) ? 1 : 0;
}

/* compute_ground_fill_effectiveness
 * L5: Copper pour EMI reduction.
 * SE_improvement = 20*log10(1 + fill% * N_vias) */
double compute_ground_fill_effectiveness(double fill_percentage,
                                          int stitching_vias_per_wavelength) {
    if (fill_percentage <= 0 || stitching_vias_per_wavelength <= 0) return 0.0;
    return 20.0 * log10(1.0 + fill_percentage
                        * (double)stitching_vias_per_wavelength / 10.0);
}

/* compute_rise_time_harmonics
 * L3: Fourier — trapezoidal waveform harmonics.
 * A_n = 2*A*(tau/T)*sinc(n*pi*tau/T)*sinc(n*pi*tr/T)
 * Reference: Ott, "EMC Engineering", Ch 2 */
double compute_rise_time_harmonics(double rise_time_ns,
                                    double amplitude_v,
                                    int harmonic_number,
                                    double period_ns) {
    if (rise_time_ns <= 0 || period_ns <= 0 || harmonic_number <= 0) return 0.0;
    double t_r = rise_time_ns;
    double T = period_ns;
    double tau = T / 2.0;
    int n = harmonic_number;
    double pi_n = M_PI * (double)n;
    double sinc1 = (pi_n * tau / T > 0.001)
                   ? sin(pi_n * tau / T) / (pi_n * tau / T) : 1.0;
    double sinc2 = (pi_n * t_r / T > 0.001)
                   ? sin(pi_n * t_r / T) / (pi_n * t_r / T) : 1.0;
    return (2.0 * amplitude_v * tau / T) * sinc1 * sinc2;
}

/* compute_corner_frequency
 * L4: f_corner = 1 / (pi * t_r)
 * The most important single parameter for EMI prediction. */
double compute_corner_frequency(double rise_time_ns) {
    if (rise_time_ns <= 0) return 1e12;
    return 1.0 / (M_PI * rise_time_ns * 1e-9);
}

/* compute_max_clock_harmonic_for_emc
 * L5: Harmonics up to f_corner need consideration for EMC. */
int compute_max_clock_harmonic_for_emc(double clock_freq_hz,
                                        double rise_time_ns) {
    if (clock_freq_hz <= 0 || rise_time_ns <= 0) return 0;
    double f_corner = compute_corner_frequency(rise_time_ns);
    int max_harmonic = (int)(f_corner / clock_freq_hz) + 1;
    if (max_harmonic < 5) max_harmonic = 5;
    if (max_harmonic > 100) max_harmonic = 100;
    return max_harmonic;
}
