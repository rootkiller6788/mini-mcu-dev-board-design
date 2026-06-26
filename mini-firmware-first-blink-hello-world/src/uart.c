/*
 * uart.c — UART/USART Implementation for ARM Cortex-M MCUs
 *
 * Knowledge points implemented (independent):
 *   1. uart_calculate_brr — baud rate generation with fractional divider
 *   2. uart_init — register configuration for word length, parity, stop bits
 *   3. uart_send_byte — blocking TX with TXE polling
 *   4. uart_receive_byte — blocking RX with timeout
 *   5. uart_send_string — null-terminated string output
 *   6. uart_printf — minimal printf with %d/%u/%x/%s/%c
 *   7. uart_ringbuf — lock-free single-producer single-consumer ring buffer
 *   8. uart_get_status — SR flag decoding
 *   9. uart_set_baudrate — runtime baud rate change
 *  10. uart_error_clear — error recovery sequence
 */

#include <stdint.h>
#include <stdbool.h>
#include <stdarg.h>
#include <string.h>
#include "uart.h"
#include "cortex_m.h"

/* ──────────────────────────────────────────────
 * UART Base Addresses (STM32F4)
 *
 * USART1: 0x40011000 (APB2, max 84 MHz)
 * USART2: 0x40004400 (APB1, max 42 MHz)
 * USART3: 0x40004800 (APB1)
 * UART4:  0x40004C00 (APB1)
 * UART5:  0x40005000 (APB1)
 * USART6: 0x40011400 (APB2)
 * ────────────────────────────────────────────── */

#define UART_NUM_PERIPHERALS 6

static const uint32_t uart_base_addresses[UART_NUM_PERIPHERALS] = {
    0x40011000U,  /* USART1 */
    0x40004400U,  /* USART2 */
    0x40004800U,  /* USART3 */
    0x40004C00U,  /* UART4  */
    0x40005000U,  /* UART5  */
    0x40011400U   /* USART6 */
};

uart_regs_t *uart_get_regs(uint8_t uart_index)
{
    if (uart_index >= UART_NUM_PERIPHERALS) {
        return NULL;
    }
    return (uart_regs_t *)(uintptr_t)uart_base_addresses[uart_index];
}

/* ──────────────────────────────────────────────
 * uart_calculate_brr — Baud Rate Register computation
 *
 * Knowledge: Fractional baud rate divider.
 *
 * The USART baud rate generator uses a fixed-point divider:
 *   USARTDIV = f_CK / (oversampling_rate × baud_rate)
 *
 * For OVER8=0 (×16 oversampling):
 *   USARTDIV = f_CK / (16 × baud)
 *
 * For OVER8=1 (×8 oversampling):
 *   USARTDIV = f_CK / (8 × baud)
 *
 * BRR register format:
 *   bits[15:4] = DIV_Mantissa[11:0] — integer part of USARTDIV
 *   bits[3:0]  = DIV_Fraction[3:0]  — fractional part
 *
 * DIV_Fraction = round((USARTDIV − DIV_Mantissa) × oversampling_rate)
 *
 * Example: f_CK = 84 MHz, baud = 115200, ×16 oversample:
 *   USARTDIV = 84,000,000 / (16 × 115200) = 84,000,000 / 1,843,200 = 45.5729...
 *   DIV_Mantissa = 45 (= 0x2D)
 *   DIV_Fraction = round(0.5729 × 16) = round(9.1667) = 9 (= 0x9)
 *   BRR = (45 << 4) | 9 = 0x2D9
 *
 * Baud rate error:
 *   Actual baud = 84,000,000 / (16 × 45.5625) = 115,200.88
 *   Error = (115,200.88 − 115,200) / 115,200 = 0.00076% → negligible
 *
 * Reference: STM32F4 §30.6.4 — Baud rate generation
 * ────────────────────────────────────────────── */

