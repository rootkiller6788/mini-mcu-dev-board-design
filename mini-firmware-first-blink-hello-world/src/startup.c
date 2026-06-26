/*
 * startup.c — MCU Startup Sequence Implementation
 *
 * Knowledge points implemented (independent):
 *   1. system_init — full startup: FPU + flash latency + clock + SysTick
 *   2. system_clock_config — PLL configuration and clock switching
 *   3. system_get_info — system state query
 *   4. copy_data_section — .data initialization from flash to RAM
 *   5. zero_bss_section — .bss zeroing (C standard requirement)
 *   6. enable_fpu — CPACR configuration for FPU access
 *   7. measure_boot_time — DWT cycle counter boot timing
 *   8. get_reset_count — backup SRAM persistent reset counter
 */

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "startup.h"
#include "cortex_m.h"
#include "arch_compat.h"

/* ──────────────────────────────────────────────
 * RCC Register Map (Reset and Clock Control)
 *
 * Base: 0x40023800 (AHB1 bus)
 * The RCC manages all clock sources (HSI, HSE, PLL), prescalers,
 * and peripheral clock enables.
 * ────────────────────────────────────────────── */

typedef struct {
    volatile uint32_t CR;         /* 0x00: Clock Control Register         */
    volatile uint32_t PLLCFGR;    /* 0x04: PLL Configuration Register     */
    volatile uint32_t CFGR;       /* 0x08: Clock Configuration Register   */
    volatile uint32_t CIR;        /* 0x0C: Clock Interrupt Register       */
    volatile uint32_t AHB1RSTR;   /* 0x10: AHB1 Peripheral Reset          */
    volatile uint32_t AHB2RSTR;   /* 0x14: AHB2 Peripheral Reset          */
    volatile uint32_t AHB3RSTR;   /* 0x18: AHB3 Peripheral Reset          */
    uint32_t _reserved0;
    volatile uint32_t APB1RSTR;   /* 0x20: APB1 Peripheral Reset          */
    volatile uint32_t APB2RSTR;   /* 0x24: APB2 Peripheral Reset          */
    uint32_t _reserved1[2];
    volatile uint32_t AHB1ENR;    /* 0x30: AHB1 Peripheral Clock Enable   */
    volatile uint32_t AHB2ENR;    /* 0x34: AHB2 Peripheral Clock Enable   */
    volatile uint32_t AHB3ENR;    /* 0x38: AHB3 Peripheral Clock Enable   */
    uint32_t _reserved2;
    volatile uint32_t APB1ENR;    /* 0x40: APB1 Peripheral Clock Enable   */
    volatile uint32_t APB2ENR;    /* 0x44: APB2 Peripheral Clock Enable   */
} rcc_regs_t;

#define RCC_BASE  0x40023800U
#define RCC       ((rcc_regs_t *)RCC_BASE)

/* RCC CR bits */
#define RCC_CR_HSION     (1U << 0)
#define RCC_CR_HSIRDY    (1U << 1)
#define RCC_CR_HSEON     (1U << 16)
#define RCC_CR_HSERDY    (1U << 17)
#define RCC_CR_HSEBYP    (1U << 18)
#define RCC_CR_PLLON     (1U << 24)
#define RCC_CR_PLLRDY    (1U << 25)

/* RCC CFGR bits */
#define RCC_CFGR_SW_POS  0U
#define RCC_CFGR_SW_MASK  (0x3U << 0U)   /* System clock switch status */
#define RCC_CFGR_SWS_POS  2U
#define RCC_CFGR_SWS_MASK (0x3U << 2U)   /* System clock switch status */
#define RCC_CFGR_SW_HSI   0x0U
#define RCC_CFGR_SW_HSE   0x1U
#define RCC_CFGR_SW_PLL   0x2U

/* AHB prescaler: CFGR[7:4] = HPRE[3:0] */
#define RCC_CFGR_HPRE_POS   4U
#define RCC_CFGR_HPRE_MASK  (0xFU << 4U)

/* APB1 prescaler: CFGR[12:10] = PPRE1[2:0] */
#define RCC_CFGR_PPRE1_POS   10U
#define RCC_CFGR_PPRE1_MASK  (0x7U << 10U)

