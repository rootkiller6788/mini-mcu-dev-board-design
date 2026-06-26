/*
 * gpio.c — GPIO Implementation for ARM Cortex-M MCUs
 *
 * Knowledge points implemented (independent):
 *   1. gpio_init — register field manipulation with bitwise masks
 *   2. gpio_write — atomic BSRR-based pin set/reset (no RMW race)
 *   3. gpio_toggle — ODR read-invert-write (non-atomic variant)
 *   4. gpio_read — IDR polling for digital input
 *   5. gpio_set_mode — runtime mode switching
 *   6. gpio_lock_config — LCKR lock sequence for safety-critical pins
 *   7. gpio_debounce_init/update — integrate-and-dump software debounce
 *   8. gpio_set_alternate — AFR register configuration
 *   9. gpio_write_port/read_port — full-port operations
 */

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "gpio.h"

/* ──────────────────────────────────────────────
 * Memory-Mapped GPIO Base Addresses (STM32F4)
 *
 * GPIO port registers are mapped to fixed addresses in the
 * peripheral memory region (0x40000000–0x5FFFFFFF).
 * Each port occupies 0x400 bytes (1 KB), starting at:
 *   GPIOA: 0x40020000
 *   GPIOB: 0x40020400
 *   GPIOC: 0x40020800
 *   GPIOD: 0x40020C00
 *   GPIOE: 0x40021000
 *   GPIOF: 0x40021400
 *   GPIOG: 0x40021800
 *   GPIOH: 0x40021C00
 *
 * Reference: STM32F4 Ref Manual §2.3 — Memory map
 * ────────────────────────────────────────────── */

static const uint32_t gpio_base_addresses[GPIO_NUM_PORTS] = {
    0x40020000U,  /* GPIO A */
    0x40020400U,  /* GPIO B */
    0x40020800U,  /* GPIO C */
    0x40020C00U,  /* GPIO D */
    0x40021000U,  /* GPIO E */
    0x40021400U,  /* GPIO F */
    0x40021800U,  /* GPIO G */
    0x40021C00U   /* GPIO H */
};

/* ──────────────────────────────────────────────
 * Utility: get register base pointer for a port
 *
 * Returns NULL if the port index is out of range, enabling
 * a single-point NULL check instead of scattered range checks.
 * ────────────────────────────────────────────── */

gpio_regs_t *gpio_get_port_base(gpio_port_t port)
{
    if (port >= GPIO_NUM_PORTS) {
        return NULL;
    }
    return (gpio_regs_t *)(uintptr_t)gpio_base_addresses[port];
}

/* ──────────────────────────────────────────────
 * gpio_init — configure a GPIO pin
 *
 * Knowledge: Register field manipulation.
 *
 * Each register (MODER, OTYPER, etc.) uses 2-bit or 1-bit fields
 * per pin. The field position is: pos = pin × bits_per_field.
 * The mask is: (bits_per_field == 2 ? 0x3 : 0x1) << pos.
 *
 * OTYPER is special: 1-bit per pin, so mask is (1U << pin).
 *
 * Implementation note: Using a read-modify-write sequence on
 * each register. This is safe only when no ISR modifies the
 * same register, which is the case during initialization.
 * ────────────────────────────────────────────── */

void gpio_init(const gpio_config_t *config)
{
    gpio_regs_t *regs;
    uint32_t pin_pos;
    uint32_t mode_val;
    uint32_t mask;
    uint32_t tmp;

    if (config == NULL) {
        return;
    }

    regs = gpio_get_port_base(config->port);
    if (regs == NULL) {
        return;
    }

    pin_pos = (uint32_t)config->pin;

    /* 1. MODER: 2 bits per pin.
     * Clear field → set mode value.
     */
    mask = 0x3U << (pin_pos * 2U);
    mode_val = ((uint32_t)config->mode & 0x3U) << (pin_pos * 2U);
    tmp = regs->MODER;
    tmp = (tmp & ~mask) | mode_val;
    regs->MODER = tmp;

    /* 2. OTYPER: 1 bit per pin. Open-drain or push-pull. */
    mask = 1U << pin_pos;
    if (config->otype == GPIO_OTYPE_OPEN_DRAIN) {
        regs->OTYPER |= mask;
    } else {
        regs->OTYPER &= ~mask;
    }

    /* 3. OSPEEDR: 2 bits per pin. Slew rate control. */
    mask = 0x3U << (pin_pos * 2U);
    mode_val = ((uint32_t)config->speed & 0x3U) << (pin_pos * 2U);
    tmp = regs->OSPEEDR;
    tmp = (tmp & ~mask) | mode_val;
    regs->OSPEEDR = tmp;

    /* 4. PUPDR: 2 bits per pin. Pull resistor configuration. */
    mask = 0x3U << (pin_pos * 2U);
    mode_val = ((uint32_t)config->pupd & 0x3U) << (pin_pos * 2U);
    tmp = regs->PUPDR;
    tmp = (tmp & ~mask) | mode_val;
    regs->PUPDR = tmp;

    /* 5. AFR: 4 bits per pin.
     * AFRL handles pins 0-7; AFRH handles pins 8-15.
     */
    if (pin_pos < 8U) {
        mask = 0xFU << (pin_pos * 4U);
        mode_val = ((uint32_t)config->alternate & 0xFU) << (pin_pos * 4U);
        tmp = regs->AFRL;
        tmp = (tmp & ~mask) | mode_val;
        regs->AFRL = tmp;
    } else {
        mask = 0xFU << ((pin_pos - 8U) * 4U);
        mode_val = ((uint32_t)config->alternate & 0xFU) << ((pin_pos - 8U) * 4U);
        tmp = regs->AFRH;
        tmp = (tmp & ~mask) | mode_val;
        regs->AFRH = tmp;
    }
}

