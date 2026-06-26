/**
 * debug_port.h - Debug Port and Access Port Interface
 *
 * L1: DP and AP register abstractions, CoreSight DAP architecture.
 * L2: Memory access via AHB-AP, debug register access.
 * L5: Flash programming, memory read/write, core debug operations.
 *
 * Reference: ARM IHI 0031E (ADIv5.2), ARM IHI 0029E (CoreSight DAP-Lite)
 * Courses: Berkeley EE16B (embedded systems), 
 *   Illinois ECE 310 (DSP/microcontroller debug)
 */

#ifndef DEBUG_PORT_H
#define DEBUG_PORT_H

#include "swd_protocol.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ==================================================================
 * L1: Debug Access Port (DAP) Types
 * ================================================================== */

/** Port types available on a debug access port */
typedef enum {
    PORT_TYPE_DEBUG = 0,  /* Debug Port */
    PORT_TYPE_ACCESS = 1  /* Access Port */
} port_type_t;

/** AP types (IDR classification) */
typedef enum {
    AP_TYPE_JTAG_AP  = 0x0,
    AP_TYPE_COM_AP   = 0x1,
    AP_TYPE_AHB_AP   = 0x2,
    AP_TYPE_APB_AP   = 0x3,
    AP_TYPE_AXI_AP   = 0x4,
    AP_TYPE_AHB5_AP  = 0x5,
    AP_TYPE_APB4_AP  = 0x6,
    AP_TYPE_AXI5_AP  = 0x7,
    AP_TYPE_CUSTOM   = 0x8
} ap_type_t;

/** DAP status information */
typedef struct {
    uint32_t dpidr;           /* DP ID register */
    uint32_t ctrl_stat;       /* Control/Status */
    uint32_t target_id;       /* Target identification */
    uint8_t  num_access_ports; /* Number of APs detected */
    bool     debug_powered;   /* Debug power domain up */
    bool     system_powered;  /* System power domain up */
    bool     overrun_detected; /* Overrun condition */
} dap_status_t;

/* ==================================================================
 * L1: CoreSight Debug Component Addresses
 * ================================================================== */

/* Cortex-M debug component base addresses */
#define CORTEX_M_DHCSR_ADDR      0xE000EDF0u  /* Debug Halting Control and Status */
#define CORTEX_M_DCRSR_ADDR      0xE000EDF4u  /* Debug Core Register Selector */
#define CORTEX_M_DCRDR_ADDR      0xE000EDF8u  /* Debug Core Register Data */
#define CORTEX_M_DEMCR_ADDR      0xE000EDFCu  /* Debug Exception and Monitor Ctrl */
#define CORTEX_M_NVIC_BASE       0xE000E100u  /* NVIC base */
#define CORTEX_M_SCB_BASE        0xE000ED00u  /* System Control Block base */
#define CORTEX_M_ITM_BASE        0xE0000000u  /* Instrumentation Trace Macrocell */
#define CORTEX_M_DWT_BASE        0xE0001000u  /* Data Watchpoint and Trace */
#define CORTEX_M_FPB_BASE        0xE0002000u  /* Flash Patch and Breakpoint */
#define CORTEX_M_ETM_BASE        0xE0040000u  /* Embedded Trace Macrocell */
#define CORTEX_M_TPIU_BASE       0xE0040000u  /* Trace Port Interface Unit */

/* DHCSR bit definitions */
#define DHCSR_C_DEBUGEN    (1u << 0)
#define DHCSR_C_HALT       (1u << 1)
#define DHCSR_C_STEP       (1u << 2)
#define DHCSR_C_MASKINTS   (1u << 3)
#define DHCSR_C_SNAPSTALL  (1u << 4)
#define DHCSR_S_REGRDY     (1u << 16)
#define DHCSR_S_HALT       (1u << 17)
#define DHCSR_S_SLEEP      (1u << 18)
#define DHCSR_S_LOCKUP     (1u << 19)
#define DHCSR_S_RETIRE_ST  (1u << 24)
#define DHCSR_S_RESET_ST   (1u << 25)

/* DEMCR bit definitions */
#define DEMCR_VC_CORERESET (1u << 0)
#define DEMCR_VC_MMERR     (1u << 4)
#define DEMCR_VC_NOCPERR   (1u << 5)
#define DEMCR_VC_CHKERR    (1u << 6)
#define DEMCR_VC_STATERR   (1u << 7)
#define DEMCR_VC_BUSERR    (1u << 8)
#define DEMCR_VC_INTERR    (1u << 9)
#define DEMCR_VC_HARDERR   (1u << 10)
#define DEMCR_TRCENA       (1u << 24)

