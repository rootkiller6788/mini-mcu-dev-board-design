/*
 * startup.h — MCU Startup Sequence and System Initialization
 *
 * Knowledge Mapping (SKILL.md L1-L6):
 *   L1 Definitions: Reset vector, stack pointer initialization,
 *                   .bss zeroing, .data copy from flash to RAM,
 *                   SystemInit, main() entry, linker script sections
 *   L2 Concepts:    Memory layout (text, rodata, data, bss, heap, stack),
 *                   scatter-loading, boot sequence state machine
 *   L3 Math:        Section alignment (2^n boundary),
 *                   Stack size: S = IRQ_depth × 64 + main_stack + safety_margin
 *                   Heap fragmentation: external vs internal fragmentation
 *   L4 Laws:        ARM AAPCS (Procedure Call Standard) — stack 8-byte aligned
 *   L5 Algorithms:  Copy table traversal for .data initialization,
 *                   memset for .bss zeroing (optimized 4-byte loop)
 *   L6 Problems:    Boot time measurement (cycle counter),
 *                   Checksum verification before boot,
 *                   Multi-image boot (bootloader → app → update agent)
 *
 * Course Mapping:
 *   MIT 6.004 §15 — Boot process, linker
 *   Berkeley CS61C — Memory layout, calling conventions
 *   Valvano Ch.2 — Startup code walkthrough
 *   ARMv7-M Architecture Manual §B1.5.2 — Reset behavior
 *
 * Reference: ARM CMSIS, STM32CubeF4 startup_stm32f407xx.s
 */

#ifndef STARTUP_H
#define STARTUP_H

#include <stdint.h>
#include <stddef.h>

/* ──────────────────────────────────────────────
 * L1 Definition: Linker Section Memory Regions
 *
 * Flash (RX): text + rodata + vector table
 * RAM (RW):   data (initialized) + bss (zero) + heap + stack
 *
 * Typical STM32F4 memory map:
 *   0x08000000 – 0x080FFFFF: 1 MB Flash (executable, read-only)
 *   0x20000000 – 0x2001FFFF: 128 KB SRAM (read-write)
 *   0x1FFF0000 – 0x1FFF7A0F: System memory (bootloader ROM)
 *
 * ────────────────────────────────────────────── */

/* Section descriptors for scatter-loading initialization */
typedef struct {
    uint32_t *load_address;    /* Flash address where init data is stored */
    uint32_t *run_address;     /* RAM address where data must be copied   */
    uint32_t  size;            /* Size in bytes                           */
} section_copy_entry_t;

/* ──────────────────────────────────────────────
 * L1 Definition: Clock Configuration
 *
 * The MCU starts on the internal HSI oscillator (16 MHz on STM32F4).
 * SystemInit typically configures:
 *   1. Enable HSE (external crystal, e.g., 8 MHz)
 *   2. Configure PLL: PLL_M × PLL_N / PLL_P / PLL_Q
 *   3. Switch system clock to PLL output
 *   4. Configure AHB/APB1/APB2 prescalers
 *   5. Update SysTick for 1 ms interrupt interval
 *
 * PLL formula for STM32F4:
 *   f_VCO = f_in / PLL_M × PLL_N
 *   f_PLL = f_VCO / PLL_P
 *   f_USB = f_VCO / PLL_Q (must be 48 MHz for USB)
 *
 * Example (168 MHz from 8 MHz HSE):
 *   PLL_M = 8, PLL_N = 336, PLL_P = 2, PLL_Q = 7
 *   f_VCO = 8/8 × 336 = 336 MHz
 *   f_PLL = 336 / 2 = 168 MHz (SYSCLK)
 *   f_USB = 336 / 7 = 48 MHz ✓
 * ────────────────────────────────────────────── */

typedef enum {
    CLOCK_SOURCE_HSI = 0,      /* Internal 16 MHz RC (±1% trimmed)       */
    CLOCK_SOURCE_HSE = 1,      /* External crystal/resonator (4–26 MHz)  */
    CLOCK_SOURCE_PLL = 2       /* PLL output (up to 168 MHz on STM32F4)  */
} clock_source_t;

typedef struct {
    clock_source_t source;     /* System clock source                     */
    uint32_t       hse_freq;   /* External crystal frequency (Hz)         */
    uint32_t       target_sysclk; /* Desired system clock frequency (Hz)  */
    uint32_t       pll_m;      /* PLLM: input divider (2–63)              */
    uint32_t       pll_n;      /* PLLN: VCO multiplier (50–432)           */
    uint32_t       pll_p;      /* PLLP: main output divider (2, 4, 6, 8) */
    uint32_t       pll_q;      /* PLLQ: USB/SAI output divider (2–15)     */
    uint32_t       ahb_prescaler;   /* HCLK = SYSCLK / AHB_PRE            */
    uint32_t       apb1_prescaler;  /* APB1 max 42 MHz                    */
    uint32_t       apb2_prescaler;  /* APB2 max 84 MHz                    */
} clock_config_t;

/* Flash latency: number of wait states needed at a given frequency.
 * STM32F4 requires:
 *   0 WS for HCLK ≤ 30 MHz
 *   1 WS for HCLK ≤ 60 MHz (3.3 V) or ≤ 54 MHz (2.7 V)
 *   2 WS for HCLK ≤ 90 MHz (3.3 V)
 *   3 WS for HCLK ≤ 120 MHz
 *   4 WS for HCLK ≤ 150 MHz
 *   5 WS for HCLK ≤ 168 MHz
 * Higher wait states reduce performance but are necessary because
 * flash access time (~30 ns) is slower than the core cycle time
 * (~6 ns at 168 MHz).
 */
