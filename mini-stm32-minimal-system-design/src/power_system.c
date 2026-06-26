/**
 * @file power_system.c
 * @brief STM32 power supply system implementation.
 *
 * Knowledge Points (each function = one independent concept):
 *   L2: validate_power_spec
 *   L4: estimate_mcu_power
 *   L4: size_bulk_capacitance
 *   L4: compute_psrr_attenuation
 *   L2: ldo_headroom_check
 *   L5: ldo_required_copper_area
 *   L2: ldo_input_capacitance
 *   L4: nrst_rc_delay
 */

#include "power_system.h"
#include <math.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

int validate_power_spec(const PowerSpec *spec, STM32Series series) {
    if (!spec) return 0;
    double vmin, vmax;
    switch (series) {
        case STM32_SERIES_F0: case STM32_SERIES_F1: case STM32_SERIES_F3:
            vmin = 2.0; vmax = 3.6; break;
        case STM32_SERIES_F2: case STM32_SERIES_F4:
            vmin = 1.8; vmax = 3.6; break;
        case STM32_SERIES_F7: case STM32_SERIES_H7:
            vmin = 1.7; vmax = 3.6; break;
        case STM32_SERIES_L0: case STM32_SERIES_L1:
            vmin = 1.8; vmax = 3.6; break;
        case STM32_SERIES_L4: case STM32_SERIES_L5:
            vmin = 1.71; vmax = 3.6; break;
        case STM32_SERIES_G0: case STM32_SERIES_G4:
            vmin = 1.7; vmax = 3.6; break;
        case STM32_SERIES_U5: case STM32_SERIES_WB: case STM32_SERIES_WL:
            vmin = 1.71; vmax = 3.6; break;
        default: vmin = 1.7; vmax = 3.6; break;
    }
    if (spec->domain == VOLTAGE_DOMAIN_VDDA) {
        if (spec->nominal_voltage < vmin || spec->nominal_voltage > vmax) return 0;
    } else if (spec->domain == VOLTAGE_DOMAIN_VBAT) {
        if (spec->nominal_voltage < 1.8 || spec->nominal_voltage > 3.6) return 0;
    } else if (spec->domain == VOLTAGE_DOMAIN_VDD_USB) {
        if (spec->nominal_voltage < 3.0 || spec->nominal_voltage > 3.6) return 0;
    } else {
        if (spec->nominal_voltage < vmin || spec->nominal_voltage > vmax) return 0;
    }
    if (spec->max_current <= 0) return 0;
    if (spec->requires_filtering && spec->ripple_tolerance > 0.050) return 0;
    return 1;
}

double estimate_mcu_power(double vdd, double core_freq_hz, int peripheral_on) {
    if (vdd <= 0 || core_freq_hz <= 0) return 0.0;
    double i_dyn_per_mhz = 0.00028;
    double i_dyn = i_dyn_per_mhz * (core_freq_hz / 1e6);
    double i_static = 0.005;
    double i_periph = peripheral_on * 0.0003;
    double i_total = i_static + i_dyn + i_periph;
    return vdd * i_total;
}

double size_bulk_capacitance(double step_current_a, double regulator_resp_us,
                             double max_droop_mv) {
    if (step_current_a <= 0 || regulator_resp_us <= 0 || max_droop_mv <= 0)
        return 0.0;
    double t_response = regulator_resp_us * 1e-6;
    double v_droop = max_droop_mv * 1e-3;
    double c_ideal = step_current_a * t_response / v_droop;
    return c_ideal * 1.5;
}

double compute_psrr_attenuation(double psrr_db, double filter_cap_f,
                                double filter_ferrite_ohm, double target_freq_hz) {
    if (filter_cap_f <= 0 || target_freq_hz <= 0) return psrr_db;
    double omega = 2.0 * M_PI * target_freq_hz;
    double z_cap = 1.0 / (omega * filter_cap_f);
    double filter_atten = 20.0 * log10(z_cap / (z_cap + filter_ferrite_ohm));
    return psrr_db + filter_atten;
}

int ldo_headroom_check(const LDOSpec *ldo, double vin_min) {
    if (!ldo) return 0;
    if (vin_min <= 0 || ldo->output_voltage <= 0) return 0;
    double required_vin = ldo->output_voltage + ldo->dropout_voltage;
    if (ldo->input_voltage > 0 && vin_min > ldo->input_voltage) return 0;
    return (vin_min >= required_vin) ? 1 : 0;
}

double ldo_required_copper_area(const LDOSpec *ldo, double load_current,
                                double ambient_temp, double max_junction) {
    if (!ldo || load_current <= 0) return 0.0;
    double p_diss = (ldo->input_voltage - ldo->output_voltage) * load_current
                  + ldo->input_voltage * ldo->quiescent_current;
    double max_temp_rise = max_junction - ambient_temp;
    if (max_temp_rise <= 0) return 0.0;
    double theta_ja_needed = max_temp_rise / p_diss;
    double theta_ja_bare = 140.0;
    if (theta_ja_needed >= theta_ja_bare) return 10.0;
    double theta_diff = theta_ja_bare - theta_ja_needed;
    double area_mm2 = (theta_diff / 3.5) * (theta_diff / 3.5);
    if (area_mm2 < 25.0) area_mm2 = 25.0;
    if (area_mm2 > 2500.0) area_mm2 = 2500.0;
    return area_mm2;
}

