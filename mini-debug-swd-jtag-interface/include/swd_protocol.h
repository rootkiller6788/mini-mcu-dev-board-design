/**
 * swd_protocol.h - Serial Wire Debug Protocol Definitions
 *
 * L1 Definitions: SWD packet format, DP/AP register addresses,
 * SWD transaction types, ACK responses, parity, CRC-5.
 * L2 Core Concepts: SWD single-wire bidirectional protocol,
 * turnaround timing, overrun detection, WAIT handling.
 * L3 Math: Parity generation, CRC-5 computation, timing calc.
 *
 * Reference: ARM Debug Interface Architecture Specification
 *   ADIv5.2 (ARM IHI 0031E), ADIv6 (ARM IHI 0074A)
 * Courses: MIT 6.450, Stanford EE359, Berkeley EE16B
 */

#ifndef SWD_PROTOCOL_H
#define SWD_PROTOCOL_H
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    SWD_LINE_IDLE       = 0,
    SWD_LINE_TURNAROUND = 1,
    SWD_LINE_ACTIVE     = 2,
    SWD_LINE_RESET      = 3,
    SWD_LINE_ERROR      = 4
} swd_line_state_t;

#define SWD_REQUEST_SIZE  8
#define SWD_ACK_SIZE      3
#define SWD_DATA_SIZE     32
#define SWD_PARITY_SIZE   1
#define SWD_TRN_SIZE_CLK  1

#define SWD_START_BIT_MASK   0x01
#define SWD_APNDP_BIT_MASK   0x02
#define SWD_RNW_BIT_MASK     0x04
#define SWD_ADDR_MASK        0x18
#define SWD_PARITY_BIT_MASK  0x20
#define SWD_STOP_BIT_MASK    0x40
#define SWD_PARK_BIT_MASK    0x80

typedef enum {
    SWD_ACK_OK      = 0x01,
    SWD_ACK_WAIT    = 0x02,
    SWD_ACK_FAULT   = 0x04,
    SWD_ACK_INVALID = 0x07
} swd_ack_t;

typedef enum { SWD_DIR_WRITE = 0, SWD_DIR_READ = 1 } swd_direction_t;
typedef enum { SWD_PORT_DP = 0, SWD_PORT_AP = 1 } swd_port_select_t;

typedef enum {
    DP_REG_DPIDR     = 0x0,
    DP_REG_ABORT     = 0x0,
    DP_REG_CTRL_STAT = 0x1,
    DP_REG_SELECT    = 0x2,
    DP_REG_RDBUFF    = 0x3
} dp_register_addr_t;

#define DP_CTRL_STAT_ORUNDETECT   (1u << 0)
#define DP_CTRL_STAT_STICKYORUN   (1u << 1)
#define DP_CTRL_STAT_STICKYCMP    (1u << 2)
#define DP_CTRL_STAT_STICKYERR    (1u << 3)
#define DP_CTRL_STAT_READOK       (1u << 4)
#define DP_CTRL_STAT_WDATAERR     (1u << 5)
#define DP_CTRL_STAT_MASKLANE     (1u << 6)
#define DP_CTRL_STAT_TRNCNT_MASK  (0xFFFull << 7)
#define DP_CTRL_STAT_CDBGPWRUPREQ (1u << 28)
#define DP_CTRL_STAT_CDBGPWRUPACK (1u << 29)
#define DP_CTRL_STAT_CSYSPWRUPREQ (1u << 30)
#define DP_CTRL_STAT_CSYSPWRUPACK (1u << 31)

#define DP_SELECT_DPBANKSEL_MASK (0xFull << 0)
#define DP_SELECT_APBANKSEL_MASK (0xFull << 4)
#define DP_SELECT_APSEL_MASK     (0xFFull << 24)

#define DP_DPIDR_REVISION_MASK   (0xFull << 28)
#define DP_DPIDR_PARTNO_MASK     (0xFFull << 20)
#define DP_DPIDR_VERSION_MASK    (0xFull << 16)
#define DP_DPIDR_DESIGNER_MASK   (0x3FFull << 6)
#define DP_DPIDR_MIN_MASK        (0x3Full << 0)
#define DP_DPIDR_VERSION_ADIv5   0x1
#define DP_DPIDR_VERSION_ADIv6   0x2
#define DP_DPIDR_DESIGNER_ARM    0x23B
#define DP_DPIDR_MIN_ERASED      0x00

typedef enum {
    AP_REG_CSW  = 0x0, AP_REG_TAR  = 0x1, AP_REG_DRW  = 0x3,
    AP_REG_BD0  = 0x4, AP_REG_BD1  = 0x5, AP_REG_BD2  = 0x6,
    AP_REG_BD3  = 0x7, AP_REG_CFG  = 0xF, AP_REG_BASE = 0xF,
    AP_REG_IDR  = 0xFC
} ap_register_addr_t;