/* APB2 prescaler: CFGR[15:13] = PPRE2[2:0] */
#define RCC_CFGR_PPRE2_POS   13U
#define RCC_CFGR_PPRE2_MASK  (0x7U << 13U)

/* ──────────────────────────────────────────────
 * FLASH Access Control Register
 *
 * Base: 0x40023C00
 * ACR: Latency + prefetch + cache control
 * ────────────────────────────────────────────── */

#define FLASH_ACR_BASE  0x40023C00U
#define FLASH_ACR        ((volatile uint32_t *)FLASH_ACR_BASE)

/* FLASH ACR bits */
#define FLASH_ACR_LATENCY_POS  0U
#define FLASH_ACR_LATENCY_MASK (0x7U << 0U)
#define FLASH_ACR_PRFTEN  (1U << 8)   /* Prefetch enable */
#define FLASH_ACR_ICEN    (1U << 9)   /* Instruction cache enable */
#define FLASH_ACR_DCEN    (1U << 10)  /* Data cache enable */

/* ──────────────────────────────────────────────
 * Global system information
 * ────────────────────────────────────────────── */

static system_info_t g_system_info = {
    .state = SYSTEM_STATE_RESET,
    .active_clock = CLOCK_SOURCE_HSI,
    .sysclk_hz = 16000000U,  /* Default HSI = 16 MHz */
    .hclk_hz = 16000000U,
    .apb1_hz = 16000000U,
    .apb2_hz = 16000000U,
    .uptime_ticks = 0,
    .reset_count = 0,
    .clock_ok = true
};

/* Backup SRAM base (4 KB, survives most resets but not power-on) */
#define BKPSRAM_BASE  0x40024000U
#define BKPSRAM_RESET_COUNT_OFFSET  0U

/* ──────────────────────────────────────────────
 * enable_fpu — enable the Floating Point Unit
 *
 * Knowledge: CPACR (Coprocessor Access Control Register).
 *
 * The Cortex-M4F includes a single-precision FPU (FPv4-SP).
 * The FPU is disabled after reset to save power. To use it:
 *
 * CPACR[23:20] = CP11 (reserved for FPU)
 * CPACR[21:20] = CP10 (full FPU access)
 *
 * Setting these bits to 0b11 grants full access (privileged + unprivileged).
 * Without this, any FPU instruction (VADD, VMUL, VLDR, etc.) triggers
 * a UsageFault with NOCP (No Coprocessor) flag.
 *
 * Reference: ARMv7-M Architecture Manual §B3.2.20 — CPACR
 * ────────────────────────────────────────────── */

void enable_fpu(void)
{
    /* CPACR is at address 0xE000ED88 */
    volatile uint32_t *cpacr = (volatile uint32_t *)0xE000ED88U;

    /* Set CP10[21:20] and CP11[23:22] to 0b11 = full access */
    *cpacr |= ((0x3U << 20U) | (0x3U << 22U));

    /* DSB + ISB: ensure the CPACR update takes effect before
     * any FPU instructions are executed */
    DSB();
    ISB();
}

/* ──────────────────────────────────────────────
 * ahb_prescaler_to_div — convert HPRE register value to divider
 *
 * HPRE[3:0] encoding (STM32F4 §6.3.3):
 *   0xxx → AHB prescaler = 1 (no division)
 *   1000 → /2
 *   1001 → /4
 *   1010 → /8
 *   1011 → /16
 *   1100 → /64
 *   1101 → /128
 *   1110 → /256
 *   1111 → /512
 * ────────────────────────────────────────────── */

static uint32_t ahb_prescaler_to_div(uint32_t hpre)
{
    if (hpre < 0x8U) {
        return 1U;
    }
    /* Map: 8→2, 9→4, A→8, B→16, C→64, D→128, E→256, F→512 */
    if (hpre == 0x8U) return 2U;
    if (hpre == 0x9U) return 4U;
    if (hpre == 0xAU) return 8U;
    if (hpre == 0xBU) return 16U;
    if (hpre == 0xCU) return 64U;
    if (hpre == 0xDU) return 128U;
    if (hpre == 0xEU) return 256U;
    return 512U;
}

/* APB prescaler: PPRE[2:0]
 *   0xx → APB prescaler = 1
 *   100 → /2
 *   101 → /4
 *   110 → /8
 *   111 → /16
 *
 * Special rule: if APB prescaler > 1, the timer clock is 2 × APB clock.
 * This is important: TIMx_CLK = 2 × APBx_CLK when prescaler > 1. */
