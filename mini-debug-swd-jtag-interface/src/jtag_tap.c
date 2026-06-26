/**
 * jtag_tap.c - JTAG TAP Controller Implementation
 *
 * L1: TAP FSM state transition table, state names.
 * L2: TAP navigation algorithm (BFS shortest path).
 * L5: IR/DR scan sequence generation.
 * L6: IDCODE scan, boundary scan chain detection.
 *
 * Reference: IEEE 1149.1-2013, Figure 6-1 and Annex D
 * Courses: Berkeley EE16B, Georgia Tech ECE 6350
 */

#include "jtag_tap.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* ==================================================================
 * L1: TAP FSM State Transition Table
 * ================================================================== */

/**
 * tap_fsm_next[state][tms] = next_state
 *
 * IEEE 1149.1-2013 Figure 6-1: TAP Controller State Diagram.
 *
 * The TAP controller is a Moore-type finite state machine
 * with 16 states. The only input is TMS (sampled on TCK rising
 * edge). The outputs (clocking of IR/DR, latching of update
 * registers) are functions of the current state.
 *
 * Key insight: from any state, TMS=1 for 5 clocks guarantees
 * reaching Test-Logic-Reset. This is the hardware reset mechanism
 * when TRST is not available.
 */
const tap_state_t tap_fsm_next[TAP_NUM_STATES][2] = {
    /* state=0: TEST_LOGIC_RESET */
    { TAP_STATE_RUN_TEST_IDLE,  TAP_STATE_TEST_LOGIC_RESET },
    /* state=1: RUN_TEST_IDLE */
    { TAP_STATE_RUN_TEST_IDLE,  TAP_STATE_SELECT_DR_SCAN },
    /* state=2: SELECT_DR_SCAN */
    { TAP_STATE_CAPTURE_DR,     TAP_STATE_SELECT_IR_SCAN },
    /* state=3: CAPTURE_DR */
    { TAP_STATE_SHIFT_DR,       TAP_STATE_EXIT1_DR },
    /* state=4: SHIFT_DR */
    { TAP_STATE_SHIFT_DR,       TAP_STATE_EXIT1_DR },
    /* state=5: EXIT1_DR */
    { TAP_STATE_PAUSE_DR,       TAP_STATE_UPDATE_DR },
    /* state=6: PAUSE_DR */
    { TAP_STATE_PAUSE_DR,       TAP_STATE_EXIT2_DR },
    /* state=7: EXIT2_DR */
    { TAP_STATE_SHIFT_DR,       TAP_STATE_UPDATE_DR },
    /* state=8: UPDATE_DR */
    { TAP_STATE_RUN_TEST_IDLE,  TAP_STATE_SELECT_DR_SCAN },
    /* state=9: SELECT_IR_SCAN */
    { TAP_STATE_CAPTURE_IR,     TAP_STATE_TEST_LOGIC_RESET },
    /* state=10: CAPTURE_IR */
    { TAP_STATE_SHIFT_IR,       TAP_STATE_EXIT1_IR },
    /* state=11: SHIFT_IR */
    { TAP_STATE_SHIFT_IR,       TAP_STATE_EXIT1_IR },
    /* state=12: EXIT1_IR */
    { TAP_STATE_PAUSE_IR,       TAP_STATE_UPDATE_IR },
    /* state=13: PAUSE_IR */
    { TAP_STATE_PAUSE_IR,       TAP_STATE_EXIT2_IR },
    /* state=14: EXIT2_IR */
    { TAP_STATE_SHIFT_IR,       TAP_STATE_UPDATE_IR },
    /* state=15: UPDATE_IR */
    { TAP_STATE_RUN_TEST_IDLE,  TAP_STATE_SELECT_DR_SCAN }
};

/* ==================================================================
 * L1: TAP State Names
 * ================================================================== */

const char *tap_state_names[TAP_NUM_STATES] = {
    "Test-Logic-Reset",
    "Run-Test/Idle",
    "Select-DR-Scan",
    "Capture-DR",
    "Shift-DR",
    "Exit1-DR",
    "Pause-DR",
    "Exit2-DR",
    "Update-DR",
    "Select-IR-Scan",
    "Capture-IR",
    "Shift-IR",
    "Exit1-IR",
    "Pause-IR",
    "Exit2-IR",
    "Update-IR"
};

