/*
 * gpio.h — General Purpose Input/Output for ARM Cortex-M MCUs
 *
 * Knowledge Mapping (SKILL.md L1-L6):
 *   L1 Definitions: GPIO pin, port, mode (input/output/AF/analog),
 *                   pull-up/down, open-drain, push-pull, slew rate
 *   L2 Concepts:    Memory-mapped I/O, bit-banding, register manipulation,
 *                   Schmitt trigger input, output drive strength
 *   L3 Math:        Bitwise mask/shift for register fields,
 *                   RC time constant for pull-up/down settling
 *   L4 Laws:        Ohm's law for current-limiting resistor on LED,
 *                   Kirchhoff's current law for total port current
 *   L5 Algorithms:  Software debounce (majority-vote / integrate-and-dump),
 *                   pin-change interrupt batching
 *
 * Course Mapping:
 *   MIT 6.004 Computation Structures — memory-mapped I/O
 *   Berkeley EE16A/B — GPIO as the bridge between digital and physical
 *   Stanford EE102A — embedded I/O fundamentals
 *   Valvano "Embedded Systems" Ch.4 — GPIO parallel ports
 *
 * Reference: STM32F4xx Reference Manual RM0090, §8 General-purpose I/Os
 */

#ifndef GPIO_H
#define GPIO_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* ──────────────────────────────────────────────
 * L1 Definitions: GPIO Register Layout (memory-mapped)
 *
 * Each GPIO port occupies 1 KB of address space.
 * The register layout follows the STM32 convention,
 * which is representative of ARM Cortex-M MCUs.
 * ────────────────────────────────────────────── */

typedef enum {
    GPIO_PORT_A = 0,
    GPIO_PORT_B = 1,
    GPIO_PORT_C = 2,
    GPIO_PORT_D = 3,
    GPIO_PORT_E = 4,
    GPIO_PORT_F = 5,
    GPIO_PORT_G = 6,
    GPIO_PORT_H = 7,
    GPIO_NUM_PORTS
} gpio_port_t;

/* Pin number within a port (0–15 for 16-bit ports) */
typedef enum {
    GPIO_PIN_0  = 0,
    GPIO_PIN_1  = 1,
    GPIO_PIN_2  = 2,
    GPIO_PIN_3  = 3,
    GPIO_PIN_4  = 4,
    GPIO_PIN_5  = 5,
    GPIO_PIN_6  = 6,
    GPIO_PIN_7  = 7,
    GPIO_PIN_8  = 8,
    GPIO_PIN_9  = 9,
    GPIO_PIN_10 = 10,
    GPIO_PIN_11 = 11,
    GPIO_PIN_12 = 12,
    GPIO_PIN_13 = 13,
    GPIO_PIN_14 = 14,
    GPIO_PIN_15 = 15
} gpio_pin_t;

/* L1 Definition: GPIO pin mode.
 * Input modes include Schmitt-trigger hysteresis (~200 mV typical).
 * Output modes: push-pull can source/sink; open-drain only sinks.
 * Analog mode disconnects digital input to save power in ADC/DAC use.
 */
typedef enum {
    GPIO_MODE_INPUT          = 0x0,  /* 00: Input (reset state, high-Z) */
    GPIO_MODE_OUTPUT         = 0x1,  /* 01: General purpose output */
    GPIO_MODE_ALT_FUNCTION   = 0x2,  /* 10: Alternate function (UART/SPI/etc.) */
    GPIO_MODE_ANALOG         = 0x3   /* 11: Analog (ADC/DAC) */
} gpio_mode_t;

/* L1 Definition: Output type.
 * Push-pull: two complementary transistors, full V_dd/V_ss swing.
 * Open-drain: only N-MOS, requires external pull-up. Used for I²C, wired-AND.
 */
typedef enum {
    GPIO_OTYPE_PUSH_PULL  = 0x0,
    GPIO_OTYPE_OPEN_DRAIN = 0x1
} gpio_otype_t;

/* L1 Definition: Output speed (slew rate control).
 * Trade-off: faster edges → more EMI / ground bounce.
 *            slower edges → less EMI but higher transition losses.
 * Reference: Paul "Introduction to EMC" §3.2 — edge rate vs. harmonic content.
 */
