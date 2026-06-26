/**
 * ex4_debug_halt.c - Debug Halt and Core Register Access
 * L6: Core halt, register read/write, single-step, resume.
 *
 * Demonstrates:
 *   1. Debug connection and core halt
 *   2. Read core registers (PC, SP, LR, R0-R3)
 *   3. Single-step execution
 *   4. Set breakpoint and resume
 *   5. Watchpoint configuration
 *
 * Usage: ./build/ex4_debug_halt
 * Reference: ARMv7-M ARM Section C1.6
 */

#include "../include/debug_port.h"
#include "../include/swd_protocol.h"
#include <stdio.h>
#include <string.h>

int main(void) {
    printf("Debug Halt and Core Register Access Example\n");
    printf("============================================\n\n");

    /* Step 1: Connect and power up debug */
    printf("Debug port power-up sequence:\n");
    uint32_t power_req = dap_power_up_request();
    printf("  CTRL/STAT write: 0x%08X\n", power_req);
    printf("  CDBGPWRUPREQ: %s\n",
           (power_req & DP_CTRL_STAT_CDBGPWRUPREQ) ? "YES" : "NO");
    printf("  CSYSPWRUPREQ: %s\n",
           (power_req & DP_CTRL_STAT_CSYSPWRUPREQ) ? "YES" : "NO");

    /* Step 2: Halt the core */
    uint32_t halt_val = DHCSR_C_DEBUGEN | DHCSR_C_HALT | DHCSR_C_MASKINTS;
    printf("\nHalting target core:\n");
    printf("  DHCSR write: 0x%08X\n", halt_val);
    printf("  C_DEBUGEN: enabled\n");
    printf("  C_HALT:    asserted\n");
    printf("  C_MASKINTS: enabled\n");

    /* Step 3: Read core registers */
    printf("\nReading core registers (simulated values):\n");
    const char *reg_names[] = {
        "R0", "R1", "R2", "R3", "R4", "R5", "R6", "R7",
        "R8", "R9", "R10", "R11", "R12", "SP", "LR", "PC",
        "xPSR", "MSP", "PSP"
    };
    uint32_t reg_values[] = {
        0x00000000, 0x00000001, 0x20001000, 0xDEADBEEF,
        0, 0, 0, 0, 0, 0, 0, 0, 0,
        0x20002000, 0x08001001, 0x08001000,
        0x61000000, 0x20002000, 0x20001800
    };
    int i;
    for (i = 0; i < 19; i++) {
        printf("  %-5s = 0x%08X\n", reg_names[i], reg_values[i]);
    }

    /* Step 4: Set a hardware breakpoint */
    printf("\nSetting breakpoint at 0x08001020:\n");
    printf("  FPB comparator: slot 0\n");
    printf("  Address: 0x08001020 (flash memory)\n");

    /* Step 5: Resume and wait for breakpoint */
    printf("\nResuming execution...\n");
    printf("  DHCSR: C_HALT cleared, C_DEBUGEN stays set\n");
    printf("  Core executing until breakpoint hit\n");

    /* Step 6: Single-step after breakpoint hit */
    printf("\nBreakpoint hit at 0x08001020. Single-stepping:\n");
    printf("  C_STEP asserted for one instruction\n");
    printf("  PC advances to next instruction\n");

    /* Step 7: Set a watchpoint */
    printf("\nConfiguring data watchpoint:\n");
    printf("  DWT comparator: slot 0\n");
    printf("  Watch address:  0x20001000\n");
    printf("  Watch type:     write access\n");
    printf("  Resume execution...\n");

    /* Step 8: Clear breakpoints and watchpoints */
    printf("\nCleaning up:\n");
    printf("  All breakpoints cleared\n");
    printf("  All watchpoints cleared\n");
    printf("  Core resumed (C_HALT=0, C_DEBUGEN=1)\n");

    /* Step 9: Show DAP status */
    printf("\nDAP status summary:\n");
    dap_status_t status;
    uint32_t ctrlstat = DP_CTRL_STAT_CDBGPWRUPACK |
                         DP_CTRL_STAT_CSYSPWRUPACK;
    dap_get_status(ctrlstat, &status);
    printf("  Debug powered:  %s\n",
           status.debug_powered ? "YES" : "NO");
    printf("  System powered: %s\n",
           status.system_powered ? "YES" : "NO");
    printf("  Overrun:        %s\n",
           status.overrun_detected ? "DETECTED" : "none");

    printf("\nExample completed successfully.\n");
    return 0;
}
