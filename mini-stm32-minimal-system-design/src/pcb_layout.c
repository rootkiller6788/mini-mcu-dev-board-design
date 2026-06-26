/**
 * @file pcb_layout.c
 * @brief PCB layout calculations.
 */

#include "pcb_layout.h"
#include <math.h>
#include <string.h>
#include <stdio.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static double oz_to_mm(double oz) { return oz * 0.0347; }
static double mm_to_mil(double mm) { return mm / 0.0254; }
static double mil_to_mm(double mil) { return mil * 0.0254; }

double ipc2221_trace_width(double current_a, double temp_rise_c,
                           double copper_weight_oz, int inner_layer) {
    if (current_a <= 0 || temp_rise_c <= 0 || copper_weight_oz <= 0) return 0.0;
    double k = inner_layer ? 0.024 : 0.048;
    double thickness_mil = mm_to_mil(oz_to_mm(copper_weight_oz));
    if (thickness_mil <= 0) return 0.0;
    double area_mil2 = pow(current_a / (k * pow(temp_rise_c, 0.44)), 1.0 / 0.725);
    double width_mil = area_mil2 / thickness_mil;
    return mil_to_mm(width_mil);
}

double ipc2221_current_capacity(double trace_width_mm, double temp_rise_c,
                                double copper_weight_oz, int inner_layer) {
    if (trace_width_mm <= 0 || temp_rise_c <= 0 || copper_weight_oz <= 0) return 0.0;
    double k = inner_layer ? 0.024 : 0.048;
    double width_mil = mm_to_mil(trace_width_mm);
    double thick_mil = mm_to_mil(oz_to_mm(copper_weight_oz));
    double area_mil2 = width_mil * thick_mil;
    return k * pow(temp_rise_c, 0.44) * pow(area_mil2, 0.725);
}

double trace_dc_resistance(double length_mm, double width_mm,
                           double copper_weight_oz, double temperature_c) {
    if (length_mm <= 0 || width_mm <= 0 || copper_weight_oz <= 0) return 0.0;
    double length_m = length_mm * 0.001;
    double width_m = width_mm * 0.001;
    double thickness_m = oz_to_mm(copper_weight_oz) * 0.001;
    double rho_20 = 1.72e-8;
    double alpha = 0.00393;
    double rho_t = rho_20 * (1.0 + alpha * (temperature_c - 20.0));
    return rho_t * length_m / (width_m * thickness_m);
}

double microstrip_impedance(double er, double trace_width_mm,
                            double thickness_mm, double height_mm) {
    if (er <= 0 || height_mm <= 0 || trace_width_mm <= 0) return 0.0;
    double w_over_h = trace_width_mm / height_mm;
    if (w_over_h >= 1.0) {
        double denom = 0.8 * trace_width_mm + thickness_mm;
        if (denom <= 0) return 0.0;
        return (87.0 / sqrt(er + 1.41)) * log(5.98 * height_mm / denom);
    } else {
        double er_eff = (er + 1.0) / 2.0
                      + (er - 1.0) / 2.0
                      * (1.0 / sqrt(1.0 + 12.0 * height_mm / trace_width_mm));
        double term1 = 8.0 * height_mm / trace_width_mm;
        double term2 = trace_width_mm / (4.0 * height_mm);
        return (60.0 / sqrt(er_eff)) * log(term1 + term2);
    }
}

double microstrip_width_for_impedance(double er, double target_z_ohm,
                                      double thickness_mm, double height_mm) {
    if (target_z_ohm <= 0 || er <= 0 || height_mm <= 0) return 0.0;
    double w_min = 0.05, w_max = 10.0;
    for (int iter = 0; iter < 50; iter++) {
        double w_mid = (w_min + w_max) / 2.0;
        double z_mid = microstrip_impedance(er, w_mid, thickness_mm, height_mm);
        if (z_mid <= 0) return 0.0;
        if (fabs(z_mid - target_z_ohm) < 0.1) return w_mid;
        if (z_mid > target_z_ohm) w_min = w_mid;
        else w_max = w_mid;
    }
    return (w_min + w_max) / 2.0;
}

