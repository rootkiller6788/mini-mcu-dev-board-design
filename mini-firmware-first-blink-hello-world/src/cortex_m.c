/*
 * cortex_m.c — Cortex-M Core Peripheral Implementation
 *
 * Knowledge points implemented (independent):
 *   1. nvic_enable/disable/set_pending/set_priority — interrupt management
 *   2. nvic_set_priority_grouping — priority grouping configuration
 *   3. systick_init — SysTick timer configuration for RTOS tick
 *   4. systick_get_count / delay_systick_elapsed — non-blocking delay
 *   5. scb_system_reset — system reset via AIRCR
 *   6. scb_get_fault_info — fault register decoding for HardFault
 *   7. fault_handler_dump — formatted fault output over UART
 *   8. enable_fault_handlers — enable MemManage/BusFault/UsageFault
 *   9. cortex_m_disable_irq/enable_irq — PRIMASK control
 *  10. scb_get_reset_reason — RCC CSR reset cause decoding
 */

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "cortex_m.h"
#include "uart.h"
#include "arch_compat.h"

/* ──────────────────────────────────────────────
 * Peripheral Base Pointers (compile-time)
 * ────────────────────────────────────────────── */

#define SCB_BASE    0xE000ED00U
#define NVIC_BASE   0xE000E100U
#define SYSTICK_BASE 0xE000E010U

#define SCB    ((scb_regs_t *)SCB_BASE)
#define NVIC   ((nvic_regs_t *)NVIC_BASE)
#define SYSTICK ((systick_regs_t *)SYSTICK_BASE)

/* ──────────────────────────────────────────────
 * Global tick counter (incremented by SysTick_Handler ISR)
 * ────────────────────────────────────────────── */

static volatile uint32_t _systick_counter = 0U;

/* ──────────────────────────────────────────────
 * NVIC API — Interrupt Enable/Disable/Priority
 *
 * Knowledge: NVIC register array indexing.
 *
 * The NVIC uses register arrays where each bit controls one IRQ:
 *   ISER[irq/32].bit[irq%32] — Set-Enable
 *   ICER[irq/32].bit[irq%32] — Clear-Enable
 *   ISPR[irq/32].bit[irq%32] — Set-Pending
 *   ICPR[irq/32].bit[irq%32] — Clear-Pending
 *   IABR[irq/32].bit[irq%32] — Active Bit (read-only)
 *   IP[irq] — Priority (byte-accessible array)
 *
 * ISER/ICER/ISPR/ICPR are "write-1" registers:
 *   Writing 1 sets/clears the bit; writing 0 has no effect.
 *   This avoids read-modify-write races in ISRs.
 *
 * Reference: ARMv7-M Architecture Manual §B3.4 — NVIC
 * ────────────────────────────────────────────── */

void nvic_enable_irq(uint8_t irq_number)
{
    uint8_t reg_index = irq_number / 32U;
    uint8_t bit_pos   = irq_number % 32U;

    if (reg_index < 8U) {
        NVIC->ISER[reg_index] = (1U << bit_pos);
    }
}

void nvic_disable_irq(uint8_t irq_number)
{
    uint8_t reg_index = irq_number / 32U;
    uint8_t bit_pos   = irq_number % 32U;

    if (reg_index < 8U) {
        NVIC->ICER[reg_index] = (1U << bit_pos);
    }
}

void nvic_set_pending(uint8_t irq_number)
{
    uint8_t reg_index = irq_number / 32U;
    uint8_t bit_pos   = irq_number % 32U;

    if (reg_index < 8U) {
        NVIC->ISPR[reg_index] = (1U << bit_pos);
    }
}

void nvic_clear_pending(uint8_t irq_number)
{
    uint8_t reg_index = irq_number / 32U;
    uint8_t bit_pos   = irq_number % 32U;

    if (reg_index < 8U) {
        NVIC->ICPR[reg_index] = (1U << bit_pos);
    }
}

void nvic_set_priority(uint8_t irq_number, uint8_t priority)
{
    /* STM32F4 implements 4 priority bits [7:4].
     * The user-provided priority is shifted into the MSB position.
     * NVIC IP registers are byte-accessible. */
    NVIC->IP[irq_number] = priority;
}

bool nvic_get_active(uint8_t irq_number)
{
    uint8_t reg_index = irq_number / 32U;
    uint8_t bit_pos   = irq_number % 32U;

    if (reg_index >= 8U) {
        return false;
    }

    return (NVIC->IABR[reg_index] & (1U << bit_pos)) != 0U;
}

