/**
 * jtag_tap.h - JTAG TAP Controller Definitions (IEEE 1149.1)
 *
 * L1 Definitions: TAP state machine states, JTAG signals,
 * instruction/data register selection, device ID code.
 * L2 Core Concepts: Boundary scan, TAP controller 16-state FSM,
 * IR/DR scan sequences, BYPASS/IDCODE instructions.
 * L4 Fundamental Laws: IEEE 1149.1-2013 standard TAP controller.
 *
 * Reference: IEEE Standard 1149.1-2013 (JTAG)
 * Courses: Berkeley EE16B, Georgia Tech ECE 6350, Illinois ECE 451
 */

#ifndef JTAG_TAP_H
#define JTAG_TAP_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ==================================================================
 * L1: JTAG Signal Definitions
 * ================================================================== */

/** JTAG physical signals - the Test Access Port (TAP):
 *  TCK  - Test Clock: synchronizes all TAP operations
 *  TMS  - Test Mode Select: controls TAP state transitions
 *  TDI  - Test Data Input: serial data into the target
 *  TDO  - Test Data Output: serial data from the target
 *  TRST - Test Reset (optional, active-low)
 */
typedef enum {
    JTAG_SIG_TCK  = 0,
    JTAG_SIG_TMS  = 1,
    JTAG_SIG_TDI  = 2,
    JTAG_SIG_TDO  = 3,
    JTAG_SIG_TRST = 4,
    JTAG_SIG_NUM  = 5
} jtag_signal_t;

/** JTAG signal level state */
typedef struct {
    bool tck;
    bool tms;
    bool tdi;
    bool tdo;
    bool trst;
} jtag_signal_levels_t;

/** JTAG signal direction */
typedef enum {
    JTAG_DIR_OUTPUT = 0,
    JTAG_DIR_INPUT  = 1,
    JTAG_DIR_HIGHZ  = 2
} jtag_signal_dir_t;

/* ==================================================================
 * L1: TAP Controller State Machine - 16 States
 * ================================================================== */

/** TAP Controller States (IEEE 1149.1-2013 Figure 6-1):
 *  The TAP controller is a 16-state finite state machine
 *  controlled by TMS on rising TCK.
 *
 *  Paths from Test-Logic-Reset:
 *    TLR --(TMS=0)--> Run-Test/Idle
 *    TLR --(TMS=1)--> TLR (stay)
 *
 *  DR scan path:  SEL-DR -> CAP-DR -> SHIFT-DR -> EXIT1-DR
 *                 -> PAUSE-DR -> EXIT2-DR -> UPD-DR -> RTI
 *  IR scan path:  SEL-IR -> CAP-IR -> SHIFT-IR -> EXIT1-IR
 *                 -> PAUSE-IR -> EXIT2-IR -> UPD-IR -> RTI
 */
typedef enum {
    TAP_STATE_TEST_LOGIC_RESET = 0x0,
    TAP_STATE_RUN_TEST_IDLE    = 0x1,
    TAP_STATE_SELECT_DR_SCAN   = 0x2,
    TAP_STATE_CAPTURE_DR       = 0x3,
    TAP_STATE_SHIFT_DR         = 0x4,
    TAP_STATE_EXIT1_DR         = 0x5,
    TAP_STATE_PAUSE_DR         = 0x6,
    TAP_STATE_EXIT2_DR         = 0x7,
    TAP_STATE_UPDATE_DR        = 0x8,
    TAP_STATE_SELECT_IR_SCAN   = 0x9,
    TAP_STATE_CAPTURE_IR       = 0xA,
    TAP_STATE_SHIFT_IR         = 0xB,
    TAP_STATE_EXIT1_IR         = 0xC,
    TAP_STATE_PAUSE_IR         = 0xD,
    TAP_STATE_EXIT2_IR         = 0xE,
    TAP_STATE_UPDATE_IR        = 0xF,
    TAP_STATE_INVALID          = 0xFF
} tap_state_t;

#define TAP_NUM_STATES 16

extern const tap_state_t tap_fsm_next[TAP_NUM_STATES][2];
extern const char *tap_state_names[TAP_NUM_STATES];

/* ==================================================================
 * L1: JTAG Instruction Register (IR)
 * ================================================================== */