static uint32_t apb_prescaler_to_div(uint32_t ppre)
{
    if (ppre < 0x4U) {
        return 1U;
    }
    if (ppre == 0x4U) return 2U;
    if (ppre == 0x5U) return 4U;
    if (ppre == 0x6U) return 8U;
    return 16U;
}

/* ──────────────────────────────────────────────
 * system_clock_config — configure system clock tree
 *
 * Knowledge: PLL configuration for STM32F4.
 *
 * The clock tree on STM32F4:
 *
 *   HSI (16 MHz) ──┐
 *                   ├── PLL ── SYSCLK ──┬── AHB prescaler ── HCLK (CPU + peripherals)
 *   HSE (8 MHz)  ──┘                    ├── APB1 prescaler ─ PCLK1 (max 42 MHz)
 *                                       └── APB2 prescaler ─ PCLK2 (max 84 MHz)
 *
 * PLL formula:
 *   f_VCO_in = f_PLL_in / PLLM    (PLLM: 2–63)
 *   f_VCO    = f_VCO_in × PLLN    (PLLN: 50–432, VCO: 100–432 MHz)
 *   f_PLL    = f_VCO / PLLP       (PLLP: 2, 4, 6, 8)
 *   f_USB    = f_VCO / PLLQ       (PLLQ: 2–15, must be 48 MHz for USB)
 *
 * Example (168 MHz from 8 MHz HSE):
 *   PLLM = 8, PLLN = 336, PLLP = 2, PLLQ = 7
 *   f_VCO_in = 8/8 = 1 MHz
 *   f_VCO    = 1 × 336 = 336 MHz
 *   f_PLL    = 336/2 = 168 MHz
 *   f_USB    = 336/7 = 48 MHz ✓
 *
 * Procedure:
 *   1. Enable HSE (if using) and wait for HSERDY.
 *   2. Configure PLL (PLLM, PLLN, PLLP, PLLQ, PLLSRC).
 *   3. Enable PLL and wait for PLLRDY.
 *   4. Configure AHB/APB prescalers.
 *   5. Configure flash wait states based on target frequency.
 *   6. Switch system clock to PLL.
 *   7. Wait for SWS (System Clock Switch Status) to confirm.
 *
 * Reference: STM32F4 §6.3 — RCC registers
 * ────────────────────────────────────────────── */

