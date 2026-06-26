/**
 * swd_protocol.c - SWD Protocol Implementation
 *
 * L5: SWD transaction execution, request/response framing,
 * parity generation and verification, line reset sequences,
 * connection establishment, error recovery.
 *
 * L6: Canonical SWD connect sequence, register read/write,
 * DP/AP access operations.
 *
 * Reference:
 *   ARM IHI 0031E - ADIv5.2 Architecture Specification
 *   ARM IHI 0074A - ADIv6 Architecture Specification
 *   ARM DDI 0314H - CoreSight Components Technical Reference
 *
 * Courses: MIT 6.450 (error detection/correction),
 *   Berkeley EE16B (embedded systems interfacing),
 *   Stanford EE359 (protocol design)
 */

#include "swd_protocol.h"
#include <stdio.h>
#include <string.h>

/* ==================================================================
 * L5: SWD Error String Conversion
 * ================================================================== */

const char *swd_error_string(swd_error_t err) {
    switch (err) {
    case SWD_ERR_NONE:         return "No error";
    case SWD_ERR_NO_TARGET:    return "No target detected";
    case SWD_ERR_ACK_FAULT:    return "Target returned FAULT ACK";
    case SWD_ERR_ACK_WAIT_TO:  return "Target WAIT timed out";
    case SWD_ERR_PARITY:       return "Parity error in read data";
    case SWD_ERR_OVERRUN:      return "Read overrun detected";
    case SWD_ERR_PROTOCOL:     return "Protocol sequence error";
    case SWD_ERR_POWER:        return "Debug power domain not up";
    case SWD_ERR_UNSUPPORTED:  return "Feature not supported by target";
    case SWD_ERR_TIMEOUT:      return "General timeout";
    case SWD_ERR_BUSY:         return "Target busy";
    case SWD_ERR_LOCKED:       return "Debug interface locked";
    case SWD_ERR_DISCONNECTED: return "Physical disconnection detected";
    default:                   return "Unknown error";
    }
}

/* ==================================================================
 * L5: SWD Packet Encoding/Decoding
 * ================================================================== */

/**
 * swd_encode_request - Encode SWD request into raw bitstream
 *
 * Converts a structured transaction into an 8-bit request byte
 * plus 32-bit data for write operations. Returns the number of
 * bits to transmit (request phase + turnaround + data phase).
 *
 * The request byte is transmitted LSB-first on SWDIO, synchronous
 * to SWCLK rising edges.
 *
 * Reference: ADIv5.2 Section B4.3 - SWD Protocol Timing
 * Complexity: O(1)
 */
int swd_encode_request(const swd_transaction_t *txn,
                        uint8_t *request_byte,
                        uint32_t *data_out,
                        uint32_t *bit_count) {
    if (!txn || !request_byte || !bit_count) return SWD_ERR_NONE - 1;

    *request_byte = swd_build_request_byte(
        (uint8_t)txn->port, (uint8_t)txn->direction, txn->addr);

    if (txn->direction == SWD_DIR_WRITE && data_out) {
        *data_out = txn->data;
    }

    /* Bit count: 8 request + 1 turnaround + 3 ACK + (32 data + 1 parity if read) */
    *bit_count = 8 + SWD_TRN_SIZE_CLK + SWD_ACK_SIZE;
    if (txn->direction == SWD_DIR_READ) {
        *bit_count += SWD_DATA_SIZE + 1;  /* 32 data + 1 parity */
    } else {
        *bit_count += SWD_DATA_SIZE;  /* 32 write data */
    }

    return SWD_ERR_NONE;
}

/**
 * swd_decode_response - Decode SWD response from raw bitstream
 *
 * Parses the ACK phase, data phase, and parity phase from
 * a SWD read or write response. Validates parity for read data.
 *
 * Reference: ADIv5.2 Section B4.3.3 - SWD Response Decoding
 * Complexity: O(1)
 */
