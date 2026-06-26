/**
 * @file reset_design.h
 * @brief MCU Development Board - Reset Circuit & Supervisory Design (L1-L8)
 *
 * Knowledge Coverage:
 *   L1: POR, BOR, manual reset, watchdog timer, supervisor IC, reset vector
 *   L2: reset sequencing for multi-rail, glitch filtering, bidirectional reset
 *   L3: RC reset timing (tau), comparator hysteresis, watchdog timeout math
 *   L4: RC charge/discharge exponential law, threshold detection
 *   L5: debounce algorithm, watchdog timeout configuration, reset source identification
 *   L6: STM32 reset tree (nRST, NRST pin, system reset), nRF52 reset behavior
 *   L7: automotive ECU reset architecture, industrial PLC watchdog design
 *   L8: IEC 61508 safety integrity reset, dual-redundant watchdog
 *   L9: AI-based anomaly detection reset (documented)
 *
 * Reference: STM32 Reference Manual RM0090 Ch.6, TI Supervisor IC Guide
 *            IEC 61508 Functional Safety Standard
 */

#ifndef RESET_DESIGN_H
#define RESET_DESIGN_H
#include <stdint.h>
#include <stddef.h>
#include <math.h>
#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * L1 Definitions
 * ======================================================================== */

/** Reset types in an MCU system.
 *  POR: Power-On Reset (first power application).
 *  BOR: Brown-Out Reset (supply dips below threshold).
 *  EXTERNAL: nRST pin assertion.
 *  WATCHDOG: Independent/IWDG or Window/WWDG timeout.
 *  SOFTWARE: CPU NVIC SYSRESETREQ.
 *  LOW_POWER: Wakeup from standby/shutdown. */
typedef enum {
    RESET_SRC_UNKNOWN = 0,
    RESET_SRC_POR      = 1,
    RESET_SRC_BOR      = 2,
    RESET_SRC_EXTERNAL = 3,
    RESET_SRC_WATCHDOG = 4,
    RESET_SRC_SOFTWARE = 5,
    RESET_SRC_LOW_POWER = 6,
    RESET_SRC_DEBUG    = 7,
    RESET_SRC_TAMPER   = 8
} reset_source_t;

/** Supervisor / voltage monitor IC parameters.
 *  threshold_V: monitored voltage threshold.
 *  hysteresis_mV: prevents oscillation near threshold.
 *  reset_timeout_ms: minimum reset assertion duration.
 *  manual_reset_debounce_ms: debounce time for manual reset input. */
typedef struct {
    double threshold_V, hysteresis_mV;
    double reset_timeout_ms, manual_reset_debounce_ms;
    double power_on_delay_ms;
    char   part_number[32];
} supervisor_ic_t;

/** Watchdog timer configuration.
 *  timeout_ms: watchdog period.
 *  window_ms: window watchdog open period (0=standard).
 *  early_warning_ms: time before timeout to trigger NMI.
 *  clock_source: 0=internal LSI, 1=LSE, 2=PCLK. */
typedef struct {
    double timeout_ms, window_ms, early_warning_ms;
    int    clock_source;
    int    enable_hardware_wdg;
} watchdog_config_t;

/** Reset timing sequence specification.
 *  Defines the complete reset waveform: assertion, hold, release, recovery. */
typedef struct {
    double assertion_delay_us;
    double hold_time_us;
    double release_rise_time_us;
    double recovery_time_us;
    double total_reset_time_us;
} reset_timing_t;

/** Multi-rail reset sequencer state.
 *  Tracks which rails have been reset and their timing.
 *  Used for coordinating complex multi-rail MCU resets. */
typedef struct {
    int    num_rails;
    int    rails_reset_ok[8];
    double rail_ready_timestamp_ms[8];
    int    all_ok;
} multi_rail_reset_state_t;

/** Glitch filter configuration.
 *  Rejects reset pulses shorter than filter_time_ns.
 *  Prevents spurious resets from noise on nRST line. */
typedef struct {
    double filter_time_ns;
    int    enabled;
    double max_glitch_energy_pJ;
} glitch_filter_config_t;

/* ========================================================================
 * L2 Core Concepts
 * ======================================================================== */

