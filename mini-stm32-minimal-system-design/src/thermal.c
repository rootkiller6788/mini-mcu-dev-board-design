/**
 * @file thermal.c
 * @brief Thermal management implementation.
 */

#include "thermal.h"
#include <math.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

double junction_temperature(double ambient_c, double power_w, double theta_ja) {
    return ambient_c + power_w * theta_ja;
}

double max_power_dissipation(double tj_max, double ambient_c, double theta_ja) {
    if (theta_ja <= 0) return 1e9;
    double max_p = (tj_max - ambient_c) / theta_ja;
    return max_p > 0 ? max_p : 0.0;
}

double required_copper_area(double pkg_theta_ja_bare, double target_theta_ja,
                            int num_layers) {
    if (target_theta_ja >= pkg_theta_ja_bare) return 0.0;
    double theta_reduction = pkg_theta_ja_bare - target_theta_ja;
    if (theta_reduction <= 0) return 0.0;
    double layer_factor = (num_layers >= 4) ? 1.5 : 1.0;
    double area = pow(50.0 / (theta_reduction * layer_factor), 2.0);
    if (area < 25.0) area = 25.0;
    if (area > 5000.0) area = 5000.0;
    return area;
}

double effective_theta_ja(const ThermalResistance *thermal,
                          const HeatsinkSpec *heatsink,
                          double tim_resistance) {
    if (!thermal) return 100.0;
    if (!heatsink || heatsink->thermal_resistance_cw <= 0) {
        return thermal->theta_ja;
    }
    double theta_sa = heatsink->thermal_resistance_cw;
    if (heatsink->has_fan && heatsink->airflow_cfm > 0) {
        double flow_factor = 1.0 / sqrt(heatsink->airflow_cfm);
        theta_sa *= flow_factor;
        if (theta_sa < 1.0) theta_sa = 1.0;
    }
    return thermal->theta_jc + tim_resistance + theta_sa;
}

double board_temp_rise(double power_w, double copper_area_mm2, int num_layers) {
    if (power_w <= 0 || copper_area_mm2 <= 0) return 0.0;
    double effective_area = copper_area_mm2 * (double)num_layers;
    double theta_ba = 100.0 / sqrt(effective_area);
    return power_w * theta_ba;
}

double thermal_via_resistance(int num_vias, double via_diameter_mm,
                              double via_wall_thickness_um,
                              double board_thickness_mm) {
    if (num_vias <= 0 || via_diameter_mm <= 0
        || via_wall_thickness_um <= 0 || board_thickness_mm <= 0)
        return 1e9;
    double k_cu = 385.0;
    double d_m = via_diameter_mm * 0.001;
    double t_m = via_wall_thickness_um * 1e-6;
    double l_m = board_thickness_mm * 0.001;
    double a_wall = M_PI * d_m * t_m;
    if (a_wall <= 0) return 1e9;
    double theta_per_via = l_m / (k_cu * a_wall);
    return theta_per_via / (double)num_vias;
}

void full_thermal_analysis(double power_w, double ambient_c, double tj_max,
                           double pkg_theta_jc, double pkg_theta_ja_min,
                           double copper_area_mm2, int num_layers,
                           const HeatsinkSpec *heatsink, ThermalPoint *result) {
    if (!result) return;
    memset(result, 0, sizeof(*result));
    result->ambient_temp_c = ambient_c;
    result->power_dissipation_w = power_w;
    ThermalResistance thermal;
    thermal.theta_jc = pkg_theta_jc;
    thermal.theta_ja = pkg_theta_ja_min;
    if (copper_area_mm2 > 0) {
        double copper_ja = pkg_theta_ja_min - 50.0 / sqrt(copper_area_mm2);
        if (copper_ja < 20.0) copper_ja = 20.0;
        thermal.theta_ja = copper_ja;
    }
    double tim_r = 1.0;
    double theta_ja_eff = effective_theta_ja(&thermal, heatsink, tim_r);
    result->junction_temp_c = junction_temperature(ambient_c, power_w, theta_ja_eff);
    double dt_board = board_temp_rise(power_w * 0.7, copper_area_mm2, num_layers);
    result->board_temp_c = ambient_c + dt_board;
    result->case_temp_c = result->junction_temp_c - power_w * pkg_theta_jc;
    if (result->case_temp_c < ambient_c) result->case_temp_c = ambient_c;
    result->margin_c = tj_max - result->junction_temp_c;
    result->safe = (result->margin_c > 0) ? 1 : 0;
}


/* Additional thermal functions */

double compute_theta_ja_from_layout(double theta_jc, double copper_area_mm2,
                                     int num_layers, int num_thermal_vias) {
    double base_ja = 60.0;
    if (copper_area_mm2 > 0) {
        base_ja -= 40.0 * (1.0 - exp(-copper_area_mm2 / 200.0));
    }
    if (num_layers >= 4) base_ja *= 0.75;
    if (num_thermal_vias > 0) {
        base_ja -= 5.0 * log((double)num_thermal_vias + 1.0);
    }
    if (base_ja < theta_jc + 5.0) base_ja = theta_jc + 5.0;
    return base_ja;
}

double compute_ambient_derating(double tj_max, double theta_ja,
                                 double power_w, double safety_margin_c) {
    if (theta_ja <= 0) return tj_max;
    double t_j = power_w * theta_ja;
    return tj_max - t_j - safety_margin_c;
}

int check_thermal_throttling_required(double junction_temp_c,
                                       double throttle_threshold_c) {
    return (junction_temp_c >= throttle_threshold_c) ? 1 : 0;
}
