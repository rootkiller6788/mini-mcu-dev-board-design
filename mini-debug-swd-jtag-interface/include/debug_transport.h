/**
 * debug_transport.h - Physical Transport Layer for Debug Interfaces
 *
 * L1: GPIO bit-bang transport, level shifting, signal integrity.
 * L2: Clock generation, timing calibration, reset sequences.
 * L5: Auto-baud detection, transport abstraction layer.
 *
 * This module abstracts the physical I/O layer that drives
 * SWD and JTAG signals on actual GPIO pins.
 *
 * Reference: ARM CoreSight DAP-Lite TRM (ARM DDI 0316)
 * Courses: Berkeley EE16B (GPIO interfacing),
 *   Michigan EECS 411 (signal integrity in high-speed digital)
 */

#ifndef DEBUG_TRANSPORT_H
#define DEBUG_TRANSPORT_H

#include "swd_protocol.h"
#include "jtag_tap.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ==================================================================
 * L1: GPIO Pin Descriptors
 * ================================================================== */

/** GPIO pin abstraction - maps logical signals to physical pins */
typedef struct {
    uint8_t  port;       /* GPIO port (0=A, 1=B, etc.) */
    uint8_t  pin;        /* GPIO pin number (0-15) */
    bool     inverted;   /* Signal inverted (active-low) */
    uint32_t base_addr;  /* GPIO port base address (memory-mapped) */
} gpio_pin_t;

/** SWD physical pin mapping */
typedef struct {
    gpio_pin_t swclk;    /* SWD clock output */
    gpio_pin_t swdio;    /* SWD data I/O (bidirectional) */
    gpio_pin_t swo;      /* Serial Wire Output (trace, optional) */
    gpio_pin_t nreset;   /* Target reset (active-low, optional) */
    gpio_pin_t vref;     /* Target voltage reference sense (analog input) */
} swd_pin_map_t;

/** JTAG physical pin mapping */
typedef struct {
    gpio_pin_t tck;      /* Test clock */
    gpio_pin_t tms;      /* Test mode select */
    gpio_pin_t tdi;      /* Test data input */
    gpio_pin_t tdo;      /* Test data output */
    gpio_pin_t trst;     /* Test reset (optional) */
    gpio_pin_t nreset;   /* System reset (optional) */
} jtag_pin_map_t;

/* ==================================================================
 * L1: Voltage Level Translation
 * ================================================================== */

/** Target voltage levels supported */
typedef enum {
    VOLTAGE_1V8  = 1800,  /* 1.8V logic */
    VOLTAGE_2V5  = 2500,  /* 2.5V logic */
    VOLTAGE_3V3  = 3300,  /* 3.3V logic (most common) */
    VOLTAGE_5V0  = 5000   /* 5.0V logic (legacy) */
} target_voltage_mv_t;

/** Level shifter configuration */
typedef struct {
    target_voltage_mv_t target_voltage;  /* Target I/O voltage */
    target_voltage_mv_t host_voltage;    /* Host I/O voltage */
    bool     bidirectional;   /* Bidirectional level shifting required */
    bool     auto_sense;      /* Auto-detect target voltage */
    double   vref_threshold;  /* VREF detection threshold (fraction of ADC max) */
    uint32_t adc_channel;     /* ADC channel for VREF sense */
} level_shifter_config_t;

/* ==================================================================
 * L2: Transport Layer Abstraction
 * ================================================================== */

/** Transport interface type */
typedef enum {
    TRANSPORT_SWD  = 0,  /* Serial Wire Debug */
    TRANSPORT_JTAG = 1,  /* JTAG IEEE 1149.1 */
    TRANSPORT_CJAG = 2   /* cJTAG (compact JTAG, IEEE 1149.7) */
} transport_type_t;

/** Transport layer operations (callback-based abstraction)
 *  Allows the debug layer to be independent of specific
 *  GPIO implementations.
 */
typedef struct transport_ops transport_ops_t;

struct transport_ops {
    /* Pin I/O */
    int (*set_clock)(void *ctx, bool level);
    int (*set_data_out)(void *ctx, bool level);
    int (*get_data_in)(void *ctx, bool *level);
    int (*set_data_dir)(void *ctx, bool is_output);

    /* Timing */
    int (*delay_ns)(void *ctx, uint32_t nanoseconds);
    int (*delay_us)(void *ctx, uint32_t microseconds);

    /* Clock */
    int (*clock_pulse)(void *ctx);  /* Toggle clock high then low */

    /* Reset */
    int (*assert_reset)(void *ctx);
    int (*deassert_reset)(void *ctx);
};