/** Compute RC power-on reset delay.
 *  V(t) = Vcc*(1 - exp(-t/RC)). Delay = -RC * ln(1 - V_th/Vcc).
 *  When V(t) reaches supervisor threshold, reset is released. */
double por_delay_seconds(double r_ohm, double c_farad, double v_cc, double v_threshold);

/** Determine reset source from status register bits.
 *  Decodes STM32 RCC_CSR register flags into reset_source_t enum.
 *  Flags: PINRSTF, PORRSTF, SFTRSTF, IWDGRSTF, WWDGRSTF, LPWRRSTF. */
reset_source_t decode_reset_source(uint32_t status_register);

/** Check if supervisor IC parameters meet MCU requirements.
 *  Supervisor threshold must be above MCU BOR threshold.
 *  Reset timeout must exceed MCU minimum reset pulse width.
 *  @return 0 if compatible, -1 if threshold too low, -2 if timeout too short */
int supervisor_compatibility_check(const supervisor_ic_t* supervisor,
                                    double mcu_bor_threshold_mV,
                                    double mcu_min_reset_pulse_us);

/** Verify reset sequencing for multi-rail system.
 *  Each rail must be reset in correct order with proper timing.
 *  Typically: core voltage before I/O voltage for safe power-up. */
int multi_rail_reset_sequence(const multi_rail_reset_state_t* state);

/** Generate complete reset timing from component values and IC specs.
 *  Combines: RC delay + supervisor timeout + MCU recovery time. */
void reset_timing_calculate(double r_reset_ohm, double c_reset_farad,
                             const supervisor_ic_t* supervisor,
                             double mcu_recovery_us, reset_timing_t* timing);

/* ========================================================================
 * L3 Mathematical Structures
 * ======================================================================== */

/** RC charge curve voltage at time t.
 *  V(t) = Vcc * (1 - exp(-t / (R*C))).
 *  Fundamental for POR/BOR timing calculations. */
double rc_charge_voltage(double v_cc, double r_ohm, double c_farad, double t_seconds);

/** RC discharge curve voltage at time t.
 *  V(t) = V0 * exp(-t / (R*C)).
 *  Used for reset assertion timing and power-down sequencing. */
double rc_discharge_voltage(double v0, double r_ohm, double c_farad, double t_seconds);

/** Time for RC circuit to reach target voltage (charging).
 *  t = -R*C * ln(1 - V_target/Vcc).
 *  Used to compute reset assertion delay timing. */
double rc_time_to_voltage(double r_ohm, double c_farad, double v_cc, double v_target);

/** Watchdog timer period from prescaler and reload value.
 *  T_wdg = (prescaler * (reload + 1)) / f_wdg_clock.
 *  STM32 IWDG: f_LSI=32kHz, prescaler=4-256, reload=0-4095. */
double watchdog_period_us(int prescaler, int reload, double wdg_clock_Hz);

/** Comparator hysteresis band calculation.
 *  V_high = V_th + V_hyst/2. V_low = V_th - V_hyst/2.
 *  Hysteresis prevents oscillation when Vin ~= V_threshold. */
void comparator_hysteresis(double threshold_V, double hysteresis_mV,
                           double* v_high, double* v_low);

/* ========================================================================
 * L4 Fundamental Laws
 * ======================================================================== */

/** Exponential decay law applied to reset timing.
 *  V(t) = V0 * exp(-t/tau). This is the fundamental physical law
 *  governing all RC-based reset circuits.
 *  @return voltage at time t */
double exponential_decay(double v0, double tau, double t);

/** Threshold crossing detection: time when V(t) crosses V_threshold.
 *  For charging: t_cross = -tau * ln(1 - V_th/Vcc).
 *  For discharging: t_cross = -tau * ln(V_th/V0).
 *  This is the mathematical foundation of all reset timing. */
double threshold_crossing_time(double v_start, double v_target, double tau, int charging);

/* ========================================================================
 * L5 Algorithms
 * ======================================================================== */

/** Button debounce algorithm (state machine with integration).
 *  Uses a counter that increments when input is high, decrements when low.
 *  Output changes only when counter reaches limit or zero.
 *  Rejects glitches shorter than (debounce_time / sample_rate) in duration. */
int debounce_button(int raw_input, int* counter, int debounce_limit);