/* ──────────────────────────────────────────────
 * gpio_write — atomic pin set or reset via BSRR
 *
 * Knowledge: BSRR atomic bit manipulation.
 *
 * BSRR is a 32-bit register:
 *   Bits [15:0]  = BS (Bit Set): write 1 to set the ODR bit
 *   Bits [31:16] = BR (Bit Reset): write 1 to reset the ODR bit
 * Writing 0 to any bit has no effect.
 *
 * Why BSRR instead of ODR?
 *   ODR read-modify-write is not atomic with respect to interrupts.
 *   If an ISR writes ODR between the CPU's read and write, the ISR's
 *   change is silently lost. BSRR avoids this because it's a
 *   write-only register that doesn't require reading ODR first.
 *
 * Reference: STM32F4 §8.4.6 — GPIO bit set/reset register
 * ────────────────────────────────────────────── */

void gpio_write(gpio_port_t port, gpio_pin_t pin, bool state)
{
    gpio_regs_t *regs = gpio_get_port_base(port);
    if (regs == NULL) {
        return;
    }

    if (state) {
        /* Set: write 1 to BS bits [15:0] */
        regs->BSRR = (1U << (uint32_t)pin);
    } else {
        /* Reset: write 1 to BR bits [31:16] */
        regs->BSRR = (1U << ((uint32_t)pin + 16U));
    }
}

/* ──────────────────────────────────────────────
 * gpio_toggle — flip an output pin state
 *
 * Knowledge: ODR read-invert-write (non-atomic).
 *
 * Reads the current ODR value, inverts the target bit, then writes
 * ODR back. This is NOT atomic: if an ISR modifies ODR (even a
 * different pin) between the read and write, the ISR's change
 * will be lost.
 *
 * To make this atomic for a single pin, use BSRR:
 *   if (regs->ODR & mask) { regs->BSRR = BR_mask; }
 *   else                   { regs->BSRR = BS_mask; }
 *
 * However, the read of ODR and the conditional write are still
 * not an atomic pair — an ISR could change the pin state between
 * the check and the write.
 *
 * For truly glitch-free toggling, use the timer's PWM toggle
 * output mode or a hardware timer one-pulse mode.
 * ────────────────────────────────────────────── */

void gpio_toggle(gpio_port_t port, gpio_pin_t pin)
{
    gpio_regs_t *regs = gpio_get_port_base(port);
    uint32_t pin_mask;
    uint32_t current;

    if (regs == NULL) {
        return;
    }

    pin_mask = 1U << (uint32_t)pin;
    current = regs->ODR;

    if (current & pin_mask) {
        /* Pin is high → reset it (BR bits are [31:16]) */
        regs->BSRR = pin_mask << 16U;
    } else {
        /* Pin is low → set it (BS bits are [15:0]) */
        regs->BSRR = pin_mask;
    }
}

/* ──────────────────────────────────────────────
 * gpio_read — digital input polling
 *
 * Knowledge: IDR captures the actual pin voltage after the
 * Schmitt trigger, independent of ODR.
 *
 * The Schmitt trigger provides hysteresis:
 *   V_IH (input high) ≈ 0.7 × V_dd (rising threshold)
 *   V_IL (input low)  ≈ 0.3 × V_dd (falling threshold)
 *   Hysteresis ≈ 0.4 × V_dd → noise immunity of ~1.3 V at 3.3 V
 *
 * This prevents noise-induced oscillations when the input voltage
 * is near the logic threshold.
 *
 * Reference: Sedra & Smith §15.2 — Schmitt trigger
 * ────────────────────────────────────────────── */

