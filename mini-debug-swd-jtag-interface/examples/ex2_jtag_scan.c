/**
 * ex2_jtag_scan.c - JTAG Boundary Scan Demo
 * L6: JTAG scan chain detection, IDCODE scan, TAP navigation.
 *
 * Demonstrates:
 *   1. TAP reset to Test-Logic-Reset
 *   2. Navigate through TAP states
 *   3. IR scan to load IDCODE instruction
 *   4. DR scan to read device ID
 *   5. Multi-device chain detection concept
 *
 * Usage: ./build/ex2_jtag_scan
 * Reference: IEEE 1149.1-2013
 */

#include "../include/jtag_tap.h"
#include <stdio.h>
#include <string.h>

int main(void) {
    printf("JTAG Boundary Scan and Chain Detection Example\n");
    printf("==============================================\n\n");

    /* Step 1: Validate TAP FSM */
    if (!tap_validate_fsm()) {
        printf("ERROR: TAP FSM validation failed!\n");
        return 1;
    }
    printf("TAP FSM validated: all 16 states reachable\n");

    /* Step 2: Show TAP state machine */
    printf("\nTAP State Machine (16 states, IEEE 1149.1):\n");
    int state;
    for (state = 0; state < TAP_NUM_STATES; state++) {
        tap_state_t next0 = tap_fsm_next[state][0];
        tap_state_t next1 = tap_fsm_next[state][1];
        printf("  %-20s TMS=0 -> %-20s TMS=1 -> %s\n",
               tap_state_get_name((tap_state_t)state),
               tap_state_get_name(next0),
               tap_state_get_name(next1));
    }

    /* Step 3: Demonstrate TAP navigation: TLR -> SHIFT-IR */
    printf("\nNavigating from TLR to SHIFT-IR:\n");
    uint8_t tms_seq[16];
    int steps = tap_navigate_to(TAP_STATE_TEST_LOGIC_RESET,
                                 TAP_STATE_SHIFT_IR, tms_seq, 16);
    printf("  Steps needed: %d\n", steps);
    printf("  TMS sequence: ");
    int i;
    for (i = 0; i < steps; i++) printf("%d ", tms_seq[i]);
    printf("\n");
    tap_state_t current = TAP_STATE_TEST_LOGIC_RESET;
    for (i = 0; i < steps; i++) {
        tap_state_t next = tap_get_next_state(current, tms_seq[i] != 0);
        printf("    %s --TMS=%d--> %s\n",
               tap_state_get_name(current), tms_seq[i],
               tap_state_get_name(next));
        current = next;
    }

    /* Step 4: Generate IDCODE scan sequence */
    printf("\nGenerating IDCODE scan sequence:\n");
    uint8_t full_seq[128];
    int dr_start;
    int total = tap_idcode_scan_sequence_generate(
        JTAG_IR_LEN_ARM_CORTEX,  /* 4-bit IR for ARM Cortex */
        ARM_INSTR_IDCODE,         /* IDCODE instruction = 0x0E */
        full_seq, 128, &dr_start);
    printf("  Total TCK cycles: %d\n", total);
    printf("  IR scan: cycles 0-%d (instruction 0x%X)\n",
           dr_start - 1, ARM_INSTR_IDCODE);
    printf("  DR scan: cycles %d-%d (32-bit IDCODE)\n",
           dr_start, total - 1);

    /* Step 5: Demonstrate IDCODE decoding */
    printf("\nDecoding known ARM Cortex-M IDCODEs:\n");
    uint32_t known_ids[] = {
        ARM_IDCODE_CORTEX_M0, ARM_IDCODE_CORTEX_M3,
        ARM_IDCODE_CORTEX_M4, ARM_IDCODE_CORTEX_M7
    };
    const char *names[] = {"Cortex-M0", "Cortex-M3", "Cortex-M4", "Cortex-M7"};
    for (i = 0; i < 4; i++) {
        jtag_idcode_t id;
        jtag_idcode_decode(known_ids[i], &id);
        printf("  %-12s: Version=%d Part=0x%04X Manuf=0x%03X Valid=%s\n",
               names[i], id.version, id.part_number, id.manufacturer,
               id.lsb_valid ? "YES" : "NO");
    }

    /* Step 6: Scan chain detection demo */
    printf("\nScan chain detection (simulated):\n");
    jtag_scan_chain_t chain;
    int devices = jtag_scan_chain_detect(&chain);
    printf("  Devices detected: %d\n", devices);
    printf("  Total IR length:  %d bits\n", chain.total_ir_length);
    for (i = 0; i < chain.device_count && i < JTAG_MAX_DEVICES_IN_CHAIN; i++) {
        printf("  Device %d: IR=%d bits, has_idcode=%s\n",
               i, chain.devices[i].ir_length,
               chain.devices[i].has_idcode ? "YES" : "NO");
    }

    printf("\nExample completed successfully.\n");
    return 0;
}