typedef enum {
    GPIO_SPEED_LOW       = 0x0,  /* ~2 MHz, for static LEDs */
    GPIO_SPEED_MEDIUM    = 0x1,  /* ~25 MHz, for general logic */
    GPIO_SPEED_HIGH      = 0x2,  /* ~50 MHz, for SPI at speed */
    GPIO_SPEED_VERY_HIGH = 0x3   /* ~100 MHz, for high-speed interfaces */
} gpio_speed_t;

/* L1 Definition: Pull-up / Pull-down.
 * Pull-up: weak (~40 kΩ) resistor to V_dd.  Default high when undriven.
 * Pull-down: weak (~40 kΩ) resistor to V_ss. Default low when undriven.
 * Floating inputs are susceptible to noise — always enable a pull resistor.
 */
typedef enum {
    GPIO_PUPD_NONE      = 0x0,  /* 00: No pull-up/pull-down (floating) */
    GPIO_PUPD_PULLUP    = 0x1,  /* 01: Pull-up enabled */
    GPIO_PUPD_PULLDOWN  = 0x2   /* 10: Pull-down enabled */
    /* 0x3 is reserved on STM32 */
} gpio_pupd_t;

/* Alternate function selection (0–15). Determines which peripheral
 * (USART, SPI, I²C, TIM) is connected to the pin. */
typedef enum {
    GPIO_AF0  = 0,   /* System (SYSCFG, SWJ) */
    GPIO_AF1  = 1,   /* TIM1/TIM2 */
    GPIO_AF2  = 2,   /* TIM3/TIM4/TIM5 */
    GPIO_AF3  = 3,   /* TIM8..TIM11 */
    GPIO_AF4  = 4,   /* I²C1..I²C3 */
    GPIO_AF5  = 5,   /* SPI1..SPI5 */
    GPIO_AF6  = 6,   /* SPI2/SPI3 */
    GPIO_AF7  = 7,   /* USART1..USART3 */
    GPIO_AF8  = 8,   /* UART4..UART8 */
    GPIO_AF9  = 9,   /* CAN1/CAN2 */
    GPIO_AF10 = 10,  /* OTG_FS/OTG_HS */
    GPIO_AF11 = 11,  /* ETH */
    GPIO_AF12 = 12,  /* FMC/SDIO/OTG */
    GPIO_AF13 = 13,  /* DCMI */
    GPIO_AF14 = 14,  /* LCD-TFT */
    GPIO_AF15 = 15   /* EVENTOUT */
} gpio_af_t;

/* ──────────────────────────────────────────────
 * Hardware Register Map (memory-mapped at port base + offset)
 *
 * The GPIO register block is repeated for each port (A..H).
 * Base address example: GPIOA = 0x40020000 (STM32F4)
 * Each register is 32 bits wide, aligned to 4-byte boundaries.
 * ────────────────────────────────────────────── */
typedef struct {
    volatile uint32_t MODER;    /* 0x00: Mode register (2 bits/pin)      */
    volatile uint32_t OTYPER;   /* 0x04: Output type register (1 bit/pin) */
    volatile uint32_t OSPEEDR;  /* 0x08: Output speed register (2 bits/pin)*/
    volatile uint32_t PUPDR;    /* 0x0C: Pull-up/down register (2 bits/pin)*/
    volatile uint32_t IDR;      /* 0x10: Input data register (read-only)  */
    volatile uint32_t ODR;      /* 0x14: Output data register             */
    volatile uint32_t BSRR;     /* 0x18: Bit set/reset register (atomic)  */
    volatile uint32_t LCKR;     /* 0x1C: Configuration lock register      */
    volatile uint32_t AFRL;     /* 0x20: Alternate function low (pins 0-7)*/
    volatile uint32_t AFRH;     /* 0x24: Alternate function high (pins 8-15)*/
} gpio_regs_t;

/* ──────────────────────────────────────────────
 * Bit-band support (Cortex-M3/M4 feature)
 *
 * Bit-banding maps each bit of the peripheral/SRAM region to a
 * separate word in the bit-band alias region, allowing atomic
 * single-bit read-modify-write without masking.
 *
 * Formula (ARMv7-M Architecture Ref Manual §B3.2):
 *   bit_word_addr = bit_band_base + (byte_offset * 32) + (bit_num * 4)
 *
 * L2 Concept: Atomic bit manipulation without critical sections.
 * ────────────────────────────────────────────── */
