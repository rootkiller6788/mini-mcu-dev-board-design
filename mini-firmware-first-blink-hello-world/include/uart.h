/*
 * uart.h — Universal Asynchronous Receiver/Transmitter (UART/USART)
 *
 * Knowledge Mapping (SKILL.md L1-L6):
 *   L1 Definitions: Baud rate, start bit, stop bit, parity,
 *                   data frame, full-duplex, half-duplex, NRZ encoding
 *   L2 Concepts:    Asynchronous serial, oversampling (×8/×16),
 *                   clock recovery from start-bit edge, FIFO buffering
 *   L3 Math:        Baud rate = f_PCLK / (8 × (2 − OVER8) × USARTDIV)
 *                   USARTDIV = f_PCLK / (baud × oversampling_rate)
 *                   Sample times: 3-of-5 majority voting at sample #8,9,10
 *   L4 Laws:        Nyquist: max symbol rate ≤ f_clk / (16 × log2(M))
 *                   Shannon: C = B × log2(1 + SNR)
 *   L5 Algorithms:  Ring-buffer RX/TX, baud-rate auto-detection,
 *                   LIN break detection, RS-485 direction control
 *   L6 Problems:    "Hello World" over serial, GPS NMEA parsing,
 *                   Modbus RTU framing (silent-interval detection)
 *
 * Course Mapping:
 *   MIT 6.450 Digital Communications §2 — baseband signaling, NRZ
 *   Stanford EE359 Wireless — serial link fundamentals
 *   Berkeley EE16B — UART as first communication protocol
 *   Proakis & Salehi §4.3 — PAM/NRZ signaling
 *
 * Reference: STM32F4xx Reference Manual RM0090, §30 USART
 */

#ifndef UART_H
#define UART_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "gpio.h"

/* ──────────────────────────────────────────────
 * L1 Definition: UART Data Word Length
 *
 * 8 data bits (7-bit ASCII + optional parity, or 8-bit raw).
 * 9-bit mode used in multiprocessor addressing (LIN, RS-485).
 * ────────────────────────────────────────────── */
typedef enum {
    UART_WORD_8BITS = 0x0,
    UART_WORD_9BITS = 0x1
} uart_wordlen_t;

/* L1 Definition: Stop bits.
 * 0.5 stop bits: tight timing for high throughput, rare.
 * 1 stop bit:    standard for most applications.
 * 1.5 stop bits: used in 5-bit character mode (legacy teletype).
 * 2 stop bits:   extra margin for noisy channels or slow receivers.
 */
typedef enum {
    UART_STOP_1   = 0x0,  /* 1 stop bit — standard */
    UART_STOP_0_5 = 0x1,  /* 0.5 stop bit */
    UART_STOP_2   = 0x2,  /* 2 stop bits */
    UART_STOP_1_5 = 0x3   /* 1.5 stop bits */
} uart_stopbits_t;

/* L1 Definition: Parity.
 * Even parity: total number of 1's (including parity bit) is even.
 * Odd parity:  total number of 1's is odd.
 * Parity detects all single-bit errors and any odd number of bit errors.
 * Fails on even number of bit flips (e.g., burst noise inverting 2 bits).
 * Reference: Hamming, "Error Detecting and Error Correcting Codes" (1950)
 */
typedef enum {
    UART_PARITY_NONE = 0x0,
    UART_PARITY_EVEN = 0x1,
    UART_PARITY_ODD  = 0x2
    /* 0x3 reserved */
} uart_parity_t;

/* L1 Definition: Oversampling mode.
 * OVER8=0: ×16 oversampling → 16 clock ticks per bit.
 *          Majority vote on samples 8, 9, 10 (of 16).
 * OVER8=1: ×8 oversampling → 8 clock ticks per bit.
 *          Majority vote on samples 4, 5, 6 (of 8).
 * ×8 doubles the achievable baud rate at the cost of reduced
 * tolerance to clock frequency mismatch.
 */
typedef enum {
    UART_OVERSAMPLE_16 = 0x0,
    UART_OVERSAMPLE_8  = 0x1
} uart_oversample_t;

/* L1 Definition: Hardware flow control.
 * RTS (Ready To Send): output, MCU signals it can receive data.
 * CTS (Clear To Send): input, peer signals it can receive data.
 * Uses separate GPIO lines (AF7/AF8 on STM32), preventing buffer
 * overflow without software intervention.
 * Reference: RS-232 standard (EIA/TIA-232-F) §4 — flow control
 */
