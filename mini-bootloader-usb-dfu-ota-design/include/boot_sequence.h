/*
 * boot_sequence.h -- MCU Boot Sequence and Vector Table Management
 *
 * Defines the complete MCU boot flow from reset vector through
 * bootloader execution to application handoff.
 *
 * Knowledge Coverage:
 *   L1: Reset vector, vector table, VTOR register, stack pointer init
 *   L2: Boot flow stages (ROM -> stage1 -> stage2 -> app), boot reasons
 *   L4: Boot chain trust invariant, vector table relocation
 *   L6: Secure boot sequence with signature verification
 *
 * Reference: ARMv7-M Architecture Reference Manual (Cortex-M3/M4/M7)
 * MIT 6.004 -- Computation Structures (boot sequences)
 * Cambridge CST IB -- Computer Design (interrupts, VTOR)
 */

#ifndef BOOT_SEQUENCE_H
#define BOOT_SEQUENCE_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* Cortex-M Vector Table Layout */
#define VECTOR_TABLE_SIZE      256
#define VECTOR_INITIAL_SP      0
#define VECTOR_RESET           1
#define VECTOR_NMI             2
#define VECTOR_HARDFAULT       3
#define VECTOR_MEMMANAGE       4
#define VECTOR_BUSFAULT        5
#define VECTOR_USAGEFAULT      6
#define VECTOR_SVCALL          11
#define VECTOR_DEBUGMON        12
#define VECTOR_PENDSV          14
#define VECTOR_SYSTICK         15
#define VECTOR_IRQ_BASE        16

/* Vector Table Entry */
typedef uint32_t vector_entry_t;

/* Vector Table Structure (Cortex-M) */
typedef struct {
    vector_entry_t initial_sp;
    vector_entry_t reset;
    vector_entry_t nmi;
    vector_entry_t hardfault;
    vector_entry_t memmanage;
    vector_entry_t busfault;
    vector_entry_t usagefault;
    vector_entry_t reserved_7;
    vector_entry_t reserved_8;
    vector_entry_t reserved_9;
    vector_entry_t reserved_10;
    vector_entry_t svcall;
    vector_entry_t debugmon;
    vector_entry_t reserved_13;
    vector_entry_t pendsv;
    vector_entry_t systick;
    vector_entry_t irq[240];
} vector_table_t;

/* Boot Reason Codes */
typedef enum {
    BOOT_REASON_POWER_ON       = 0,
    BOOT_REASON_EXTERNAL_RESET = 1,
    BOOT_REASON_WDT_RESET      = 2,
    BOOT_REASON_SOFTWARE_RESET = 3,
    BOOT_REASON_LOW_POWER_WAKE = 4,
    BOOT_REASON_BOR_RESET      = 5,
    BOOT_REASON_DFU_DETACH     = 6,
    BOOT_REASON_OTA_REBOOT     = 7,
    BOOT_REASON_DEBUG_ATTACH   = 8,
    BOOT_REASON_UNKNOWN        = 255
} boot_reason_t;

/* Boot Stage Enumeration */
typedef enum {
    BOOT_STAGE_ROM           = 0,
    BOOT_STAGE_STAGE1        = 1,
    BOOT_STAGE_STAGE2        = 2,
    BOOT_STAGE_APPLICATION   = 3,
    BOOT_STAGE_DFU_MODE      = 4,
    BOOT_STAGE_RECOVERY      = 5
} boot_stage_t;

/* Boot Mode Detection Flags */
#define BOOT_FLAG_DFU_PIN          (1u << 0)
#define BOOT_FLAG_MAGIC_VALUE      (1u << 1)
#define BOOT_FLAG_NO_APP           (1u << 2)
#define BOOT_FLAG_WDT_BOOT         (1u << 3)
#define BOOT_FLAG_BOOT_SEL_PIN     (1u << 4)
#define BOOT_FLAG_APP_REQUEST      (1u << 5)
#define BOOT_FLAG_FORCE_DFU        (1u << 6)

/* Magic Value for Bootloader Entry */
#define BOOT_MAGIC_ENTER_DFU    0xDF00DF00u
#define BOOT_MAGIC_ENTER_OTA    0x0A000A00u
#define BOOT_MAGIC_FACTORY      0xFACFAC00u

/* Boot Configuration */
typedef struct {
    uint32_t  app_start_addr;
    uint32_t  app_stack_top;
    uint32_t  app_reset_handler;
    uint32_t  bootloader_start;
    uint32_t  bootloader_end;
    uint32_t  magic_addr;
    uint32_t  dfu_entry_pin;
    uint8_t   dfu_entry_pin_active;
    uint8_t   check_app_signature;
    uint8_t   allow_fallback;
    uint8_t   enable_watchdog;
    uint8_t   boot_pin_state;
} boot_config_t;

/* Boot Context */
typedef struct {
    boot_stage_t   current_stage;
    boot_reason_t  boot_reason;
    boot_config_t  config;
    vector_table_t app_vector_table;
    uint32_t       boot_flags;
    uint32_t       boot_timestamp;
    uint8_t        dfu_requested;
    uint8_t        app_valid;
    uint8_t        app_verified;
    uint8_t        boot_complete;
} boot_context_t;

/* System Control Block (SCB) Register Offsets */
#define SCB_VTOR    0xE000ED08
#define SCB_AIRCR   0xE000ED0C
#define SCB_SCR     0xE000ED10
#define SCB_CCR     0xE000ED14
#define SCB_SHCSR   0xE000ED24
#define SCB_CFSR    0xE000ED28

#define AIRCR_SYSRESETREQ   (1u << 2)
#define AIRCR_VECTKEY       (0x05FAu << 16)

/* API Declarations */
void boot_init(boot_context_t *ctx, const boot_config_t *config);
boot_reason_t boot_detect_reason(uint32_t reset_flags);
boot_stage_t boot_determine_stage(boot_context_t *ctx);
bool boot_should_enter_dfu(const boot_context_t *ctx);
bool boot_should_enter_ota(const boot_context_t *ctx);
bool boot_validate_vector_table(const vector_table_t *vt,
                                 uint32_t flash_base, uint32_t flash_end);
bool boot_relocate_vector_table(uint32_t new_vtor_addr);
void boot_prepare_app_launch(boot_context_t *ctx);
void boot_jump_to_application(uint32_t stack_top,
                               uint32_t reset_handler) __attribute__((noreturn));
bool boot_set_magic_value(uint32_t addr, uint32_t magic);
bool boot_clear_magic_value(uint32_t addr);
bool boot_check_magic_value(uint32_t addr, uint32_t magic);
void boot_system_reset(void) __attribute__((noreturn));
void boot_enter_dfu_mode(boot_context_t *ctx);
const char *boot_reason_name(boot_reason_t reason);
const char *boot_stage_name(boot_stage_t stage);

#endif /* BOOT_SEQUENCE_H */