#define AP_CSW_SIZE_MASK      (0x7u << 0)
#define AP_CSW_SIZE_8BIT      (0x0u)
#define AP_CSW_SIZE_16BIT     (0x1u)
#define AP_CSW_SIZE_32BIT     (0x2u)
#define AP_CSW_ADDRINC_MASK   (0x3u << 4)
#define AP_CSW_ADDRINC_OFF    (0x0u << 4)
#define AP_CSW_ADDRINC_SINGLE (0x1u << 4)
#define AP_CSW_ADDRINC_PACKED (0x2u << 4)
#define AP_CSW_DBGSTATUS      (1u << 6)
#define AP_CSW_HPROT_MASK     (0xFull << 24)
#define AP_CSW_MASTER_DEBUG   (1u << 29)
#define AP_CSW_SPIDEN         (1u << 23)
#define AP_CSW_DEVICEEN       (1u << 14)
#define AP_CSW_TRINPROG       (1u << 7)

#define SWD_LINE_RESET_CLOCKS_MIN  50
#define SWD_LINE_RESET_CLOCKS_SAFE 80

#define SWD_CONNECT_SEQ_ARM     0xE79E
#define SWD_CONNECT_SEQ_JTAG2SW 0xBCDA
#define SWD_CONNECT_SEQ_DORMANT 0xBFDE
#define SWD_ESC_SEQ_TO_JTAG     0xFFF
#define SWD_ESC_SEQ_TO_SWD      0xE79E

typedef struct {
    double swclk_freq_hz;
    double turnaround_time_ns;
    double data_setup_time_ns;
    double data_hold_time_ns;
    double idle_cycles;
    uint32_t max_retries;
    bool use_overrun_detect;
    bool use_parity_check;
} swd_timing_params_t;

#define SWD_DEFAULT_FREQ_HZ      10e6
#define SWD_DEFAULT_TRN_NS       150.0
#define SWD_DEFAULT_SETUP_NS     20.0
#define SWD_DEFAULT_HOLD_NS      10.0
#define SWD_DEFAULT_IDLE_CYCLES  2
#define SWD_DEFAULT_MAX_RETRIES  64

#define SWD_V1_MAX_FREQ_HZ       25e6
#define SWD_V2_MAX_FREQ_HZ       50e6
#define SWD_MIN_FREQ_HZ          100e3

typedef struct {
    swd_port_select_t port;
    swd_direction_t direction;
    uint8_t addr;
    uint32_t data;
    swd_ack_t ack;
    bool parity_error;
    bool overrun;
    uint32_t retry_count;
} swd_transaction_t;

typedef enum {
    SWD_ERR_NONE         = 0,
    SWD_ERR_NO_TARGET    = -1,
    SWD_ERR_ACK_FAULT    = -2,
    SWD_ERR_ACK_WAIT_TO  = -3,
    SWD_ERR_PARITY       = -4,
    SWD_ERR_OVERRUN      = -5,
    SWD_ERR_PROTOCOL     = -6,
    SWD_ERR_POWER        = -7,
    SWD_ERR_UNSUPPORTED  = -8,
    SWD_ERR_TIMEOUT      = -9,
    SWD_ERR_BUSY         = -10,
    SWD_ERR_LOCKED       = -11,
    SWD_ERR_DISCONNECTED = -12
} swd_error_t;

const char *swd_error_string(swd_error_t err);

typedef enum { SWD_VERSION_V1 = 0x1, SWD_VERSION_V2 = 0x2 } swd_version_t;

#define SWDV2_TARGET_ID_BITS    4
#define SWDV2_MAX_TARGETS       16
#define SWDV2_TARGET_ID_DEFAULT 0x0
#define SWDV2_CONNECT(tid) ((uint32_t)(0xBCDA) | ((uint32_t)((tid) & 0xF) << 16))

static inline uint8_t swd_compute_parity(uint8_t apndp, uint8_t rnw,
                                          uint8_t a2, uint8_t a3) {
    uint8_t sum_bits = (apndp & 1) + (rnw & 1) + (a2 & 1) + (a3 & 1);
    return (~sum_bits) & 0x01;
}

static inline uint8_t swd_compute_parity_32(uint32_t data) {
    data ^= data >> 16; data ^= data >> 8;
    data ^= data >> 4;  data ^= data >> 2; data ^= data >> 1;
    return data & 0x01;
}