bool gpio_read(gpio_port_t port, gpio_pin_t pin)
{
    gpio_regs_t *regs = gpio_get_port_base(port);
    if (regs == NULL) {
        return false;
    }
    return (regs->IDR & (1U << (uint32_t)pin)) != 0U;
}

/* ──────────────────────────────────────────────
 * gpio_set_mode — change pin mode at runtime
 *
 * Useful for multi-function pins. Example: an I²C SDA line switches
 * between open-drain output (when driving) and input (when reading).
 * This is managed by the I²C hardware, but manual mode switching is
 * needed for bit-banged protocols.
 * ────────────────────────────────────────────── */

void gpio_set_mode(gpio_port_t port, gpio_pin_t pin, gpio_mode_t mode)
{
    gpio_regs_t *regs = gpio_get_port_base(port);
    uint32_t pin_pos;
    uint32_t mask;
    uint32_t tmp;

    if (regs == NULL) {
        return;
    }

    pin_pos = (uint32_t)pin;
    mask = 0x3U << (pin_pos * 2U);
    tmp = regs->MODER;
    tmp = (tmp & ~mask) | (((uint32_t)mode & 0x3U) << (pin_pos * 2U));
    regs->MODER = tmp;
}

/* ──────────────────────────────────────────────
 * gpio_write_port — full-port output write
 *
 * Writes all 16 bits to ODR. Non-atomic with respect to ISRs
 * that also write ODR (even different pins).
 * ────────────────────────────────────────────── */

void gpio_write_port(gpio_port_t port, uint16_t value)
{
    gpio_regs_t *regs = gpio_get_port_base(port);
    if (regs == NULL) {
        return;
    }
    regs->ODR = (uint32_t)value;
}

/* ──────────────────────────────────────────────
 * gpio_read_port — full-port input read
 * ────────────────────────────────────────────── */

uint16_t gpio_read_port(gpio_port_t port)
{
    gpio_regs_t *regs = gpio_get_port_base(port);
    if (regs == NULL) {
        return 0U;
    }
    return (uint16_t)(regs->IDR & 0xFFFFU);
}

/* ──────────────────────────────────────────────
 * gpio_debounce_init — initialise software debounce
 *
 * Knowledge: Integrate-and-dump debouncer.
 *
 * Mechanical switches bounce: the contacts physically rebound
 * multiple times before settling. Bounce duration is typically
 * 1–10 ms for tactile switches, up to 50 ms for large relays.
 *
 * The integrate-and-dump method samples the pin at a fixed rate
 * and uses a shift register to track recent samples. The state
 * transitions only when all samples in the window agree.
 *
 * Window size = threshold (in sample periods).
 * Latency = threshold × sample_period.
 * False-trigger immunity proportional to threshold.
 *
 * Reference: Ganssle, "A Guide to Debouncing" (2008)
 * ────────────────────────────────────────────── */

void gpio_debounce_init(gpio_debounce_t *db, uint8_t threshold, uint32_t sample_ms)
{
    if (db == NULL) {
        return;
    }
    if (threshold == 0) {
        threshold = 1;  /* At least one sample required */
    }
    if (threshold > 8) {
        threshold = 8;  /* History is 8-bit; cap at 8 samples */
    }

    db->history = 0x00;
    db->stable_count = 0;
    db->threshold = threshold;
    db->current_state = 0;
    db->sample_ticks = sample_ms;  /* Ticks = ms for SysTick at 1 kHz */
    db->last_sample = 0;
}

/* ──────────────────────────────────────────────
 * gpio_debounce_update — feed a raw sample
 *
 * Knowledge: Majority-vote finite state machine.
 *
 * State transition logic:
 *   1. Shift the raw bit into history.
 *   2. Count how many of the last N bits match the candidate state.
 *   3. If count >= threshold → transition confirmed.
 *   4. Otherwise → retain current state.
 *
 * This is essentially a moving-average filter on binary data,
 * equivalent to a low-pass filter with cut-off f_c ≈ 1/(2π × threshold × T_sample).
 *
 * For threshold = 8, sample = 1 ms:
 *   f_c ≈ 1/(2π × 8 × 0.001) ≈ 20 Hz
 *   So bouncing above 20 Hz is filtered out.
 *
 * Complexity: O(1) with popcount (or loop over 8 bits max).
 * ────────────────────────────────────────────── */