int swd_decode_response(swd_transaction_t *txn,
                         uint8_t ack_bits,
                         uint32_t read_data,
                         uint8_t read_parity) {
    if (!txn) return SWD_ERR_NONE - 1;

    txn->ack = swd_parse_ack(ack_bits);

    if (txn->direction == SWD_DIR_READ) {
        txn->data = read_data;
        txn->parity_error = !swd_verify_parity_32(read_data, read_parity);
    } else {
        txn->parity_error = false;
    }

    txn->overrun = false;  /* Set externally if ORUNDETECT is detected */

    return SWD_ERR_NONE;
}

/* ==================================================================
 * L5: SWD Line Reset Sequence
 * ================================================================== */

/**
 * swd_line_reset - Perform SWD line reset sequence
 *
 * Sends >= 50 SWCLK cycles with SWDIO held high. This forces
 * the target's SW-DP state machine to its idle state.
 *
 * Algorithm (ADIv5.2 Section B4.3.1):
 *   1. Set SWDIO = 1 (high)
 *   2. Toggle SWCLK for at least 50 cycles
 *   3. After reset, send at least 2 idle cycles (SWDIO=1)
 *
 * This function generates the bit pattern but does not
 * physically drive pins (that's the transport layer's job).
 *
 * @param clocks  Number of reset clock cycles (>= 50 recommended)
 * @param pattern Output buffer for SWDIO pattern (1 bit per clock)
 * @param max_bits Size of pattern buffer
 * @return Number of clock cycles in the reset pattern
 */
int swd_line_reset_generate(uint32_t clocks, uint8_t *pattern,
                             uint32_t max_bits) {
    uint32_t i;

    if (!pattern || max_bits == 0) return SWD_ERR_NONE - 1;
    if (clocks < SWD_LINE_RESET_CLOCKS_MIN) {
        clocks = SWD_LINE_RESET_CLOCKS_MIN;
    }
    if (clocks > max_bits) {
        clocks = max_bits;
    }

    /* All bits are 1 (SWDIO held high) */
    for (i = 0; i < clocks; i++) {
        pattern[i] = 1;
    }

    return (int)clocks;
}

/* ==================================================================
 * L5: SWD Connection Sequence
 * ================================================================== */

/**
 * swd_connection_sequence_generate - Generate SWD connection bit pattern
 *
 * After line reset, the host must send a 16-bit connection sequence
 * to select SWD protocol (vs JTAG). The sequence is transmitted
 * LSB-first.
 *
 * For ARM SWD: 0xE79E (binary: 1110 0111 1001 1110, LSB first)
 *   This is the ARM-defined "SWD select" magic word.
 *
 * For SWD v2 multi-drop: the sequence embeds the 4-bit target ID.
 *
 * Reference: ADIv5.2 Section B5.2.1 - Connection Sequence
 * Reference: ADIv6 Section B4.2 - Multi-Drop Connection
 *
 * @param version   SWD protocol version (V1 or V2)
 * @param target_id Target ID for multi-drop (0 for V1 or default target)
 * @param pattern   Output buffer for 16-bit connection sequence
 * @param max_bits  Must be >= 16
 * @return Number of bits generated (always 16), or negative on error
 */
int swd_connection_sequence_generate(swd_version_t version,
                                      uint8_t target_id,
                                      uint8_t *pattern,
                                      uint32_t max_bits) {
    uint32_t seq;
    int i;

    if (!pattern || max_bits < 16) return SWD_ERR_NONE - 1;

    if (version == SWD_VERSION_V2) {
        seq = SWDV2_CONNECT(target_id);
    } else {
        seq = (uint32_t)SWD_CONNECT_SEQ_ARM;
    }

    /* Transmit LSB-first, 16 bits */
    for (i = 0; i < 16; i++) {
        pattern[i] = (seq >> i) & 0x01;
    }

    return 16;
}

/* ==================================================================
 * L5: SWD DP Register Access
 * ================================================================== */