/* ==================================================================
 * L1: Transport Context (Bit-Bang Engine)
 * ================================================================== */

/** Bit-bang transport engine state */
typedef struct {
    transport_type_t type;           /* SWD or JTAG */
    void            *gpio_ctx;       /* Platform-specific GPIO context */
    const transport_ops_t *ops;      /* Transport operations */

    /* Timing state */
    double     clock_freq_hz;        /* Current clock frequency */
    double     clock_period_ns;      /* Clock period (calculated) */
    double     half_period_ns;       /* Half period for clock toggle */

    /* SWD-specific */
    swd_timing_params_t swd_timing;  /* SWD timing parameters */
    swd_line_state_t    line_state;  /* Current SWD line state */

    /* JTAG-specific */
    jtag_timing_t       jtag_timing; /* JTAG timing parameters */
    tap_state_t         tap_state;   /* Current TAP state */

    /* Level shifting */
    level_shifter_config_t level_shifter;

    /* Statistics */
    uint32_t   transaction_count;
    uint32_t   error_count;
    uint32_t   retry_count;
    uint32_t   clock_cycles;
    bool       connected;
    bool       initialized;
} debug_transport_t;

/* ==================================================================
 * L2: Transport Initialization
 * ================================================================== */

/**
 * transport_init - Initialize the debug transport layer
 *
 * Configures GPIO pins, sets initial signal levels,
 * performs voltage detection if auto_sense is enabled.
 *
 * @param transport  Transport context to initialize
 * @param type       Transport type (SWD or JTAG)
 * @param ops        Platform-specific I/O operations
 * @param gpio_ctx   Platform-specific GPIO context
 * @return 0 on success, negative on error
 */
int transport_init(debug_transport_t *transport,
                    transport_type_t type,
                    const transport_ops_t *ops,
                    void *gpio_ctx);

/**
 * transport_deinit - Deinitialize transport and release resources
 */
int transport_deinit(debug_transport_t *transport);

/* ==================================================================
 * L2: Clock Generation
 * ================================================================== */

/**
 * transport_set_clock_freq - Set debug clock frequency
 *
 * Recalculates clock period and half-period timing.
 * Validates that the frequency is within supported range.
 *
 * @param freq_hz  Desired clock frequency in Hz
 * @return 0 on success, negative if frequency out of range
 */
int transport_set_clock_freq(debug_transport_t *transport, double freq_hz);

/**
 * transport_clock_pulse - Generate a single clock pulse
 *
 * Drives SWCLK/TCK: high -> delay(Th) -> low -> delay(Tl).
 * Returns the sampled data value on the rising edge.
 *
 * @param data_out  Data to drive on falling edge (for SWD)
 * @param data_in   Output: data sampled on rising edge
 * @return 0 on success
 */
int transport_clock_pulse(debug_transport_t *transport,
                           bool data_out, bool *data_in);

/* ==================================================================
 * L2: SWD Transport Operations
 * ================================================================== */

/**
 * swd_transport_line_reset - Perform SWD line reset via GPIO
 *
 * Drives SWCLK for 50+ cycles with SWDIO=1.
 */
int swd_transport_line_reset(debug_transport_t *transport);

/**
 * swd_transport_connect - Establish SWD connection
 *
 * Full connection sequence:
 *   1. Line reset (50+ clocks with SWDIO=1)
 *   2. Send 16-bit ARM connection sequence (0xE79E)
 *   3. Send 2+ idle cycles
 *   4. Read DPIDR to verify connection
 *
 * @return 0 on success, negative error code
 */
int swd_transport_connect(debug_transport_t *transport);

/**
 * swd_transport_transact - Execute a single SWD transaction
 *
 * Low-level bit-bang implementation of a SWD read or write.
 *
 * Sequence:
 *   1. Send 8-bit request byte (LSB first)
 *   2. Turnaround: host releases SWDIO
 *   3. Receive 3-bit ACK
 *   4. If ACK=OK and read: receive 32-bit data + 1 parity
 *   5. If ACK=OK and write: send 32-bit data
 *   6. Turnaround (for reads): target releases, host drives
 *   7. Return status
 *
 * @param txn  Transaction descriptor (filled on return)
 * @return 0 on success, negative error code
 */
int swd_transport_transact(debug_transport_t *transport,
                            swd_transaction_t *txn);

/* ==================================================================
 * L2: JTAG Transport Operations
 * ================================================================== */

/**
 * jtag_transport_tap_reset - Reset TAP to Test-Logic-Reset
 *
 * Method 1: Assert TRST (if connected)
 * Method 2: Clock 5 TCK cycles with TMS=1 (guaranteed to reach TLR)
 */
