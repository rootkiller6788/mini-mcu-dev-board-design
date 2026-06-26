/*
 * cortex_m.h — ARM Cortex-M Core Peripherals (NVIC, SCB, SysTick, MPU)
 *
 * Knowledge Mapping (SKILL.md L1-L6):
 *   L1 Definitions: NVIC (Nested Vectored Interrupt Controller),
 *                   SCB (System Control Block), SysTick,
 *                   MPU (Memory Protection Unit), priority grouping,
 *                   exception number, vector table
 *   L2 Concepts:    Preemptive vs cooperative multitasking,
 *                   tail-chaining, late-arriving preemption,
 *                   interrupt priority and subpriority (grouping),
 *                   stack frame (8 registers auto-pushed)
 *   L3 Math:        Priority grouping: N groups = 2^(PRIGROUP) groups,
 *                   each with 2^(8-PRIGROUP) subpriorities.
 *                   Exception number → vector address:
 *                     addr = VTOR + (exception_number × 4)
 *   L4 Laws:        Rate Monotonic Scheduling (RMS) for priority assignment:
 *                   higher rate → higher priority. Liu & Layland (1973)
 *                   Deadline Monotonic Scheduling: shorter deadline → higher priority.
 *   L5 Algorithms:  PendSV context switch (OS tick → PendSV → task switch),
 *                   Fault analyzer (stack frame dump on HardFault),
 *                   Priority ceiling protocol (avoid priority inversion)
 *   L6 Problems:    RTOS context switch, fault handler debugging,
 *                   interrupt latency measurement
 *
 * Course Mapping:
 *   MIT 6.004 — Exceptions and interrupts
 *   Berkeley CS162 — Operating systems, context switching
 *   Valvano Ch.9 — Interrupts on Cortex-M
 *   ARMv7-M Architecture Reference Manual §B1.5 — Exception model
 *
 * Reference: ARMv7-M Architecture Reference Manual (ARM DDI 0403E)
 */

#ifndef CORTEX_M_H
#define CORTEX_M_H

#include <stdint.h>
#include <stdbool.h>

/* ──────────────────────────────────────────────
 * Exception Numbers (ARMv7-M Architecture §B1.5.2)
 *
 * Exception numbers 0–15 are system exceptions (fixed).
 * Numbers 16+ are external interrupts (vendor-specific IRQ0–IRQn).
 * The vector table has (16 + N_IRQ) entries, each 4 bytes.
 * ────────────────────────────────────────────── */

/* System exception numbers */
#define EXCEPTION_RESET             1U
#define EXCEPTION_NMI               2U
#define EXCEPTION_HARD_FAULT        3U
#define EXCEPTION_MEMMANAGE_FAULT   4U
#define EXCEPTION_BUS_FAULT         5U
#define EXCEPTION_USAGE_FAULT       6U
#define EXCEPTION_SVCALL            11U
#define EXCEPTION_DEBUG_MONITOR     12U
#define EXCEPTION_PENDSV            14U
#define EXCEPTION_SYSTICK           15U

/* First external interrupt number (IRQ0) */
#define EXCEPTION_IRQ0              16U

/* ──────────────────────────────────────────────
 * L1 Definition: Interrupt Priority
 *
 * Cortex-M supports up to 256 priority levels (8-bit field),
 * but most implementations use only the top 3–4 bits.
 * STM32F4: 4 bits implemented → 16 levels (0–15).
 *
 * Priority 0 = highest urgency (NMI-like but maskable).
 * Lower numerical value = higher priority.
 *
 * Priority Grouping: splits the priority field into
 *   Group priority (preemption) + Subpriority (order within group).
 *   Preemption: higher group priority interrupts a lower group ISR.
 *   Subpriority: only used when two interrupts with the same group
 *                are pending simultaneously.
 * ────────────────────────────────────────────── */
typedef enum {
    PRIORITY_GROUP0 = 0,  /* 0 bits group, 4 bits sub (16 subpriorities, no preempt) */
    PRIORITY_GROUP1 = 1,  /* 1 bit group (2), 3 bits sub (8)                         */
    PRIORITY_GROUP2 = 2,  /* 2 bits group (4), 2 bits sub (4)                        */
    PRIORITY_GROUP3 = 3,  /* 3 bits group (8), 1 bit sub (2)                         */
    PRIORITY_GROUP4 = 4,  /* 4 bits group (16), 0 bits sub (no subpriority)           */
    PRIORITY_GROUP5 = 5,  /* 5 bits group*, 3 bits reserved (*if 8-bit implemented)   */
    PRIORITY_GROUP6 = 6,
    PRIORITY_GROUP7 = 7
} nvic_priority_group_t;

