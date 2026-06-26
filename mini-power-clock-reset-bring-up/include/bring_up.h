/**
 * @file bring_up.h
 * @brief MCU Development Board - Board Bring-Up Procedures (L1-L8)
 *
 * Knowledge Coverage:
 *   L1: bring-up checklist, visual inspection, continuity test, smoke test
 *   L2: power-up sequence, first firmware flash, debug connectivity
 *   L3: voltage/current measurement math, frequency counting, logic analyzer timing
 *   L4: Ohm's law verification, power calculation on first power-up
 *   L5: automated bring-up test algorithm, boundary scan, built-in self-test
 *   L6: STM32 Nucleo-style bring-up, Arduino bootloader burn, ESP32 first flash
 *   L7: production line test procedure, automated board validation
 *   L8: flying probe test, X-ray inspection for BGA, AI-driven defect detection
 *   L9: self-healing boards, in-field reconfiguration (documented)
 *
 * Reference: Valvano "Embedded Systems: Real-Time Operating Systems" (2019)
 *            IPC-A-610 Acceptability of Electronic Assemblies
 */

#ifndef BRING_UP_H
#define BRING_UP_H
#include <stdint.h>
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * L1 Definitions
 * ======================================================================== */

/** Board bring-up step definition.
 *  Each step has a description, pass/fail criteria, and diagnostic guidance. */
typedef struct {
    int    step_number;
    char   description[128];
    int    is_automated;
    int    blocking_on_fail;
    double timeout_seconds;
} bringup_step_t;

/** Board power measurement on first power-up.
 *  Measures all critical voltages and currents during initial power application. */
typedef struct {
    double v_input_V, i_input_mA;
    double v_3v3_V, v_1v8_V, v_1v2_V, v_core_V;
    double v_vdda_V, v_vbat_V, v_vref_V;
    double power_total_mW;
    int    short_circuit_detected;
    int    overcurrent_detected;
    int    all_rails_ok;
} first_power_measurement_t;

/** Visual inspection checklist item.
 *  Covers PCB assembly quality, solder joint integrity, component placement. */
typedef struct {
    int    item_id;
    char   description[128];
    int    checked;
    int    passed;
    char   note[64];
} visual_inspection_item_t;

/** Board connectivity test result.
 *  Tests: power-ground short, JTAG/SWD connectivity, UART loopback. */
typedef struct {
    int    power_to_gnd_short;
    int    jtag_connected;
    int    swd_connected;
    int    uart_tx_functional;
    int    uart_rx_functional;
    int    bootloader_responding;
    char   serial_number[32];
} connectivity_test_t;

/** Firmware download and verification result.
 *  Records flash programming status, verification, and option bytes. */
typedef struct {
    int    flash_erase_ok;
    int    flash_program_ok;
    int    flash_verify_ok;
    uint32_t firmware_size_bytes;
    uint32_t firmware_crc;
    uint32_t expected_crc;
    int    option_bytes_set;
} firmware_flash_result_t;

/** Board bring-up status for the entire process.
 *  Tracks completion of each phase and overall pass/fail. */
typedef struct {
    int    visual_inspection_done;
    int    continuity_test_done;
    int    first_power_done;
    int    clock_verified;
    int    reset_verified;
    int    jtag_connected;
    int    firmware_flashed;
    int    peripherals_tested;
    int    overall_pass;
    int    total_steps;
    int    passed_steps;
} bringup_status_t;

/* ========================================================================
 * L2 Core Concepts
 * ======================================================================== */

/** Execute pre-power continuity check.
 *  Measures resistance between power rails and ground.
 *  A short (<10 Ohm typically) indicates assembly defect.
 *  Also checks for open circuits on critical nets. */
int continuity_check(double* resistance_power_gnd_ohm, int* short_detected);

/** Execute first power-up with current limiting.
 *  Apply power through current-limited supply.
 *  Monitor voltage rails come up in sequence.
 *  Check for overcurrent conditions indicating shorts. */
int first_power_up(double current_limit_mA, double voltage_target_V,
                   first_power_measurement_t* measurement);

/** Verify all clock sources are operational.
 *  Check: HSE crystal oscillation, LSE crystal (if populated),
 *  internal HSI (16MHz typical), PLL lock status.
 *  Measure frequency tolerance against specification. */
int clock_verification(double hse_freq_Hz, double lse_freq_Hz,
                       int* hse_ok, int* lse_ok, int* pll_locked);

/** Verify reset circuit behavior.
 *  Check: POR assertion on power-up, nRST pin toggling,
 *  reset vector execution, BOR threshold. */
int reset_verification(int* por_detected, int* nrst_functional,
                       double* actual_bor_threshold_V);

/** Establish debug interface connectivity.
 *  Attempt JTAG/SWD connection, read device ID, verify
 *  debug access to core registers. */
int debug_connectivity_check(const char* interface_type,
                              uint32_t* device_id, int* core_halted);

/* ========================================================================
 * L3 Mathematical Structures
 * ======================================================================== */

/** Voltage divider measurement math.
 *  V_actual = V_measured * (R1 + R2) / R2.
 *  Used to translate ADC readings to actual rail voltages
 *  during bring-up measurements. */
double voltage_divider_actual(double v_measured, double r1_ohm, double r2_ohm);

/** Current measurement via shunt resistor.
 *  I = V_shunt / R_shunt.
 *  Use Kelvin (4-wire) connection for accuracy below 10mOhm. */
double shunt_current(double v_shunt_mV, double r_shunt_mOhm);

/** Frequency measurement by period counting.
 *  f = N_cycles / T_gate.
 *  Resolution: df = f^2 * dt_gate_error.
 *  For 10MHz signal, 1s gate time: resolution = 1Hz. */