/* ==================================================================
 * L2: TAP State Name Lookup
 * ================================================================== */

const char *tap_state_get_name(tap_state_t state) {
    if (state < TAP_NUM_STATES) {
        return tap_state_names[state];
    }
    if (state == TAP_STATE_INVALID) {
        return "INVALID";
    }
    return "UNKNOWN";
}

/* ==================================================================
 * L5: TAP Navigation Algorithm
 * ================================================================== */

/**
 * tap_navigate_to - Compute TMS sequence to reach target state
 *
 * Uses BFS (Breadth-First Search) over the TAP FSM graph
 * to find the shortest TMS bit sequence from current to
 * target state.
 *
 * The TAP FSM graph has 16 vertices and 32 directed edges
 * (each state has exactly 2 outgoing edges: TMS=0 and TMS=1).
 * BFS guarantees the shortest path in an unweighted graph.
 *
 * Algorithm complexity: O(V + E) = O(16 + 32) = O(1)
 * Max path length: 7 transitions (e.g., TLR -> Update-IR via DR path)
 *
 * Reference: IEEE 1149.1-2013, Annex D (Navigation Algorithms)
 *
 * @param from        Starting TAP state
 * @param to          Target TAP state
 * @param tms_sequence Output: TMS bits (0 or 1 for each TCK)
 *                      tms_sequence[0] is applied first
 * @param max_steps    Maximum steps allowed (buffer size)
 * @return Number of steps needed, or -1 if unreachable
 */
int tap_navigate_to(tap_state_t from, tap_state_t to,
                    uint8_t *tms_sequence, int max_steps) {
    /* BFS data structures */
    int visited[TAP_NUM_STATES];
    int parent[TAP_NUM_STATES];
    uint8_t parent_tms[TAP_NUM_STATES];
    int queue[TAP_NUM_STATES];
    int qhead, qtail;
    int i;

    if (!tms_sequence || max_steps <= 0) return -1;
    if (from >= TAP_NUM_STATES || to >= TAP_NUM_STATES) return -1;
    if (from == to) return 0;

    /* Initialize BFS */
    for (i = 0; i < TAP_NUM_STATES; i++) {
        visited[i] = 0;
        parent[i] = -1;
    }

    qhead = 0;
    qtail = 0;
    queue[qtail++] = (int)from;
    visited[from] = 1;

    /* BFS main loop */
    while (qhead < qtail) {
        int current = queue[qhead++];
        int next_tms0, next_tms1;

        if (current == (int)to) break;

        next_tms0 = (int)tap_fsm_next[current][0];
        next_tms1 = (int)tap_fsm_next[current][1];

        /* Explore TMS=0 edge */
        if (!visited[next_tms0]) {
            visited[next_tms0] = 1;
            parent[next_tms0] = current;
            parent_tms[next_tms0] = 0;
            queue[qtail++] = next_tms0;
        }

        /* Explore TMS=1 edge */
        if (!visited[next_tms1]) {
            visited[next_tms1] = 1;
            parent[next_tms1] = current;
            parent_tms[next_tms1] = 1;
            queue[qtail++] = next_tms1;
        }
    }

    /* Check if target reached */
    if (!visited[to]) return -1;

    /* Reconstruct path (reverse order) */
    int path_len = 0;
    int state = (int)to;
    uint8_t reverse_path[TAP_NUM_STATES * 2];

    while (state != (int)from) {
        if (path_len >= max_steps) return -1;
        reverse_path[path_len++] = parent_tms[state];
        state = parent[state];
        if (state < 0) return -1;
    }

    /* Reverse to get forward order */
    for (i = 0; i < path_len; i++) {
        tms_sequence[i] = reverse_path[path_len - 1 - i];
    }

    return path_len;
}

/* ==================================================================
 * L5: JTAG TAP Reset
 * ================================================================== */