/* ──────────────────────────────────────────────
 * nvic_set_priority_grouping — configure PRIGROUP
 *
 * Knowledge: Priority grouping for nested interrupts.
 *
 * AIRCR[10:8] = PRIGROUP selects how the 4-bit priority field
 * (or 8-bit on some implementations) is split:
 *
 * PRIGROUP | Group bits | Sub bits | Groups | Sub per group
 *     0    |     0      |    4     |    1   |    16
 *     1    |     1      |    3     |    2   |     8
 *     2    |     2      |    2     |    4   |     4
 *     3    |     3      |    1     |    8   |     2
 *     4    |     4      |    0     |   16   |     1
 *
 * Group priority controls preemption: a higher group priority
 * (lower number) can preempt a lower group priority ISR.
 *
 * Subpriority only matters when two interrupts with the same group
 * priority are pending simultaneously → higher subpriority is
 * serviced first.
 *
 * Writing AIRCR requires VECTKEY in the upper 16 bits to prevent
 * accidental writes. Without VECTKEY, the write is ignored.
 * ────────────────────────────────────────────── */

void nvic_set_priority_grouping(nvic_priority_group_t group)
{
    uint32_t aircr;

    /* Read current AIRCR, preserve VECTKEYSTAT (upper 16 bits read-only) */
    aircr = SCB->AIRCR;

    /* Clear PRIGROUP field [10:8] */
    aircr &= ~AIRCR_PRIGROUP_MASK;

    /* Set new PRIGROUP with VECTKEY */
    aircr |= AIRCR_VECTKEY;
    aircr |= ((uint32_t)group << AIRCR_PRIGROUP_POS) & AIRCR_PRIGROUP_MASK;

    SCB->AIRCR = aircr;

    /* Data synchronization barrier: ensure write completes before
     * any subsequent NVIC operations that depend on the new grouping. */
    DSB();
}

/* ──────────────────────────────────────────────
 * systick_init — configure SysTick timer
 *
 * Knowledge: SysTick as a 24-bit countdown timer.
 *
 * SysTick is a simple decrementing counter:
 *   1. Load RVR (Reload Value Register) with (ticks_per_period − 1).
 *   2. Write any value to CVR (Current Value Register) to clear it.
 *   3. Set CSR: CLKSOURCE + TICKINT + ENABLE bits.
 *
 * Clock source:
 *   CLKSOURCE=1 → processor clock (HCLK)
 *   CLKSOURCE=0 → processor clock / 8 (HCLK/8, for low power)
 *
 * TICKINT=1 → SysTick_Handler exception when counter reaches 0.
 * ENABLE=1 → counter starts.
 *
 * Reload calculation:
 *   reload = (cpu_freq_hz / tick_rate_hz) − 1
 *
 * For 168 MHz, 1 kHz tick:
 *   reload = (168,000,000 / 1,000) − 1 = 167,999
 *
 * But SysTick is only 24-bit → max reload = 0xFFFFFF = 16,777,215.
 * At 168 MHz: max tick period = 16,777,216 / 168,000,000 ≈ 0.1 s.
 * So with HCLK as source, the minimum tick rate is:
 *   168,000,000 / 16,777,216 ≈ 10 Hz.
 *
 * For slower tick rates, use HCLK/8 (21 MHz effective):
 *   min rate = 21,000,000 / 16,777,216 ≈ 1.25 Hz.
 * ────────────────────────────────────────────── */

void systick_init(uint32_t cpu_freq_hz, uint32_t tick_rate_hz, bool use_hclk)
{
    uint32_t reload;

    if (tick_rate_hz == 0 || cpu_freq_hz == 0) {
        return;
    }

    if (!use_hclk) {
        cpu_freq_hz /= 8U;
    }

    reload = cpu_freq_hz / tick_rate_hz;

    /* Clamp to 24-bit max */
    if (reload > 0xFFFFFFU) {
        reload = 0xFFFFFFU;
    }
    if (reload > 0U) {
        reload -= 1U;
    }

    /* Program SysTick */
    SYSTICK->RVR = reload & 0xFFFFFFU;
    SYSTICK->CVR = 0U;  /* Write any value to clear counter */

    /* Configure and enable:
     *   CLKSOURCE (bit 2): 1 = HCLK, 0 = HCLK/8
     *   TICKINT (bit 1):   1 = enable interrupt
     *   ENABLE (bit 0):    1 = enable counter */
    {
        uint32_t csr = 0;
        if (use_hclk) {
            csr |= SYSTICK_CSR_CLKSOURCE;
        }
        csr |= SYSTICK_CSR_TICKINT;
        csr |= SYSTICK_CSR_ENABLE;
        SYSTICK->CSR = csr;
    }

    _systick_counter = 0U;
}