#define FLASH_LATENCY(Hz) \
    ((Hz) <= 30000000 ? 0 : \
     (Hz) <= 60000000 ? 1 : \
     (Hz) <= 90000000 ? 2 : \
     (Hz) <= 120000000 ? 3 : \
     (Hz) <= 150000000 ? 4 : 5)

/* ──────────────────────────────────────────────
 * System State Tracking
 * ────────────────────────────────────────────── */
typedef enum {
    SYSTEM_STATE_RESET          = 0,
    SYSTEM_STATE_CLOCK_INIT     = 1,
    SYSTEM_STATE_PERIPH_INIT    = 2,
    SYSTEM_STATE_RUNNING        = 3,
    SYSTEM_STATE_FAULT          = 4,
    SYSTEM_STATE_LOW_POWER      = 5
} system_state_t;

typedef struct {
    system_state_t state;
    clock_source_t active_clock;
    uint32_t       sysclk_hz;
    uint32_t       hclk_hz;
    uint32_t       apb1_hz;
    uint32_t       apb2_hz;
    uint32_t       uptime_ticks;     /* SysTick counter since boot */
    uint32_t       reset_count;      /* Number of resets since power-on */
    bool           clock_ok;         /* HSE stable, PLL locked */
} system_info_t;

/* ──────────────────────────────────────────────
 * Weak default handler declarations
 *
 * The linker script's vector table points to these handlers by default.
 * The application overrides them by defining a function with the same
 * name (strong definition replaces weak definition at link time).
 *
 * L2 Concept: Weak linking — the default handlers provide safe fallbacks
 *   (infinite loop) so an unhandled interrupt doesn't jump to a random
 *   address.
 * ────────────────────────────────────────────── */

void NMI_Handler(void);
void HardFault_Handler(void);
void MemManage_Handler(void);
void BusFault_Handler(void);
void UsageFault_Handler(void);
void SVC_Handler(void);
void DebugMon_Handler(void);
void PendSV_Handler(void);
void SysTick_Handler(void);

/* Default handler for all unimplemented interrupts */
void Default_Handler(void);

/* ──────────────────────────────────────────────
 * Startup API
 * ────────────────────────────────────────────── */

/*
 * system_init — early hardware initialization
 *
 * Called before main(). Sets up:
 *   1. FPU enable (CPACR register, if FPU present)
 *   2. Flash wait states and prefetch buffer
 *   3. Clock configuration (HSI→HSE→PLL→SYSCLK)
 *   4. Vector table relocation (to RAM, if desired)
 *   5. System tick (SysTick) for 1 ms interrupts
 *
 * @param config: clock configuration
 */
void system_init(const clock_config_t *config);

/*
 * system_clock_config — configure the system clock tree
 *
 * Pure clock setup, without FPU or flash configuration.
 * Can be called at runtime to change clock frequencies.
 *
 * @param config: clock configuration
 * @return true if PLL locked and clock switched successfully
 */
bool system_clock_config(const clock_config_t *config);

/*
 * system_get_info — retrieve current system state
 *
 * @param info: pointer to system_info_t to fill
 */
void system_get_info(system_info_t *info);

/*
 * copy_data_section — copy .data from flash to RAM
 *
 * The initialized data section (.data) has its initial values
 * stored in flash. At startup, these values must be copied to RAM
 * because global variables live in RAM.
 *
 * This is typically called from Reset_Handler before main().
 *
 * @param copy_table: array of section copy entries
 * @param num_entries: number of entries in the table
 *
 * Complexity: O(total_data_size)
 * Reference: ARM document DUI0472 — Scatter-loading
 */
void copy_data_section(const section_copy_entry_t *copy_table, size_t num_entries);

/*
 * zero_bss_section — fill .bss with zeros
 *
 * The uninitialized data section (.bss) must be zeroed at startup
 * per the C standard (§6.7.8: static storage duration objects
 * without explicit initialization are zero-initialized).
 *
 * @param start: start address of .bss section
 * @param size:  size of .bss in bytes
 *
 * Complexity: O(size)
 */
void zero_bss_section(uint8_t *start, size_t size);

/*
 * enable_fpu — enable the Floating Point Unit (Cortex-M4F/M7)
 *
 * Sets CP10 and CP11 full access bits in CPACR.
 * Without this, any FPU instruction triggers a UsageFault (NOCP).
 *
 * Complexity: O(1)
 * Reference: ARMv7-M Architecture Manual §B3.2.20 — CPACR
 */
void enable_fpu(void);

/*
 * system_reset — trigger a software system reset
 *
 * Clears all pending operations (DSB), writes SYSRESETREQ to SCB AIRCR.
 *
 * Does not return.
 */
void system_reset(void) __attribute__((noreturn));

/*
 * measure_boot_time — measure time from reset to main()
 *
 * Reads the DWT cycle counter (if available) to measure how long
 * startup (system_init + data copy + bss zero) took.
 *
 * @return boot time in CPU cycles
 *
 * Complexity: O(1)
 * Reference: ARMv7-M §C1 — DWT (Data Watchpoint and Trace)
 */
uint32_t measure_boot_time(void);

/*
 * get_reset_count — return number of system resets
 *
 * Stores a counter in backup SRAM (0x40024000 on STM32F4, 4 KB
 * that survives most resets but not power-on).
 *
 * @return reset counter value
 */
uint32_t get_reset_count(void);

#endif /* STARTUP_H */