#define GPIO_BITBAND_BASE      0x42000000U
#define GPIO_PERIPH_BASE       0x40000000U
#define BITBAND_PERIPH(addr, bit_num) \
    (*(volatile uint32_t *)(GPIO_BITBAND_BASE + \
     (((uint32_t)(addr) - GPIO_PERIPH_BASE) * 32U) + ((bit_num) * 4U)))

/* ──────────────────────────────────────────────
 * GPIO Configuration Structure
 *
 * Bundles all per-pin settings into a single initialisation descriptor.
 * A const array of these can be used to configure an entire port at boot.
 * ────────────────────────────────────────────── */
typedef struct {
    gpio_port_t   port;
    gpio_pin_t    pin;
    gpio_mode_t   mode;
    gpio_otype_t  otype;
    gpio_speed_t  speed;
    gpio_pupd_t   pupd;
    gpio_af_t     alternate;
} gpio_config_t;

/* ──────────────────────────────────────────────
 * Software Debounce State (L5 Algorithm)
 *
 * Integrate-and-dump debouncer: samples the pin at a fixed rate
 * and transitions only when N consecutive samples agree.
 * This is a moving-average filter on binary data.
 *
 * Mathematical basis: For a noisy mechanical switch with bounce
 * period T_b ≤ 5 ms, sampling at f_s = 1 kHz (T_s = 1 ms) and
 * requiring N = 8 samples of agreement yields:
 *   Debounce latency = N * T_s = 8 ms
 *   False-trigger probability ≈ (bit-error-rate)^(N/2) for symmetric noise.
 * ────────────────────────────────────────────── */
typedef struct {
    uint8_t    history;        /* Shift register of recent samples */
    uint8_t    stable_count;   /* Consecutive matching samples   */
    uint8_t    threshold;      /* Required matches to transition  */
    uint8_t    current_state;  /* Debounced output state          */
    uint32_t   sample_ticks;   /* System ticks between samples    */
    uint32_t   last_sample;    /* Tick at last sample             */
} gpio_debounce_t;

/* ──────────────────────────────────────────────
 * GPIO API
 * ────────────────────────────────────────────── */

/*
 * gpio_init — configure a single GPIO pin
 *
 * Sets MODER, OTYPER, OSPEEDR, PUPDR, and AFR registers
 * for the specified pin. Must be called after RCC clock enable.
 *
 * @param config: pointer to configuration descriptor
 *
 * Complexity: O(1) — register field manipulation
 * Reference: STM32F4 Ref Manual §8.4 — GPIO register descriptions
 */
void gpio_init(const gpio_config_t *config);

/*
 * gpio_write — set output pin state
 *
 * Writes to BSRR register for atomic bit-set/bit-reset.
 * Lower 16 bits of BSRR: set (write 1 to set ODR bit)
 * Upper 16 bits of BSRR: reset (write 1 to clear ODR bit)
 *
 * Using BSRR avoids read-modify-write races on ODR.
 *
 * @param port: GPIO port (A..H)
 * @param pin:  pin number (0..15)
 * @param state: true = high, false = low
 *
 * Complexity: O(1) — single atomic store
 * Reference: ARMv7-M Architecture Ref §B3.2.6 — Bit-banding
 */
void gpio_write(gpio_port_t port, gpio_pin_t pin, bool state);

/*
 * gpio_toggle — atomically flip an output pin
 *
 * Reads ODR, inverts the target bit, writes back.
 * For truly atomic toggle on a single pin, use BSRR with a
 * conditional: if ODR bit is set, write to BR; else write to BS.
 *
 * @param port: GPIO port
 * @param pin:  pin number
 *
 * Complexity: O(1)
 * Caution: ODR read-back is NOT atomic if another ISR writes ODR simultaneously.
 *          Use hardware timer PWM for true glitch-free toggling.
 */
void gpio_toggle(gpio_port_t port, gpio_pin_t pin);