bool system_clock_config(const clock_config_t *config)
{
    uint32_t tmp;
    uint32_t pllm;
    uint32_t plln;
    uint32_t pllp;
    uint32_t pllq;

    if (config == NULL) {
        return false;
    }

    /* 1. Enable HSE if needed */
    if (config->source == CLOCK_SOURCE_HSE || config->source == CLOCK_SOURCE_PLL) {
        RCC->CR |= RCC_CR_HSEON;
        /* Wait for HSE to become ready (timeout ~2 ms for 8 MHz crystal) */
        uint32_t timeout = 2000000U;
        while (((RCC->CR & RCC_CR_HSERDY) == 0U) && (timeout > 0U)) {
            timeout--;
        }
        if (timeout == 0U) {
            g_system_info.clock_ok = false;
            return false;
        }
    }

    /* 2. If using PLL, configure and enable it */
    if (config->source == CLOCK_SOURCE_PLL) {
        /* Disable PLL before reconfiguring */
        RCC->CR &= ~RCC_CR_PLLON;
        while (RCC->CR & RCC_CR_PLLRDY) {
            /* Wait for PLL to stop */
        }

        /* Configure PLLM, PLLN, PLLP, PLLQ */
        pllm = config->pll_m;
        plln = config->pll_n;
        pllp = config->pll_p;
        pllq = config->pll_q;

        /* PLLP encoding:
         *   00 → PLLP = 2
         *   01 → PLLP = 4
         *   10 → PLLP = 6
         *   11 → PLLP = 8 */
        {
            uint32_t pllp_reg;
            if (pllp == 2U) pllp_reg = 0x0U;
            else if (pllp == 4U) pllp_reg = 0x1U;
            else if (pllp == 6U) pllp_reg = 0x2U;
            else pllp_reg = 0x3U;

            /* PLLCFGR: PLLM[5:0], PLLN[8:6]∪[14:6], PLLP[17:16], PLLQ[27:24],
             *          PLLSRC[22] (0=HSI, 1=HSE) */
            tmp = 0;
            tmp |= (pllm & 0x3FU);                      /* PLLM[5:0] */
            tmp |= ((plln & 0x1FFU) << 6U);             /* PLLN[14:6] */
            tmp |= (pllp_reg << 16U);                     /* PLLP[17:16] */
            tmp |= ((pllq & 0xFU) << 24U);                /* PLLQ[27:24] */
            tmp |= (1U << 22U);                            /* PLLSRC = HSE */

            RCC->PLLCFGR = tmp;
        }

        /* Enable PLL */
        RCC->CR |= RCC_CR_PLLON;
        /* Wait for PLL to lock (~1 ms) */
        {
            uint32_t timeout = 2000000U;
            while (((RCC->CR & RCC_CR_PLLRDY) == 0U) && (timeout > 0U)) {
                timeout--;
            }
            if (timeout == 0U) {
                g_system_info.clock_ok = false;
                return false;
            }
        }
    }

    /* 3. Configure flash wait states BEFORE increasing clock frequency.
     * Flash access must be slow enough for the new frequency. */
    {
        uint32_t latency;
        if (config->target_sysclk <= 30000000U)   latency = 0U;
        else if (config->target_sysclk <= 60000000U)  latency = 1U;
        else if (config->target_sysclk <= 90000000U)  latency = 2U;
        else if (config->target_sysclk <= 120000000U) latency = 3U;
        else if (config->target_sysclk <= 150000000U) latency = 4U;
        else                                      latency = 5U;

        tmp = *FLASH_ACR;
        tmp = (tmp & ~FLASH_ACR_LATENCY_MASK) | (latency << FLASH_ACR_LATENCY_POS);
        tmp |= FLASH_ACR_PRFTEN | FLASH_ACR_ICEN | FLASH_ACR_DCEN;
        *FLASH_ACR = tmp;

        /* Verify the new latency was applied */
        if (((*FLASH_ACR) & FLASH_ACR_LATENCY_MASK) != (latency << FLASH_ACR_LATENCY_POS)) {
            g_system_info.clock_ok = false;
            return false;
        }
    }

    /* 4. Configure AHB and APB prescalers */
    {
        tmp = RCC->CFGR;
        /* Clear prescaler fields */
        tmp &= ~(RCC_CFGR_HPRE_MASK | RCC_CFGR_PPRE1_MASK | RCC_CFGR_PPRE2_MASK);
        /* Set HPRE (AHB prescaler) */
        tmp |= ((config->ahb_prescaler & 0xFU) << RCC_CFGR_HPRE_POS);
        /* Set PPRE1 (APB1 prescaler) */
        tmp |= ((config->apb1_prescaler & 0x7U) << RCC_CFGR_PPRE1_POS);
        /* Set PPRE2 (APB2 prescaler) */
        tmp |= ((config->apb2_prescaler & 0x7U) << RCC_CFGR_PPRE2_POS);
        RCC->CFGR = tmp;
    }

    /* 5. Switch system clock */
    {
        tmp = RCC->CFGR;
        tmp &= ~RCC_CFGR_SW_MASK;

        if (config->source == CLOCK_SOURCE_HSI) {
            tmp |= (RCC_CFGR_SW_HSI << RCC_CFGR_SW_POS);
        } else if (config->source == CLOCK_SOURCE_HSE) {
            tmp |= (RCC_CFGR_SW_HSE << RCC_CFGR_SW_POS);
        } else {
            tmp |= (RCC_CFGR_SW_PLL << RCC_CFGR_SW_POS);
        }

        RCC->CFGR = tmp;

        /* Wait for switch to complete */
        {
            uint32_t expected_sws = 0;
            if (config->source == CLOCK_SOURCE_HSI) expected_sws = (RCC_CFGR_SW_HSI << RCC_CFGR_SWS_POS);
            else if (config->source == CLOCK_SOURCE_HSE) expected_sws = (RCC_CFGR_SW_HSE << RCC_CFGR_SWS_POS);
            else expected_sws = (RCC_CFGR_SW_PLL << RCC_CFGR_SWS_POS);

            uint32_t timeout = 2000000U;
            while (((RCC->CFGR & RCC_CFGR_SWS_MASK) != expected_sws) && (timeout > 0U)) {
                timeout--;
            }
            if (timeout == 0U) {
                g_system_info.clock_ok = false;
                return false;
            }
        }
    }

    /* 6. Update system info with actual frequencies */
    {
        uint32_t sysclk = config->target_sysclk;
        uint32_t hpre = (RCC->CFGR & RCC_CFGR_HPRE_MASK) >> RCC_CFGR_HPRE_POS;
        uint32_t ppre1 = (RCC->CFGR & RCC_CFGR_PPRE1_MASK) >> RCC_CFGR_PPRE1_POS;
        uint32_t ppre2 = (RCC->CFGR & RCC_CFGR_PPRE2_MASK) >> RCC_CFGR_PPRE2_POS;
        uint32_t ahb_div = ahb_prescaler_to_div(hpre);
        uint32_t apb1_div = apb_prescaler_to_div(ppre1);
        uint32_t apb2_div = apb_prescaler_to_div(ppre2);

        g_system_info.sysclk_hz = sysclk;
        g_system_info.hclk_hz = sysclk / ahb_div;
        g_system_info.apb1_hz = g_system_info.hclk_hz / apb1_div;
        g_system_info.apb2_hz = g_system_info.hclk_hz / apb2_div;
        g_system_info.active_clock = config->source;
        g_system_info.clock_ok = true;
    }

    return true;
}