/** JTAG standard instructions (IEEE 1149.1 mandatory + common):
 *  BYPASS - 1-bit DR chain bypass
 *  SAMPLE/PRELOAD - capture I/O state, preload boundary values
 *  EXTEST - drive boundary scan cells externally
 *  IDCODE - read device identification register
 */
typedef enum {
    JTAG_INSTR_EXTEST          = 0x00,
    JTAG_INSTR_IDCODE          = 0x01,
    JTAG_INSTR_SAMPLE_PRELOAD  = 0x02,
    JTAG_INSTR_BYPASS          = 0x03,
    JTAG_INSTR_INTEST          = 0x04,
    JTAG_INSTR_HIGHZ           = 0x05,
    JTAG_INSTR_CLAMP           = 0x06,
    JTAG_INSTR_USER1           = 0x07,
    JTAG_INSTR_USER2           = 0x08,
    JTAG_INSTR_CUSTOM_BASE     = 0x10
} jtag_instruction_t;

#define JTAG_IR_LEN_ARM_CORTEX  4
#define JTAG_IR_LEN_XILINX      6
#define JTAG_IR_LEN_ALTERA      10
#define JTAG_IR_LEN_MAX         32

/** ARM Cortex-M JTAG instructions (4-bit IR) */
typedef enum {
    ARM_INSTR_IDCODE = 0x0E,
    ARM_INSTR_DPACC  = 0x0A,
    ARM_INSTR_APACC  = 0x0B,
    ARM_INSTR_ABORT  = 0x08,
    ARM_INSTR_BYPASS = 0x0F
} arm_jtag_instruction_t;

/* ==================================================================
 * L1: JTAG Data Register (DR) Types
 * ================================================================== */

typedef enum {
    JTAG_DR_BYPASS   = 0,
    JTAG_DR_IDCODE   = 1,
    JTAG_DR_BOUNDARY = 2,
    JTAG_DR_DEBUG    = 3,
    JTAG_DR_CUSTOM   = 4
} jtag_dr_type_t;

/** Device Identification Register (IDCODE) - IEEE 1149.1 Section 9.3:
 *  +-----------+------------+-----------------+--------+
 *  | Version   | Part No.   | Manufacturer    | LSB=1  |
 *  | [31:28]   | [27:12]    | [11:1]          | [0]    |
 *  +-----------+------------+-----------------+--------+
 */
typedef struct {
    uint32_t version;
    uint32_t part_number;
    uint32_t manufacturer;
    bool     lsb_valid;
} jtag_idcode_t;

#define JTAG_IDCODE_VERSION(v)      (((v) >> 28) & 0xF)
#define JTAG_IDCODE_PARTNO(v)       (((v) >> 12) & 0xFFFF)
#define JTAG_IDCODE_MANUFACTURER(v) (((v) >> 1)  & 0x7FF)
#define JTAG_IDCODE_LSB_VALID(v)    (((v) & 0x1) == 0x1)

static inline void jtag_idcode_decode(uint32_t raw, jtag_idcode_t *id) {
    if (!id) return;
    id->version      = JTAG_IDCODE_VERSION(raw);
    id->part_number  = JTAG_IDCODE_PARTNO(raw);
    id->manufacturer = JTAG_IDCODE_MANUFACTURER(raw);
    id->lsb_valid    = JTAG_IDCODE_LSB_VALID(raw);
}

#define ARM_IDCODE_CORTEX_M0   0x0BB11477u
#define ARM_IDCODE_CORTEX_M3   0x2BA01477u
#define ARM_IDCODE_CORTEX_M4   0x2BA01477u
#define ARM_IDCODE_CORTEX_M7   0x0BA02477u
#define ARM_IDCODE_CORTEX_M33  0x0BE12477u

/* ==================================================================
 * L1: TAP Scan Chain Configuration
 * ================================================================== */

#define JTAG_MAX_DEVICES_IN_CHAIN 16

typedef struct {
    uint32_t idcode;
    uint32_t ir_length;
    uint32_t dr_prescan_bits;
    uint32_t dr_postscan_bits;
    uint32_t total_ir_bits;
    bool     has_idcode;
} jtag_device_t;

typedef struct {
    jtag_device_t devices[JTAG_MAX_DEVICES_IN_CHAIN];
    uint32_t device_count;
    uint32_t total_ir_length;
    bool     chain_detected;
} jtag_scan_chain_t;

/* ==================================================================
 * L1: JTAG Timing Parameters
 * ================================================================== */