uint32_t uart_calculate_brr(uint32_t pclk_freq, uint32_t baudrate,
                            uart_oversample_t oversample)
{
    uint32_t oversample_rate;
    uint64_t usartdiv_times_1000;  /* USARTDIV × 1000 for precision */
    uint32_t mantissa;
    uint32_t fraction;
    uint32_t fraction_div;

    if (baudrate == 0) {
        return 0;
    }

    oversample_rate = (oversample == UART_OVERSAMPLE_8) ? 8U : 16U;

    /* Compute USARTDIV with millesimal precision to avoid float.
     * USARTDIV × 1000 = (pclk_freq × 1000) / (oversample_rate × baudrate)
     */
    usartdiv_times_1000 = ((uint64_t)pclk_freq * 1000ULL) /
                          ((uint64_t)oversample_rate * (uint64_t)baudrate);

    mantissa = (uint32_t)(usartdiv_times_1000 / 1000ULL);
    /* Fractional part in thousandths: remainder / 1000 = fraction / oversample_rate
     * → fraction = (remainder × oversample_rate + 500) / 1000  (round to nearest)
     */
    fraction = (uint32_t)(((usartdiv_times_1000 % 1000ULL) *
                           (uint64_t)oversample_rate + 500ULL) / 1000ULL);

    /* Divide fraction by oversample_rate/16 to fit into 4-bit field.
     * For ×16: fraction is already 0–15 (4 bits).
     * For ×8:  fraction is 0–7 (3 bits), pad LSB with 0. */
    fraction_div = (oversample == UART_OVERSAMPLE_8) ? 2U : 1U;
    fraction /= fraction_div;

    /* Assemble BRR: mantissa in bits[15:4], fraction in bits[3:0] */
    return ((mantissa & 0xFFFU) << 4U) | (fraction & 0xFU);
}

/* ──────────────────────────────────────────────
 * uart_init — configure and enable a UART
 *
 * Knowledge: UART register configuration for data format.
 *
 * CR1 setup: oversample mode, word length, parity, TX/RX enable.
 * CR2 setup: stop bits.
 * CR3 setup: hardware flow control (RTS/CTS).
 *
 * The UE (USART Enable) bit must be set LAST, after all configuration,
 * because some registers are write-protected when UE=1.
 * ────────────────────────────────────────────── */

void uart_init(const uart_config_t *config)
{
    uart_regs_t *regs;
    uint32_t cr1_val;
    uint32_t cr2_val;
    uint32_t cr3_val;

    if (config == NULL) {
        return;
    }

    regs = uart_get_regs(config->uart_index);
    if (regs == NULL) {
        return;
    }

    /* Disable UART before configuration */
    regs->CR1 &= ~UART_CR1_UE;

    /* Configure baud rate */
    regs->BRR = (uint16_t)uart_calculate_brr(config->pclk_freq, config->baudrate,
                                              config->oversample);

    /* CR1: oversample, word length, parity, TX/RX enable */
    cr1_val = 0;
    if (config->oversample == UART_OVERSAMPLE_8) {
        cr1_val |= (1U << 15);  /* OVER8 bit */
    }
    if (config->wordlen == UART_WORD_9BITS) {
        cr1_val |= UART_CR1_M;
    }
    if (config->parity != UART_PARITY_NONE) {
        cr1_val |= UART_CR1_PCE;
        if (config->parity == UART_PARITY_ODD) {
            cr1_val |= UART_CR1_PS;
        }
    }
    cr1_val |= UART_CR1_TE;
    cr1_val |= UART_CR1_RE;
    regs->CR1 = cr1_val;

    /* CR2: stop bits (bits [13:12]) */
    cr2_val = regs->CR2 & ~(0x3U << 12U);
    cr2_val |= (((uint32_t)config->stopbits & 0x3U) << 12U);
    regs->CR2 = cr2_val;

    /* CR3: flow control */
    cr3_val = regs->CR3 & ~(0x3U << 8U);  /* Clear CTSE, RTSE */
    if (config->flow_control == UART_FLOW_CTS || config->flow_control == UART_FLOW_RTS_CTS) {
        cr3_val |= (1U << 9U);  /* CTSE */
    }
    if (config->flow_control == UART_FLOW_RTS || config->flow_control == UART_FLOW_RTS_CTS) {
        cr3_val |= (1U << 8U);  /* RTSE */
    }
    regs->CR3 = cr3_val;

    /* Enable UART */
    regs->CR1 |= UART_CR1_UE;
}