uint8_t gpio_debounce_update(gpio_debounce_t *db, uint8_t raw_bit)
{
    uint8_t i;
    uint8_t count_ones;
    uint8_t history_byte;

    if (db == NULL) {
        return 0;
    }

    /* Shift history register left and insert new bit at LSB */
    db->history = (uint8_t)((db->history << 1U) | (raw_bit & 0x01U));

    /* Count ones in history (popcount). We only need the last
     * `threshold` bits for the decision. */
    history_byte = db->history;
    count_ones = 0;
    for (i = 0; i < db->threshold; i++) {
        if (history_byte & (1U << i)) {
            count_ones++;
        }
    }

    /* Decision: if all bits agree on 1 or 0, transition */
    if (count_ones >= db->threshold) {
        /* All recent samples are HIGH → output HIGH */
        db->current_state = 1;
    } else if (count_ones == 0) {
        /* All recent samples are LOW → output LOW */
        db->current_state = 0;
    }
    /* Otherwise, not enough agreement → keep current state */

    return db->current_state;
}

/* ──────────────────────────────────────────────
 * gpio_lock_config — lock GPIO pin configuration
 *
 * Knowledge: LCKR hardware lock sequence.
 *
 * The GPIO configuration can be frozen until the next reset by
 * writing a specific sequence to LCKR. This protects safety-critical
 * pins from accidental reconfiguration (e.g., motor H-bridge enable
 * or power-supply enable pins).
 *
 * Lock sequence (STM32F4 §8.4.8):
 *   1. WR LCKR[16] = 1, LCKR[pin] = 1
 *   2. WR LCKR[16] = 0, LCKR[pin] = 1
 *   3. WR LCKR[16] = 1, LCKR[pin] = 1
 *   4. RD LCKR (optional, to confirm)
 *
 * After step 3, LCKR[16] reads as 1 (lock active) and the
 * MODER/OTYPER/OSPEEDR/PUPDR register bits for that pin are
 * read-only until reset.
 * ────────────────────────────────────────────── */

bool gpio_lock_config(gpio_port_t port, gpio_pin_t pin)
{
    gpio_regs_t *regs = gpio_get_port_base(port);
    uint32_t lock_bit;
    uint32_t seq16;

    if (regs == NULL) {
        return false;
    }

    lock_bit = 1U << (uint32_t)pin;
    seq16 = 1U << 16U;

    /* Step 1: LCKR[16]=1 + LCKR[pin]=1 */
    regs->LCKR = seq16 | lock_bit;

    /* Step 2: LCKR[16]=0 + LCKR[pin]=1 */
    regs->LCKR = lock_bit;

    /* Step 3: LCKR[16]=1 + LCKR[pin]=1 */
    regs->LCKR = seq16 | lock_bit;

    /* Step 4: Read LCKR to confirm lock */
    (void)regs->LCKR;

    /* Check if lock bit asserted */
    if (regs->LCKR & seq16) {
        return true;
    }

    return false;
}

/* ──────────────────────────────────────────────
 * gpio_set_alternate — configure alternate function
 *
 * Knowledge: AFR register for peripheral pin-muxing.
 *
 * On STM32, each pin can be connected to one of 16 internal
 * peripherals via the alternate function multiplexer. The AFR
 * selects which peripheral is routed to the pin.
 *
 * AFRL (pins 0–7): 4 bits per pin → 32 bits total.
 * AFRH (pins 8–15): same layout.
 *
 * Typical assignments (STM32F4):
 *   USART1_TX: PA9, AF7
 *   USART1_RX: PA10, AF7
 *   I2C1_SCL:  PB6, AF4
 *   I2C1_SDA:  PB7, AF4
 *   SPI1_SCK:  PA5, AF5
 *   SPI1_MOSI: PA7, AF5
 *   SPI1_MISO: PA6, AF5
 * ────────────────────────────────────────────── */

void gpio_set_alternate(gpio_port_t port, gpio_pin_t pin, gpio_af_t af)
{
    gpio_regs_t *regs = gpio_get_port_base(port);
    uint32_t pin_pos;
    uint32_t mask;
    uint32_t tmp;

    if (regs == NULL) {
        return;
    }

    pin_pos = (uint32_t)pin;
    if (pin_pos < 8U) {
        mask = 0xFU << (pin_pos * 4U);
        tmp = (regs->AFRL & ~mask) | (((uint32_t)af & 0xFU) << (pin_pos * 4U));
        regs->AFRL = tmp;
    } else {
        mask = 0xFU << ((pin_pos - 8U) * 4U);
        tmp = (regs->AFRH & ~mask) | (((uint32_t)af & 0xFU) << ((pin_pos - 8U) * 4U));
        regs->AFRH = tmp;
    }
}