/* ──────────────────────────────────────────────
 * system_init — full system initialization
 *
 * Called from Reset_Handler before main().
 * Order matters: FPU → flash latency → clock → SysTick.
 * ────────────────────────────────────────────── */

void system_init(const clock_config_t *config)
{
    g_system_info.state = SYSTEM_STATE_CLOCK_INIT;

    /* 1. Enable FPU if present */
    enable_fpu();

    /* 2. Configure clocks */
    if (!system_clock_config(config)) {
        /* Clock config failed — stay on HSI (safe fallback) */
        g_system_info.clock_ok = false;
    }

    /* 3. Configure SysTick for 1 ms tick */
    systick_init(g_system_info.hclk_hz, 1000U, true);

    /* 4. Enable fault handlers for better debugging */
    enable_fault_handlers();

    /* 5. Increment reset counter in backup SRAM */
    {
        volatile uint32_t *bkpsram = (volatile uint32_t *)BKPSRAM_BASE;
        uint32_t count = bkpsram[BKPSRAM_RESET_COUNT_OFFSET];
        count++;
        bkpsram[BKPSRAM_RESET_COUNT_OFFSET] = count;
        g_system_info.reset_count = count;
    }

    g_system_info.state = SYSTEM_STATE_RUNNING;
}

/* ──────────────────────────────────────────────
 * system_get_info — retrieve system state
 * ────────────────────────────────────────────── */

void system_get_info(system_info_t *info)
{
    if (info == NULL) {
        return;
    }
    /* Update uptime */
    g_system_info.uptime_ticks = systick_get_count();
    memcpy(info, &g_system_info, sizeof(system_info_t));
}

/* ──────────────────────────────────────────────
 * copy_data_section — copy .data from load to run address
 *
 * Knowledge: Scatter-loading initialization.
 *
 * The C runtime requires initialized global variables (those with
 * non-zero initializers like `int x = 42;`) to have their initial
 * values in RAM at program start. The initial values are stored
 * in flash (in the .data section's load region) and must be copied
 * to the .data section's run region in RAM.
 *
 * This is handled by the linker's scatter-loading mechanism:
 *   - The linker defines symbols like _sidata, _sdata, _edata.
 *   - The copy loop copies from _sidata (flash) to _sdata.._edata (RAM).
 *
 * Generic implementation: iterate over a table of
 * {load_addr, run_addr, size} entries.
 *
 * Reference: ARM IHI0044 — ELF for the ARM Architecture
 * ────────────────────────────────────────────── */