/* ──────────────────────────────────────────────
 * uart_send_byte — blocking single-byte transmit
 *
 * Knowledge: TXE vs TC distinction.
 *
 * TXE (Transmit data register Empty): set when DR is ready to accept
 *   the next byte. The shift register may still be transmitting
 *   the previous byte.
 *
 * TC (Transmission Complete): set when the shift register is empty
 *   AND the stop bit has been sent. This means the line is idle.
 *
 * For back-to-back byte transmission, poll TXE (not TC) — by the
 * time DR is written, the shift register has likely emptied and
 * will immediately start the new byte (no gap between frames).
 *
 * For RS-485 half-duplex direction control, poll TC before
 * switching the transceiver from TX to RX mode, because the
 * line must be idle before releasing the driver.
 * ────────────────────────────────────────────── */

void uart_send_byte(uint8_t uart_index, uint8_t byte)
{
    uart_regs_t *regs = uart_get_regs(uart_index);
    if (regs == NULL) {
        return;
    }

    /* Wait for TXE: Transmit Data Register Empty */
    while ((regs->SR & UART_SR_TXE) == 0U) {
        /* Spin-wait. In a real RTOS, this would yield the CPU.
         * For bare-metal, this is acceptable for short strings. */
    }

    /* Write data to DR. On STM32, DR[8:0] is used for TX. */
    regs->DR = (uint32_t)byte;
}

/* ──────────────────────────────────────────────
 * uart_receive_byte — blocking receive with timeout
 *
 * Knowledge: RXNE polling with timeout protection.
 *
 * RXNE (RX Not Empty): set when a complete byte (start + data +
 *   parity + stop) has been received and moved to DR.
 *
 * If RXNE is not cleared by reading DR before the next byte
 * arrives, the ORE (Overrun Error) flag is set and the new
 * byte is lost. DMA is recommended for high-throughput RX.
 *
 * Timeout implementation: polls RXNE in a loop, using the global
 * millisecond tick counter (systick_get_count). This is a
 * coarse but effective timeout mechanism on bare-metal.
 * ────────────────────────────────────────────── */

bool uart_receive_byte(uint8_t uart_index, uint32_t timeout_ms, uint8_t *byte_out)
{
    uart_regs_t *regs;
    uint32_t start_tick;
    uint32_t elapsed;

    if (byte_out == NULL) {
        return false;
    }

    regs = uart_get_regs(uart_index);
    if (regs == NULL) {
        return false;
    }

    start_tick = 0;
    /* Only track timeout if timeout_ms > 0 */
    if (timeout_ms > 0U) {
        start_tick = systick_get_count();
    }

    for (;;) {
        /* Check for received byte */
        if (regs->SR & UART_SR_RXNE) {
            /* Read DR. On STM32, DR[8:0] contains received data.
             * Reading DR clears RXNE. */
            *byte_out = (uint8_t)(regs->DR & 0xFFU);
            return true;
        }

        /* Check for errors that need clearing */
        if (regs->SR & (UART_SR_ORE | UART_SR_FE | UART_SR_NF | UART_SR_PE)) {
            /* Read SR then DR to clear error flags */
            (void)regs->SR;
            (void)regs->DR;
        }

        /* Timeout check */
        if (timeout_ms > 0U) {
            elapsed = systick_get_count() - start_tick;
            if (elapsed >= timeout_ms) {
                return false;  /* Timeout */
            }
        } else if (timeout_ms == 0U) {
            /* Poll-once mode: check RXNE once and return */
            return false;
        }
    }
}

/* ──────────────────────────────────────────────
 * uart_send_string — transmit null-terminated string
 *
 * Knowledge: Character-by-character output loop.
 *
 * Blocking: the caller is stalled until the entire string is sent.
 * For long strings at low baud rates (e.g., 9600 baud → ~1 ms/char),
 * this can block the main loop for seconds. Solution: interrupt-
 * driven TX using TXE interrupt and a ring buffer.
 * ────────────────────────────────────────────── */

void uart_send_string(uint8_t uart_index, const char *str)
{
    if (str == NULL) {
        return;
    }
    while (*str != '\0') {
        uart_send_byte(uart_index, (uint8_t)(*str));
        str++;
    }
}

/* ──────────────────────────────────────────────
 * uart_send_buffer — transmit buffer of known length
 * ────────────────────────────────────────────── */

void uart_send_buffer(uint8_t uart_index, const uint8_t *data, size_t len)
{
    size_t i;
    if (data == NULL) {
        return;
    }
    for (i = 0; i < len; i++) {
        uart_send_byte(uart_index, data[i]);
    }
}

