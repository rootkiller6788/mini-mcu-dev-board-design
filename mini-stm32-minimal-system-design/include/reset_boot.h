/**
 * @file reset_boot.h
 * @brief Reset circuit design and boot mode configuration.
 * Knowledge Level: L1, L2, L4
 * Reference: STM32F103 RM0008 Sections 7.1, 2.4
 * Course mapping: Berkeley EE16A/B, Michigan EECS 411
 */
#ifndef RESET_BOOT_H
#define RESET_BOOT_H
#include "stm32_minimal_config.h"

typedef struct {
    double threshold_voltage, hysteresis_voltage, reset_timeout_ms, supply_current_ua;
    int open_drain;
} VoltageSupervisor;

typedef struct {
    double internal_pullup_ohm, vil_max, vih_min, output_low_vol, min_reset_pulse_ns;
} NRSTPinCharacteristics;

typedef struct {
    double external_pullup_ohm, external_capacitor_f, rc_time_constant;
    int has_supervisor, has_button;
    VoltageSupervisor supervisor;
    double button_debounce_ms;
} ResetCircuit;

/**
 * NRST voltage at time t during RC charging. L4: V(t)=VDD*(1-e^(-t/tau))
 */
double nrst_voltage_at_time(double t_seconds, double vdd, double r_pullup,
                            double c_total, double v_init);
/**
 * Time for NRST to reach VIH. L4: t=-tau*ln(1-V_th/VDD)
 */
double nrst_time_to_vih(double vdd, double vih_min, double r_pullup, double c_total);
double minimum_reset_pulse_width(double r_pullup, double c_total, double datasheet_min_ns);
int check_nrst_timing(const ResetCircuit *circuit,
                      const NRSTPinCharacteristics *nrst_pin, double vdd_ramp_ms);
/**
 * BOOT1:BOOT0 -> x0=Flash, 01=System, 11=SRAM
 */
BootMode determine_boot_mode(int boot0_state, int boot1_state);
double compute_boot0_pulldown(double vdd, double vil_max, double max_leakage_ua);
int bor_check(double vdd, double bor_threshold);
double recommend_bor_threshold(double vdd_nominal, double vdd_min_worst, STM32Series series);

#endif /* RESET_BOOT_H */

double compute_power_on_reset_delay(double vdd_ramp_rate_v_per_ms,
                                     double vpor_threshold, double vdd_nominal);
int check_boot_pin_conflict(int boot0_pin, int boot1_pin,
                             int used_gpio_pins[], int num_used);
double compute_reset_glitch_filter_cutoff(double resistance, double capacitance);
int check_vcap_stability(double vcap_voltage, double vcap_expected,
                          double tolerance_percent);