/* ──────────────────────────────────────────────
 * SysTick_Handler — increment global tick counter
 *
 * This is the standard SysTick ISR. On each tick:
 *   1. Increment the global millisecond counter (_systick_counter)
 *   2. The application can use this counter for non-blocking delays
 *      and timeouts.
 *
 * In a real RTOS, this ISR would also call the scheduler.
 * ────────────────────────────────────────────── */

void SysTick_Handler(void)
{
    _systick_counter++;
}

/* ──────────────────────────────────────────────
 * systick_get_count — read global tick counter
 * ────────────────────────────────────────────── */

uint32_t systick_get_count(void)
{
    return _systick_counter;
}

/* ──────────────────────────────────────────────
 * delay_systick_elapsed — non-blocking delay check
 *
 * Knowledge: Non-blocking delay using a target timestamp.
 *
 * Unsigned subtraction naturally handles counter wrap-around:
 *   (current − target) < 0x80000000 → target not yet reached
 *   (current − target) ≥ 0x80000000 → (would be) overflow → target reached
 *
 * More precisely: the delay has elapsed if
 *   (current − start) ≥ wait_ticks, where all values are unsigned.
 * Since current = systick_get_count() and target = start + wait_ticks:
 *   elapsed when (current − start) ≥ wait_ticks.
 *
 * The function signature takes a target value rather than a duration
 * to allow the same target to be checked in a loop efficiently.
 * ────────────────────────────────────────────── */

bool delay_systick_elapsed(uint32_t target)
{
    uint32_t current = _systick_counter;
    /* Elapsed if current has passed target (unsigned comparison) */
    return ((int32_t)(current - target) >= 0);
}

/* ──────────────────────────────────────────────
 * scb_system_reset — trigger full system reset
 *
 * Knowledge: System reset via AIRCR.
 *
 * AIRCR[2] = SYSRESETREQ: requests a system reset.
 * The reset is not instantaneous — it takes a few clock cycles
 * for the reset controller to assert and deassert the reset signal.
 *
 * The VECTKEY must be written in the same write as SYSRESETREQ.
 * A DSB (Data Synchronization Barrier) instruction ensures all
 * prior memory writes are complete before the reset is triggered.
 *
 * Reference: ARMv7-M §B3.2.5 — AIRCR
 * ────────────────────────────────────────────── */

void scb_system_reset(void)
{
    /* Data synchronization barrier: ensure all stores complete */
    DSB();

    /* Request system reset */
    SCB->AIRCR = AIRCR_VECTKEY | AIRCR_SYSRESETREQ;

    /* Data synchronization barrier: ensure the reset request is issued */
    DSB();

    /* Wait for reset (should be unreachable) */
    for (;;) {
        WFI();
    }
}

/* ──────────────────────────────────────────────
 * enable_fault_handlers — enable configurable fault handlers
 *
 * Knowledge: Fault escalation prevention.
 *
 * By default (after reset), MemManage, BusFault, and UsageFault
 * are disabled. Any fault in these categories escalates to
 * HardFault. This loses the specific fault information (MMAR,
 * BFAR, CFSR breakdown).
 *
 * Enabling these fault handlers provides more precise debugging:
 *   - MemManage: MPU violation, access to XN region
 *   - BusFault: bad memory access (unmapped address, bus error)
 *   - UsageFault: undefined instruction, bad state, divide-by-zero
 *
 * SHCSR register bits:
 *   [18] = USGFAULTENA  — UsageFault enable
 *   [17] = BUSFAULTENA  — BusFault enable
 *   [16] = MEMFAULTENA  — MemManage enable
 * ────────────────────────────────────────────── */

void enable_fault_handlers(void)
{
    SCB->SHCSR |= (SHCSR_MEMFAULTENA | SHCSR_BUSFAULTENA | SHCSR_USGFAULTENA);
    DSB();
}