/* ──────────────────────────────────────────────
 * Helper: reverse a string in place
 * ────────────────────────────────────────────── */

static void reverse_string(char *s, size_t len)
{
    size_t i, j;
    char tmp;
    for (i = 0, j = len - 1; i < j; i++, j--) {
        tmp = s[i];
        s[i] = s[j];
        s[j] = tmp;
    }
}

/* ──────────────────────────────────────────────
 * Helper: integer to ASCII (unsigned, any base 2–16)
 *
 * Knowledge: itoa (integer to ASCII) algorithm.
 * Repeatedly divides by the base, collecting remainders as digits.
 * The result is reversed because the division produces LSB first.
 *
 * Complexity: O(log_base(value))
 * ────────────────────────────────────────────── */

static size_t u32_to_str(uint32_t value, char *buf, uint8_t base, bool uppercase)
{
    size_t len = 0;
    const char digits_lower[] = "0123456789abcdef";
    const char digits_upper[] = "0123456789ABCDEF";
    const char *digits = uppercase ? digits_upper : digits_lower;

    if (value == 0) {
        buf[0] = '0';
        return 1;
    }

    while (value > 0) {
        buf[len++] = digits[value % base];
        value /= base;
    }

    reverse_string(buf, len);
    return len;
}

/* ──────────────────────────────────────────────
 * Helper: signed integer to ASCII
 *
 * Knowledge: Two's complement negation for negative numbers.
 * For the most negative integer (−2^31), negating overflows,
 * so we handle it as unsigned and prepend the minus sign.
 * ────────────────────────────────────────────── */

static size_t i32_to_str(int32_t value, char *buf)
{
    uint32_t abs_val;
    size_t len;

    if (value >= 0) {
        return u32_to_str((uint32_t)value, buf, 10, false);
    }

    /* Negative: negate absolute value */
    buf[0] = '-';
    abs_val = (uint32_t)(-(int64_t)value);  /* Cast to int64 to avoid overflow on INT32_MIN */
    len = u32_to_str(abs_val, buf + 1, 10, false);
    return len + 1;  /* +1 for the minus sign */
}

/* ──────────────────────────────────────────────
 * uart_printf — minimal formatted UART output
 *
 * Knowledge: Variadic argument parsing with va_list.
 *
 * Supported format specifiers:
 *   %c  — single character
 *   %s  — null-terminated string
 *   %d  — signed decimal integer
 *   %u  — unsigned decimal integer
 *   %x  — lowercase hexadecimal
 *   %X  — uppercase hexadecimal
 *   %p  — pointer (formatted as 0xXXXXXXXX with uppercase hex)
 *   %%  — literal percent sign
 *
 * Unsupported (by design — use a full printf library if needed):
 *   %f, %e, %g (floating point — large code size)
 *   Width/precision (%8d, %04x)
 *   Length modifiers (%ld, %lld)
 *
 * Stack usage: char buf[12] for the largest integer (10 digits + sign + NUL).
 *
 * Reference: Kernighan & Ritchie §7.2 — Formatted Output
 * ────────────────────────────────────────────── */

