/*
 * watchdog.h — Independent and Window Watchdog Timers
 *
 * Knowledge Mapping (SKILL.md L1-L6):
 *   L1 Definitions: Watchdog timer, timeout period, kick/refresh,
 *                   window watchdog (must refresh in a time window,
 *                   not too early, not too late)
 *   L2 Concepts:    Fail-safe design, fault recovery, liveness monitoring,
 *                   hardware vs software watchdog, early-warning interrupt
 *   L3 Math:        IWDG timeout: T = (4 / f_LSI) × (PR[2:0] + 2)⁽PR_div⁾ × (RLR + 1)
 *                   WWDG timeout: T = (1 / f_PCLK) × 4096 × 2^WDGTB × (T[6:0] + 1)
 *   L4 Laws:        Murphy's Law applied to embedded systems — if software
 *                   can hang, it eventually will. Watchdog is the backstop.
 *   L5 Algorithms:  Multi-stage watchdog (early warning → soft recovery → hard reset),
 *                   task monitoring watchdog (each task must report in),
 *                   window watchdog for control loop timing integrity
 *   L6 Problems:    Recovering from a hung I²C bus,
 *                   detecting a deadlocked RTOS task,
 *                   guaranteed watchdog refresh in main loop
 *
 * Course Mapping:
 *   Valvano Ch.2 — Watchdog timer design patterns
 *   MIT 6.033 Computer System Engineering — fault tolerance
 */

#ifndef WATCHDOG_H
#define WATCHDOG_H

#include <stdint.h>
#include <stdbool.h>

/* ──────────────────────────────────────────────
 * L1 Definition: Watchdog types
 *
 * IWDG (Independent Watchdog): Clocked by internal 32 kHz LSI RC
 *   oscillator. Runs even if the main clock fails. Cannot be
 *   disabled once enabled (until reset). Truly independent.
 *
 * WWDG (Window Watchdog): Clocked by APB1 clock. Must be refreshed
 *   within a defined time window — refreshing too early (when the
 *   counter is still above the window value) also triggers a reset.
 *   This detects both slow and fast code execution.
 *
 * L2 Concept: A window watchdog ensures the main loop executes
 *   at a predictable rate. If the loop runs too fast (skipping
 *   work) or too slow (stuck), the watchdog fires.
 * ────────────────────────────────────────────── */

typedef enum {
    WATCHDOG_INDEPENDENT = 0,
    WATCHDOG_WINDOW      = 1
} watchdog_type_t;

/* IWDG Prescaler: divides the LSI clock (32 kHz typical) */
typedef enum {
    IWDG_PRESCALER_4     = 0x0,  /* f = LSI / 4   = 8 kHz  */
    IWDG_PRESCALER_8     = 0x1,  /* f = LSI / 8   = 4 kHz  */
    IWDG_PRESCALER_16    = 0x2,  /* f = LSI / 16  = 2 kHz  */
    IWDG_PRESCALER_32    = 0x3,  /* f = LSI / 32  = 1 kHz  */
    IWDG_PRESCALER_64    = 0x4,  /* f = LSI / 64  = 500 Hz */
    IWDG_PRESCALER_128   = 0x5,  /* f = LSI / 128 = 250 Hz */
    IWDG_PRESCALER_256   = 0x6   /* f = LSI / 256 = 125 Hz */
} iwdg_prescaler_t;

/* WWDG Prescaler: divides APB1 clock (typically 42 MHz on STM32F4) */
typedef enum {
    WWDG_PRESCALER_1    = 0x0,  /* 1/4096 APB1  */
    WWDG_PRESCALER_2    = 0x1,  /* 2/4096 APB1  */
    WWDG_PRESCALER_4    = 0x2,  /* 4/4096 APB1  */
    WWDG_PRESCALER_8    = 0x3   /* 8/4096 APB1  */
} wwdg_prescaler_t;

/* ──────────────────────────────────────────────
 * IWDG Register Map
 *
 * Base: 0x40003000 (APB1 bus)
 * Protection: IWDG_KR (Key Register) must be written with
 *   0x5555 to enable register writes, 0xAAAA to refresh,
 *   0xCCCC to start the watchdog.
 * ────────────────────────────────────────────── */
typedef struct {
    volatile uint32_t KR;    /* 0x00: Key Register               */
    volatile uint32_t PR;    /* 0x04: Prescaler Register         */
    volatile uint32_t RLR;   /* 0x08: Reload Register            */
    volatile uint32_t SR;    /* 0x0C: Status Register            */
} iwdg_regs_t;

/* IWDG Key values */
#define IWDG_KEY_ENABLE     0x0000CCCCU  /* Start watchdog (enable) */
#define IWDG_KEY_REFRESH    0x0000AAAAU  /* Kick/refresh watchdog   */
#define IWDG_KEY_UNLOCK     0x00005555U  /* Unlock PR/RLR write     */

/* ──────────────────────────────────────────────
 * WWDG Register Map
 *
 * Base: 0x40002C00 (APB1 bus)
 * Counter: 7-bit down-counter (T[6:0]) clocked by APB1/4096/prescaler.
 *   When CNT reaches 0x3F (binary 0111111), a reset is generated.
 *   When CNT reaches 0x40, a configurable early-warning interrupt fires.
 * ────────────────────────────────────────────── */
typedef struct {
    volatile uint32_t CR;    /* 0x00: Control Register (T[6:0] + WDGA) */
    volatile uint32_t CFR;   /* 0x04: Configuration Register           */
    volatile uint32_t SR;    /* 0x08: Status Register (EWIF)           */
} wwdg_regs_t;