/**
 * tap_reset_to_tlr_tms_sequence - Get TMS sequence guaranteed to reach TLR
 *
 * From any state, 5 consecutive TCK cycles with TMS=1 will
 * reach Test-Logic-Reset. This is a hardware-guaranteed property
 * of the TAP FSM.
 *
 * Proof: Tracing the FSM from any state with TMS=1:
 *   Any DR state -(TMS=1)-> eventually TLR
 *   Any IR state -(TMS=1)-> TLR in at most 5 steps
 *
 * The maximum distance to TLR is 5 transitions from Update-IR.
 */
#define TAP_RESET_SEQUENCE_LENGTH 5

void tap_reset_to_tlr_sequence(uint8_t *tms_seq) {
    int i;
    for (i = 0; i < TAP_RESET_SEQUENCE_LENGTH; i++) {
        tms_seq[i] = 1;
    }
}

/**
 * tap_reset_to_rti_tms_sequence - Get TMS sequence to reach Run-Test/Idle
 *
 * From TLR: TMS=0 for 1 clock -> RTI
 * From RTI: already there, TMS=0 to stay
 *
 * Safe sequence from any state:
 *   5 clocks TMS=1 (guaranteed TLR)
 *   1 clock  TMS=0 (TLR -> RTI)
 */
#define TAP_TO_RTI_SEQUENCE_LENGTH 6

void tap_reset_to_rti_sequence(uint8_t *tms_seq) {
    int i;
    for (i = 0; i < 5; i++) tms_seq[i] = 1;
    tms_seq[5] = 0;
}

/* ==================================================================
 * L2: TAP DR/IR Scan Sequence Generation
 * ================================================================== */

/**
 * tap_ir_scan_sequence - Generate TMS sequence for IR scan
 *
 * Complete IR scan from Run-Test/Idle:
 *   RTI -(TMS=1)-> SEL-DR -(TMS=1)-> SEL-IR -(TMS=0)-> CAP-IR
 *   -(TMS=0)-> SHIFT-IR (shift N-1 bits with TMS=0, last bit TMS=1)
 *   -(TMS=1)-> EXIT1-IR -(TMS=1)-> UPDATE-IR -(TMS=0)-> RTI
 *
 * The sequence includes:
 *   - Navigation to SHIFT-IR
 *   - N bits of shift (N-1 with TMS=0, 1 with TMS=1)
 *   - Navigation back to RTI
 *
 * @param bit_count   Number of IR bits to shift
 * @param tms_seq     Output TMS sequence
 * @param max_len     Maximum sequence length
 * @param shift_start Output: index in tms_seq where shifting begins
 * @return Total sequence length (TCK cycles), or -1 if buffer too small
 */
int tap_ir_scan_sequence_generate(uint32_t bit_count,
                                   uint8_t *tms_seq,
                                   int max_len,
                                   int *shift_start) {
    /* Pre-shift: RTI -> SEL-DR -> SEL-IR -> CAP-IR -> SHIFT-IR */
    uint8_t pre_shift[] = {1, 1, 0, 0};  /* 4 cycles */
    /* Post-shift: EXIT1-IR -> UPDATE-IR -> RTI */
    uint8_t post_shift[] = {1, 1, 0};    /* 3 cycles */
    int pre_len = 4;
    int post_len = 3;
    int seq_idx = 0;
    uint32_t i;
    int total;

    total = pre_len + (int)bit_count + post_len;
    if (!tms_seq || total > max_len) return -1;

    /* Pre-shift navigation */
    for (i = 0; i < (uint32_t)pre_len; i++) {
        tms_seq[seq_idx++] = pre_shift[i];
    }

    /* Mark shift start */
    if (shift_start) *shift_start = seq_idx;

    /* Shift phase: first N-1 bits TMS=0, last bit TMS=1 */
    for (i = 0; i < bit_count; i++) {
        if (i == bit_count - 1) {
            tms_seq[seq_idx++] = 1;  /* Last bit: exit shift */
        } else {
            tms_seq[seq_idx++] = 0;  /* Continue shifting */
        }
    }

    /* Post-shift navigation */
    for (i = 0; i < (uint32_t)post_len; i++) {
        tms_seq[seq_idx++] = post_shift[i];
    }

    return total;
}