/* ──────────────────────────────────────────────
 * scb_get_fault_info — decode CFSR and HFSR
 *
 * Knowledge: Fault register decoding for crash diagnostics.
 *
 * After a fault, the handler can read these registers to determine
 * what went wrong. The CFSR is actually three sub-registers in one:
 *   CFSR[7:0]   = MMFSR (MemManage Fault Status Register)
 *   CFSR[15:8]  = BFSR  (Bus Fault Status Register)
 *   CFSR[31:16] = UFSR  (Usage Fault Status Register)
 *
 * MMFSR flags:
 *   [7] = MMARVALID — the fault address in MMAR is valid
 *   [4] = MSTKERR — stacking error
 *   [3] = MUNSTKERR — unstacking error
 *   [1] = DACCVIOL — data access violation
 *   [0] = IACCVIOL — instruction access violation
 *
 * BFSR flags:
 *   [7] = BFARVALID — the fault address in BFAR is valid
 *   [4] = STKERR — stacking error (bus fault on exception entry)
 *   [3] = UNSTKERR — unstacking error (bus fault on exception return)
 *   [2] = IMPRECISERR — imprecise data access (address unknown!)
 *   [1] = PRECISERR — precise data access (BFAR has the address)
 *   [0] = IBUSERR — instruction bus error
 *
 * UFSR flags:
 *   [9] = DIVBYZERO — SDIV/UDIV with zero divisor
 *   [8] = UNALIGNED — unaligned access when UNALIGN_TRP=1
 *   [3] = NOCP — no coprocessor (FPU instruction with FPU disabled)
 *   [2] = INVPC — invalid PC loaded (e.g., EXC_RETURN with reserved bits set)
 *   [1] = INVSTATE — invalid EPSR (e.g., trying to execute in ARM state)
 *   [0] = UNDEFINSTR — undefined instruction
 *
 * HFSR flags:
 *   [30] = FORCED — HardFault was forced (another fault escalated)
 *   [1]  = VECTTBL — bus fault on vector table read
 * ────────────────────────────────────────────── */

void scb_get_fault_info(fault_info_t *info)
{
    uint8_t mmfsr;
    uint8_t bfsr;
    uint16_t ufsr;
    uint32_t cfsr;
    uint32_t hfsr;

    if (info == NULL) {
        return;
    }

    memset(info, 0, sizeof(*info));

    cfsr = SCB->CFSR;
    hfsr = SCB->HFSR;

    /* Byte-level access to CFSR sub-fields */
    mmfsr = (uint8_t)(cfsr & 0xFFU);
    bfsr  = (uint8_t)((cfsr >> 8U) & 0xFFU);
    ufsr  = (uint16_t)((cfsr >> 16U) & 0xFFFFU);

    /* MMFSR decoding */
    info->mm_instruction_violation = (mmfsr & 0x01U) != 0U;
    info->mm_data_violation        = (mmfsr & 0x02U) != 0U;
    info->mm_unstack_error         = (mmfsr & 0x08U) != 0U;
    info->mm_stack_error           = (mmfsr & 0x10U) != 0U;
    info->mm_address_valid         = (mmfsr & 0x80U) != 0U;

    /* BFSR decoding */
    info->bf_instruction_error     = (bfsr & 0x01U) != 0U;
    info->bf_precise_error         = (bfsr & 0x02U) != 0U;
    info->bf_imprecise_error       = (bfsr & 0x04U) != 0U;
    info->bf_address_valid         = (bfsr & 0x80U) != 0U;

    /* UFSR decoding */
    info->uf_undefined_instruction  = (ufsr & 0x0001U) != 0U;
    info->uf_invalid_state          = (ufsr & 0x0002U) != 0U;
    info->uf_invalid_pc             = (ufsr & 0x0004U) != 0U;
    info->uf_no_coprocessor         = (ufsr & 0x0008U) != 0U;
    info->uf_div_by_zero            = (ufsr & 0x0200U) != 0U;
    info->uf_unaligned              = (ufsr & 0x0100U) != 0U;

    /* HFSR decoding */
    info->hf_forced     = (hfsr & (1U << 30)) != 0U;
    info->hf_vector_table = (hfsr & 0x02U) != 0U;

    /* Capture fault address if valid */
    if (info->bf_address_valid) {
        info->fault_address = SCB->BFAR;
    } else if (info->mm_address_valid) {
        info->fault_address = SCB->MMAR;
    } else {
        info->fault_address = 0xFFFFFFFFU;  /* Unknown */
    }
}