/* WWDG bits */
#define WWDG_CR_WDGA  (1U << 7)    /* Activation bit */
#define WWDG_CR_T_MASK 0x7FU        /* Counter value bits [6:0] */

/* ──────────────────────────────────────────────
 * Watchdog Configuration
 * ────────────────────────────────────────────── */
typedef struct {
    watchdog_type_t  type;
    uint32_t         timeout_ms;     /* Desired timeout in milliseconds */

    /* IWDG-specific */
    iwdg_prescaler_t iwdg_prescaler;
    uint32_t         iwdg_reload;    /* 12-bit reload value (0–0xFFF)  */

    /* WWDG-specific */
    wwdg_prescaler_t wwdg_prescaler;
    uint8_t          wwdg_window;    /* 7-bit window value (must be ≤ CNT) */
    uint8_t          wwdg_counter;   /* 7-bit counter start value          */
    bool             wwdg_ewi_enable; /* Enable early-warning interrupt     */
} watchdog_config_t;

/* ──────────────────────────────────────────────
 * L5 Algorithm: Multi-stage watchdog recovery
 *
 * Stage 1: Early-warning interrupt (WWDG only) — attempt graceful
 *          recovery (flush logs, close files, save state).
 * Stage 2: Soft reset — software-triggered reset after recovery attempt.
 * Stage 3: Hard reset — watchdog timeout if stage 1/2 failed.
 *
 * State machine for recovery tracking.
 * ────────────────────────────────────────────── */
typedef enum {
    WDOG_STAGE_NORMAL     = 0,  /* Running normally, periodic refresh */
    WDOG_STAGE_WARNING    = 1,  /* Early warning triggered, attempting recovery */
    WDOG_STAGE_RECOVERY   = 2,  /* Running recovery routines */
} watchdog_stage_t;

/* ──────────────────────────────────────────────
 * Watchdog API
 * ────────────────────────────────────────────── */

/*
 * iwdg_init — configure and start the independent watchdog
 *
 * Once started, the IWDG cannot be stopped except by a system reset.
 * The timeout is calculated from LSI frequency (nominally 32 kHz,
 * but varies ±50% over temperature → design for worst case).
 *
 * Timeout formula: T = (1 / f_LSI) × 4 × 2^(PR) × (RLR + 1)
 *
 * @param config: watchdog configuration
 *
 * Complexity: O(1)
 * Reference: STM32F4 §23 — Independent Watchdog (IWDG)
 */
void iwdg_init(const watchdog_config_t *config);

/*
 * iwdg_refresh — kick/refresh the independent watchdog
 *
 * Writes 0xAAAA to KR, resetting the down-counter to RLR.
 * Must be called before the timeout expires.
 *
 * Call placement:
 *   - At the end of main loop (slowest possible path).
 *   - Do NOT call in a timer ISR — this defeats the purpose
 *     (ISR can fire even when main loop is hung).
 *
 * Complexity: O(1)
 */
void iwdg_refresh(void);

/*
 * iwdg_calculate_timeout — compute IWDG timeout in ms for given settings
 *
 * @param lsi_freq_hz: LSI clock frequency (nominally 32000)
 * @param prescaler:   IWDG prescaler
 * @param reload:      reload value (0–0xFFF)
 * @return timeout in milliseconds
 *
 * Complexity: O(1)
 */
uint32_t iwdg_calculate_timeout(uint32_t lsi_freq_hz, iwdg_prescaler_t prescaler,
                                uint32_t reload);

/*
 * wwdg_init — configure and start the window watchdog
 *
 * Unlike IWDG, WWDG can be configured and started in one step.
 * The early-warning interrupt (EWI) fires when the counter
 * reaches 0x40, giving the application one more chance
 * to recover before the reset at 0x3F.
 *
 * Timeout formula: T = (1 / f_PCLK) × 4096 × 2^WDGTB × (T[6:0] + 1)
 *
 * @param config: watchdog configuration (WWDG-specific fields)
 *
 * Complexity: O(1)
 * Reference: STM32F4 §24 — Window Watchdog (WWDG)
 */
void wwdg_init(const watchdog_config_t *config);

/*
 * wwdg_refresh — reload the window watchdog counter
 *
 * Writes the counter reload value to CR. This must happen
 * when CNT is below the window value but above 0x3F.
 *
 * Complexity: O(1)
 */
void wwdg_refresh(const watchdog_config_t *config);

/*
 * wwdg_calculate_timeout — compute WWDG timeout in ms
 *
 * @param pclk_hz:   APB1 clock frequency
 * @param prescaler: WWDG prescaler
 * @param counter:   7-bit counter start value
 * @return timeout in milliseconds
 *
 * Complexity: O(1)
 */
uint32_t wwdg_calculate_timeout(uint32_t pclk_hz, wwdg_prescaler_t prescaler,
                                uint8_t counter);

/*
 * watchdog_self_test — verify the watchdog is functional
 *
 * Periodic self-test: briefly let the watchdog approach its
 * timeout, then refresh. If the MCU doesn't reset within the
 * test window, the watchdog is working.
 *
 * Do NOT let the watchdog actually fire — just approach the
 * threshold and then refresh before the reset point.
 *
 * @param config: current watchdog configuration
 * @return true if watchdog appears functional
 *
 * Complexity: O(timeout / 2)
 */
bool watchdog_self_test(const watchdog_config_t *config);

#endif /* WATCHDOG_H */
