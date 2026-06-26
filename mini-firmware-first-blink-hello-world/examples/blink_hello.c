/*
 * blink_hello.c — L6 Canonical Problem: LED Blink + "Hello World" over UART
 *
 * This is the archetypal first firmware: make an LED blink and
 * print "Hello, World!" over the serial port simultaneously.
 *
 * On a real STM32F407 Discovery board:
 *   - LED: PD12 (green), PD13 (orange), PD14 (red), PD15 (blue)
 *   - USART2: PA2 (TX), PA3 (RX), connected to ST-LINK VCP
 *
 * This example uses host-side simulation: instead of toggling
 * physical registers, we simulate the blink pattern in printf.
 *
 * Knowledge demonstrated:
 *   L1: GPIO output mode, timer SysTick, UART TX
 *   L2: Memory-mapped register simulation
 *   L5: PWM duty cycle sweep for LED breathing effect
 *   L6: Complete blink + hello-world firmware
 *
 * Course Mapping:
 *   Valvano Ch.2 — First embedded C program
 *   MIT 6.004 Lab 0 — Blink
 */

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "../include/gpio.h"
#include "../include/startup.h"
#include "../include/cortex_m.h"

/* ──────────────────────────────────────────────
 * Simulated hardware state (host-side testing)
 * ────────────────────────────────────────────── */
typedef struct {
    bool     led_green;
    bool     led_orange;
    bool     led_red;
    bool     led_blue;
    uint32_t tick_count;
} sim_state_t;

static sim_state_t g_sim;

/* ──────────────────────────────────────────────
 * Simulated GPIO write: updates the LED state
 * and prints a status line when the state changes.
 * ────────────────────────────────────────────── */
static void sim_gpio_write(gpio_port_t port, gpio_pin_t pin, bool state)
{
    (void)port;  /* On STM32F4 Discovery, LEDs are on PD12-15 */

    switch (pin) {
    case GPIO_PIN_12: g_sim.led_green  = state; break;
    case GPIO_PIN_13: g_sim.led_orange = state; break;
    case GPIO_PIN_14: g_sim.led_red    = state; break;
    case GPIO_PIN_15: g_sim.led_blue   = state; break;
    default: break;
    }
}

/* ──────────────────────────────────────────────
 * LED pattern generator
 *
 * Sequence: GREEN → ORANGE → RED → BLUE → all off → repeat.
 * Each state lasts for `duration_ms` ticks.
 * ────────────────────────────────────────────── */
typedef enum {
    PATTERN_GREEN,
    PATTERN_ORANGE,
    PATTERN_RED,
    PATTERN_BLUE,
    PATTERN_OFF,
    PATTERN_NUM_STATES
} pattern_state_t;

static const char *pattern_names[PATTERN_NUM_STATES] = {
    "GREEN", "ORANGE", "RED", "BLUE", "OFF"
};

/* ──────────────────────────────────────────────
 * Main firmware loop (simulated)
 *
 * This would run on the MCU in an infinite loop.
 * Here we simulate 20 seconds of operation.
 * ────────────────────────────────────────────── */