/* ==================================================================
 * L1: Memory Access via AHB-AP
 * ================================================================== */

/** Memory access transaction descriptor */
typedef struct {
    uint32_t address;       /* Target memory address */
    uint32_t data;          /* Data to write / data read */
    uint8_t  access_size;   /* 8, 16, or 32 bits */
    bool     is_read;       /* true=read, false=write */
    bool     auto_increment; /* Increment TAR after access */
    bool     secure_access;  /* Use secure privilege */
    uint32_t csw_value;      /* CSW register value */
} mem_access_txn_t;

/** Memory access result */
typedef struct {
    uint32_t value;          /* Read value (for reads) */
    bool     success;        /* Access completed successfully */
    bool     fault;          /* Access fault (invalid address, protection) */
    uint32_t retries;        /* Number of WAIT retries */
} mem_access_result_t;

/* ==================================================================
 * L1: Debug Core Register Access
 * ================================================================== */

/** ARM Cortex-M core register indices (for DCRSR) */
typedef enum {
    CORE_REG_R0  = 0,   CORE_REG_R1  = 1,
    CORE_REG_R2  = 2,   CORE_REG_R3  = 3,
    CORE_REG_R4  = 4,   CORE_REG_R5  = 5,
    CORE_REG_R6  = 6,   CORE_REG_R7  = 7,
    CORE_REG_R8  = 8,   CORE_REG_R9  = 9,
    CORE_REG_R10 = 10,  CORE_REG_R11 = 11,
    CORE_REG_R12 = 12,
    CORE_REG_SP  = 13,  CORE_REG_LR  = 14,
    CORE_REG_PC  = 15,
    CORE_REG_XPSR = 16,
    CORE_REG_MSP  = 17, CORE_REG_PSP  = 18,
    CORE_REG_CONTROL = 20,
    CORE_REG_COUNT   = 21
} core_register_t;

/** DCRSR write format for core register access:
 *  Bits [6:0] = REGSEL (register selector, Rn=0-12, SP=13, LR=14, PC=15)
 *  Bit  [16]  = REGWnR (0=read, 1=write)
 */
static inline uint32_t dcrsr_read_format(core_register_t reg) {
    return (uint32_t)(reg & 0x7F);
}

static inline uint32_t dcrsr_write_format(core_register_t reg) {
    return (uint32_t)(reg & 0x7F) | (1u << 16);
}

/* ==================================================================
 * L5: Debug Port Operations
 * ================================================================== */

/**
 * dap_power_up - Power up debug and system domains
 * Returns the CTRL/STAT value needed to request power-up.
 */
uint32_t dap_power_up_request(void);

/**
 * dap_get_status - Extract status from CTRL/STAT register
 */
void dap_get_status(uint32_t ctrlstat, dap_status_t *status);

/**
 * dap_clear_errors - Clear all sticky error flags
 */
uint32_t dap_clear_errors(void);

/**
 * dap_read_id - Read and decode DPIDR
 */
void dap_decode_dpidr(uint32_t dpidr, uint32_t *version,
                       uint32_t *revision, uint32_t *designer);

/* ==================================================================
 * L5: Memory Access API
 * ================================================================== */

/**
 * mem_ap_setup_csw - Build CSW value for memory access
 *
 * @param size        Transfer size (8, 16, or 32)
 * @param auto_inc    Enable TAR auto-increment
 * @param secure      Use secure access
 * @return CSW register value
 */
uint32_t mem_ap_setup_csw(uint8_t size, bool auto_inc, bool secure);

/**
 * mem_ap_read32 - Read a 32-bit word from target memory
 *
 * Complete flow:
 *   1. Select AHB-AP via DP SELECT
 *   2. Write CSW (32-bit, auto-increment on)
 *   3. Write TAR (target address)
 *   4. Read AP DRW (initiates read, returns previous value)
 *   5. Read DP RDBUFF (returns actual read value)
 *
 * @param address  Target memory address (must be word-aligned for 32-bit)
 * @param value    Output: the value read
 * @return 0 on success, negative error code
 */
int mem_ap_read32(uint32_t address, uint32_t *value);

/**
 * mem_ap_write32 - Write a 32-bit word to target memory
 *
 * @param address  Target memory address
 * @param value    Value to write
 * @return 0 on success, negative error code
 */
int mem_ap_write32(uint32_t address, uint32_t value);