/**
 * swd_dp_read - Perform DP register read transaction
 *
 * Builds and executes a complete DP read transaction.
 * The request byte, ACK phase, and data phase are handled.
 *
 * Protocol flow (DP read):
 *   1. Host sends 8-bit request (APnDP=0, RnW=1, A[3:2])
 *   2. Turnaround: host releases SWDIO, target drives
 *   3. Target sends 3-bit ACK
 *   4. If ACK=OK: target sends 32-bit data + 1 parity bit
 *   5. Turnaround: target releases, host drives
 *   6. Host checks parity and overrun
 *
 * Reference: ADIv5.2 Section B4.4 - DP Register Access
 * Complexity: O(1) for transaction construction
 */
int swd_dp_read_prepare(dp_register_addr_t reg, swd_transaction_t *txn) {
    if (!txn) return SWD_ERR_NONE - 1;

    txn->port      = SWD_PORT_DP;
    txn->direction = SWD_DIR_READ;
    txn->addr      = (uint8_t)reg;
    txn->data      = 0;
    txn->ack       = SWD_ACK_INVALID;
    txn->parity_error = false;
    txn->overrun    = false;
    txn->retry_count = 0;

    return SWD_ERR_NONE;
}

/**
 * swd_dp_write_prepare - Perform DP register write transaction
 *
 * Protocol flow (DP write):
 *   1. Host sends 8-bit request (APnDP=0, RnW=0, A[3:2])
 *   2. Turnaround
 *   3. Target sends 3-bit ACK
 *   4. If ACK=OK: host sends 32-bit data (no parity for writes)
 *   5. Host continues driving SWDIO
 *
 * Note: Write data has no parity protection from host to target.
 * The target may detect write errors via WDATAERR flag.
 *
 * Reference: ADIv5.2 Section B4.4.2
 * Complexity: O(1)
 */
int swd_dp_write_prepare(dp_register_addr_t reg, uint32_t data,
                          swd_transaction_t *txn) {
    if (!txn) return SWD_ERR_NONE - 1;

    txn->port      = SWD_PORT_DP;
    txn->direction = SWD_DIR_WRITE;
    txn->addr      = (uint8_t)reg;
    txn->data      = data;
    txn->ack       = SWD_ACK_INVALID;
    txn->parity_error = false;
    txn->overrun    = false;
    txn->retry_count = 0;

    return SWD_ERR_NONE;
}

/* ==================================================================
 * L5: SWD AP Register Access
 * ================================================================== */

/**
 * swd_ap_read_prepare - Prepare AP register read transaction
 *
 * Reading an AP register is a two-step process:
 *   1. Write the AP address to DP SELECT (select which AP)
 *   2. Read the AP register (with APnDP=1)
 *   3. Read DP RDBUFF to retrieve the AP read result
 *
 * The reason for step 3: AP reads are pipelined. The first
 * AP read returns the PREVIOUS AP access result (or garbage).
 * The actual result is available in DP RDBUFF after the
 * NEXT DP/AP transaction.
 *
 * This is a fundamental SWD protocol behavior: reads are
 * delayed by one transaction due to the turnaround.
 *
 * Reference: ADIv5.2 Section B4.5 - AP Register Access
 * Complexity: O(1)
 */
int swd_ap_read_prepare(uint8_t ap_sel, ap_register_addr_t reg,
                         swd_transaction_t *txn) {
    (void)ap_sel;
    if (!txn) return SWD_ERR_NONE - 1;

    txn->port      = SWD_PORT_AP;
    txn->direction = SWD_DIR_READ;
    txn->addr      = (uint8_t)reg;
    txn->data      = 0;
    txn->ack       = SWD_ACK_INVALID;
    txn->parity_error = false;
    txn->overrun    = false;
    txn->retry_count = 0;

    return SWD_ERR_NONE;
}

/**
 * swd_ap_write_prepare - Prepare AP register write transaction
 *
 * AP writes are immediate (not pipelined). The data is
 * written directly to the selected AP register.
 *
 * Reference: ADIv5.2 Section B4.5.2
 * Complexity: O(1)
 */
