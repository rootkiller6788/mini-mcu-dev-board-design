/*
 * test_uart.c — UART API tests (assert-based)
 *
 * Tests: baud rate calculation, ring buffer, printf formatting,
 * status register decoding, error handling.
 */

#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <stdbool.h>
#include "../include/uart.h"

/* ──────────────────────────────────────────────
 * Test: Baud rate calculation (L3 — fixed-point divider)
 *
 * Verify the BRR formula for known configurations.
 * ────────────────────────────────────────────── */
static void test_baud_rate_calculation(void)
{
    uint32_t brr;

    /* Case 1: 84 MHz, 115200 baud, ×16 oversample
     * USARTDIV = 84,000,000 / (16 × 115200) = 45.5729...
     * Mantissa = 45, Fraction = round(0.5729 × 16) = 9
     * BRR = (45 << 4) | 9 = 0x2D9 = 729 */
    brr = uart_calculate_brr(84000000U, 115200U, UART_OVERSAMPLE_16);
    assert(brr == 0x2D9U);

    /* Case 2: 84 MHz, 9600 baud, ×16
     * USARTDIV = 84,000,000 / (16 × 9600) = 546.875
     * Mantissa = 546, Fraction = round(0.875 × 16) = 14
     * BRR = (546 << 4) | 14 = 0x222E = 8750 */
    brr = uart_calculate_brr(84000000U, 9600U, UART_OVERSAMPLE_16);
    assert(brr == 0x222EU);

    /* Case 3: 84 MHz, 115200 baud, ×8 oversample
     * USARTDIV = 84,000,000 / (8 × 115200) = 91.146...
     * Mantissa = 91, Fraction = round(0.146 × 8) / 2 = 1/2? Actually:
     *   fraction = round(0.146 × 8) = 1
     * But ×8 mode divides fraction by 2 → 0 (lower 4 bits of BRR)
     * BRR = (91 << 4) | 0 = 0x5B0 = 1456 */
    brr = uart_calculate_brr(84000000U, 115200U, UART_OVERSAMPLE_8);
    assert(brr == 0x5B0U);

    /* Case 4: baud rate = 0 → returns 0 */
    brr = uart_calculate_brr(84000000U, 0U, UART_OVERSAMPLE_16);
    assert(brr == 0U);

    printf("  PASS: baud rate calculation\n");
}

/* ──────────────────────────────────────────────
 * Test: Ring buffer operations (L5 — circular queue)
 *
 * Verify producer-consumer semantics, overflow detection,
 * and count calculation.
 * ────────────────────────────────────────────── */
static void test_ring_buffer(void)
{
    uart_ringbuf_t rb;
    uint8_t byte;
    uint8_t i;

    /* Init */
    uart_ringbuf_init(&rb);
    assert(rb.head == 0);
    assert(rb.tail == 0);
    assert(rb.overflow == 0);
    assert(uart_ringbuf_available(&rb) == 0);

    /* Get from empty → fails */
    assert(uart_ringbuf_get(&rb, &byte) == false);

    /* Put a few bytes */
    for (i = 0; i < 10; i++) {
        assert(uart_ringbuf_put(&rb, i) == true);
    }
    assert(uart_ringbuf_available(&rb) == 10);

    /* Get them back in order */
    for (i = 0; i < 10; i++) {
        assert(uart_ringbuf_get(&rb, &byte) == true);
        assert(byte == i);
    }
    assert(uart_ringbuf_available(&rb) == 0);

    /* Fill to capacity (UART_RXBUF_SIZE - 1 = 255 slots usable) */
    uart_ringbuf_init(&rb);
    for (i = 0; i < UART_RXBUF_SIZE - 1; i++) {
        assert(uart_ringbuf_put(&rb, (uint8_t)(i & 0xFF)) == true);
    }
    assert(uart_ringbuf_available(&rb) == UART_RXBUF_SIZE - 1);

    /* Next put should overflow */
    assert(uart_ringbuf_put(&rb, 0xFF) == false);
    assert(rb.overflow == 1);

    printf("  PASS: ring buffer\n");
}

/* ──────────────────────────────────────────────
 * Test: Ring buffer wrap-around
 * ────────────────────────────────────────────── */