typedef enum {
    UART_FLOW_NONE     = 0x0,
    UART_FLOW_RTS      = 0x1,
    UART_FLOW_CTS      = 0x2,
    UART_FLOW_RTS_CTS  = 0x3
} uart_flow_t;

/* ──────────────────────────────────────────────
 * UART Register Map (typedef-based, memory-mapped)
 *
 * Representative of STM32 USART peripheral.
 * Each USART occupies 0x400 bytes of address space.
 * ────────────────────────────────────────────── */
typedef struct {
    volatile uint32_t SR;     /* 0x00: Status Register (TXE, TC, RXNE, etc.)   */
    volatile uint32_t DR;     /* 0x04: Data Register (read=RX, write=TX)       */
    volatile uint32_t BRR;    /* 0x08: Baud Rate Register                      */
    volatile uint32_t CR1;    /* 0x0C: Control Register 1 (UE, M, PCE, etc.)   */
    volatile uint32_t CR2;    /* 0x10: Control Register 2 (STOP, CLKEN, etc.)  */
    volatile uint32_t CR3;    /* 0x14: Control Register 3 (CTSE, RTSE, etc.)   */
    volatile uint32_t GTPR;   /* 0x18: Guard Time and Prescaler (IrDA/Smartcard)*/
} uart_regs_t;

/* Status Register (SR) bit definitions */
#define UART_SR_TXE    (1U << 7)   /* Transmit Data Register Empty            */
#define UART_SR_TC     (1U << 6)   /* Transmission Complete                   */
#define UART_SR_RXNE   (1U << 5)   /* Read Data Register Not Empty            */
#define UART_SR_IDLE   (1U << 4)   /* Idle line detected                      */
#define UART_SR_ORE    (1U << 3)   /* Overrun Error (RX data lost)            */
#define UART_SR_NF     (1U << 2)   /* Noise Flag (sampled value ≠ majority)   */
#define UART_SR_FE     (1U << 1)   /* Framing Error (invalid stop bit)        */
#define UART_SR_PE     (1U << 0)   /* Parity Error                            */

/* Control Register 1 (CR1) bit definitions */
#define UART_CR1_UE    (1U << 13)  /* USART Enable                            */
#define UART_CR1_M     (1U << 12)  /* Word length (0=8bit, 1=9bit)            */
#define UART_CR1_PCE   (1U << 10)  /* Parity Control Enable                   */
#define UART_CR1_PS    (1U << 9)   /* Parity Selection (0=even, 1=odd)        */
#define UART_CR1_TE    (1U << 3)   /* Transmitter Enable                      */
#define UART_CR1_RE    (1U << 2)   /* Receiver Enable                         */
#define UART_CR1_RXNEIE (1U << 5)  /* RXNE Interrupt Enable                   */
#define UART_CR1_TXEIE  (1U << 7)  /* TXE Interrupt Enable                    */

/* ──────────────────────────────────────────────
 * UART Configuration Structure
 * ────────────────────────────────────────────── */
typedef struct {
    uint8_t            uart_index;     /* USART1=0, USART2=1, ..., USART6=5   */
    uint32_t           baudrate;       /* Target baud rate (9600, 115200, etc.)*/
    uart_wordlen_t     wordlen;        /* 8 or 9 data bits                     */
    uart_stopbits_t    stopbits;       /* Stop bit count                       */
    uart_parity_t      parity;         /* Even/odd/none                        */
    uart_oversample_t  oversample;     /* ×8 or ×16                            */
    uart_flow_t        flow_control;   /* Hardware flow control mode           */
    uint32_t           pclk_freq;      /* Peripheral clock frequency (APB bus) */
} uart_config_t;

/* ──────────────────────────────────────────────
 * Ring Buffer (L5 Algorithm: circular queue)
 *
 * Decouples ISR-level byte production from main-loop consumption.
 * Power-of-2 size allows modulo by mask (no expensive % operation).
 *
 * Capacity: (size - 1) usable slots to distinguish empty vs full.
 * Empty: head == tail.  Full: (head + 1) % size == tail.
 * ────────────────────────────────────────────── */
