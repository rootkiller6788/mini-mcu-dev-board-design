/**
 * @file reset_boot.c
 * @brief Reset circuit and boot mode implementation.
 */

#include "reset_boot.h"
#include <math.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

double nrst_voltage_at_time(double t_seconds, double vdd, double r_pullup,
                            double c_total, double v_init) {
    if (t_seconds < 0 || vdd <= 0 || r_pullup <= 0 || c_total < 0) return 0.0;
    double tau = r_pullup * c_total;
    if (tau <= 0) return vdd;
    double exp_term = exp(-t_seconds / tau);
    return vdd * (1.0 - exp_term) + v_init * exp_term;
}

double nrst_time_to_vih(double vdd, double vih_min, double r_pullup,
                        double c_total) {
    if (vdd <= 0 || vih_min <= 0 || r_pullup <= 0 || c_total < 0) return 0.0;
    if (vih_min >= vdd) return 1e6;
    double tau = r_pullup * c_total;
    if (tau <= 0) return 0.0;
    return -tau * log(1.0 - vih_min / vdd);
}

double minimum_reset_pulse_width(double r_pullup, double c_total,
                                 double datasheet_min_ns) {
    if (datasheet_min_ns <= 0) return 0.0;
    double tau = r_pullup * c_total;
    double t_discharge = 5.0 * tau;
    return datasheet_min_ns * 1e-9 + t_discharge;
}

int check_nrst_timing(const ResetCircuit *circuit,
                      const NRSTPinCharacteristics *nrst_pin,
                      double vdd_ramp_ms) {
    if (!circuit || !nrst_pin) return 0;
    double vdd = 3.3;
    double r_total;
    if (circuit->external_pullup_ohm > 0 && nrst_pin->internal_pullup_ohm > 0) {
        r_total = 1.0 / (1.0 / circuit->external_pullup_ohm
                       + 1.0 / nrst_pin->internal_pullup_ohm);
    } else if (circuit->external_pullup_ohm > 0) {
        r_total = circuit->external_pullup_ohm;
    } else {
        r_total = nrst_pin->internal_pullup_ohm;
    }
    double c_total = circuit->external_capacitor_f;
    double t_rise = nrst_time_to_vih(vdd, nrst_pin->vih_min, r_total, c_total);
    double t_rise_ms = t_rise * 1000.0;
    if (t_rise_ms > 20.0) return 0;
    if (t_rise_ms < 0.001) return 0;
    if (vdd_ramp_ms > 0 && (vdd_ramp_ms + t_rise_ms) > 50.0) return 0;
    return 1;
}

BootMode determine_boot_mode(int boot0_state, int boot1_state) {
    if (boot0_state == 0) return BOOT_MODE_MAIN_FLASH;
    if (boot1_state == 0) return BOOT_MODE_SYSTEM_MEM;
    return BOOT_MODE_SRAM;
}

double compute_boot0_pulldown(double vdd, double vil_max, double max_leakage_ua) {
    if (vdd <= 0 || vil_max <= 0) return 10000.0;
    double leakage = max_leakage_ua * 1e-6;
    if (leakage <= 0) return 10000.0;
    double r_max = vil_max / leakage;
    if (r_max > 100000.0) r_max = 100000.0;
    return r_max;
}

int bor_check(double vdd, double bor_threshold) {
    return (vdd > bor_threshold) ? 1 : 0;
}

double recommend_bor_threshold(double vdd_nominal, double vdd_min_worst,
                               STM32Series series) {
    double min_flash_vdd;
    switch (series) {
        case STM32_SERIES_F0: case STM32_SERIES_F1: case STM32_SERIES_F3:
            min_flash_vdd = 2.0; break;
        case STM32_SERIES_L0: case STM32_SERIES_L1:
            min_flash_vdd = 1.8; break;
        default: min_flash_vdd = 1.7; break;
    }
    double bor = min_flash_vdd + 0.2;
    if (bor > vdd_min_worst - 0.3) bor = vdd_min_worst - 0.3;
    if (bor > vdd_nominal - 0.5) bor = vdd_nominal - 0.5;
    if (bor < min_flash_vdd) bor = min_flash_vdd;
    return bor;
}


/* Additional reset / boot functions */

double compute_power_on_reset_delay(double vdd_ramp_rate_v_per_ms,
                                     double vpor_threshold,
                                     double vdd_nominal) {
    (void)vdd_nominal;
    if (vdd_ramp_rate_v_per_ms <= 0) return 0.0;
    double t_por = vpor_threshold / vdd_ramp_rate_v_per_ms;
    return t_por;
}

int check_boot_pin_conflict(int boot0_pin, int boot1_pin,
                             int used_gpio_pins[], int num_used) {
    for (int i = 0; i < num_used; i++) {
        if (boot0_pin == used_gpio_pins[i] || boot1_pin == used_gpio_pins[i])
            return 1;
    }
    return 0;
}

double compute_reset_glitch_filter_cutoff(double resistance,
                                           double capacitance) {
    if (resistance <= 0 || capacitance <= 0) return 0.0;
    return 1.0 / (2.0 * M_PI * resistance * capacitance);
}

int check_vcap_stability(double vcap_voltage, double vcap_expected,
                          double tolerance_percent) {
    if (vcap_expected <= 0) return 0;
    double deviation = fabs(vcap_voltage - vcap_expected) / vcap_expected * 100.0;
    return (deviation <= tolerance_percent) ? 1 : 0;
}