double frequency_counter(double cycles_counted, double gate_time_s);

/** Logic analyzer sample rate calculation.
 *  fs_sample >= 4 * f_signal_max for reliable edge detection.
 *  Memory_depth = fs_sample * capture_time.
 *  @return minimum sample rate (Hz) */
double logic_analyzer_sample_rate(double f_signal_max_Hz, double capture_time_s);

/* ========================================================================
 * L4 Fundamental Laws
 * ======================================================================== */

/** Ohm's Law verification on actual board.
 *  Apply known voltage across a calibration resistor.
 *  Measure current, verify V = I * R within tolerance.
 *  This validates the measurement setup before trusting other readings. */
int ohms_law_verify(double v_applied, double r_known, double i_measured,
                    double tolerance_percent);

/** Power calculation from first power-up measurements.
 *  P_total = sum(V_rail_i * I_rail_i).
 *  Compares against expected power budget from design calculations. */
double bringup_power_total(const first_power_measurement_t* m);

/** Compare measured vs expected voltage with tolerance.
 *  Checks if measured voltage is within tolerance% of expected.
 *  Used throughout bring-up to validate each power rail. */
int voltage_within_tolerance(double measured_V, double expected_V, double tolerance_pct);

/* ========================================================================
 * L5 Algorithms
 * ======================================================================== */

/** Automated bring-up test sequencer.
 *  Executes predefined test steps in order.
 *  Stops on first blocking failure or completes all steps.
 *  Logs results for each step with timestamp.
 *  Complexity: O(N) where N = number of steps. */
int automated_bringup_sequence(const bringup_step_t* steps, int num_steps,
                                bringup_status_t* status);

/** JTAG boundary scan (IEEE 1149.1) connectivity test.
 *  Uses EXTEST to verify interconnects between JTAG-compliant devices.
 *  Detects: open, short, stuck-at faults on board traces. */
int boundary_scan_test(int* faults_detected, int max_faults,
                       int* fault_nets, int* fault_types);

/** Built-in self-test (BIST) sequence for MCU.
 *  MCU runs internal test routines: RAM March C-, ROM CRC,
 *  CPU register test, interrupt controller test.
 *  Reports overall pass/fail and specific failure codes. */
int mcu_bist_run(int* ram_test_ok, int* rom_crc_ok, int* cpu_reg_ok,
                 int* int_ctrl_ok, uint32_t* failure_code);

/* ========================================================================
 * L6 Canonical Problems
 * ======================================================================== */

/** STM32 Nucleo-style board bring-up checklist.
 *  Step 1: Visual inspection (solder bridges, polarity)
 *  Step 2: Power-ground resistance check (>1kOhm)
 *  Step 3: Apply 3.3V with 100mA limit
 *  Step 4: Check VDD, VDDA, nRST high
 *  Step 5: Connect ST-LINK, read device ID
 *  Step 6: Flash LED blinky firmware
 *  Step 7: Verify LED blinks, UART output
 *  @return bitmask of failed steps */
int nucleo_bringup_checklist(int step_results[7]);

/** Arduino bootloader burn procedure validation.
 *  Steps: enter bootloader mode (reset timing), verify device signature,
 *  flash bootloader, set fuses, verify bootloader operation. */
int arduino_bootloader_burn(int* device_sig_ok, int* flash_ok,
                             int* fuse_ok, int* bootloader_running);

/** ESP32 first flash procedure.
 *  Put ESP32 in download mode (GPIO0 low + reset).
 *  Verify ROM bootloader responds on UART.
 *  Flash firmware at correct baud rate.
 *  Verify flash contents. */
int esp32_first_flash(int gpio0_state, int en_state, int* bootloader_responded,
                       int* flash_complete, int* verify_ok);

/* ========================================================================
 * L7 Applications
 * ======================================================================== */

/** Production line automated test procedure.
 *  Models a complete manufacturing test flow:
 *  1. In-circuit test (ICT) for passive components
 *  2. Power-up test with current monitoring
 *  3. JTAG boundary scan
 *  4. Firmware download
 *  5. Functional test (all peripherals)
 *  6. Burn-in test (temperature cycling)
 *  7. Final QC inspection
 *  @return overall pass/fail, fault category if failed */
int production_test_flow(int enable_ict, int enable_boundary_scan,
                          int enable_burnin, double burnin_hours,
                          int* test_passed, int* fault_category);

/** Automated board validation report generation.
 *  Compiles all bring-up test results into a structured report.
 *  Includes: serial number, test date, all measurements,
 *  pass/fail status per test, overall acceptance. */
void bringup_report_generate(const bringup_status_t* status,
                              const first_power_measurement_t* power,
                              const connectivity_test_t* conn,
                              char* report_buffer, int buffer_size);

/* ========================================================================
 * L8 Advanced Topics
 * ======================================================================== */

/** Flying probe test coverage analysis.
 *  Calculates test coverage for flying probe testing.
 *  Coverage = tested_nets / total_nets. Targets >95% for production.
 *  BGA and fine-pitch components may require additional X-ray inspection. */
double flying_probe_coverage(int total_nets, int accessible_nets,
                              int tested_nets, int bga_present);

/** X-ray inspection for hidden solder joints (BGA/QFN).
 *  Detects: voiding (>25% void area = reject per IPC-7095),
 *  head-in-pillow, bridging, insufficient solder.
 *  @return void percentage, pass/fail per IPC-7095 class 2 */
int xray_bga_inspection(int bga_ball_count, double void_percent_per_ball[256],
                         int ball_count, double* max_void_pct, int* failing_balls);

#ifdef __cplusplus
}
#endif
#endif /* BRING_UP_H */