/* ──────────────────────────────────────────────
 * fault_handler_dump — print fault info over UART
 *
 * Knowledge: HardFault debugging via serial output.
 *
 * This is the most useful debugging tool when the MCU crashes
 * in the field and a debugger is not connected. The fault handler
 * captures the register state at the moment of the fault and
 * outputs it over a UART (which must be pre-initialized).
 *
 * Key information from the exception frame (auto-stacked):
 *   - R0–R3, R12: function arguments and temporaries
 *   - LR: return address (can identify which function was executing)
 *   - PC: the instruction that caused the fault
 *   - xPSR: program status (condition codes, exception number)
 *
 * The stack frame address is the SP value at the moment the
 * exception was taken. It is passed to fault_handler_dump from
 * the assembly wrapper in the exception handler.
 * ────────────────────────────────────────────── */

void fault_handler_dump(uint8_t uart_index, const fault_info_t *info, uint32_t sp)
{
    const exception_frame_t *frame;

    if (info == NULL) {
        return;
    }

    uart_printf(uart_index, "\r\n========================================\r\n");
    uart_printf(uart_index, "  HARD FAULT — System Crash\r\n");
    uart_printf(uart_index, "========================================\r\n");

    /* Fault type */
    if (info->hf_forced) {
        uart_printf(uart_index, "Cause: Escalated fault (see details below)\r\n");
    }
    if (info->hf_vector_table) {
        uart_printf(uart_index, "Cause: Vector table read failed\r\n");
        return;  /* Can't recover — vector table is corrupt */
    }

    /* MemManage faults */
    if (info->mm_instruction_violation)
        uart_printf(uart_index, "MMFSR: Instruction access violation\r\n");
    if (info->mm_data_violation)
        uart_printf(uart_index, "MMFSR: Data access violation\r\n");
    if (info->mm_unstack_error)
        uart_printf(uart_index, "MMFSR: Unstack error\r\n");
    if (info->mm_stack_error)
        uart_printf(uart_index, "MMFSR: Stack error\r\n");

    /* Bus faults */
    if (info->bf_instruction_error)
        uart_printf(uart_index, "BFSR: Instruction bus error\r\n");
    if (info->bf_precise_error)
        uart_printf(uart_index, "BFSR: Precise data bus error\r\n");
    if (info->bf_imprecise_error)
        uart_printf(uart_index, "BFSR: Imprecise data bus error\r\n");

    /* Usage faults */
    if (info->uf_undefined_instruction)
        uart_printf(uart_index, "UFSR: Undefined instruction\r\n");
    if (info->uf_invalid_state)
        uart_printf(uart_index, "UFSR: Invalid state (ARM mode?)\r\n");
    if (info->uf_invalid_pc)
        uart_printf(uart_index, "UFSR: Invalid PC / EXC_RETURN\r\n");
    if (info->uf_no_coprocessor)
        uart_printf(uart_index, "UFSR: No coprocessor (FPU disabled?)\r\n");
    if (info->uf_div_by_zero)
        uart_printf(uart_index, "UFSR: Divide by zero\r\n");
    if (info->uf_unaligned)
        uart_printf(uart_index, "UFSR: Unaligned access\r\n");

    /* Fault address */
    if (info->mm_address_valid || info->bf_address_valid) {
        uart_printf(uart_index, "Fault address: 0x%X\r\n", info->fault_address);
    }

    /* Dump exception frame (stack contents) */
    frame = (const exception_frame_t *)(uintptr_t)sp;
    if (frame != NULL) {
        uart_printf(uart_index, "--- Exception Stack Frame ---\r\n");
        uart_printf(uart_index, "R0  = 0x%X\r\n", frame->r0);
        uart_printf(uart_index, "R1  = 0x%X\r\n", frame->r1);
        uart_printf(uart_index, "R2  = 0x%X\r\n", frame->r2);
        uart_printf(uart_index, "R3  = 0x%X\r\n", frame->r3);
        uart_printf(uart_index, "R12 = 0x%X\r\n", frame->r12);
        uart_printf(uart_index, "LR  = 0x%X\r\n", frame->lr);
        uart_printf(uart_index, "PC  = 0x%X  <-- faulting instruction\r\n", frame->pc);
        uart_printf(uart_index, "xPSR = 0x%X\r\n", frame->xpsr);
    }

    uart_printf(uart_index, "========================================\r\n");
    uart_printf(uart_index, "System halted. Reset required.\r\n");
}