void copy_data_section(const section_copy_entry_t *copy_table, size_t num_entries)
{
    size_t i, j;
    const section_copy_entry_t *entry;

    if (copy_table == NULL) {
        return;
    }

    for (i = 0; i < num_entries; i++) {
        entry = &copy_table[i];

        /* Copy word-aligned data from load to run address.
         * Source is in flash (read-only), dest is in RAM (read-write). */
        for (j = 0; j < (entry->size / 4U); j++) {
            entry->run_address[j] = entry->load_address[j];
        }

        /* Copy remaining bytes (if size not word-aligned) */
        {
            uint8_t *src_bytes = (uint8_t *)(&entry->load_address[entry->size / 4U]);
            uint8_t *dst_bytes = (uint8_t *)(&entry->run_address[entry->size / 4U]);
            uint32_t remaining = entry->size % 4U;
            for (j = 0; j < (size_t)remaining; j++) {
                dst_bytes[j] = src_bytes[j];
            }
        }
    }
}

/* ──────────────────────────────────────────────
 * zero_bss_section — fill .bss with zero
 *
 * Knowledge: BSS zeroing (§6.7.8 of C standard).
 *
 * Per the C standard, static storage duration objects without
 * explicit initialization are initialized to zero (for arithmetic
 * types) or null pointer (for pointer types). This must be done
 * before main() is called.
 *
 * The .bss section contains uninitialized global and static variables.
 * At startup, this entire region in RAM is set to zero.
 *
 * Optimization: zero in 4-byte words for speed, then bytes for remainder.
 *
 * Reference: ISO C99 §6.7.8 §10
 * ────────────────────────────────────────────── */

void zero_bss_section(uint8_t *start, size_t size)
{
    uint32_t *ptr32;
    size_t word_count;
    size_t remaining;
    size_t i;

    if (start == NULL || size == 0) {
        return;
    }

    /* Zero word-aligned portion */
    ptr32 = (uint32_t *)(uintptr_t)start;
    word_count = size / 4U;
    for (i = 0; i < word_count; i++) {
        ptr32[i] = 0U;
    }

    /* Zero remaining bytes */
    remaining = size % 4U;
    for (i = 0; i < remaining; i++) {
        start[word_count * 4U + i] = 0U;
    }
}

/* ──────────────────────────────────────────────
 * measure_boot_time — measure boot time in cycles
 *
 * Knowledge: DWT cycle counter for performance measurement.
 *
 * The DWT (Data Watchpoint and Trace) unit includes a 32-bit cycle
 * counter (DWT_CYCCNT) that increments every CPU clock cycle.
 * This can be used to precisely measure execution time.
 *
 * Enable sequence:
 *   1. Unlock DWT access via CoreDebug->DEMCR[24] (TRCENA).
 *   2. Reset and enable DWT->CYCCNT with DWT->CTRL[0] (CYCCNTENA).
 *
 * Boot time measurement:
 *   - Record CYCCNT at the very start of Reset_Handler.
 *   - Record CYCCNT just before calling main().
 *   - Difference = boot time in CPU cycles.
 *
 * For 168 MHz: 1 cycle = 5.95 ns.
 * Typical boot time: ~10,000–50,000 cycles (60–300 µs).
 * ────────────────────────────────────────────── */

uint32_t measure_boot_time(void)
{
    /* DWT registers */
    volatile uint32_t *demcr    = (volatile uint32_t *)0xE000EDFCU;
    volatile uint32_t *dwt_ctrl = (volatile uint32_t *)0xE0001000U;
    volatile uint32_t *dwt_cyccnt = (volatile uint32_t *)0xE0001004U;

    /* Enable DWT cycle counter:
     *   DEMCR[24] = TRCENA (Trace enable)
     *   DWT_CTRL[0] = CYCCNTENA */
    *demcr |= (1U << 24U);
    *dwt_ctrl |= 1U;

    /* Read current cycle count */
    return *dwt_cyccnt;
}

/* ──────────────────────────────────────────────
 * system_reset — software system reset
 * ────────────────────────────────────────────── */

void system_reset(void)
{
    scb_system_reset();
    /* Not reached */
    for (;;);
}

/* ──────────────────────────────────────────────
 * get_reset_count — read persistent reset counter
 * ────────────────────────────────────────────── */

uint32_t get_reset_count(void)
{
    volatile uint32_t *bkpsram = (volatile uint32_t *)BKPSRAM_BASE;
    return bkpsram[BKPSRAM_RESET_COUNT_OFFSET];
}