/*
 * gpio_read — read input pin state
 *
 * Reads the IDR (Input Data Register). On STM32, IDR captures
 * the pin level after the Schmitt trigger (and optional pull resistor),
 * so it reflects the actual pin voltage, not the ODR value.
 *
 * @param port: GPIO port
 * @param pin:  pin number
 * @return true if pin is logic high (> V_IH, typically 0.7 * V_dd)
 *
 * Complexity: O(1) — single load
 */
bool gpio_read(gpio_port_t port, gpio_pin_t pin);

/*
 * gpio_set_mode — change pin mode at runtime
 *
 * Useful for reconfiguring a pin (e.g., from output to analog
 * after driving an LED, or switching I²C SDA between output and input).
 *
 * @param port: GPIO port
 * @param pin:  pin number
 * @param mode: new mode
 *
 * Complexity: O(1)
 */
void gpio_set_mode(gpio_port_t port, gpio_pin_t pin, gpio_mode_t mode);

/*
 * gpio_write_port — write a 16-bit value to entire port
 *
 * Direct ODR write. Use with caution: non-atomic with respect to
 * interrupts that also write ODR.
 *
 * @param port:  GPIO port
 * @param value: 16-bit value (upper 16 bits ignored)
 */
void gpio_write_port(gpio_port_t port, uint16_t value);

/*
 * gpio_read_port — read 16-bit input from entire port
 *
 * @param port: GPIO port
 * @return 16-bit value from IDR
 */
uint16_t gpio_read_port(gpio_port_t port);

/*
 * gpio_debounce_init — initialise a software debounce filter
 *
 * @param db:         debounce state structure
 * @param threshold:  consecutive samples needed for state transition
 * @param sample_ms:  sampling period in milliseconds
 *
 * Typical threshold = 5–10 for mechanical switches.
 * Higher threshold → more noise immunity, longer latency.
 *
 * Reference: Ganssle "A Guide to Debouncing" (2008)
 *            — compares integrate-and-dump vs. cross-coupled latch approaches.
 */
void gpio_debounce_init(gpio_debounce_t *db, uint8_t threshold, uint32_t sample_ms);

/*
 * gpio_debounce_update — feed a raw sample into the debouncer
 *
 * Call periodically (in timer ISR or main loop). Returns the
 * new debounced state if a transition is confirmed, otherwise
 * returns the previous stable state.
 *
 * @param db:      debounce state
 * @param raw_bit: current raw pin reading (0 or 1)
 * @return debounced state (0 or 1)
 *
 * Algorithm: Integrate-and-dump (moving-average threshold detector)
 *   history = (history << 1) | raw_bit
 *   If all N bits == state → confirm transition
 *
 * Complexity: O(1)
 */
uint8_t gpio_debounce_update(gpio_debounce_t *db, uint8_t raw_bit);

/*
 * gpio_lock_config — lock GPIO configuration
 *
 * Writing the correct sequence to LCKR freezes the MODER, OTYPER,
 * OSPEEDR, PUPDR, and AFR registers until the next system reset.
 * Used for safety-critical pins (motor control, power enables).
 *
 * Lock sequence: WR LCKR[16] = 1 + LCKR[pin] = 1, WR LCKR[16] = 0 + LCKR[pin] = 1,
 *                RD LCKR, WR LCKR[pin] = 1 (optional read).
 *
 * @param port: GPIO port
 * @param pin:  pin to lock
 * @return true if lock succeeded
 *
 * Reference: STM32F4 §8.4.8 — GPIO port configuration lock register
 */
bool gpio_lock_config(gpio_port_t port, gpio_pin_t pin);

/*
 * gpio_set_alternate — assign alternate function to a pin
 *
 * Used to route internal peripherals (USART, SPI, I²C, TIM)
 * to physical pins. AFRL handles pins 0-7; AFRH handles pins 8-15.
 *
 * @param port: GPIO port
 * @param pin:  pin number
 * @param af:   alternate function number (0-15)
 */
void gpio_set_alternate(gpio_port_t port, gpio_pin_t pin, gpio_af_t af);

/*
 * gpio_get_port_base — return base address of port registers
 *
 * Returns a pointer to the gpio_regs_t structure for direct
 * register access. Used by low-level drivers (e.g., fast bit-banding).
 *
 * @param port: GPIO port
 * @return pointer to volatile register struct, or NULL if invalid port
 */
gpio_regs_t *gpio_get_port_base(gpio_port_t port);

#endif /* GPIO_H */