double differential_impedance(double z0_odd, double spacing_mm, double height_mm) {
    if (z0_odd <= 0 || height_mm <= 0) return 0.0;
    double coupling_factor = 0.48 * exp(-0.96 * spacing_mm / height_mm);
    return 2.0 * z0_odd * (1.0 - coupling_factor);
}

double stripline_impedance(double er, double trace_width_mm,
                           double thickness_mm, double spacing_mm) {
    if (er <= 0 || trace_width_mm <= 0 || spacing_mm <= 0) return 0.0;
    double t_over_w = (trace_width_mm > 0) ? thickness_mm / trace_width_mm : 0;
    double effective_w = trace_width_mm * (0.8 + t_over_w);
    double denom = 0.67 * M_PI * effective_w;
    if (denom <= 0) return 0.0;
    return (60.0 / sqrt(er)) * log(4.0 * spacing_mm / denom);
}

double estimate_next_crosstalk(double spacing_mm, double height_mm,
                               double parallel_length_mm) {
    if (height_mm <= 0) return 0.0;
    double s_over_h = spacing_mm / height_mm;
    double k = 1.0 / (1.0 + s_over_h * s_over_h);
    double length_factor = (parallel_length_mm < 50.0) ? parallel_length_mm / 50.0 : 1.0;
    double next = k * length_factor;
    if (next <= 0) return -60.0;
    return 20.0 * log10(next);
}

double minimum_crosstalk_spacing(double height_mm, double target_next_db) {
    if (height_mm <= 0) return 0.0;
    double next_linear = pow(10.0, target_next_db / 20.0);
    if (next_linear <= 0 || next_linear >= 1.0) return 0.0;
    return height_mm * sqrt(1.0 / next_linear - 1.0);
}

double via_inductance(double via_height_mm, double drill_diameter_mm) {
    if (via_height_mm <= 0 || drill_diameter_mm <= 0) return 0.0;
    double ratio = (4.0 * via_height_mm) / drill_diameter_mm;
    double l_nh = 5.08 * via_height_mm * (log(ratio) + 1.0);
    return l_nh * 1e-9;
}

double via_pad_capacitance(double pad_diameter_mm, double antipad_diameter_mm,
                           double plane_thickness_mm, double er) {
    if (antipad_diameter_mm <= pad_diameter_mm) return 0.0;
    if (plane_thickness_mm <= 0 || er <= 0) return 0.0;
    double gap = antipad_diameter_mm - pad_diameter_mm;
    if (gap <= 0) return 0.0;
    double c_pf = (1.41 * er * pad_diameter_mm * plane_thickness_mm) / gap;
    return c_pf * 1e-12;
}

void validate_stm32_layout(double crystal_dist_mm, double max_decoup_dist_mm,
                           double swd_trace_length_mm, int gnd_plane_continuous,
                           double power_trace_width_mm, double power_trace_current_a,
                           double copper_weight_oz, LayoutValidation *result) {
    if (!result) return;
    memset(result, 0, sizeof(*result));
    result->passes = 1;
    result->margin_worst_case = 1.0;
    result->total_checks++;
    if (crystal_dist_mm > 10.0) {
        sprintf(result->failing_items[result->violations],
                "Crystal distance %.1fmm exceeds 10mm", crystal_dist_mm);
        result->violations++; result->passes = 0;
    }
    result->total_checks++;
    if (max_decoup_dist_mm > 2.0) {
        sprintf(result->failing_items[result->violations],
                "Decoupling cap distance %.1fmm exceeds 2mm", max_decoup_dist_mm);
        result->violations++; result->passes = 0;
    }
    result->total_checks++;
    if (swd_trace_length_mm > 50.0) {
        sprintf(result->failing_items[result->violations],
                "SWD trace length %.1fmm exceeds 50mm", swd_trace_length_mm);
        result->violations++; result->passes = 0;
    }
    result->total_checks++;
    if (!gnd_plane_continuous) {
        sprintf(result->failing_items[result->violations],
                "Ground plane not continuous under MCU");
        result->violations++; result->passes = 0;
    }
    result->total_checks++;
    if (power_trace_width_mm > 0 && power_trace_current_a > 0) {
        double max_curr = ipc2221_current_capacity(power_trace_width_mm, 10.0,
                                                    copper_weight_oz, 0);
        if (max_curr < power_trace_current_a) {
            sprintf(result->failing_items[result->violations],
                    "Power trace too narrow: %.2fmm", power_trace_width_mm);
            result->violations++; result->passes = 0;
        }
    }
}