double ldo_input_capacitance(const LDOSpec *ldo, int pre_reg_switching,
                             double pre_reg_freq_hz, double load_current_a) {
    if (!ldo) return 1.0e-6;
    if (pre_reg_switching && pre_reg_freq_hz > 0) {
        double v_ripple_target = 0.050;
        double c_min = load_current_a / (pre_reg_freq_hz * v_ripple_target);
        if (c_min < 4.7e-6) c_min = 4.7e-6;
        return c_min;
    }
    return 1.0e-6;
}

double nrst_rc_delay(double pullup_ohm, double cap_farad,
                     double vdd, double vth) {
    if (pullup_ohm <= 0 || cap_farad <= 0 || vdd <= 0) return 0.0;
    if (vth <= 0) return 0.0;
    double tau = pullup_ohm * cap_farad;
    if (vth >= vdd) return tau * 10.0;
    double ratio = 1.0 - (vth / vdd);
    if (ratio <= 0) return tau * 10.0;
    return -tau * log(ratio);
}


/* Additional Power System functions */

/*
 * estimate_vdd_rail_voltage
 * L4: Ohm's Law + power supply output impedance.
 * Computes voltage at the VDD pin considering trace IR drop.
 * V_load = V_supply - I_load * (R_trace + R_connector)
 * This is critical for verifying the MCU sees adequate voltage.
 */
double estimate_vdd_rail_voltage(double v_supply, double i_load,
                                  double r_trace, double r_connector) {
    return v_supply - i_load * (r_trace + r_connector);
}

/*
 * compute_power_efficiency
 * L4: Efficiency = P_out / P_in = (V_out * I_out) / (V_in * I_in)
 * For LDO: I_in ~ I_out + I_q, so efficiency ~ V_out / V_in
 * For DC-DC: efficiency can reach 85-95 percent.
 * LDO efficiency drops dramatically when V_in >> V_out.
 */
double compute_power_efficiency(double v_in, double i_in,
                                 double v_out, double i_out) {
    if (v_in <= 0 || i_in <= 0) return 0.0;
    double p_in = v_in * i_in;
    double p_out = v_out * i_out;
    if (p_in <= 0) return 0.0;
    return p_out / p_in;
}

/*
 * compute_inrush_current
 * L4: Charging current into bulk capacitance at power-on.
 * I_inrush = C_bulk * dV/dt (during power ramp)
 * Peak current limited by regulator current limit or trace resistance.
 * Excessive inrush can cause voltage sag or trigger overcurrent protection.
 */
double compute_inrush_current(double bulk_capacitance,
                               double voltage_rise_time_s,
                               double v_nominal) {
    if (bulk_capacitance <= 0 || voltage_rise_time_s <= 0) return 0.0;
    return bulk_capacitance * v_nominal / voltage_rise_time_s;
}

/*
 * compute_ripple_voltage
 * L4: Output ripple from capacitor and load.
 * For a switching regulator: V_ripple = I_load / (f_sw * C_out)
 * Plus ESR contribution: V_ripple_ESR = I_ripple * ESR
 * Total ripple = sqrt(V_ripple_cap^2 + V_ripple_ESR^2) (approx)
 * For linear regulator: ripple determined by PSRR and input ripple.
 */
double compute_ripple_voltage(double load_current, double switching_freq,
                               double output_cap, double esr) {
    if (output_cap <= 0) return 1e9;
    double v_ripple_cap = 0.0;
    if (switching_freq > 0) {
        v_ripple_cap = load_current / (switching_freq * output_cap);
    }
    double v_ripple_esr = load_current * esr;
    return sqrt(v_ripple_cap * v_ripple_cap + v_ripple_esr * v_ripple_esr);
}

/*
 * compute_vbat_lifetime
 * L5: Battery life estimation for VBAT domain.
 * VBAT powers RTC + backup registers during power-off.
 * RTC current: ~1-3 uA (LSI) or ~0.5-1 uA (LSE)
 * Coin cell capacity: CR2032 = ~225 mAh
 * Lifetime = Capacity / I_load
 * With 1uA load: 225 mAh / 0.001 mA = 225,000 hours = ~25 years
 * Self-discharge dominates for very low loads.
 */
double compute_vbat_lifetime(double battery_capacity_mah,
                              double load_current_ua,
                              double self_discharge_percent_per_year) {
    if (load_current_ua <= 0) return 1e9;
    double i_load_ma = load_current_ua * 0.001;
    double lifetime_hours = battery_capacity_mah / i_load_ma;
    double lifetime_years = lifetime_hours / (365.25 * 24.0);
    if (self_discharge_percent_per_year > 0) {
        double self_discharge_factor = 1.0 - self_discharge_percent_per_year / 100.0;
        if (self_discharge_factor > 0) {
            lifetime_years = lifetime_years * self_discharge_factor;
        }
    }
    return lifetime_years;
}

/*
 * compute_decoupling_impedance_vs_frequency
 * L5: Impedance of a single capacitor across a frequency sweep.
 * Returns |Z| at the given frequency, handling the transition
 * from capacitive to inductive behavior at SRF.
 * This is the fundamental building block of PDN simulation.
 */
double compute_decoupling_impedance_vs_frequency(double capacitance,
                                                   double esr, double esl,
                                                   double freq_hz) {
    if (capacitance <= 0 || freq_hz <= 0) return 1e9;
    double omega = 2.0 * M_PI * freq_hz;
    double xl = omega * esl;
    double xc = 1.0 / (omega * capacitance);
    double reactance = xl - xc;
    return sqrt(esr * esr + reactance * reactance);
}