int jtag_transport_tap_reset(debug_transport_t *transport);

/**
 * jtag_transport_shift - Shift bits through IR or DR
 *
 * Generic shift operation:
 *   1. Navigate TAP to SHIFT-IR or SHIFT-DR
 *   2. For each bit:
 *      - Set TDI to output bit (LSB first)
 *      - Pulse TCK
 *      - Sample TDO for input bit
 *   3. Navigate to EXIT1 and UPDATE
 *
 * @param is_ir      true for IR shift, false for DR shift
 * @param data_out   Data to shift out (TDI)
 * @param data_in    Output: data shifted in (TDO)
 * @param bit_count  Number of bits to shift
 * @param last_tms   TMS value on last bit (0=continue, 1=exit)
 * @return 0 on success
 */
int jtag_transport_shift(debug_transport_t *transport,
                          bool is_ir,
                          const uint8_t *data_out,
                          uint8_t *data_in,
                          uint32_t bit_count,
                          bool last_tms);

/**
 * jtag_transport_navigate - Navigate TAP to a specific state
 *
 * Computes the TMS sequence using the FSM and clocks
 * the TAP through the required states.
 */
int jtag_transport_navigate(debug_transport_t *transport,
                             tap_state_t target_state);

/* ==================================================================
 * L5: Auto-Baud Detection for SWD
 * ================================================================== */

/**
 * swd_transport_autobaud - Auto-detect optimal SWCLK frequency
 *
 * Algorithm:
 *   1. Start at highest frequency (SWD_V2_MAX_FREQ_HZ)
 *   2. Attempt DPIDR read
 *   3. If ACK is valid, return current frequency
 *   4. If ACK is invalid or no response, halve frequency
 *   5. Repeat until minimum frequency reached
 *
 * This is a binary search over the supported frequency range
 * to find the maximum reliable SWCLK frequency for the
 * current target and wiring conditions.
 *
 * @return Detected frequency in Hz, or 0 if no connection
 */
double swd_transport_autobaud(debug_transport_t *transport);

/* ==================================================================
 * L5: Signal Integrity Monitoring
 * ================================================================== */

/** Signal quality metrics */
typedef struct {
    double   signal_to_noise_ratio_db;  /* Estimated SNR */
    uint32_t glitch_count;      /* Detected glitches on SWDIO/TCK */
    uint32_t parity_errors;     /* Parity error count */
    uint32_t ack_errors;        /* Invalid ACK count */
    uint32_t overrun_errors;    /* Overrun errors */
    double   effective_freq_hz; /* Actual measured clock frequency */
    double   rise_time_ns;      /* Measured signal rise time */
    double   fall_time_ns;      /* Measured signal fall time */
} signal_quality_t;

/**
 * transport_get_signal_quality - Get signal quality metrics
 *
 * Aggregates error statistics into a signal quality report.
 * Used for debugging wiring issues and optimizing timing.
 */
void transport_get_signal_quality(const debug_transport_t *transport,
                                   signal_quality_t *quality);

/* ==================================================================
 * L3: Timing Calibration
 * ================================================================== */

/**
 * transport_calibrate_delay - Calibrate delay loop for bit-banging
 *
 * On platforms without hardware timers, software delay loops
 * must be calibrated. This function measures the actual
 * delay per loop iteration and stores it for use by the
 * transport layer.
 *
 * Algorithm: measure N iterations against a known reference
 * clock, then compute nanoseconds per iteration.
 *
 * @param iterations  Number of calibration iterations
 * @return Calibrated nanoseconds per delay unit
 */
double transport_calibrate_delay(debug_transport_t *transport,
                                  uint32_t iterations);

/* ==================================================================
 * L2: Multi-Target Support (SWD v2)
 * ================================================================== */

/**
 * swd_transport_select_target - Select target in multi-drop SWD
 *
 * Issues the SWD v2 target selection sequence with the
 * specified 4-bit target ID.
 */
int swd_transport_select_target(debug_transport_t *transport,
                                 uint8_t target_id);

/**
 * swd_transport_scan_targets - Scan for targets on SWD bus
 *
 * Iterates through all 16 possible target IDs and attempts
 * to read DPIDR from each. Returns a bitmap of detected targets.
 *
 * @param detected  Output: bitmask of detected target IDs
 * @return Number of targets detected
 */
int swd_transport_scan_targets(debug_transport_t *transport,
                                uint16_t *detected);

#ifdef __cplusplus
}
#endif
#endif /* DEBUG_TRANSPORT_H */