void uart_printf(uint8_t uart_index, const char *fmt, ...)
{
    va_list args;
    char c;
    const char *s;
    int32_t d_val;
    uint32_t u_val;
    void *p_val;
    char buf[12];  /* Enough for +2^31-1 = "2147483647" + NUL */
    size_t buf_len;

    if (fmt == NULL) {
        return;
    }

    va_start(args, fmt);

    while (*fmt != '\0') {
        if (*fmt != '%') {
            uart_send_byte(uart_index, (uint8_t)(*fmt));
            fmt++;
            continue;
        }

        /* Process format specifier */
        fmt++;  /* Skip '%' */

        switch (*fmt) {
        case 'c':
            c = (char)va_arg(args, int);  /* char promoted to int in varargs */
            uart_send_byte(uart_index, (uint8_t)c);
            break;

        case 's':
            s = va_arg(args, const char *);
            if (s == NULL) {
                uart_send_string(uart_index, "(null)");
            } else {
                uart_send_string(uart_index, s);
            }
            break;

        case 'd':
            d_val = va_arg(args, int32_t);
            buf_len = i32_to_str(d_val, buf);
            uart_send_buffer(uart_index, (const uint8_t *)buf, buf_len);
            break;

        case 'u':
            u_val = va_arg(args, uint32_t);
            buf_len = u32_to_str(u_val, buf, 10, false);
            uart_send_buffer(uart_index, (const uint8_t *)buf, buf_len);
            break;

        case 'x':
            u_val = va_arg(args, uint32_t);
            buf_len = u32_to_str(u_val, buf, 16, false);
            uart_send_buffer(uart_index, (const uint8_t *)buf, buf_len);
            break;

        case 'X':
            u_val = va_arg(args, uint32_t);
            buf_len = u32_to_str(u_val, buf, 16, true);
            uart_send_buffer(uart_index, (const uint8_t *)buf, buf_len);
            break;

        case 'p':
            p_val = va_arg(args, void *);
            uart_send_string(uart_index, "0x");
            buf_len = u32_to_str((uint32_t)(uintptr_t)p_val, buf, 16, true);
            /* Pad pointer to 8 hex digits for 32-bit addresses */
            while (buf_len < 8U) {
                uart_send_byte(uart_index, '0');
                buf_len++;
            }
            uart_send_buffer(uart_index, (const uint8_t *)buf, buf_len);
            break;

        case '%':
            uart_send_byte(uart_index, '%');
            break;

        case '\0':
            /* % at end of string → ignore */
            va_end(args);
            return;

        default:
            /* Unknown specifier → output raw (including the %) */
            uart_send_byte(uart_index, '%');
            uart_send_byte(uart_index, (uint8_t)(*fmt));
            break;
        }

        if (*fmt != '\0') {
            fmt++;
        }
    }

    va_end(args);
}

/* ──────────────────────────────────────────────
 * uart_get_status — decode SR flags
 *
 * Knowledge: UART error conditions and their causes.
 *
 * ORE (Overrun Error): data arrived while DR was full (previous byte
 *   not read). Indicates the CPU is not keeping up with the RX rate.
 *   Solution: higher priority for UART RX ISR, or use DMA.
 *
 * NF (Noise Flag): the three sample values (at 8th, 9th, 10th bit
 *   period in ×16 mode) did not agree. Indicates a noisy channel.
 *
 * FE (Framing Error): the stop bit was not logic-1. Possible causes:
 *   baud rate mismatch, clock drift, noise, or the transmitter sent
 *   a break condition (line held low for > 1 frame).
 *
 * PE (Parity Error): the computed parity does not match the received
 *   parity bit. Indicates a bit error in the data or parity bit.
 *
 * IDLE: the RX line has been idle (high) for at least one full frame
 *   time. Used in Modbus RTU to detect end-of-frame (≥3.5 char times).
 *
 * TXE vs TC: see uart_send_byte documentation.
 * ────────────────────────────────────────────── */

void uart_get_status(uint8_t uart_index, uart_status_t *status)
{
    uart_regs_t *regs;
    uint32_t sr;

    if (status == NULL) {
        return;
    }

    regs = uart_get_regs(uart_index);
    if (regs == NULL) {
        memset(status, 0, sizeof(*status));
        return;
    }

    sr = regs->SR;

    status->tx_empty       = (sr & UART_SR_TXE) != 0U;
    status->tx_complete    = (sr & UART_SR_TC) != 0U;
    status->rx_ready       = (sr & UART_SR_RXNE) != 0U;
    status->idle_line      = (sr & UART_SR_IDLE) != 0U;
    status->overrun_error  = (sr & UART_SR_ORE) != 0U;
    status->noise_error    = (sr & UART_SR_NF) != 0U;
    status->framing_error  = (sr & UART_SR_FE) != 0U;
    status->parity_error   = (sr & UART_SR_PE) != 0U;
}

/* ──────────────────────────────────────────────
 * uart_set_baudrate — change baud rate at runtime
 *
 * Knowledge: Safe run-time peripheral reconfiguration.
 *
 * UE must be cleared before writing BRR because BRR is
 * write-protected when UE=1 (to prevent glitches during
 * active transmission).
 * ────────────────────────────────────────────── */

