/**
 * @file emi_emc.h
 * @brief EMI/EMC considerations for STM32 minimal system.
 * Knowledge Level: L2, L3, L4, L5
 */
#ifndef EMI_EMC_H
#define EMI_EMC_H

#include "stm32_minimal_config.h"

double compute_loop_antenna_radiation(double loop_current_a,
                                       double loop_area_m2,
                                       double distance_m, double freq_hz);
double compute_common_mode_radiation(double cm_current_a, double cable_length_m,
                                      double freq_hz, double distance_m);
double compute_ferrite_bead_impedance(double impedance_at_100mhz,
                                       double target_freq_hz);
double compute_shielding_effectiveness(double freq_hz, double thickness_mm,
                                        double conductivity_s_per_m,
                                        double permeability);
int check_fcc_part15_compliance(double e_field_uv_per_m, double freq_hz);
double compute_ground_fill_effectiveness(double fill_percentage,
                                          int stitching_vias_per_wavelength);
double compute_rise_time_harmonics(double rise_time_ns, double amplitude_v,
                                    int harmonic_number, double period_ns);
double compute_corner_frequency(double rise_time_ns);
int compute_max_clock_harmonic_for_emc(double clock_freq_hz,
                                        double rise_time_ns);

#endif /* EMI_EMC_H */