/**
 * mem_ap_read_burst - Read multiple words efficiently
 *
 * Uses TAR auto-increment to avoid rewriting TAR for each word.
 *
 * @param address   Starting address
 * @param buffer    Output buffer
 * @param count     Number of 32-bit words to read
 * @return Number of words successfully read, or negative on error
 */
int mem_ap_read_burst32(uint32_t address, uint32_t *buffer, uint32_t count);

/**
 * mem_ap_write_burst - Write multiple words efficiently
 */
int mem_ap_write_burst32(uint32_t address, const uint32_t *buffer, uint32_t count);

/* ==================================================================
 * L5: Core Debug Operations
 * ================================================================== */

/**
 * core_debug_halt - Halt the target core
 *
 * Writes DHCSR with C_DEBUGEN | C_HALT | C_MASKINTS.
 */
int core_debug_halt(void);

/**
 * core_debug_resume - Resume target core execution
 */
int core_debug_resume(void);

/**
 * core_debug_step - Single-step the target core
 */
int core_debug_step(void);

/**
 * core_debug_read_reg - Read a core register
 *
 * Uses DCRSR/DCRDR protocol:
 *   1. Write DCRSR with REGSEL and REGWnR=0 (read)
 *   2. Poll DHCSR.S_REGRDY until set
 *   3. Read DCRDR for register value
 */
int core_debug_read_reg(core_register_t reg, uint32_t *value);

/**
 * core_debug_write_reg - Write a core register
 *
 *   1. Write DCRDR with value
 *   2. Write DCRSR with REGSEL and REGWnR=1 (write)
 *   3. Poll DHCSR.S_REGRDY until set
 */
int core_debug_write_reg(core_register_t reg, uint32_t value);

/**
 * core_debug_is_halted - Check if core is halted
 *
 * Reads DHCSR and checks S_HALT bit.
 */
bool core_debug_is_halted(void);

/* ==================================================================
 * L5: Breakpoint and Watchpoint Management
 * ================================================================== */

/** Flash Patch and Breakpoint (FPB) unit */
#define FPB_CTRL_ADDR    0xE0002000u
#define FPB_REMAP_ADDR   0xE0002004u
#define FPB_COMP_BASE    0xE0002008u  /* Comparator 0-7 */

typedef struct {
    uint32_t address;       /* Breakpoint address */
    bool     enabled;       /* Breakpoint enabled */
    uint8_t  comp_index;    /* Comparator index (0-5 for hardware) */
    bool     is_hw;         /* Hardware breakpoint */
} breakpoint_t;

#define FPB_MAX_HW_BREAKPOINTS 6

/** Data Watchpoint and Trace (DWT) unit */
#define DWT_CTRL_ADDR    0xE0001000u
#define DWT_COMP_BASE    0xE0001020u  /* Comparator 0-15 */
#define DWT_MASK_BASE    0xE0001024u
#define DWT_FUNCTION_BASE 0xE0001028u

typedef struct {
    uint32_t address;       /* Watchpoint address */
    uint32_t mask;          /* Address mask */
    bool     is_read;       /* Watch reads */
    bool     is_write;      /* Watch writes */
    bool     enabled;       /* Watchpoint enabled */
    uint8_t  comp_index;    /* Comparator index (0-3 for DWT) */
} watchpoint_t;

#define DWT_MAX_WATCHPOINTS 4

/** Breakpoint API */
int fpb_set_breakpoint(uint32_t addr, uint8_t comp_index);
int fpb_clear_breakpoint(uint8_t comp_index);
int fpb_clear_all_breakpoints(void);

/** Watchpoint API */
int dwt_set_watchpoint(uint32_t addr, uint32_t mask,
                        bool is_read, bool is_write, uint8_t comp_index);
int dwt_clear_watchpoint(uint8_t comp_index);
int dwt_clear_all_watchpoints(void);

/* L5: Flash programming helpers */
void flash_unlock_keys(uint32_t *key1, uint32_t *key2);
bool flash_is_busy(uint32_t sr_value);
bool flash_has_error(uint32_t sr_value);
int flash_program_word_sequence(uint32_t flash_base, uint32_t addr, uint32_t data);

/* L3: Address alignment utilities */
bool check_word_aligned(uint32_t addr);
bool check_halfword_aligned(uint32_t addr);
uint32_t align_address_down(uint32_t addr, uint32_t boundary);
uint32_t align_address_up(uint32_t addr, uint32_t boundary);

#ifdef __cplusplus
}
#endif
#endif /* DEBUG_PORT_H */