typedef struct {
    double tck_freq_hz;
    double tms_setup_ns;
    double tms_hold_ns;
    double tdi_setup_ns;
    double tdi_hold_ns;
    double tdo_delay_ns;
    double trst_pulse_width_ns;
    uint32_t power_up_reset_us;
} jtag_timing_t;

#define JTAG_DEFAULT_TCK_FREQ     10e6
#define JTAG_DEFAULT_TMS_SETUP_NS 15.0
#define JTAG_DEFAULT_TMS_HOLD_NS  10.0
#define JTAG_DEFAULT_TDI_SETUP_NS 15.0
#define JTAG_DEFAULT_TDI_HOLD_NS  10.0
#define JTAG_DEFAULT_TDO_DELAY_NS 25.0
#define JTAG_DEFAULT_TRST_PULSE_NS 100.0
#define JTAG_DEFAULT_POWERUP_US   1000

/* ==================================================================
 * L2: TAP State Machine Navigation API
 * ================================================================== */

/**
 * tap_get_next_state - Compute next TAP state based on TMS
 * Complexity: O(1) table lookup
 */
static inline tap_state_t tap_get_next_state(tap_state_t current, bool tms) {
    if (current >= TAP_NUM_STATES) return TAP_STATE_INVALID;
    return tap_fsm_next[current][tms ? 1 : 0];
}

int tap_navigate_to(tap_state_t from, tap_state_t to,
                    uint8_t *tms_sequence, int max_steps);
const char *tap_state_get_name(tap_state_t state);
void jtag_timing_init(jtag_timing_t *timing);
double jtag_calculate_min_tck_period(const jtag_timing_t *timing);
double jtag_calculate_max_tck_freq(const jtag_timing_t *timing);
bool tap_validate_fsm(void);
void tap_reset_to_tlr_sequence(uint8_t *tms_seq);
void tap_reset_to_rti_sequence(uint8_t *tms_seq);
int tap_ir_scan_sequence_generate(uint32_t bit_count, uint8_t *tms_seq,
                                   int max_len, int *shift_start);
int tap_dr_scan_sequence_generate(uint32_t bit_count, uint8_t *tms_seq,
                                   int max_len, int *shift_start);
int tap_idcode_scan_sequence_generate(uint32_t ir_length, uint32_t idcode_instr,
                                       uint8_t *tms_seq, int max_len,
                                       int *dr_shift_start);
int jtag_scan_chain_detect(jtag_scan_chain_t *chain);
bool jtag_check_even_parity_32(uint32_t data);
uint32_t boundary_scan_cell_count_estimate(uint32_t pin_count);
int boundary_scan_extest_vector(uint32_t pin_index, uint32_t chain_length,
                                 uint8_t *tdi_data, uint32_t tdi_bytes);

static inline bool tap_is_stable_state(tap_state_t state) {
    return (state == TAP_STATE_TEST_LOGIC_RESET) ||
           (state == TAP_STATE_RUN_TEST_IDLE)    ||
           (state == TAP_STATE_PAUSE_DR)         ||
           (state == TAP_STATE_PAUSE_IR);
}

/** tap_is_shift_state - Check if state allows data shifting */
static inline bool tap_is_shift_state(tap_state_t state) {
    return (state == TAP_STATE_SHIFT_DR) ||
           (state == TAP_STATE_SHIFT_IR);
}

/** tap_is_update_state - Check if state latches shift register */
static inline bool tap_is_update_state(tap_state_t state) {
    return (state == TAP_STATE_UPDATE_DR) ||
           (state == TAP_STATE_UPDATE_IR);
}

/** tap_is_capture_state - Check if state captures data into shift reg */
static inline bool tap_is_capture_state(tap_state_t state) {
    return (state == TAP_STATE_CAPTURE_DR) ||
           (state == TAP_STATE_CAPTURE_IR);
}

/** tap_scan_path - Determine if in DR or IR scan path */
static inline bool tap_is_dr_path(tap_state_t state) {
    return (state >= TAP_STATE_SELECT_DR_SCAN &&
            state <= TAP_STATE_UPDATE_DR);
}

static inline bool tap_is_ir_path(tap_state_t state) {
    return (state >= TAP_STATE_SELECT_IR_SCAN &&
            state <= TAP_STATE_UPDATE_IR);
}

#ifdef __cplusplus
}
#endif
#endif /* JTAG_TAP_H */
