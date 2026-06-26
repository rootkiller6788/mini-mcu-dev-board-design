/**
 * ex1_swd_connect.c - SWD Connect and IDCODE Read
 * L6: Complete SWD connection sequence and device identification.
 *
 * Demonstrates:
 *   1. SWD line reset (50+ clocks with SWDIO=1)
 *   2. Connection sequence (0xE79E ARM magic word)
 *   3. DPIDR read to identify target
 *   4. Device type detection from IDCODE
 *
 * Usage: ./build/ex1_swd_connect
 * Reference: ARM ADIv5.2 Section B5.2
 */

#include "../include/swd_protocol.h"
#include "../include/jtag_tap.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

int main(void) {
    printf("SWD Connect and IDCODE Read Example\n");
    printf("====================================\n\n");

    /* Step 1: Initialize SWD timing parameters */
    swd_timing_params_t timing;
    swd_protocol_reset_state(&timing);
    printf("SWD timing initialized: %.1f MHz SWCLK\n",
           timing.swclk_freq_hz / 1e6);

    /* Step 2: Generate line reset pattern (50+ clocks with SWDIO=1) */
    uint8_t reset_pattern[80];
    int reset_bits = swd_line_reset_generate(SWD_LINE_RESET_CLOCKS_SAFE,
                                              reset_pattern, 80);
    printf("Line reset: %d clock cycles with SWDIO=1\n", reset_bits);

    /* Step 3: Generate connection sequence */
    uint8_t connect_pattern[16];
    int conn_bits = swd_connection_sequence_generate(SWD_VERSION_V1, 0,
                                                      connect_pattern, 16);
    printf("Connection sequence: %d bits (0x%04X)\n",
           conn_bits, SWD_CONNECT_SEQ_ARM);

    /* Step 4: Prepare DPIDR read transaction */
    swd_transaction_t txn;
    swd_dp_read_prepare(DP_REG_DPIDR, &txn);
    printf("DPIDR read prepared: request_byte=0x%02X\n",
           swd_build_request_byte(0, 1, DP_REG_DPIDR));

    /* Step 5: Simulate successful read */
    txn.ack = SWD_ACK_OK;
    txn.data = (DP_DPIDR_VERSION_ADIv5 << 16) |
               (DP_DPIDR_DESIGNER_ARM << 6) | 0x01;
    txn.parity_error = false;

    printf("\nTransaction result:\n");
    printf("  ACK: %s\n", swd_ack_to_string(txn.ack));
    printf("  Data: 0x%08X\n", txn.data);
    printf("  Parity error: %s\n", txn.parity_error ? "YES" : "NO");

    /* Step 6: Verify DPIDR */
    bool valid = swd_verify_dpidr(txn.data);
    printf("  DPIDR valid: %s\n", valid ? "YES" : "NO");

    if (valid) {
        uint32_t version  = (txn.data & DP_DPIDR_VERSION_MASK) >> 16;
        uint32_t revision = (txn.data & DP_DPIDR_REVISION_MASK) >> 28;
        uint32_t designer = (txn.data & DP_DPIDR_DESIGNER_MASK) >> 6;
        uint32_t partno   = (txn.data & DP_DPIDR_PARTNO_MASK) >> 20;
        printf("\nDPIDR Decoded:\n");
        printf("  ADI Version: v%d\n", version);
        printf("  Revision:    r%dp%d\n", revision >> 2, revision & 0x3);
        printf("  Designer:    0x%03X (ARM JEP106)\n", designer);
        printf("  Part Number: 0x%04X\n", partno);

        printf("\nTarget Identification:\n");
        if (version == DP_DPIDR_VERSION_ADIv5)
            printf("  Protocol: SWD v1 (ADIv5)\n");
        else if (version == DP_DPIDR_VERSION_ADIv6)
            printf("  Protocol: SWD v2 (ADIv6)\n");

        printf("  Max SWCLK: %.0f MHz\n",
               version == 1 ? SWD_V1_MAX_FREQ_HZ / 1e6
                            : SWD_V2_MAX_FREQ_HZ / 1e6);
    }

    /* Step 7: Show equivalent JTAG IDCODE for comparison */
    jtag_idcode_t jtag_id;
    jtag_idcode_decode(ARM_IDCODE_CORTEX_M4, &jtag_id);
    printf("\nEquivalent JTAG IDCODE (ARM Cortex-M4):\n");
    printf("  Raw:      0x%08X\n", ARM_IDCODE_CORTEX_M4);
    printf("  Version:  %d\n", jtag_id.version);
    printf("  Part No:  0x%04X\n", jtag_id.part_number);
    printf("  Valid:    %s\n", jtag_id.lsb_valid ? "YES" : "NO");

    printf("\nExample completed successfully.\n");
    return 0;
}