static inline bool swd_verify_parity_32(uint32_t data, uint8_t parity_bit) {
    return (swd_compute_parity_32(data) ^ (parity_bit & 1)) == 0;
}

static inline bool swd_check_overrun(uint32_t ctrlstat) {
    return (ctrlstat & DP_CTRL_STAT_ORUNDETECT) != 0;
}

static inline bool swd_check_sticky_error(uint32_t ctrlstat) {
    return (ctrlstat & (DP_CTRL_STAT_STICKYORUN |
                        DP_CTRL_STAT_STICKYCMP  |
                        DP_CTRL_STAT_STICKYERR)) != 0;
}

static inline uint8_t swd_build_request_byte(uint8_t apndp, uint8_t rnw,
                                              uint8_t addr) {
    uint8_t req = 0, a2 = (addr >> 2) & 0x01, a3 = (addr >> 3) & 0x01;
    req |= (1u << 0);
    req |= ((apndp & 0x01) << 1);
    req |= ((rnw & 0x01) << 2);
    req |= (a2 << 3);
    req |= (a3 << 4);
    req |= (swd_compute_parity(apndp, rnw, a2, a3) << 5);
    req |= (0u << 6);
    req |= (1u << 7);
    return req;
}

static inline swd_ack_t swd_parse_ack(uint8_t ack_bits) {
    switch (ack_bits & 0x07) {
    case 0x01: return SWD_ACK_OK;
    case 0x02: return SWD_ACK_WAIT;
    case 0x04: return SWD_ACK_FAULT;
    default:   return SWD_ACK_INVALID;
    }
}

static inline const char *swd_ack_to_string(swd_ack_t ack) {
    switch (ack) {
    case SWD_ACK_OK:      return "OK";
    case SWD_ACK_WAIT:    return "WAIT";
    case SWD_ACK_FAULT:   return "FAULT";
    case SWD_ACK_INVALID: return "INVALID";
    default:              return "UNKNOWN";
    }
}

#define SWD_DP_WRITE(addr) swd_build_request_byte(0, 0, (addr))
#define SWD_DP_READ(addr)  swd_build_request_byte(0, 1, (addr))
#define SWD_AP_WRITE(addr) swd_build_request_byte(1, 0, (addr))
#define SWD_AP_READ(addr)  swd_build_request_byte(1, 1, (addr))

/* Declarations from swd_protocol.c */
int swd_encode_request(const swd_transaction_t *txn, uint8_t *request_byte,
                        uint32_t *data_out, uint32_t *bit_count);
int swd_decode_response(swd_transaction_t *txn, uint8_t ack_bits,
                         uint32_t read_data, uint8_t read_parity);
int swd_line_reset_generate(uint32_t clocks, uint8_t *pattern, uint32_t max_bits);
int swd_connection_sequence_generate(swd_version_t version, uint8_t target_id,
                                      uint8_t *pattern, uint32_t max_bits);
int swd_dp_read_prepare(dp_register_addr_t reg, swd_transaction_t *txn);
int swd_dp_write_prepare(dp_register_addr_t reg, uint32_t data, swd_transaction_t *txn);
int swd_ap_read_prepare(uint8_t ap_sel, ap_register_addr_t reg, swd_transaction_t *txn);
int swd_ap_write_prepare(uint8_t ap_sel, ap_register_addr_t reg, uint32_t data, swd_transaction_t *txn);
int swd_select_dp_bank(uint8_t dp_bank, uint8_t ap_bank, uint8_t ap_sel, swd_transaction_t *txn);
uint32_t swd_power_up_request(void);
bool swd_power_up_acknowledged(uint32_t ctrlstat);
uint32_t swd_abort_clear_all(void);
void swd_protocol_reset_state(swd_timing_params_t *params);
double swd_calculate_swclk_period_ns(double freq_hz);
double swd_calculate_max_freq_for_turnaround(double trn_ns, double setup_ns, double hold_ns);
double swd_calculate_min_retry_wait_us(double freq_hz);
bool swd_verify_dpidr(uint32_t dpidr);
uint32_t swd_burst_read_transaction_count(uint32_t count, uint32_t addr_inc);
uint8_t swd_crc5_compute(const uint8_t *data, uint32_t bit_length);
uint32_t swd_cortex_m_halt_value(void);
uint32_t swd_cortex_m_resume_value(void);
uint32_t swd_cortex_m_step_value(void);
struct swd_flash_write_config;
void swd_flash_write_get_defaults(struct swd_flash_write_config *cfg);
bool swd_flash_wait_bsy_clear(uint32_t sr_value, uint32_t bsy_bit);

#ifdef __cplusplus
}
#endif
#endif /* SWD_PROTOCOL_H */