/* ──────────────────────────────────────────────
 * cortex_m_disable_irq — disable interrupts (set PRIMASK)
 *
 * Knowledge: PRIMASK for global interrupt masking.
 *
 * PRIMASK is a 1-bit register in the Cortex-M core.
 *   PRIMASK=0 → interrupts enabled (default)
 *   PRIMASK=1 → all interrupts with configurable priority are disabled
 *
 * NMI and HardFault are NOT affected by PRIMASK.
 *
 * Use cases:
 *   - Critical section: protect a shared variable from ISR access
 *   - Context switch: atomic stack pointer swap
 *   - Bootloader jump: ensure no ISR fires mid-jump
 *
 * Reference: ARMv7-M §B1.4.1 — Special-purpose program status registers
 * ────────────────────────────────────────────── */

void cortex_m_disable_irq(void)
{
    CPSID_I();
}

void cortex_m_enable_irq(void)
{
    CPSIE_I();
}

/* ──────────────────────────────────────────────
 * scb_get_reset_reason — read RCC CSR
 *
 * Knowledge: Reset cause determination.
 *
 * The RCC CSR (Control/Status Register) records the reason for
 * the last reset. After reading, the RMVF (Remove Reset Flag) bit
 * must be set to clear the flags for the next reset detection.
 *
 * Full CSR bit decode:
 *   [31] = LPWRRSTF — Low-power reset
 *   [30] = WWDGRSTF — Window watchdog reset
 *   [29] = IWDGRSTF — Independent watchdog reset
 *   [28] = SFTRSTF  — Software reset
 *   [27] = PORRSTF  — Power-on reset
 *   [26] = PINRSTF  — External pin reset (NRST)
 *   [25] = BORRSTF  — Brown-out reset
 *   [24] = RMVF     — Remove reset flag (write 1 to clear all flags)
 * ────────────────────────────────────────────── */

uint8_t scb_get_reset_reason(void)
{
    volatile uint32_t *rcc_csr = (volatile uint32_t *)0x40023874U;
    uint32_t csr = *rcc_csr;

    /* Clear flags for next detection */
    *rcc_csr |= (1U << 24);

    if (csr & RCC_CSR_LPWRRSTF) return 5;  /* Low-power */
    if (csr & RCC_CSR_WWDGRSTF) return 4;  /* Window WDG */
    if (csr & RCC_CSR_IWDGRSTF) return 3;  /* Indep WDG */
    if (csr & RCC_CSR_SFTRSTF)  return 2;  /* Software */
    if (csr & RCC_CSR_PORRSTF)  return 0;  /* Power-on */
    if (csr & RCC_CSR_PINRSTF)  return 1;  /* Pin reset */
    if (csr & RCC_CSR_BORRSTF)  return 6;  /* Brown-out */

    return 7;  /* Unknown */
}

/* ──────────────────────────────────────────────
 * Default fault handlers (weak — can be overridden by application)
 *
 * Each handler enters an infinite loop. The application can
 * provide its own implementation (e.g., fault_handler_dump).
 * ────────────────────────────────────────────── */

void __attribute__((weak)) NMI_Handler(void)
{
    for (;;);
}

void __attribute__((weak)) HardFault_Handler(void)
{
    /* Capture fault info and halt */
    fault_info_t info;
    uint32_t msp = 0;
#if defined(__arm__) || defined(__thumb__)
    __asm__ volatile ("mrs %0, msp" : "=r" (msp));
#endif
    scb_get_fault_info(&info);
    fault_handler_dump(0, &info, msp);
    for (;;);
}

void __attribute__((weak)) MemManage_Handler(void)
{
    for (;;);
}

void __attribute__((weak)) BusFault_Handler(void)
{
    for (;;);
}

void __attribute__((weak)) UsageFault_Handler(void)
{
    for (;;);
}

void __attribute__((weak)) SVC_Handler(void)
{
    for (;;);
}

void __attribute__((weak)) DebugMon_Handler(void)
{
    for (;;);
}

void __attribute__((weak)) PendSV_Handler(void)
{
    for (;;);
}

void __attribute__((weak)) Default_Handler(void)
{
    for (;;);
}