#define NVIC_PRIORITY_HIGHEST   0U    /* Highest priority (lowest number) */
#define NVIC_PRIORITY_LOWEST    15U   /* Lowest priority (highest number) */

/* ──────────────────────────────────────────────
 * System Control Block (SCB) Register Map
 *
 * The SCB provides system control and status:
 *   - CPU ID (CPUID): silicon revision, implementer, variant
 *   - Interrupt control (ICSR): pending NMI, PendSV clear/set
 *   - Vector Table Offset (VTOR): relocate vector table
 *   - Application Interrupt/Reset Control (AIRCR): system reset, priority grouping
 *   - System Handler Control (SHCSR): fault enable, pending, active
 *   - Configurable Fault Status Registers (CFSR/MMFSR/BFSR/UFSR):
 *     precise fault diagnosis after a fault exception
 *   - HardFault Status (HFSR): escalation info
 * ────────────────────────────────────────────── */

/* SCB Register addresses (relative to SCB base = 0xE000ED00) */
typedef struct {
    volatile uint32_t CPUID;       /* 0xE000ED00: CPU ID Base Register */
    volatile uint32_t ICSR;        /* 0xE000ED04: Interrupt Control and State */
    volatile uint32_t VTOR;        /* 0xE000ED08: Vector Table Offset */
    volatile uint32_t AIRCR;       /* 0xE000ED0C: App Interrupt/Reset Control */
    volatile uint32_t SCR;         /* 0xE000ED10: System Control Register */
    volatile uint32_t CCR;         /* 0xE000ED14: Configuration and Control */
    volatile uint32_t SHPR[3];     /* 0xE000ED18: System Handler Priority Regs */
    volatile uint32_t SHCSR;       /* 0xE000ED24: System Handler Control/State */
    volatile uint32_t CFSR;        /* 0xE000ED28: Configurable Fault Status */
    volatile uint32_t HFSR;        /* 0xE000ED2C: HardFault Status */
    volatile uint32_t DFSR;        /* 0xE000ED30: Debug Fault Status */
    volatile uint32_t MMAR;        /* 0xE000ED34: MemManage Fault Address */
    volatile uint32_t BFAR;        /* 0xE000ED38: BusFault Address */
    volatile uint32_t AFSR;        /* 0xE000ED3C: Auxiliary Fault Status */
} scb_regs_t;

/* ICSR bits */
#define ICSR_PENDSVSET  (1U << 28)
#define ICSR_PENDSVCLR  (1U << 27)
#define ICSR_PENDSTSET  (1U << 26)
#define ICSR_PENDSTCLR  (1U << 25)
#define ICSR_VECTACTIVE_MASK  0x1FFU

/* AIRCR bits */
#define AIRCR_VECTKEY       (0x5FAU << 16)  /* Write key for register access */
#define AIRCR_PRIGROUP_POS  8U
#define AIRCR_PRIGROUP_MASK  (7U << 8)
#define AIRCR_SYSRESETREQ   (1U << 2)       /* System reset request */

/* SHCSR bits */
#define SHCSR_MEMFAULTENA    (1U << 16)
#define SHCSR_BUSFAULTENA    (1U << 17)
#define SHCSR_USGFAULTENA    (1U << 18)

/* CFSR sub-register byte offsets */
#define CFSR_MMFSR   0U   /* MemManage Fault Status (bits[7:0])   */
#define CFSR_BFSR    1U   /* BusFault Status (bits[15:8])         */
#define CFSR_UFSR    2U   /* UsageFault Status (bits[31:16])      */

/* ──────────────────────────────────────────────
 * Nested Vectored Interrupt Controller (NVIC) Register Map
 *
 * The NVIC manages interrupt enabling, pending, active status,
 * and priority for up to 240 external interrupts (IRQ0–IRQ239).
 * STM32F4 implements ~82 interrupts (IRQ0–IRQ81).
 *
 * Registers are 32-bit arrays:
 *   ISER[8]: Interrupt Set-Enable (write 1 to enable)
 *   ICER[8]: Interrupt Clear-Enable (write 1 to disable)
 *   ISPR[8]: Interrupt Set-Pending (software trigger)
 *   ICPR[8]: Interrupt Clear-Pending
 *   IABR[8]: Interrupt Active Bit (read-only)
 *   IP[240]: Interrupt Priority (8-bit, byte-accessible)
 * ────────────────────────────────────────────── */