#define UART_RXBUF_SIZE       256    /* Must be power of 2 for fast modulo */
#define UART_RXBUF_MASK       (UART_RXBUF_SIZE - 1)

typedef struct {
    uint8_t         buffer[UART_RXBUF_SIZE];
    volatile uint8_t head;    /* ISR writes here (producer index)     */
    volatile uint8_t tail;    /* Application reads here (consumer)    */
    volatile uint8_t overflow;/* Set when data is lost due to full buffer */
} uart_ringbuf_t;

/* ──────────────────────────────────────────────
 * UART Status Structure
 *
 * Captures instantaneous UART state for diagnostics.
 * L1 Definitions: each field maps to a UART_SR bit.
 * ────────────────────────────────────────────── */
typedef struct {
    bool tx_empty;        /* TX data register empty (ready for next byte)    */
    bool tx_complete;     /* TX shift register empty (all bits sent)         */
    bool rx_ready;        /* RX data register has byte                      */
    bool idle_line;       /* RX line has been idle for 1 frame               */
    bool overrun_error;   /* New RX byte arrived before previous was read    */
    bool noise_error;     /* Sampled bit values did not agree (majority fail) */
    bool framing_error;   /* Stop bit was not logic-1                        */
    bool parity_error;    /* Calculated parity ≠ received parity bit         */
} uart_status_t;

/* ──────────────────────────────────────────────
 * UART API
 * ────────────────────────────────────────────── */

/*
 * uart_init — configure and enable a UART peripheral
 *
 * Sets up baud rate (BRR register), data format (CR1-CR3), enables
 * transmitter and receiver. GPIO alternate function configuration
 * must be done separately for TX/RX pins.
 *
 * Baud rate formula (×16 oversample):
 *   BRR = pclk_freq / baudrate
 *   Fractional part: DIV_Fraction = ((fraction * 16) + 0.5) integer
 *
 * @param config: UART configuration descriptor
 *
 * Complexity: O(1)
 * Reference: STM32F4 Ref Manual §30.6.4 — Baud Rate Generation
 */
void uart_init(const uart_config_t *config);

/*
 * uart_send_byte — transmit a single byte (blocking)
 *
 * Waits until TXE (Transmit Data Register Empty) is set, then writes
 * the data register. Does NOT wait for TC (Transmission Complete),
 * so the caller can queue the next byte immediately.
 *
 * TXE vs TC: TXE=1 means the data register is free (next byte can be
 *            written immediately). TC=1 means the shift register is
 *            empty (all bits including stop bit have been sent).
 *
 * @param uart_index: UART peripheral index
 * @param byte:       data to send
 *
 * Complexity: O(1) average, O(busy-wait) worst case
 */
void uart_send_byte(uint8_t uart_index, uint8_t byte);

/*
 * uart_receive_byte — receive a single byte (blocking with timeout)
 *
 * Waits for RXNE (RX Not Empty) or until the timeout expires.
 * On overrun (ORE), reads DR and SR to clear the flag.
 *
 * @param uart_index: UART peripheral index
 * @param timeout_ms: maximum wait in milliseconds (0 = poll once)
 * @param byte_out:   pointer to store received byte
 * @return true if byte received, false on timeout or error
 *
 * Complexity: O(timeout_ms / poll_period)
 */
bool uart_receive_byte(uint8_t uart_index, uint32_t timeout_ms, uint8_t *byte_out);

/*
 * uart_send_string — transmit a null-terminated string (blocking)
 *
 * Calls uart_send_byte for each character until the NUL terminator.
 * For long strings, consider using the interrupt-driven ring buffer
 * to avoid blocking the main loop.
 *
 * @param uart_index: UART peripheral index
 * @param str: pointer to null-terminated string
 *
 * Complexity: O(n) where n = string length
 */
void uart_send_string(uint8_t uart_index, const char *str);

/*
 * uart_send_buffer — transmit a buffer of known length (blocking)
 *
 * @param uart_index: UART peripheral index
 * @param data:       pointer to data buffer
 * @param len:        number of bytes to send
 *
 * Complexity: O(len)
 */
void uart_send_buffer(uint8_t uart_index, const uint8_t *data, size_t len);