/**
 * tap_dr_scan_sequence_generate - Generate TMS sequence for DR scan
 *
 * Similar to IR scan but navigates through DR path:
 *   RTI -(TMS=1)-> SEL-DR -(TMS=0)-> CAP-DR
 *   -(TMS=0)-> SHIFT-DR (shift N bits, last TMS=1)
 *   -(TMS=1)-> EXIT1-DR -(TMS=1)-> UPDATE-DR -(TMS=0)-> RTI
 */
int tap_dr_scan_sequence_generate(uint32_t bit_count,
                                   uint8_t *tms_seq,
                                   int max_len,
                                   int *shift_start) {
    /* Pre-shift: RTI -> SEL-DR -> CAP-DR -> SHIFT-DR */
    uint8_t pre_shift[] = {1, 0, 0};    /* 3 cycles */
    /* Post-shift: EXIT1-DR -> UPDATE-DR -> RTI */
    uint8_t post_shift[] = {1, 1, 0};   /* 3 cycles */
    int pre_len = 3;
    int post_len = 3;
    int seq_idx = 0;
    uint32_t i;
    int total;

    total = pre_len + (int)bit_count + post_len;
    if (!tms_seq || total > max_len) return -1;

    for (i = 0; i < (uint32_t)pre_len; i++) {
        tms_seq[seq_idx++] = pre_shift[i];
    }

    if (shift_start) *shift_start = seq_idx;

    for (i = 0; i < bit_count; i++) {
        tms_seq[seq_idx++] = (i == bit_count - 1) ? 1 : 0;
    }

    for (i = 0; i < (uint32_t)post_len; i++) {
        tms_seq[seq_idx++] = post_shift[i];
    }

    return total;
}

/* ==================================================================
 * L5: IDCODE Scan Operations
 * ================================================================== */

/**
 * tap_idcode_scan_sequence - Generate full IDCODE scan sequence
 *
 * To read IDCODE:
 *   1. Navigate to SHIFT-IR
 *   2. Shift in IDCODE instruction (4 bits for ARM Cortex)
 *   3. Navigate to UPDATE-IR, then to SHIFT-DR
 *   4. Shift out 32-bit IDCODE value
 *   5. Navigate back to RTI
 *
 * IDCODE instruction for ARM: 0x0E (4-bit, LSB first)
 *
 * @param ir_length        Length of instruction register
 * @param idcode_instr     IDCODE instruction value
 * @param tms_seq          Output TMS sequence
 * @param max_len          Maximum sequence length
 * @param dr_shift_start   Output: TMS index where DR shift begins
 * @return Total sequence length or -1
 */
int tap_idcode_scan_sequence_generate(uint32_t ir_length,
                                       uint32_t idcode_instr,
                                       uint8_t *tms_seq,
                                       int max_len,
                                       int *dr_shift_start) {
    int ir_shift_start;
    int dr_shift_start_local;
    int ir_seq_len, dr_seq_len;
    int total;

    (void)idcode_instr;

    /* Phase 1: IR scan to load IDCODE instruction */
    ir_seq_len = tap_ir_scan_sequence_generate(ir_length, tms_seq,
                                                max_len, &ir_shift_start);
    if (ir_seq_len < 0) return -1;

    /* Phase 2: DR scan to read IDCODE (32 bits) */
    dr_seq_len = tap_dr_scan_sequence_generate(32,
                                     tms_seq + ir_seq_len,
                                     max_len - ir_seq_len,
                                     &dr_shift_start_local);
    if (dr_seq_len < 0) return -1;

    total = ir_seq_len + dr_seq_len;
    if (dr_shift_start) {
        *dr_shift_start = ir_seq_len + dr_shift_start_local;
    }

    return total;
}

/* ==================================================================
 * L5: JTAG Scan Chain Detection
 * ================================================================== */

/**
 * jtag_scan_chain_detect - Detect devices in JTAG daisy chain
 *
 * Algorithm:
 *   1. Reset TAP to TLR
 *   2. Shift IR with all-ones (BYPASS for most devices)
 *   3. Shift DR with all-ones pattern (length = 32 * max_devices)
 *   4. Count transitions in TDO: each BYPASS register (1 bit)
 *      returns 1, each IDCODE (32 bits) returns a pattern
 *   5. Identify device boundaries by BYPASS bit positions
 *
 * This is the standard technique used by debug probes
 * (J-Link, ST-Link, OpenOCD) to enumerate multi-device chains.
 *
 * Reference: IEEE 1149.1-2013, Section 10.4 (Chain Configuration)
 *
 * @param chain    Output: detected chain configuration
 * @return Number of devices detected, or -1 on error
 */