typedef struct {
    volatile uint32_t ISER[8];   /* 0xE000E100: Set Enable Registers       */
    uint32_t _reserved0[24];
    volatile uint32_t ICER[8];   /* 0xE000E180: Clear Enable Registers     */
    uint32_t _reserved1[24];
    volatile uint32_t ISPR[8];   /* 0xE000E200: Set Pending Registers      */
    uint32_t _reserved2[24];
    volatile uint32_t ICPR[8];   /* 0xE000E280: Clear Pending Registers    */
    uint32_t _reserved3[24];
    volatile uint32_t IABR[8];   /* 0xE000E300: Active Bit Registers       */
    uint32_t _reserved4[56];
    volatile uint8_t  IP[240];   /* 0xE000E400: Priority (byte access)     */
    uint32_t _reserved5[580];
    volatile uint32_t STIR;      /* 0xE000EF00: Software Trigger Interrupt */
} nvic_regs_t;

/* ──────────────────────────────────────────────
 * SysTick Timer (System Tick)
 *
 * A 24-bit countdown timer integrated into the Cortex-M core.
 * Generates a periodic interrupt for OS tick or simple delays.
 *
 * Clock source: AHB clock (HCLK) or AHB/8 (HCLK/8).
 * Reload value: (tick_period × clock_freq) − 1
 *
 * For 1 ms tick at 168 MHz:
 *   RELOAD = (0.001 × 168,000,000) − 1 = 167,999
 *
 * L1 Definition: System tick — the heartbeat of an RTOS.
 *   Each tick triggers the scheduler to decide if a context
 *   switch is needed (round-robin, priority-based preemption).
 * ────────────────────────────────────────────── */

typedef struct {
    volatile uint32_t CSR;       /* 0xE000E010: Control and Status         */
    volatile uint32_t RVR;       /* 0xE000E014: Reload Value Register      */
    volatile uint32_t CVR;       /* 0xE000E018: Current Value Register     */
    volatile uint32_t CALIB;     /* 0xE000E01C: Calibration Value Register */
} systick_regs_t;

#define SYSTICK_CSR_ENABLE    (1U << 0)
#define SYSTICK_CSR_TICKINT   (1U << 1)
#define SYSTICK_CSR_CLKSOURCE (1U << 2)
#define SYSTICK_CSR_COUNTFLAG (1U << 16)

/* ──────────────────────────────────────────────
 * L2 Concept: Exception Stack Frame
 *
 * When an exception occurs, the processor pushes 8 registers
 * (xPSR, PC, LR, R12, R3, R2, R1, R0) onto the stack automatically.
 * This is called "stacking" and takes 12 cycles.
 *
 * Unstacking (restoring) takes 12 cycles on exception return.
 * Late-arriving preemption: if a higher-priority exception arrives
 * during stacking, the processor re-starts stacking for the new
 * exception after the current stacking completes.
 *
 * Tail-chaining: when one exception completes and a pending
 * exception exists, the processor skips unstacking+stacking
 * and proceeds directly to the pending exception, saving 24 cycles.
 *
 * Reference: ARMv7-M §B1.5.6 — Exception entry and return
 * ────────────────────────────────────────────── */

/* Stack frame: 8 words (32 bytes) pushed onto process stack */
typedef struct __attribute__((packed)) {
    uint32_t r0;
    uint32_t r1;
    uint32_t r2;
    uint32_t r3;
    uint32_t r12;
    uint32_t lr;          /* Link Register (return address) */
    uint32_t pc;          /* Program Counter (faulting instruction) */
    uint32_t xpsr;        /* Program Status Register */
} exception_frame_t;

/* ──────────────────────────────────────────────
 * L1 Definition: Fault Status Information
 *
 * Decoded from CFSR provides precise fault analysis:
 *
 * MemManage Fault (MMFSR):
 *   - IACCVIOL: instruction fetch from XN (eXecute Never) region
 *   - DACCVIOL: data access to memory not permitted by MPU
 *   - MUNSTKERR: unstacking from an illegal address (exception return)
 *   - MSTKERR: stacking to an illegal address (exception entry)
 *
 * BusFault (BFSR):
 *   - IBUSERR: instruction fetch from an address that causes a bus error
 *   - PRECISERR: precise data bus error (address in BFAR)
 *   - IMPRECISERR: imprecise data bus error (address unknown)
 *
 * UsageFault (UFSR):
 *   - UNDEFINSTR: undefined instruction (trying to execute data)
 *   - INVSTATE: invalid EPSR state (e.g., switching to ARM state in Cortex-M)
 *   - INVPC: invalid EXC_RETURN value
 *   - NOCP: no coprocessor (e.g., FPU instruction when FPU disabled)
 *   - DIVBYZERO: divide-by-zero (if DIV_0_TRP is enabled)
 *   - UNALIGNED: unaligned access (if UNALIGN_TRP is enabled)
 *
 * HardFault (HFSR):
 *   - VECTTBL: vector table read on exception processing failed
 *   - FORCED: another fault (MemManage/Bus/Usage) was escalated to HardFault
 *             because the fault handler was not enabled or returned a fault
 * ────────────────────────────────────────────── */

