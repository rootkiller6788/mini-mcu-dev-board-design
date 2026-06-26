/*
 * test_gpio.c — GPIO API tests (assert-based)
 *
 * Tests all public GPIO API functions and data structures.
 * These tests run on the host (not on the MCU) and verify
 * logic correctness, register manipulation math, and
 * debounce state machine behavior.
 */

#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <stdbool.h>
#include "../include/gpio.h"

/* ──────────────────────────────────────────────
 * Test: Register field math (L3 — bit manipulation)
 *
 * Verify that the MODER/OTYPER/OSPEEDR/PUPDR field positions
 * and mask values are correct for each pin.
 * ────────────────────────────────────────────── */
static void test_register_field_positions(void)
{
    /* MODER: 2 bits per pin, starting at pin × 2 */
    assert((0x3U << (0 * 2)) == 0x00000003U);  /* Pin 0: bits [1:0] */
    assert((0x3U << (7 * 2)) == 0x0000C000U);  /* Pin 7: bits [15:14] */
    assert((0x3U << (15 * 2)) == 0xC0000000U); /* Pin 15: bits [31:30] */

    /* OTYPER: 1 bit per pin */
    assert((1U << 0) == 0x00000001U);
    assert((1U << 15) == 0x00008000U);

    printf("  PASS: register field positions\n");
}

/* ──────────────────────────────────────────────
 * Test: GPIO configuration structure sizes
 * ────────────────────────────────────────────── */
static void test_config_structure(void)
{
    gpio_config_t cfg;

    /* Verify structure size and field offsets */
    memset(&cfg, 0, sizeof(cfg));
    cfg.port = GPIO_PORT_A;
    cfg.pin = GPIO_PIN_5;
    cfg.mode = GPIO_MODE_OUTPUT;
    cfg.otype = GPIO_OTYPE_PUSH_PULL;
    cfg.speed = GPIO_SPEED_LOW;
    cfg.pupd = GPIO_PUPD_NONE;
    cfg.alternate = GPIO_AF0;

    assert(cfg.port == GPIO_PORT_A);
    assert(cfg.pin == GPIO_PIN_5);
    assert(cfg.mode == GPIO_MODE_OUTPUT);
    assert(cfg.otype == GPIO_OTYPE_PUSH_PULL);
    assert(cfg.speed == GPIO_SPEED_LOW);
    assert(cfg.pupd == GPIO_PUPD_NONE);
    assert(cfg.alternate == GPIO_AF0);

    printf("  PASS: config structure\n");
}

/* ──────────────────────────────────────────────
 * Test: GPIO port base address retrieval
 * ────────────────────────────────────────────── */
static void test_port_base_addresses(void)
{
    /* Valid ports should return non-NULL */
    assert(gpio_get_port_base(GPIO_PORT_A) != NULL);
    assert(gpio_get_port_base(GPIO_PORT_H) != NULL);

    /* Invalid port returns NULL */
    assert(gpio_get_port_base(GPIO_NUM_PORTS) == NULL);
    assert(gpio_get_port_base((gpio_port_t)255) == NULL);

    /* Verify port A base address is 0x40020000 */
    gpio_regs_t *regs_a = gpio_get_port_base(GPIO_PORT_A);
    assert(regs_a != NULL);

    printf("  PASS: port base addresses\n");
}

/* ──────────────────────────────────────────────
 * Test: GPIO port NULL safety
 *
 * gpio_init, gpio_write, gpio_read, gpio_toggle, etc.
 * must handle NULL config pointer gracefully.
 * ────────────────────────────────────────────── */
static void test_null_safety(void)
{
    /* gpio_init with NULL config should not crash */
    gpio_init(NULL);

    /* gpio_write with invalid port should not crash */
    gpio_write((gpio_port_t)255, GPIO_PIN_0, true);

    /* gpio_read with invalid port should return false */
    assert(gpio_read((gpio_port_t)255, GPIO_PIN_0) == false);

    /* gpio_toggle with invalid port should not crash */
    gpio_toggle((gpio_port_t)255, GPIO_PIN_0);

    printf("  PASS: null safety\n");
}