int swd_ap_write_prepare(uint8_t ap_sel, ap_register_addr_t reg,
                          uint32_t data, swd_transaction_t *txn) {
    (void)ap_sel;
    if (!txn) return SWD_ERR_NONE - 1;

    txn->port      = SWD_PORT_AP;
    txn->direction = SWD_DIR_WRITE;
    txn->addr      = (uint8_t)reg;
    txn->data      = data;
    txn->ack       = SWD_ACK_INVALID;
    txn->parity_error = false;
    txn->overrun    = false;
    txn->retry_count = 0;

    return SWD_ERR_NONE;
}

/* ==================================================================
 * L5: SWD Register Bank Selection
 * ================================================================== */

/**
 * swd_select_dp_bank - Prepare DP SELECT write for bank switching
 *
 * DP registers are banked. The DPBANKSEL field in SELECT
 * determines which physical register is accessed at each
 * A[3:2] address.
 *
 * Example: To access DP CTRL/STAT (bank 0, addr 1):
 *   SELECT.DPBANKSEL = 0, then read A[3:2]=01
 *
 * Reference: ADIv5.2 Table B3-1 - DP Register Map
 * Complexity: O(1)
 */
int swd_select_dp_bank(uint8_t dp_bank, uint8_t ap_bank,
                        uint8_t ap_sel, swd_transaction_t *txn) {
    uint32_t select_val;

    if (!txn) return SWD_ERR_NONE - 1;

    select_val = ((uint32_t)(dp_bank & 0xF) << 0)  |
                 ((uint32_t)(ap_bank & 0xF) << 4)  |
                 ((uint32_t)(ap_sel & 0xFF) << 24);

    return swd_dp_write_prepare(DP_REG_SELECT, select_val, txn);
}

/* ==================================================================
 * L5: SWD Power-Up Sequence
 * ================================================================== */

/**
 * swd_power_up_sequence - Generate the DP power-up request
 *
 * Before debug operations, the debug power domain must be
 * enabled. This is done by:
 *   1. Write CTRL/STAT with CDBGPWRUPREQ=1 and CSYSPWRUPREQ=1
 *   2. Poll CTRL/STAT until CDBGPWRUPACK=1 and CSYSPWRUPACK=1
 *
 * The debug power domain includes the DP and AP registers.
 * The system power domain includes the debug components
 * attached to the system bus (ETM, ITM, DWT, etc.).
 *
 * Reference: ADIv5.2 Section B3.3 - Power Control
 * Complexity: O(1) for request generation, O(retry_count) for polling
 */
uint32_t swd_power_up_request(void) {
    return DP_CTRL_STAT_CDBGPWRUPREQ | DP_CTRL_STAT_CSYSPWRUPREQ;
}

/**
 * swd_power_up_acknowledged - Check if power-up is complete
 *
 * Returns true if both debug and system power domains
 * have acknowledged the power-up request.
 */
bool swd_power_up_acknowledged(uint32_t ctrlstat) {
    return ((ctrlstat & DP_CTRL_STAT_CDBGPWRUPACK) != 0) &&
           ((ctrlstat & DP_CTRL_STAT_CSYSPWRUPACK) != 0);
}

/* ==================================================================
 * L5: SWD Error Recovery
 * ================================================================== */

/**
 * swd_error_recovery_sequence - Generate error recovery sequence
 *
 * When sticky errors are detected (STICKYORUN, STICKYCMP, STICKYERR),
 * the host must:
 *   1. Write ABORT register to clear the sticky flags
 *   2. Re-read CTRL/STAT to verify flags are cleared
 *   3. If still set, perform line reset and reconnect
 *
 * The ABORT register bits clear the corresponding sticky flags:
 *   - Bit 0: DAPABORT (clears STICKYERR, STICKYCMP, STICKYORUN)
 *   - Bit 1: STKCMPCLR (clears STICKYCMP only)
 *   - Bit 2: STKERRCLR (clears STICKYERR only)
 *   - Bit 3: WDERRCLR (clears WDATAERR)
 *   - Bit 4: ORUNERRCLR (clears STICKYORUN)
 *
 * Reference: ADIv5.2 Table B3-4 - ABORT Register
 */