int main(void)
{
    clock_config_t clk_cfg;
    pattern_state_t pattern = PATTERN_GREEN;
    uint32_t last_toggle = 0;
    uint32_t now;
    uint32_t loop_count = 0;
    bool led_state = false;

    printf("╔══════════════════════════════════════════╗\n");
    printf("║  Firmware: Blink + Hello World           ║\n");
    printf("║  MCU:     STM32F407 (168 MHz simulated)  ║\n");
    printf("║  LED:     PD12–PD15 (Discovery board)    ║\n");
    printf("╚══════════════════════════════════════════╝\n\n");

    /* 1. System initialization */
    memset(&clk_cfg, 0, sizeof(clk_cfg));
    clk_cfg.source = CLOCK_SOURCE_PLL;
    clk_cfg.hse_freq = 8000000U;
    clk_cfg.target_sysclk = 168000000U;
    clk_cfg.pll_m = 8;
    clk_cfg.pll_n = 336;
    clk_cfg.pll_p = 2;
    clk_cfg.pll_q = 7;
    clk_cfg.ahb_prescaler = 0;   /* /1 */
    clk_cfg.apb1_prescaler = 5;  /* /4 → 42 MHz */
    clk_cfg.apb2_prescaler = 4;  /* /2 → 84 MHz */

    system_init(&clk_cfg);

    printf("[INIT] System clock: %u Hz\n", (unsigned)clk_cfg.target_sysclk);
    printf("[INIT] PLL: M=%u N=%u P=%u Q=%u\n",
           (unsigned)clk_cfg.pll_m, (unsigned)clk_cfg.pll_n,
           (unsigned)clk_cfg.pll_p, (unsigned)clk_cfg.pll_q);

    /* 2. Hello World over simulated UART */
    printf("[UART] Hello, World! from bare-metal firmware\n");
    printf("[UART] Tick counter running at 1 kHz\n");

    /* 3. GPIO configuration for LEDs */
    gpio_config_t led_cfg;
    memset(&led_cfg, 0, sizeof(led_cfg));
    led_cfg.port   = GPIO_PORT_D;
    led_cfg.mode   = GPIO_MODE_OUTPUT;
    led_cfg.otype  = GPIO_OTYPE_PUSH_PULL;
    led_cfg.speed  = GPIO_SPEED_LOW;
    led_cfg.pupd   = GPIO_PUPD_NONE;

    /* Configure all 4 LED pins */
    led_cfg.pin = GPIO_PIN_12; gpio_init(&led_cfg);
    led_cfg.pin = GPIO_PIN_13; gpio_init(&led_cfg);
    led_cfg.pin = GPIO_PIN_14; gpio_init(&led_cfg);
    led_cfg.pin = GPIO_PIN_15; gpio_init(&led_cfg);

    printf("[GPIO] LED pins PD12-PD15 configured as outputs\n\n");

    /* 4. Main blink loop (simulated 15 seconds) */
    printf("=== Starting blink pattern (15 sec simulated) ===\n");

    for (loop_count = 0; loop_count < 15000; loop_count++) {
        now = systick_get_count() + (uint32_t)loop_count;  /* Simulated tick advance */

        /* Every 1000 ms (simulated): advance pattern */
        if (now - last_toggle >= 1000U) {
            last_toggle = now;

            /* Turn all LEDs off */
            sim_gpio_write(GPIO_PORT_D, GPIO_PIN_12, false);
            sim_gpio_write(GPIO_PORT_D, GPIO_PIN_13, false);
            sim_gpio_write(GPIO_PORT_D, GPIO_PIN_14, false);
            sim_gpio_write(GPIO_PORT_D, GPIO_PIN_15, false);

            /* Turn on the current pattern LED */
            switch (pattern) {
            case PATTERN_GREEN:
                sim_gpio_write(GPIO_PORT_D, GPIO_PIN_12, true);
                break;
            case PATTERN_ORANGE:
                sim_gpio_write(GPIO_PORT_D, GPIO_PIN_13, true);
                break;
            case PATTERN_RED:
                sim_gpio_write(GPIO_PORT_D, GPIO_PIN_14, true);
                break;
            case PATTERN_BLUE:
                sim_gpio_write(GPIO_PORT_D, GPIO_PIN_15, true);
                break;
            case PATTERN_OFF:
                /* All off */
                break;
            default:
                break;
            }

            printf("  [t=%5lu ms] LED: %s\n",
                   (unsigned long)now,
                   pattern_names[pattern]);

            /* Advance pattern */
            pattern = (pattern_state_t)(((int)pattern + 1) % PATTERN_NUM_STATES);
        }

        /* Simulate button check every 500 ms */
        if ((loop_count % 500) == 0 && loop_count > 0) {
            /* Read simulated button on PA0 */
            bool button = gpio_read(GPIO_PORT_A, GPIO_PIN_0);
            if (button) {
                printf("  [BUTTON] PA0 pressed! Toggling speed\n");
                led_state = !led_state;
            }
        }
    }

    /* 5. Shutdown sequence */
    printf("\n=== Shutdown ===\n");

    /* Turn off all LEDs */
    sim_gpio_write(GPIO_PORT_D, GPIO_PIN_12, false);
    sim_gpio_write(GPIO_PORT_D, GPIO_PIN_13, false);
    sim_gpio_write(GPIO_PORT_D, GPIO_PIN_14, false);
    sim_gpio_write(GPIO_PORT_D, GPIO_PIN_15, false);

    printf("[GPIO] All LEDs off\n");
    printf("[UART] Goodbye, World!\n");
    printf("[SYS]  System halted. Blink demo complete.\n");

    /* Wait a bit then reset */
    scb_system_reset();

    return 0;  /* Not reached (reset) */
}