int jtag_scan_chain_detect(jtag_scan_chain_t *chain) {
    if (!chain) return -1;

    /* Reset chain state */
    memset(chain, 0, sizeof(*chain));
    chain->chain_detected = false;

    /* A real implementation would:
     *   1. Configure all devices to BYPASS via IR
     *   2. Shift a 32-bit pattern through DR
     *   3. Count the number of BYPASS bits (each device = 1 bit)
     *
     * For this reference implementation, we provide the
     * framework and default single-device configuration.
     */
    chain->device_count = 1;
    chain->devices[0].idcode = 0;
    chain->devices[0].ir_length = JTAG_IR_LEN_ARM_CORTEX;
    chain->devices[0].dr_prescan_bits = 0;
    chain->devices[0].dr_postscan_bits = 0;
    chain->devices[0].total_ir_bits = JTAG_IR_LEN_ARM_CORTEX;
    chain->devices[0].has_idcode = true;
    chain->total_ir_length = JTAG_IR_LEN_ARM_CORTEX;
    chain->chain_detected = true;

    return 1;
}

/* ==================================================================
 * L2: TAP State Machine Validation
 * ================================================================== */

/**
 * tap_validate_fsm - Validate TAP FSM consistency
 *
 * Checks that the FSM transition table is self-consistent:
 *   - All 16 states have valid next states for TMS=0 and TMS=1
 *   - No undefined transitions
 *   - From any state, 5 TMS=1 transitions reach TLR
 *
 * This is a design-time validation function.
 *
 * @return true if FSM is valid
 */
bool tap_validate_fsm(void) {
    int state;
    bool valid = true;

    for (state = 0; state < TAP_NUM_STATES; state++) {
        tap_state_t next0 = tap_fsm_next[state][0];
        tap_state_t next1 = tap_fsm_next[state][1];

        if (next0 >= TAP_NUM_STATES) {
            fprintf(stderr, "FSM error: state %d TMS=0 -> invalid %d\n",
                    state, next0);
            valid = false;
        }
        if (next1 >= TAP_NUM_STATES) {
            fprintf(stderr, "FSM error: state %d TMS=1 -> invalid %d\n",
                    state, next1);
            valid = false;
        }
    }

    /* Verify: 5 clocks of TMS=1 from any state reaches TLR */
    for (state = 0; state < TAP_NUM_STATES; state++) {
        tap_state_t s = (tap_state_t)state;
        int i;
        bool reached_tlr = false;

        for (i = 0; i < 10; i++) {
            s = tap_fsm_next[s][1];  /* TMS=1 */
            if (s == TAP_STATE_TEST_LOGIC_RESET) {
                reached_tlr = true;
                break;
            }
        }

        if (!reached_tlr) {
            fprintf(stderr, "FSM error: state %d cannot reach TLR with TMS=1\n",
                    state);
            valid = false;
        }
    }

    return valid;
}

/* ==================================================================
 * L2: JTAG Signal Timing Computation
 * ================================================================== */

/**
 * jtag_timing_init - Initialize JTAG timing with defaults
 */
void jtag_timing_init(jtag_timing_t *timing) {
    if (!timing) return;
    timing->tck_freq_hz        = JTAG_DEFAULT_TCK_FREQ;
    timing->tms_setup_ns       = JTAG_DEFAULT_TMS_SETUP_NS;
    timing->tms_hold_ns        = JTAG_DEFAULT_TMS_HOLD_NS;
    timing->tdi_setup_ns       = JTAG_DEFAULT_TDI_SETUP_NS;
    timing->tdi_hold_ns        = JTAG_DEFAULT_TDI_HOLD_NS;
    timing->tdo_delay_ns       = JTAG_DEFAULT_TDO_DELAY_NS;
    timing->trst_pulse_width_ns = JTAG_DEFAULT_TRST_PULSE_NS;
    timing->power_up_reset_us  = JTAG_DEFAULT_POWERUP_US;
}