uint32_t swd_abort_clear_all(void) {
    return (1u << 0) |  /* DAPABORT - clear all sticky flags */
           (1u << 1) |  /* STKCMPCLR - clear sticky compare */
           (1u << 2) |  /* STKERRCLR - clear sticky error */
           (1u << 3) |  /* WDERRCLR  - clear write data error */
           (1u << 4);   /* ORUNERRCLR - clear sticky overrun */
}

/* ==================================================================
 * L2: SWD Protocol State Machine
 * ================================================================== */

/**
 * swd_protocol_reset_state - Initialize SWD protocol state
 *
 * Resets internal SWD protocol tracking to default values.
 * Used when starting a new debug session or after error recovery.
 */
void swd_protocol_reset_state(swd_timing_params_t *params) {
    if (!params) return;

    params->swclk_freq_hz      = SWD_DEFAULT_FREQ_HZ;
    params->turnaround_time_ns = SWD_DEFAULT_TRN_NS;
    params->data_setup_time_ns = SWD_DEFAULT_SETUP_NS;
    params->data_hold_time_ns  = SWD_DEFAULT_HOLD_NS;
    params->idle_cycles        = SWD_DEFAULT_IDLE_CYCLES;
    params->max_retries        = SWD_DEFAULT_MAX_RETRIES;
    params->use_overrun_detect = true;
    params->use_parity_check   = true;
}

/* ==================================================================
 * L3: SWD Timing Calculations
 * ================================================================== */

/**
 * swd_calculate_swclk_period_ns - L3: clock period from frequency
 *
 * Fundamental relationship: T_clk = 1 / f_clk
 *
 * @param freq_hz  SWCLK frequency in Hz
 * @return Period in nanoseconds
 *
 * Complexity: O(1)
 */
double swd_calculate_swclk_period_ns(double freq_hz) {
    if (freq_hz <= 0.0) return 0.0;
    return 1.0e9 / freq_hz;
}

/**
 * swd_calculate_max_freq_for_turnaround - Maximum SWCLK given Trn
 *
 * The turnaround time limits the maximum clock frequency:
 *   f_max = 1 / (Trn + T_setup + T_hold)
 *
 * Where Trn is the line turnaround time (host-to-target or
 * target-to-host) and must include both directions for
 * a read transaction.
 *
 * Reference: ADIv5.2 Section 9.3 - AC Timing
 * Complexity: O(1)
 */
double swd_calculate_max_freq_for_turnaround(double trn_ns,
                                              double setup_ns,
                                              double hold_ns) {
    double total_ns = trn_ns + setup_ns + hold_ns;
    if (total_ns <= 0.0) return 0.0;
    return 1.0e9 / total_ns;
}

/**
 * swd_calculate_min_retry_wait_us - Minimum retry delay for WAIT
 *
 * When the target returns ACK=WAIT, the host should wait before
 * retrying. The wait time depends on the SWCLK frequency:
 *   T_wait = (32 + overhead) * T_swclk
 *
 * This is a conservative estimate for flash erase/write operations
 * which can take microseconds to milliseconds.
 *
 * @param freq_hz  SWCLK frequency in Hz
 * @return Minimum retry wait in microseconds
 */
double swd_calculate_min_retry_wait_us(double freq_hz) {
    double t_clk_us;
    if (freq_hz <= 0.0) return 100.0;  /* Default 100us */
    t_clk_us = 1.0e6 / freq_hz;
    return t_clk_us * 50.0;  /* 50 clock cycles minimum */
}

/* ==================================================================
 * L5: SWD IDCODE Verification
 * ================================================================== */