/** Watchdog timer optimal timeout computation.
 *  Balances: too short = false resets, too long = slow fault detection.
 *  T_wdg_optimal = max(2 * longest_task_time, 10 * tick_period).
 *  Safety margin factor: 1.5-2x for industrial, 3x for consumer. */
double watchdog_optimal_timeout(double longest_task_ms, double tick_period_ms,
                                 double safety_margin);

/** Reset source logging with timestamp.
 *  Maintains a circular buffer of recent reset events for diagnostics.
 *  Each entry records: source, timestamp, supply voltage at reset.
 *  Useful for field failure analysis and reliability tracking. */
typedef struct {
    reset_source_t source;
    uint32_t       timestamp_ms;
    double         vdd_at_reset_V;
} reset_log_entry_t;

void reset_log_record(reset_log_entry_t* log_buffer, int buffer_size,
                      int* write_index, reset_source_t source,
                      uint32_t timestamp_ms, double vdd_V);

/** Multi-rail power-good aggregation.
 *  All PG signals must be high for global PGOOD assertion.
 *  Individual rail status tracked for diagnostics. */
void power_good_aggregate(const int* rail_pg, int num_rails,
                          int* global_pgood, int* first_failing_rail);

/* ========================================================================
 * L6 Canonical Problems
 * ======================================================================== */

/** STM32F4 reset tree configuration.
 *  Models: VDD->POR/BOR->nRST pin->System Reset->Peripheral Resets.
 *  Configurable BOR levels: 2.1V, 2.4V, 2.7V, 3.0V. */
int stm32f4_reset_config(double bor_threshold_V, int enable_iwdg,
                          double iwdg_timeout_ms, int enable_wwdg,
                          double wwdg_timeout_ms, double wwdg_window_ms);

/** STM32 reset cause identification from RCC_CSR.
 *  Reads hardware status register to determine what triggered
 *  the most recent reset. Clears flags after reading.
 *  @return reset_source_t enum value */
reset_source_t stm32_reset_cause_get(uint32_t rcc_csr);

/** nRF52 reset behavior analysis.
 *  nRF52840 has multiple reset sources: pin reset, power-on, brown-out,
 *  watchdog, soft reset, CPU lockup, debug interface.
 *  RESETREAS register records the source. */
reset_source_t nrf52_reset_cause_get(uint32_t resetreas);

/* ========================================================================
 * L7 Applications
 * ======================================================================== */

/** Automotive ECU reset architecture (ISO 26262).
 *  Multi-stage: PMIC supervisor -> SBC (System Basis Chip) -> MCU.
 *  SBC provides: watchdog, fail-safe state machine, wake-up control.
 *  Redundant reset paths for ASIL B/C/D compliance. */
int automotive_reset_architecture(int num_independent_reset_paths,
                                  int enable_sbc_watchdog,
                                  int enable_external_watchdog,
                                  double sbc_timeout_ms,
                                  double ext_wdg_timeout_ms);

/** Industrial PLC watchdog design (IEC 61131-2).
 *  Requirements: relay output must go to safe state on watchdog timeout.
 *  Typical: 50-200ms timeout, 1-10ms window for refresh. */
int plc_watchdog_validate(double timeout_ms, double max_scan_time_ms,
                          double relay_response_ms, int* safe_state_guaranteed);

/* ========================================================================
 * L8 Advanced Topics
 * ======================================================================== */

/** IEC 61508 Safety Integrity Level (SIL) reset architecture.
 *  SIL2: single-channel with diagnostics.
 *  SIL3: dual-channel with comparison (1oo2 architecture).
 *  Diagnostic coverage must meet SIL requirements.
 *  @return Diagnostic coverage ratio (0.0-1.0) */
double sil_reset_diagnostic_coverage(int num_channels, int num_comparators,
                                      int num_watchdogs, int enable_diverse_wdg);

/** Dual-redundant watchdog with cross-check.
 *  Two independent watchdogs monitor the same system.
 *  Output relay de-energizes only if BOTH watchdogs timeout.
 *  This provides fault tolerance (1oo2) for watchdog failures. */
void dual_watchdog_check(int wdg1_timeout, int wdg2_timeout, int wdg1_ok,
                         int wdg2_ok, int* system_ok, int* wdg_disagree);

#ifdef __cplusplus
}
#endif
#endif /* RESET_DESIGN_H */