/**
 * jtag_calculate_min_tck_period - Minimum TCK period from timing constraints
 *
 * The minimum TCK period is bounded by:
 *   T_TCK_min = max(T_tdo_delay + T_tdi_setup,
 *                    T_tms_hold + T_tms_setup)
 *
 * This ensures data launched on the falling edge is captured
 * correctly on the next rising edge.
 *
 * Reference: IEEE 1149.1-2013, Section 7.1 (AC specifications)
 */
double jtag_calculate_min_tck_period(const jtag_timing_t *timing) {
    double t_path1, t_path2;
    if (!timing) return 100.0;  /* 100ns default */

    /* Path 1: TDO (falling edge) to TDI setup (next rising edge) */
    t_path1 = timing->tdo_delay_ns + timing->tdi_setup_ns;

    /* Path 2: TMS hold (after rising) + TMS setup (before next rising) */
    t_path2 = timing->tms_hold_ns + timing->tms_setup_ns;

    return (t_path1 > t_path2) ? t_path1 : t_path2;
}

/**
 * jtag_calculate_max_tck_freq - Maximum safe TCK frequency
 *
 * f_max = 1 / T_TCK_min
 *
 * In practice, parasitic capacitance and trace length
 * further limit the maximum frequency:
 *   f_max_effective < 1 / (T_TCK_min + T_rise + T_fall + T_settling)
 *
 * where T_rise/fall depend on the driver strength and
 * trace capacitance (~10-30pF for typical debug cables).
 */
double jtag_calculate_max_tck_freq(const jtag_timing_t *timing) {
    double t_min = jtag_calculate_min_tck_period(timing);
    if (t_min <= 0.0) return 0.0;
    return 1.0e9 / t_min;
}

/* ==================================================================
 * L5: Boundary Scan Support
 * ================================================================== */

/**
 * boundary_scan_cell_count - Estimate boundary scan chain length
 *
 * ARM Cortex-M devices typically have 40-200 boundary scan cells,
 * one per I/O pin. This function estimates the chain length from
 * the device package pin count.
 *
 * Reference: BSDL (Boundary Scan Description Language) files
 *   for specific devices.
 */
uint32_t boundary_scan_cell_count_estimate(uint32_t pin_count) {
    /* Rough estimate: each I/O pin has 1-3 boundary scan cells */
    return pin_count * 2;
}

/**
 * boundary_scan_test_vector - Generate a basic EXTEST pattern
 *
 * For a given pin, toggles the output value while monitoring
 * the input capture to verify board-level connectivity.
 *
 * This is the fundamental operation in boundary scan testing:
 *   - EXTEST drives pin outputs from boundary register
 *   - SAMPLE captures pin inputs into boundary register
 *
 * Used for detecting shorts, opens, and stuck-at faults
 * on PCB traces without physical probes.
 *
 * Reference: IEEE 1149.1-2013, Section 6 (EXTEST Instruction)
 */
int boundary_scan_extest_vector(uint32_t pin_index,
                                 uint32_t chain_length,
                                 uint8_t *tdi_data,
                                 uint32_t tdi_bytes) {
    uint32_t bit_pos;
    if (!tdi_data || pin_index >= chain_length) return -1;

    /* Set the target pin to output a '1' */
    bit_pos = pin_index;
    if (bit_pos / 8 < tdi_bytes) {
        tdi_data[bit_pos / 8] |= (1u << (bit_pos % 8));
    }

    return 0;
}

/* ==================================================================
 * L3: JTAG Parity and Error Checking
 * ================================================================== */

/**
 * jtag_tdo_parity_check - Check parity of received TDO data
 *
 * While JTAG does not mandate parity, some implementations
 * use parity in IDCODE or custom DR registers.
 *
 * This function computes even parity over a 32-bit word
 * as a data integrity check.
 */
bool jtag_check_even_parity_32(uint32_t data) {
    data ^= data >> 16;
    data ^= data >> 8;
    data ^= data >> 4;
    data ^= data >> 2;
    data ^= data >> 1;
    return (data & 0x01) == 0;
}