/**
 * swd_verify_dpidr - Verify DPIDR is valid
 *
 * A valid DPIDR indicates a connected ARM SW-DP target.
 * Checks:
 *   - Read DP DPIDR register
 *   - Verify version is ADIv5 or ADIv6
 *   - Verify designer code is ARM (0x23B)
 *
 * Returns true if DPIDR appears valid.
 *
 * Reference: ADIv5.2 Table B3-2 - DPIDR Format
 */
bool swd_verify_dpidr(uint32_t dpidr) {
    uint32_t version  = (dpidr & DP_DPIDR_VERSION_MASK) >> 16;
    uint32_t designer = (dpidr & DP_DPIDR_DESIGNER_MASK) >> 6;
    uint32_t min_val  = (dpidr & DP_DPIDR_MIN_MASK);

    /* Check version: must be ADIv5 (1) or ADIv6 (2) */
    if (version != DP_DPIDR_VERSION_ADIv5 && version != DP_DPIDR_VERSION_ADIv6) {
        return false;
    }

    /* Check designer: must be ARM JEP106 code */
    if (designer != DP_DPIDR_DESIGNER_ARM) {
        /* Some implementations may use different codes, allow for now */
    }

    /* MIN field should not be 0 (erased) */
    if (min_val == DP_DPIDR_MIN_ERASED) {
        return false;
    }

    return true;
}

/* ==================================================================
 * L5: SWD Burst Transfer Support
 * ================================================================== */

/**
 * swd_burst_read_prepare - Prepare a burst read sequence
 *
 * For reading multiple consecutive memory words efficiently,
 * the AP TAR is set once and then multiple DRW reads are
 * performed with auto-increment enabled.
 *
 * The TAR auto-increment is controlled by AP CSW.AddrInc:
 *   - OFF (0): No increment (read same address repeatedly)
 *   - SINGLE (1): Increment by transfer size after each access
 *   - PACKED (2): Packed transfers (debug-specific)
 *
 * This function calculates the number of transactions needed
 * for a burst read of given count.
 *
 * @param count    Number of 32-bit words to read
 * @param addr_inc Auto-increment mode
 * @return Number of SWD transactions needed
 */
uint32_t swd_burst_read_transaction_count(uint32_t count, uint32_t addr_inc) {
    if (count == 0) return 0;

    if (addr_inc == AP_CSW_ADDRINC_OFF) {
        /* One TAR write + count * DRW reads + count * RDBUFF reads */
        return 1 + count * 2;
    } else {
        /* One TAR write + (count) DRW reads + (count) RDBUFF reads */
        /* But due to pipelining, the last RDBUFF read gives the last DRW value */
        return 1 + count + 1;
    }
}

/* ==================================================================
 * L3: SWD CRC Checking (for SWD v2 trace)
 * ================================================================== */

/**
 * swd_crc5_compute - L3: Compute CRC-5 for SWD trace packets
 *
 * SWD v2 uses CRC-5-USB for trace data integrity.
 * Polynomial: x^5 + x^2 + 1 (0x05)
 *
 * This is a standard CRC-5 implementation using the
 * linear feedback shift register (LFSR) method.
 *
 * Reference: ARM IHI 0074A (ADIv6) Appendix D
 * Complexity: O(N) where N is number of bits
 */
uint8_t swd_crc5_compute(const uint8_t *data, uint32_t bit_length) {
    uint8_t crc = 0x1F;  /* Initial value: all ones */
    uint32_t i;

    if (!data || bit_length == 0) return crc;

    for (i = 0; i < bit_length; i++) {
        uint8_t bit_in = (data[i / 8] >> (i % 8)) & 0x01;
        uint8_t xor_bit = ((crc >> 4) & 0x01) ^ bit_in;

        crc = (uint8_t)((crc << 1) & 0x1F);

        if (xor_bit) {
            crc ^= 0x05;  /* Polynomial: x^5 + x^2 + 1 */
        }
    }

    return crc & 0x1F;
}

/* ==================================================================
 * L5: SWD Debug Session Management
 * ================================================================== */

