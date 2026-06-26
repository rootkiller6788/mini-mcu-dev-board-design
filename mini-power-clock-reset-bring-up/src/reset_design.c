#include "reset_design.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

/* ===================================================================
 * L2 Core Concepts Implementation
 * =================================================================== */

double por_delay_seconds(double r_ohm, double c_farad, double v_cc, double v_threshold)
{
    if (r_ohm <= 0.0 || c_farad <= 0.0 || v_cc <= 0.0 || v_threshold <= 0.0) return 0.0;
    if (v_threshold >= v_cc) return 1e9;
    return -r_ohm * c_farad * log(1.0 - v_threshold / v_cc);
}

reset_source_t decode_reset_source(uint32_t status_register)
{
    if (status_register & (1u << 26)) return RESET_SRC_LOW_POWER;
    if (status_register & (1u << 24)) return RESET_SRC_WATCHDOG;
    if (status_register & (1u << 23)) return RESET_SRC_SOFTWARE;
    if (status_register & (1u << 19)) return RESET_SRC_EXTERNAL;
    if (status_register & (1u << 18)) return RESET_SRC_BOR;
    if (status_register & (1u << 17)) return RESET_SRC_POR;
    return RESET_SRC_UNKNOWN;
}

int supervisor_compatibility_check(const supervisor_ic_t* supervisor,
                                    double mcu_bor_threshold_mV, double mcu_min_reset_pulse_us)
{
    if (!supervisor) return -1;
    double sup_threshold_mV = supervisor->threshold_V * 1000.0;
    if (sup_threshold_mV < mcu_bor_threshold_mV) return -1;
    if (supervisor->reset_timeout_ms * 1000.0 < mcu_min_reset_pulse_us) return -2;
    return 0;
}

int multi_rail_reset_sequence(const multi_rail_reset_state_t* state)
{
    if (!state) return 0;
    return state->all_ok;
}

void reset_timing_calculate(double r_reset_ohm, double c_reset_farad,
                             const supervisor_ic_t* supervisor, double mcu_recovery_us,
                             reset_timing_t* timing)
{
    if (!timing) return;
    memset(timing, 0, sizeof(*timing));
    double rc_delay_us = 0.0;
    if (r_reset_ohm > 0.0 && c_reset_farad > 0.0 && supervisor) {
        rc_delay_us = -r_reset_ohm * c_reset_farad *
                      log(1.0 - supervisor->threshold_V / 3.3) * 1e6;
    }
    timing->assertion_delay_us = rc_delay_us;
    timing->hold_time_us = supervisor ? supervisor->reset_timeout_ms * 1000.0 : 100.0;
    timing->release_rise_time_us = 10.0;
    timing->recovery_time_us = mcu_recovery_us;
    timing->total_reset_time_us = rc_delay_us + timing->hold_time_us +
                                  timing->release_rise_time_us + mcu_recovery_us;
}

/* ===================================================================
 * L3 Mathematical Structures Implementation
 * =================================================================== */

double rc_charge_voltage(double v_cc, double r_ohm, double c_farad, double t_seconds)
{
    if (r_ohm <= 0.0 || c_farad <= 0.0) return (t_seconds >= 0.0) ? v_cc : 0.0;
    double tau = r_ohm * c_farad;
    return v_cc * (1.0 - exp(-t_seconds / tau));
}

double rc_discharge_voltage(double v0, double r_ohm, double c_farad, double t_seconds)
{
    if (r_ohm <= 0.0 || c_farad <= 0.0) return 0.0;
    double tau = r_ohm * c_farad;
    return v0 * exp(-t_seconds / tau);
}

double rc_time_to_voltage(double r_ohm, double c_farad, double v_cc, double v_target)
{
    if (r_ohm <= 0.0 || c_farad <= 0.0) return 0.0;
    if (v_target >= v_cc) return 1e9;
    if (v_target <= 0.0) return 0.0;
    return -r_ohm * c_farad * log(1.0 - v_target / v_cc);
}

double watchdog_period_us(int prescaler, int reload, double wdg_clock_Hz)
{
    if (wdg_clock_Hz <= 0.0 || prescaler <= 0 || reload < 0) return 0.0;
    return (prescaler * (reload + 1)) / wdg_clock_Hz * 1e6;
}

void comparator_hysteresis(double threshold_V, double hysteresis_mV,
                           double* v_high, double* v_low)
{
    double half_hyst = hysteresis_mV * 0.001 * 0.5;
    if (v_high) *v_high = threshold_V + half_hyst;
    if (v_low) *v_low = threshold_V - half_hyst;
}

/* ===================================================================
 * L4 Fundamental Laws Implementation
 * =================================================================== */

double exponential_decay(double v0, double tau, double t)
{
    if (tau <= 0.0) return (t >= 0.0) ? 0.0 : v0;
    return v0 * exp(-t / tau);
}

double threshold_crossing_time(double v_start, double v_target, double tau, int charging)
{
    if (tau <= 0.0) return 0.0;
    if (charging) {
        if (v_target >= v_start) return 1e9;
        return -tau * log(1.0 - v_target / v_start);
    } else {
        if (v_start <= 0.0 || v_target >= v_start) return 0.0;
        return -tau * log(v_target / v_start);
    }
}

/* ===================================================================
 * L5 Algorithms Implementation
 * =================================================================== */

int debounce_button(int raw_input, int* counter, int debounce_limit)
{
    if (!counter) return 0;
    if (raw_input) {
        if (*counter < debounce_limit) (*counter)++;
    } else {
        if (*counter > 0) (*counter)--;
    }
    return (*counter >= debounce_limit) ? 1 : 0;
}