/* Additional PCB functions — each implements an independent knowledge point */

double compute_trace_inductance(double length_mm, double width_mm,
                                double thickness_mm) {
    if (length_mm <= 0 || width_mm <= 0) return 0.0;
    double w_plus_t = width_mm + thickness_mm;
    if (w_plus_t <= 0) return 0.0;
    double ratio = (2.0 * length_mm) / w_plus_t;
    double l_nh = 2.0 * length_mm * (log(ratio) + 0.5
                  + 0.2235 * w_plus_t / length_mm);
    return l_nh * 1e-9;
}

double compute_propagation_delay(double length_mm, double epsilon_r_eff) {
    if (length_mm <= 0 || epsilon_r_eff <= 0) return 0.0;
    double c = 2.998e8;
    double v_p = c / sqrt(epsilon_r_eff);
    double t_pd_per_m = 1.0 / v_p;
    double length_m = length_mm * 0.001;
    return length_m * t_pd_per_m;
}

double compute_critical_length(double rise_time_ps, double epsilon_r_eff) {
    if (rise_time_ps <= 0 || epsilon_r_eff <= 0) return 0.0;
    double c = 2.998e8;
    double v_p = c / sqrt(epsilon_r_eff);
    double t_pd_per_mm = 0.001 / v_p;
    double rise_time_s = rise_time_ps * 1e-12;
    return rise_time_s / (2.0 * t_pd_per_mm);
}

double compute_return_path_inductance(double signal_height_mm,
                                       double gap_length_mm,
                                       double gap_width_mm) {
    if (signal_height_mm <= 0 || gap_length_mm <= 0 || gap_width_mm <= 0)
        return 0.0;
    double mu_0 = 1.2566e-6;
    double h_m = signal_height_mm * 0.001;
    double l_m = gap_length_mm * 0.001;
    double w_m = gap_width_mm * 0.001;
    if (w_m <= 0) return 0.0;
    return mu_0 * h_m * l_m / (2.0 * M_PI * w_m);
}

double compute_decoupling_radius(double rise_time_ps, double epsilon_r_eff) {
    if (rise_time_ps <= 0 || epsilon_r_eff <= 0) return 0.0;
    double c = 2.998e8;
    double t_rise_s = rise_time_ps * 1e-12;
    double radius_m = t_rise_s * c / (2.0 * sqrt(epsilon_r_eff));
    return radius_m * 1000.0;
}

double compute_ground_plane_impedance(double plane_length_mm,
                                       double plane_width_mm,
                                       double copper_weight_oz,
                                       double freq_hz) {
    if (plane_length_mm <= 0 || plane_width_mm <= 0 || copper_weight_oz <= 0)
        return 0.0;
    double length_m = plane_length_mm * 0.001;
    double width_m = plane_width_mm * 0.001;
    double thickness_m = oz_to_mm(copper_weight_oz) * 0.001;
    double rho = 1.72e-8;
    double r_dc = rho * length_m / (width_m * thickness_m);
    if (freq_hz <= 0) return r_dc;
    double mu_0 = 1.2566e-6;
    double skin_depth = sqrt(rho / (M_PI * freq_hz * mu_0));
    if (skin_depth < thickness_m && skin_depth > 0) {
        return r_dc * thickness_m / skin_depth;
    }
    return r_dc;
}

double compute_fr4_dielectric_constant(double freq_hz,
                                        double er_at_1mhz,
                                        double dispersion_coefficient) {
    if (freq_hz <= 0 || er_at_1mhz <= 0) return er_at_1mhz;
    double freq_mhz = freq_hz / 1e6;
    if (freq_mhz < 1.0) freq_mhz = 1.0;
    double er = er_at_1mhz - dispersion_coefficient * log10(freq_mhz);
    if (er < 3.0) er = 3.0;
    return er;
}