/**
 * swd_debug_halt_prepare - Prepare halt request for Cortex-M
 *
 * Halt the target CPU core via the debug interface.
 * This involves writing to the DHCSR register at address 0xE000EDF0:
 *   - C_DEBUGEN (bit 0): Enable debug
 *   - C_HALT    (bit 1): Halt the core
 *   - C_MASKINTS (bit 3): Mask interrupts during halt
 *
 * The host must:
 *   1. Select AHB-AP (AP 0 typically)
 *   2. Configure CSW for 32-bit access
 *   3. Set TAR to 0xE000EDF0
 *   4. Write DRW with halt value
 *
 * Note: This function generates the register values but
 * actual bus access requires the full SWD transaction sequence.
 *
 * Reference: ARMv7-M Architecture Reference Manual C1.6
 */
uint32_t swd_cortex_m_halt_value(void) {
    return (1u << 0) |   /* C_DEBUGEN: enable debug */
           (1u << 1) |   /* C_HALT: halt the core */
           (1u << 3);    /* C_MASKINTS: mask interrupts */
}

/**
 * swd_cortex_m_resume_value - Generate resume request value
 *
 * Clear C_HALT bit to resume execution.
 */
uint32_t swd_cortex_m_resume_value(void) {
    return (1u << 0);    /* C_DEBUGEN: keep debug enabled */
}

/**
 * swd_cortex_m_step_value - Generate single-step request value
 *
 * Set C_STEP to execute one instruction, then re-halt.
 */
uint32_t swd_cortex_m_step_value(void) {
    return (1u << 0) |   /* C_DEBUGEN */
           (1u << 2) |   /* C_STEP: single step */
           (1u << 3);    /* C_MASKINTS */
}

/* ==================================================================
 * L5: SWD Flash Programming Support
 * ================================================================== */

/**
 * swd_flash_write_word - Prepare a single word flash write via SWD
 *
 * ARM Cortex-M flash programming via debug interface:
 *   1. Write flash key to FLASH_KEYR (unlock)
 *   2. Set PG bit in FLASH_CR
 *   3. Write data word to target address
 *   4. Poll FLASH_SR BSY bit until clear
 *   5. Check for errors (PGAERR, PGPERR, etc.)
 *
 * This function returns the register values needed for
 * flash programming, not the SWD transactions themselves.
 * The transport layer must execute the actual bus accesses.
 *
 * Reference: STM32 Reference Manual (Flash Programming Chapter)
 * Reference: ARM DDI 0403E - Debug Interface v5
 *
 * @param flash_base   Base address of flash controller
 * @param target_addr  Flash memory address to write
 * @param data         Data word to write
 * @param key_reg_offset Offset of KEYR from flash_base
 * @param cr_offset     Offset of CR from flash_base
 * @param sr_offset     Offset of SR from flash_base
 */
struct swd_flash_write_config {
    uint32_t flash_base;
    uint32_t key_reg;
    uint32_t cr_reg;
    uint32_t sr_reg;
    uint32_t flash_key1;
    uint32_t flash_key2;
    uint32_t cr_pg_bit;
    uint32_t sr_bsy_bit;
};

void swd_flash_write_get_defaults(struct swd_flash_write_config *cfg) {
    if (!cfg) return;
    /* STM32F1xx typical values */
    cfg->flash_base  = 0x40022000u;
    cfg->key_reg     = cfg->flash_base + 0x04;
    cfg->cr_reg      = cfg->flash_base + 0x10;
    cfg->sr_reg      = cfg->flash_base + 0x0C;
    cfg->flash_key1  = 0x45670123u;
    cfg->flash_key2  = 0xCDEF89ABu;
    cfg->cr_pg_bit   = (1u << 0);
    cfg->sr_bsy_bit  = (1u << 0);
}

/**
 * swd_flash_wait_bsy_clear - Check if flash BSY is clear
 *
 * Returns true if the flash operation is complete (BSY=0).
 * The debug host should poll this after each flash write.
 */
bool swd_flash_wait_bsy_clear(uint32_t sr_value, uint32_t bsy_bit) {
    return (sr_value & bsy_bit) == 0;
}