void uart_set_baudrate(uint8_t uart_index, uint32_t baudrate, uint32_t pclk_freq)
{
    uart_regs_t *regs = uart_get_regs(uart_index);
    uint32_t cr1_save;

    if (regs == NULL) {
        return;
    }

    /* Save and disable UE */
    cr1_save = regs->CR1;
    regs->CR1 = cr1_save & ~UART_CR1_UE;

    /* Write new baud rate. Assume ×16 oversample (original setting preserved in CR1). */
    regs->BRR = (uint16_t)uart_calculate_brr(pclk_freq, baudrate,
                                              (cr1_save & (1U << 15)) ?
                                              UART_OVERSAMPLE_8 : UART_OVERSAMPLE_16);

    /* Restore UE */
    regs->CR1 = cr1_save;
}

/* ──────────────────────────────────────────────
 * uart_ringbuf_init — initialise ring buffer
 *
 * Knowledge: Circular buffer data structure.
 *
 * Ring buffer properties:
 *   - Fixed size of UART_RXBUF_SIZE (must be power of 2).
 *   - head/tail indices advance modulo SIZE.
 *   - Empty: head == tail.
 *   - Full:  (head + 1) % SIZE == tail → one slot wasted.
 *   - Single-producer (ISR writes head), single-consumer
 *     (main loop reads tail) → lock-free when head and tail
 *     are accessed by only one side each.
 *
 * Reference: Tanenbaum §2.3 — Producer-consumer problem
 * ────────────────────────────────────────────── */

void uart_ringbuf_init(uart_ringbuf_t *rb)
{
    if (rb == NULL) {
        return;
    }
    memset(rb->buffer, 0, sizeof(rb->buffer));
    rb->head = 0;
    rb->tail = 0;
    rb->overflow = 0;
}

/* ──────────────────────────────────────────────
 * uart_ringbuf_put — ISR-side producer
 *
 * Called from UART RX interrupt handler.
 * Must be fast (no blocking, no loops), as ISRs should be short.
 * ────────────────────────────────────────────── */

bool uart_ringbuf_put(uart_ringbuf_t *rb, uint8_t byte)
{
    uint8_t next_head;

    if (rb == NULL) {
        return false;
    }

    next_head = (rb->head + 1U) & UART_RXBUF_MASK;

    if (next_head == rb->tail) {
        /* Buffer is full — overflow */
        rb->overflow = 1;
        return false;
    }

    rb->buffer[rb->head] = byte;
    rb->head = next_head;
    return true;
}

/* ──────────────────────────────────────────────
 * uart_ringbuf_get — main-loop consumer
 *
 * Called from application code (non-ISR context).
 * Accessing tail from non-ISR context is safe because the ISR
 * only reads tail and writes head. Brief interrupt disable
 * prevents read inconsistency if arch doesn't guarantee
 * single-instruction 8-bit read atomicity.
 * ────────────────────────────────────────────── */

bool uart_ringbuf_get(uart_ringbuf_t *rb, uint8_t *byte_out)
{
    if (rb == NULL || byte_out == NULL) {
        return false;
    }

    if (rb->head == rb->tail) {
        /* Buffer is empty */
        return false;
    }

    *byte_out = rb->buffer[rb->tail];
    rb->tail = (rb->tail + 1U) & UART_RXBUF_MASK;
    return true;
}

/* ──────────────────────────────────────────────
 * uart_ringbuf_available — count unread bytes
 *
 * Safe to call from any context because head and tail
 * are monotonic and the buffer size is small.
 * ────────────────────────────────────────────── */

uint8_t uart_ringbuf_available(const uart_ringbuf_t *rb)
{
    if (rb == NULL) {
        return 0;
    }
    if (rb->head >= rb->tail) {
        return rb->head - rb->tail;
    } else {
        return (uint8_t)(UART_RXBUF_SIZE - (rb->tail - rb->head));
    }
}

/* ──────────────────────────────────────────────
 * uart_error_clear — clear all error flags
 * ────────────────────────────────────────────── */

void uart_error_clear(uint8_t uart_index)
{
    uart_regs_t *regs = uart_get_regs(uart_index);
    uint32_t sr;
    if (regs == NULL) {
        return;
    }
    /* Read SR to get error flags, then read DR to clear them */
    sr = regs->SR;
    (void)sr;
    (void)regs->DR;  /* Reading DR clears ORE, NF, FE, PE */
}