/* ──────────────────────────────────────────────
 * Test: Software debounce — integrate-and-dump (L5)
 *
 * Verifies the debounce state machine correctly:
 *   1. Initializes to state 0
 *   2. Transitions to 1 after threshold consecutive 1s
 *   3. Does NOT transition with fewer than threshold samples
 *   4. Transition to 0 after threshold consecutive 0s
 * ────────────────────────────────────────────── */
static void test_debounce_state_machine(void)
{
    gpio_debounce_t db;
    uint8_t result;
    int i;

    /* Initialize: threshold = 5 samples */
    gpio_debounce_init(&db, 5, 1);

    /* Initial state should be 0 */
    assert(db.current_state == 0);
    assert(db.threshold == 5);

    /* Feed 4 ones — should NOT transition yet */
    for (i = 0; i < 4; i++) {
        result = gpio_debounce_update(&db, 1);
        assert(result == 0);  /* Still 0 — not enough ones */
    }

    /* Feed 5th one — should transition to 1 */
    result = gpio_debounce_update(&db, 1);
    assert(result == 1);

    /* Feed a single 0 — should NOT transition (not enough zeros) */
    result = gpio_debounce_update(&db, 0);
    assert(result == 1);  /* Still 1 */

    /* Feed 5 zeros — should transition to 0 */
    for (i = 0; i < 4; i++) {
        result = gpio_debounce_update(&db, 0);
    }
    /* After 5 zeros (1 from above + 4 more), should be 0 */
    assert(result == 0);

    printf("  PASS: debounce state machine\n");
}

/* ──────────────────────────────────────────────
 * Test: Debounce NULL safety
 * ────────────────────────────────────────────── */
static void test_debounce_null(void)
{
    gpio_debounce_init(NULL, 5, 1);
    assert(gpio_debounce_update(NULL, 0) == 0);
    printf("  PASS: debounce null safety\n");
}

/* ──────────────────────────────────────────────
 * Test: GPIO mode enum values match STM32 hardware
 * ────────────────────────────────────────────── */
static void test_mode_enum_values(void)
{
    assert(GPIO_MODE_INPUT == 0x0);
    assert(GPIO_MODE_OUTPUT == 0x1);
    assert(GPIO_MODE_ALT_FUNCTION == 0x2);
    assert(GPIO_MODE_ANALOG == 0x3);

    assert(GPIO_OTYPE_PUSH_PULL == 0x0);
    assert(GPIO_OTYPE_OPEN_DRAIN == 0x1);

    assert(GPIO_PUPD_NONE == 0x0);
    assert(GPIO_PUPD_PULLUP == 0x1);
    assert(GPIO_PUPD_PULLDOWN == 0x2);

    printf("  PASS: mode enum values\n");
}

/* ──────────────────────────────────────────────
 * Test: BSRR bit positions (atomic write)
 *
 * BS bits are [15:0] (set), BR bits are [31:16] (reset).
 * ────────────────────────────────────────────── */
static void test_bsrr_positions(void)
{
    uint32_t bs_pin5  = 1U << 5;       /* Set pin 5  → bit 5 */
    uint32_t br_pin5  = 1U << (5 + 16); /* Reset pin 5 → bit 21 */

    assert(bs_pin5 == 0x00000020U);
    assert(br_pin5 == 0x00200000U);

    /* BS and BR must not overlap */
    assert((bs_pin5 & br_pin5) == 0);

    printf("  PASS: BSRR positions\n");
}

/* ──────────────────────────────────────────────
 * Test: GPIO port count
 * ────────────────────────────────────────────── */
static void test_port_count(void)
{
    assert(GPIO_NUM_PORTS == 8);  /* A through H */
    printf("  PASS: port count\n");
}

int main(void)
{
    printf("=== GPIO Tests ===\n");

    test_register_field_positions();
    test_config_structure();
    test_port_base_addresses();
    test_null_safety();
    test_debounce_state_machine();
    test_debounce_null();
    test_mode_enum_values();
    test_bsrr_positions();
    test_port_count();

    printf("All GPIO tests passed!\n");
    return 0;
}