static void test_ring_buffer_wraparound(void)
{
    uart_ringbuf_t rb;
    uint8_t byte;
    uint8_t i;

    uart_ringbuf_init(&rb);

    /* Fill almost to capacity */
    for (i = 0; i < 200; i++) {
        uart_ringbuf_put(&rb, i);
    }

    /* Drain half */
    for (i = 0; i < 100; i++) {
        uart_ringbuf_get(&rb, &byte);
        assert(byte == i);
    }

    /* Fill again to test wrap */
    for (i = 0; i < 100; i++) {
        uart_ringbuf_put(&rb, (uint8_t)(200 + i));
    }

    /* Drain remaining — should be bytes 100–199 then 200–299 */
    for (i = 0; i < 100; i++) {
        assert(uart_ringbuf_get(&rb, &byte) == true);
        assert(byte == (uint8_t)(100 + i));
    }
    for (i = 0; i < 100; i++) {
        assert(uart_ringbuf_get(&rb, &byte) == true);
        assert(byte == (uint8_t)(200 + i));
    }

    assert(uart_ringbuf_available(&rb) == 0);

    printf("  PASS: ring buffer wrap-around\n");
}

/* ──────────────────────────────────────────────
 * Test: Ring buffer NULL safety
 * ────────────────────────────────────────────── */
static void test_ring_buffer_null(void)
{
    uint8_t byte;

    uart_ringbuf_init(NULL);
    assert(uart_ringbuf_put(NULL, 0x42) == false);
    assert(uart_ringbuf_get(NULL, &byte) == false);
    assert(uart_ringbuf_get(NULL, NULL) == false);
    assert(uart_ringbuf_available(NULL) == 0);

    printf("  PASS: ring buffer null safety\n");
}

/* ──────────────────────────────────────────────
 * Test: UART configuration structure
 * ────────────────────────────────────────────── */
static void test_uart_config(void)
{
    uart_config_t cfg;

    memset(&cfg, 0, sizeof(cfg));
    cfg.uart_index = 0;
    cfg.baudrate = 115200;
    cfg.wordlen = UART_WORD_8BITS;
    cfg.stopbits = UART_STOP_1;
    cfg.parity = UART_PARITY_NONE;
    cfg.oversample = UART_OVERSAMPLE_16;
    cfg.flow_control = UART_FLOW_NONE;
    cfg.pclk_freq = 84000000U;

    assert(cfg.baudrate == 115200U);
    assert(cfg.wordlen == UART_WORD_8BITS);
    assert(cfg.stopbits == UART_STOP_1);
    assert(cfg.parity == UART_PARITY_NONE);

    printf("  PASS: UART config\n");
}

/* ──────────────────────────────────────────────
 * Test: Status register bit definitions
 * ────────────────────────────────────────────── */
static void test_status_bits(void)
{
    /* Verify status register bit positions match hardware */
    assert(UART_SR_TXE == (1U << 7));
    assert(UART_SR_TC == (1U << 6));
    assert(UART_SR_RXNE == (1U << 5));
    assert(UART_SR_ORE == (1U << 3));
    assert(UART_CR1_UE == (1U << 13));

    /* Verify CR1 bits are disjoint */
    assert((UART_CR1_TE & UART_CR1_RE) == 0);

    printf("  PASS: status bits\n");
}

/* ──────────────────────────────────────────────
 * Test: UART base address retrieval
 * ────────────────────────────────────────────── */
static void test_uart_base_addresses(void)
{
    assert(uart_get_regs(0) != NULL);  /* USART1 */
    assert(uart_get_regs(5) != NULL);  /* USART6 */
    assert(uart_get_regs(6) == NULL);  /* Invalid */
    assert(uart_get_regs(255) == NULL);

    printf("  PASS: UART base addresses\n");
}

int main(void)
{
    printf("=== UART Tests ===\n");

    test_baud_rate_calculation();
    test_ring_buffer();
    test_ring_buffer_wraparound();
    test_ring_buffer_null();
    test_uart_config();
    test_status_bits();
    test_uart_base_addresses();

    printf("All UART tests passed!\n");
    return 0;
}