typedef struct {
    /* MMFSR: MemManage Fault Status */
    bool mm_instruction_violation;    /* IACCVIOL */
    bool mm_data_violation;           /* DACCVIOL */
    bool mm_unstack_error;            /* MUNSTKERR */
    bool mm_stack_error;              /* MSTKERR */
    bool mm_address_valid;            /* MMARVALID */

    /* BFSR: Bus Fault Status */
    bool bf_instruction_error;        /* IBUSERR  */
    bool bf_precise_error;            /* PRECISERR */
    bool bf_imprecise_error;          /* IMPRECISERR */
    bool bf_address_valid;            /* BFARVALID */

    /* UFSR: Usage Fault Status */
    bool uf_undefined_instruction;    /* UNDEFINSTR */
    bool uf_invalid_state;            /* INVSTATE   */
    bool uf_invalid_pc;               /* INVPC      */
    bool uf_no_coprocessor;           /* NOCP       */
    bool uf_div_by_zero;              /* DIVBYZERO  */
    bool uf_unaligned;                /* UNALIGNED  */

    /* HFSR: HardFault Status */
    bool hf_forced;                   /* FORCED  */
    bool hf_vector_table;             /* VECTTBL */

    /* Addresses */
    uint32_t fault_address;           /* BFAR or MMAR */
} fault_info_t;

/* ──────────────────────────────────────────────
 * System Reset Reasons (RCC CSR)
 * ────────────────────────────────────────────── */
#define RCC_CSR_PORRSTF     (1U << 27)  /* Power-On Reset Flag */
#define RCC_CSR_PINRSTF     (1U << 26)  /* Pin Reset Flag      */
#define RCC_CSR_BORRSTF     (1U << 25)  /* Brown-Out Reset     */
#define RCC_CSR_SFTRSTF     (1U << 24)  /* Software Reset      */
#define RCC_CSR_IWDGRSTF    (1U << 29)  /* Independent WDG     */
#define RCC_CSR_WWDGRSTF    (1U << 30)  /* Window Watchdog     */
#define RCC_CSR_LPWRRSTF    (1U << 31)  /* Low-Power Reset     */

/* ──────────────────────────────────────────────
 * Cortex-M Core API
 * ────────────────────────────────────────────── */

/*
 * nvic_enable_irq — enable an external interrupt
 *
 * Writes to ISER[n], where n = irq_number / 32.
 * Writing 1 to a bit enables the interrupt; writing 0 has no effect.
 * This is a write-1-to-set register.
 *
 * @param irq_number: IRQ number (0 = first external interrupt, EXCEPTION_IRQ0)
 *
 * Complexity: O(1)
 */
void nvic_enable_irq(uint8_t irq_number);

/*
 * nvic_disable_irq — disable an external interrupt
 *
 * Writes to ICER[n]. Writing 1 disables; writing 0 has no effect.
 *
 * @param irq_number: IRQ number
 */
void nvic_disable_irq(uint8_t irq_number);

/*
 * nvic_set_pending — force an interrupt to pending state
 *
 * Software interrupt trigger. The interrupt fires when its priority
 * is higher than the currently executing code.
 *
 * @param irq_number: IRQ number
 */
void nvic_set_pending(uint8_t irq_number);

/*
 * nvic_clear_pending — remove pending state
 *
 * Cancels a pending interrupt before it fires.
 *
 * @param irq_number: IRQ number
 */
void nvic_clear_pending(uint8_t irq_number);

/*
 * nvic_set_priority — set interrupt priority
 *
 * The NVIC IP register is byte-accessible. Each IRQ has one byte
 * of priority field, but only the top N bits are implemented
 * (N = 3, 4, or 8 depending on the implementation).
 *
 * STM32F4 implements 4 bits → priority values 0x00, 0x10, ..., 0xF0.
 * The hardware shifts the value into the MSB of the 8-bit field.
 *
 * @param irq_number: IRQ number
 * @param priority:   priority value (hardware-shifted into MSB)
 *
 * Complexity: O(1)
 */