/*
 * uart_printf — minimal printf-to-UART
 *
 * Supports: %c (char), %s (string), %d (signed decimal),
 *           %u (unsigned decimal), %x/%X (hex), %p (pointer).
 * Does NOT support floating-point (%f), width/precision, or * format.
 *
 * @param uart_index: UART peripheral index
 * @param fmt:        format string
 * @param ...:        variable arguments
 *
 * Complexity: O(n) where n = output length
 *
 * Implementation: recursive itoa + reverse for integers,
 *                 with sign handling for negative values.
 */
void uart_printf(uint8_t uart_index, const char *fmt, ...);

/*
 * uart_get_status — read all status flags atomically
 *
 * Reads SR once and decodes all flags. Useful for error handling
 * (logging which error occurred) and for checking TX readiness
 * before starting a new transmission.
 *
 * @param uart_index: UART peripheral index
 * @param status:     pointer to status structure to fill
 */
void uart_get_status(uint8_t uart_index, uart_status_t *status);

/*
 * uart_set_baudrate — change baud rate at runtime
 *
 * Disables UART, programs new BRR value, re-enables.
 * Must ensure TX is complete before calling.
 *
 * @param uart_index: UART peripheral index
 * @param baudrate:   new baud rate
 * @param pclk_freq:  peripheral clock frequency
 *
 * Complexity: O(1)
 */
void uart_set_baudrate(uint8_t uart_index, uint32_t baudrate, uint32_t pclk_freq);

/*
 * uart_ringbuf_init — reset a ring buffer to empty state
 *
 * @param rb: pointer to ring buffer structure
 */
void uart_ringbuf_init(uart_ringbuf_t *rb);

/*
 * uart_ringbuf_put — insert a byte into the ring buffer
 *
 * Called from UART RX interrupt handler (ISR context).
 * If the buffer is full, sets overflow flag and discards the byte.
 *
 * @param rb:  ring buffer
 * @param byte: byte to store
 * @return true if byte was stored, false on overflow
 *
 * Complexity: O(1)
 */
bool uart_ringbuf_put(uart_ringbuf_t *rb, uint8_t byte);

/*
 * uart_ringbuf_get — remove a byte from the ring buffer
 *
 * Called from main loop (non-ISR context).
 * Disables the UART interrupt briefly to avoid race on tail.
 *
 * @param rb:       ring buffer
 * @param byte_out: pointer to store retrieved byte
 * @return true if a byte was available, false if buffer is empty
 *
 * Complexity: O(1)
 */
bool uart_ringbuf_get(uart_ringbuf_t *rb, uint8_t *byte_out);

/*
 * uart_ringbuf_available — number of bytes in the ring buffer
 *
 * @param rb: ring buffer
 * @return count of unread bytes (0 to UART_RXBUF_SIZE - 1)
 */
uint8_t uart_ringbuf_available(const uart_ringbuf_t *rb);

/*
 * uart_calculate_brr — compute BRR register value from baud rate
 *
 * Implements the hardware baud-rate formula:
 *   For OVER8=0 (×16):  BRR = pclk_freq / (16 × baud)  (integer part)
 *     DIV_Fraction = round(((pclk_freq / (16 × baud)) - integer) * 16)
 *   For OVER8=1 (×8):   BRR = pclk_freq / (8 × baud)
 *     DIV_Fraction = round(((pclk_freq / (8 × baud)) - integer) * 8)
 *
 * BRR format: bits[15:4] = DIV_Mantissa, bits[3:0] = DIV_Fraction
 *
 * @param pclk_freq:  peripheral clock (APB bus) frequency in Hz
 * @param baudrate:   desired baud rate
 * @param oversample: ×8 or ×16 mode
 * @return 16-bit BRR register value
 *
 * Complexity: O(1)
 * Reference: STM32F4 §30.6.4 equation for USARTDIV
 */
uint32_t uart_calculate_brr(uint32_t pclk_freq, uint32_t baudrate,
                            uart_oversample_t oversample);

/*
 * uart_error_clear — clear all error flags
 *
 * Reads SR then DR to clear ORE, NF, FE, PE flags.
 * Must be called after an error is detected before further RX.
 *
 * @param uart_index: UART peripheral index
 */
void uart_error_clear(uint8_t uart_index);

/*
 * uart_get_regs — return pointer to UART register map
 *
 * @param uart_index: USART index (0..5)
 * @return pointer to uart_regs_t or NULL if invalid
 */
uart_regs_t *uart_get_regs(uint8_t uart_index);

#endif /* UART_H */