double watchdog_optimal_timeout(double longest_task_ms, double tick_period_ms, double safety_margin)
{
    double base = longest_task_ms > (10.0 * tick_period_ms) ? longest_task_ms : (10.0 * tick_period_ms);
    return base * 2.0 * safety_margin;
}

void reset_log_record(reset_log_entry_t* log_buffer, int buffer_size,
                      int* write_index, reset_source_t source,
                      uint32_t timestamp_ms, double vdd_V)
{
    if (!log_buffer || !write_index || buffer_size <= 0) return;
    int idx = *write_index;
    log_buffer[idx].source = source;
    log_buffer[idx].timestamp_ms = timestamp_ms;
    log_buffer[idx].vdd_at_reset_V = vdd_V;
    *write_index = (idx + 1) % buffer_size;
}

void power_good_aggregate(const int* rail_pg, int num_rails,
                          int* global_pgood, int* first_failing_rail)
{
    if (global_pgood) *global_pgood = 1;
    if (first_failing_rail) *first_failing_rail = -1;
    if (!rail_pg || num_rails <= 0) return;
    for (int i = 0; i < num_rails; i++) {
        if (!rail_pg[i]) {
            if (global_pgood) *global_pgood = 0;
            if (first_failing_rail && *first_failing_rail < 0) *first_failing_rail = i;
            break;
        }
    }
}

/* ===================================================================
 * L6 Canonical Problems Implementation
 * =================================================================== */

int stm32f4_reset_config(double bor_threshold_V, int enable_iwdg,
                          double iwdg_timeout_ms, int enable_wwdg,
                          double wwdg_timeout_ms, double wwdg_window_ms)
{
    int faults = 0;
    double valid_bor[] = {2.1, 2.4, 2.7, 3.0};
    int bor_ok = 0;
    for (int i = 0; i < 4; i++) {
        if (fabs(bor_threshold_V - valid_bor[i]) < 0.05) { bor_ok = 1; break; }
    }
    if (!bor_ok) faults |= (1 << 0);
    if (enable_iwdg && iwdg_timeout_ms > 32768.0) faults |= (1 << 1);
    if (enable_wwdg && wwdg_timeout_ms > 100.0) faults |= (1 << 2);
    if (enable_wwdg && wwdg_window_ms >= wwdg_timeout_ms) faults |= (1 << 3);
    return faults;
}

reset_source_t stm32_reset_cause_get(uint32_t rcc_csr)
{
    if (rcc_csr & (1u << 27)) return RESET_SRC_LOW_POWER;
    if (rcc_csr & (1u << 26)) return RESET_SRC_WATCHDOG;
    if (rcc_csr & (1u << 21)) return RESET_SRC_SOFTWARE;
    if (rcc_csr & (1u << 20)) return RESET_SRC_EXTERNAL;
    if (rcc_csr & (1u << 19)) return RESET_SRC_BOR;
    if (rcc_csr & (1u << 18)) return RESET_SRC_POR;
    return RESET_SRC_UNKNOWN;
}

reset_source_t nrf52_reset_cause_get(uint32_t resetreas)
{
    if (resetreas & (1u << 4)) return RESET_SRC_LOW_POWER;
    if (resetreas & (1u << 3)) return RESET_SRC_SOFTWARE;
    if (resetreas & (1u << 2)) return RESET_SRC_WATCHDOG;
    if (resetreas & (1u << 1)) return RESET_SRC_BOR;
    if (resetreas & (1u << 0)) return RESET_SRC_EXTERNAL;
    return RESET_SRC_UNKNOWN;
}

/* ===================================================================
 * L7 Applications Implementation
 * =================================================================== */

int automotive_reset_architecture(int num_independent_reset_paths,
                                  int enable_sbc_watchdog, int enable_external_watchdog,
                                  double sbc_timeout_ms, double ext_wdg_timeout_ms)
{
    int asil_level = 0;
    if (num_independent_reset_paths >= 2 && enable_sbc_watchdog && enable_external_watchdog)
        asil_level = 3;
    else if (num_independent_reset_paths >= 1 && (enable_sbc_watchdog || enable_external_watchdog))
        asil_level = 2;
    else if (num_independent_reset_paths >= 1)
        asil_level = 1;
    (void)sbc_timeout_ms; (void)ext_wdg_timeout_ms;
    return asil_level;
}

int plc_watchdog_validate(double timeout_ms, double max_scan_time_ms,
                          double relay_response_ms, int* safe_state_guaranteed)
{
    double total_detect_time = max_scan_time_ms + relay_response_ms;
    int ok = (total_detect_time <= timeout_ms) ? 1 : 0;
    if (safe_state_guaranteed) *safe_state_guaranteed = ok;
    return ok ? 0 : -1;
}

/* ===================================================================
 * L8 Advanced Topics Implementation
 * =================================================================== */

double sil_reset_diagnostic_coverage(int num_channels, int num_comparators,
                                      int num_watchdogs, int enable_diverse_wdg)
{
    double dc = 0.60;
    if (num_channels >= 2) dc += 0.15;
    if (num_comparators >= 1) dc += 0.05;
    if (num_watchdogs >= 2) dc += 0.10;
    if (enable_diverse_wdg) dc += 0.08;
    if (dc > 0.99) dc = 0.99;
    return dc;
}

void dual_watchdog_check(int wdg1_timeout, int wdg2_timeout, int wdg1_ok,
                         int wdg2_ok, int* system_ok, int* wdg_disagree)
{
    int ok = wdg1_ok || wdg2_ok;
    int disagree = (wdg1_ok != wdg2_ok) ? 1 : 0;
    if (system_ok) *system_ok = ok;
    if (wdg_disagree) *wdg_disagree = disagree;
    (void)wdg1_timeout; (void)wdg2_timeout;
}