void nvic_set_priority(uint8_t irq_number, uint8_t priority);

/*
 * nvic_get_active — check if an interrupt is currently active
 *
 * Reads IABR[n]. An interrupt is "active" when its ISR is executing
 * or has been preempted by a higher-priority interrupt.
 *
 * @param irq_number: IRQ number
 * @return true if active
 */
bool nvic_get_active(uint8_t irq_number);

/*
 * nvic_set_priority_grouping — configure priority grouping
 *
 * Writes PRIGROUP field in SCB->AIRCR. Must be the same across
 * the entire application; setting it after interrupts are enabled
 * may cause race conditions.
 *
 * @param group: priority grouping configuration
 */
void nvic_set_priority_grouping(nvic_priority_group_t group);

/*
 * systick_init — configure and enable the SysTick timer
 *
 * Sets the reload value for the desired tick rate.
 * Source: HCLK or HCLK/8 (CLKSOURCE bit).
 * Enables the counter and interrupt (TICKINT).
 *
 * For 1 ms tick at 168 MHz with HCLK source:
 *   systick_init(168000000, 1000, true) → RELOAD = 167999
 *
 * @param cpu_freq_hz: CPU clock frequency (HCLK)
 * @param tick_rate_hz: desired interrupt rate (e.g., 1000 for 1 kHz)
 * @param use_hclk: true = HCLK, false = HCLK/8
 *
 * Complexity: O(1)
 */
void systick_init(uint32_t cpu_freq_hz, uint32_t tick_rate_hz, bool use_hclk);

/*
 * systick_interrupt_count — global tick counter
 *
 * Incremented by the SysTick_Handler ISR. Applications can
 * use this for non-blocking delays and timeouts.
 *
 * @return current tick count
 */
uint32_t systick_get_count(void);

/*
 * delay_systick — non-blocking delay using SysTick
 *
 * Compares the global tick counter against a target.
 * The caller must poll this function, allowing other work
 * to be done while waiting.
 *
 * Usage:
 *   uint32_t target = systick_get_count() + 1000; // 1000 ms
 *   while (!delay_systick_elapsed(target)) { do_other_work(); }
 *
 * @param target: tick count at which delay expires
 * @return true when elapsed
 */
bool delay_systick_elapsed(uint32_t target);

/*
 * scb_system_reset — trigger a full system reset
 *
 * Writes the VECTKEY + SYSRESETREQ sequence to AIRCR.
 * The reset occurs after a short delay (a few clock cycles).
 * A DSB instruction precedes the write to ensure all memory
 * accesses are complete.
 *
 * Complexity: O(1)
 * Does not return.
 */
void scb_system_reset(void) __attribute__((noreturn));

/*
 * scb_get_fault_info — decode fault registers after a HardFault
 *
 * Reads CFSR, HFSR, MMAR, and BFAR to produce a human-readable
 * fault diagnosis. Call from HardFault_Handler.
 *
 * @param info: pointer to fault_info_t to fill
 */
void scb_get_fault_info(fault_info_t *info);

/*
 * fault_handler_dump — dump fault information over UART
 *
 * Formats fault_info_t as human-readable text and transmits
 * over the specified UART for debugging.
 *
 * @param uart_index: UART for output
 * @param info:       decoded fault information
 * @param sp:         stack pointer at time of fault (from asm)
 *
 * Usage (in HardFault_Handler):
 *   fault_info_t info;
 *   scb_get_fault_info(&info);
 *   fault_handler_dump(0, &info, __get_MSP());
 *   while(1);
 */
void fault_handler_dump(uint8_t uart_index, const fault_info_t *info, uint32_t sp);

/*
 * enable_fault_handlers — enable MemManage, BusFault, and UsageFault
 *
 * Without this, all faults escalate to HardFault, losing
 * the specific fault status information.
 */
void enable_fault_handlers(void);

/*
 * __disable_irq / __enable_irq wrappers (in C)
 *
 * Disables all interrupts with configurable priority
 * (i.e., sets PRIMASK to 1). This is a global disable;
 * NMI and HardFault are NOT disabled.
 *
 * __enable_irq clears PRIMASK.
 */
void cortex_m_disable_irq(void);
void cortex_m_enable_irq(void);

/*
 * scb_get_reset_reason — determine the reset cause
 *
 * Reads RCC CSR, returns reset reason, and clears the flags
 * for subsequent detection.
 *
 * @return reset reason (power-on, watchdog, software, etc.)
 */
uint8_t scb_get_reset_reason(void);

#endif /* CORTEX_M_H */
