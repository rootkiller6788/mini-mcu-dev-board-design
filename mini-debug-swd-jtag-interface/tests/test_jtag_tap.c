/**
 * test_jtag_tap.c - JTAG TAP Controller Unit Tests
 * Tests: FSM transitions, navigation, scan sequence generation.
 */
#include "../include/jtag_tap.h"
#include <stdio.h>
#include <assert.h>
#include <string.h>

static int passed = 0, failed = 0;
#define T(name) do { printf("  TEST: %s ... ", name); } while(0)
#define P() do { printf("PASS\n"); passed++; } while(0)
#define F(m) do { printf("FAIL: %s\n", m); failed++; } while(0)
#define C(c,m) do { if (c) P(); else F(m); } while(0)

static void test_fsm_transitions(void) {
    T("TLR + TMS=0 => RTI");
    C(tap_fsm_next[TAP_STATE_TEST_LOGIC_RESET][0] == TAP_STATE_RUN_TEST_IDLE, "wrong");
    T("TLR + TMS=1 => TLR");
    C(tap_fsm_next[TAP_STATE_TEST_LOGIC_RESET][1] == TAP_STATE_TEST_LOGIC_RESET, "wrong");
    T("SHIFT-DR + TMS=0 => SHIFT-DR");
    C(tap_fsm_next[TAP_STATE_SHIFT_DR][0] == TAP_STATE_SHIFT_DR, "wrong");
    T("SHIFT-DR + TMS=1 => EXIT1-DR");
    C(tap_fsm_next[TAP_STATE_SHIFT_DR][1] == TAP_STATE_EXIT1_DR, "wrong");
}

static void test_navigation(void) {
    T("TLR to RTI (1 step)");
    uint8_t seq[16];
    int n = tap_navigate_to(TAP_STATE_TEST_LOGIC_RESET,
                             TAP_STATE_RUN_TEST_IDLE, seq, 16);
    C(n == 1 && seq[0] == 0, "TLR->RTI should be TMS=0, 1 step");

    T("TLR to TLR (0 steps)");
    n = tap_navigate_to(TAP_STATE_TEST_LOGIC_RESET,
                         TAP_STATE_TEST_LOGIC_RESET, seq, 16);
    C(n == 0, "TLR->TLR should be 0 steps");

    T("NULL buffer");
    n = tap_navigate_to(TAP_STATE_TEST_LOGIC_RESET,
                         TAP_STATE_RUN_TEST_IDLE, NULL, 16);
    C(n == -1, "NULL buffer should return -1");
}

static void test_stable_states(void) {
    T("TLR is stable");
    C(tap_is_stable_state(TAP_STATE_TEST_LOGIC_RESET), "TLR not stable");
    T("RTI is stable");
    C(tap_is_stable_state(TAP_STATE_RUN_TEST_IDLE), "RTI not stable");
    T("SHIFT-DR is NOT stable");
    C(!tap_is_stable_state(TAP_STATE_SHIFT_DR), "SHIFT-DR should not be stable");
}

static void test_scan_sequences(void) {
    T("IR scan sequence length");
    uint8_t seq[64];
    int start;
    int len = tap_ir_scan_sequence_generate(4, seq, 64, &start);
    C(len == 4 + 4 + 3, "IR scan length wrong");

    T("DR scan sequence length");
    len = tap_dr_scan_sequence_generate(32, seq, 64, &start);
    C(len == 3 + 32 + 3, "DR scan length wrong");

    T("Buffer too small");
    len = tap_ir_scan_sequence_generate(100, seq, 10, &start);
    C(len == -1, "should reject buffer too small");
}

static void test_idcode_decode(void) {
    T("IDCODE decode ARM Cortex-M4");
    jtag_idcode_t id;
    jtag_idcode_decode(ARM_IDCODE_CORTEX_M4, &id);
    C(id.lsb_valid == true, "LSB should be valid");
    C(id.manufacturer != 0, "manufacturer should not be 0");
}

static void test_fsm_validation(void) {
    T("FSM self-validation");
    C(tap_validate_fsm() == true, "FSM validation failed");
}

int main(void) {
    printf("JTAG TAP Unit Tests\n");
    printf("===================\n\n");
    test_fsm_transitions();
    test_navigation();
    test_stable_states();
    test_scan_sequences();
    test_idcode_decode();
    test_fsm_validation();
    printf("\nResults: %d passed, %d failed\n", passed, failed);
    return failed > 0 ? 1 : 0;
}
