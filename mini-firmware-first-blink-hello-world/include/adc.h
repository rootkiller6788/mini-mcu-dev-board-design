/*
 * adc.h — Analog-to-Digital Converter for ARM Cortex-M MCUs
 *
 * Knowledge Mapping (SKILL.md L1-L6):
 *   L1 Definitions: ADC resolution (bits), ENOB, sampling rate (SPS),
 *                   conversion time, reference voltage (V_ref),
 *                   quantization error, LSB = V_ref / 2^N
 *   L2 Concepts:    SAR (Successive Approximation Register) ADC,
 *                   sigma-delta ADC, pipelined ADC, sample-and-hold,
 *                   aliasing (Nyquist: f_sample > 2 × f_signal)
 *   L3 Math:        Quantization noise power: σ²_q = (LSB²) / 12
 *                   SQNR (dB) = 6.02 × N + 1.76  (for full-scale sine)
 *                   ENOB = (SINAD_dB − 1.76) / 6.02
 *                   DNL (Differential Nonlinearity): actual step − ideal LSB
 *                   INL (Integral Nonlinearity): max deviation from ideal line
 *   L4 Laws:        Nyquist-Shannon sampling theorem
 *                   Johnson-Nyquist thermal noise: v_n = √(4kTRB)
 *   L5 Algorithms:  Oversampling + averaging: N_oversample = 4^(extra_bits)
 *                   Dithering to decorrelate quantization noise
 *                   Running average filter for sensor noise reduction
 *   L6 Problems:    Reading a potentiometer (voltage divider),
 *                   thermistor temperature measurement (Steinhart-Hart),
 *                   current sensing via shunt resistor (Ohm's law)
 *
 * Course Mapping:
 *   MIT 6.301 Solid State Circuits — ADC architectures
 *   Berkeley EE105 — Data converters
 *   Sedra & Smith §10 — A/D and D/A converters
 *   Proakis DSP §6 — Quantization effects
 *
 * Reference: STM32F4xx Reference Manual RM0090 §13 ADC
 */

#ifndef ADC_H
#define ADC_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* ──────────────────────────────────────────────
 * L1 Definition: ADC Resolution
 *
 * STM32F4 ADC: 12-bit successive approximation (SAR).
 * Range: 0 to 4095 (2¹² − 1).
 * LSB = V_ref / 4096. For V_ref = 3.3 V, LSB ≈ 0.806 mV.
 * ────────────────────────────────────────────── */
#define ADC_RESOLUTION_12BIT  12U
#define ADC_MAX_VALUE_12BIT   4095U
#define ADC_LSB_MV(VREF_MV)   ((VREF_MV) / 4096.0f)

/* ──────────────────────────────────────────────
 * L1 Definition: Conversion modes
 *
 * Single: one conversion per trigger, then stops.
 * Continuous: automatically starts a new conversion after each completion.
 * Scan: converts a sequence of channels in order.
 * Discontinuous: converts one channel per trigger, then stops
 *                (for scan mode: one channel per trigger per group).
 * Injected: higher-priority channel group that interrupts regular conversion.
 * ────────────────────────────────────────────── */
typedef enum {
    ADC_MODE_SINGLE       = 0x0,
    ADC_MODE_CONTINUOUS   = 0x1,
    ADC_MODE_SCAN         = 0x2,
    ADC_MODE_DISCONTINUOUS = 0x3
} adc_mode_t;

/* ──────────────────────────────────────────────
 * L1 Definition: Sampling time
 *
 * The ADC input capacitance (C_s ≈ 4 pF) must charge through
 * the source resistance (R_ain). The required sampling time is:
 *
 *   t_sample ≥ (R_ain + R_adc) × C_s × ln(2^(N+2))
 *            ≈ (R_ain + R_adc) × C_s × (N+2) × ln(2)
 *
 * For R_ain = 10 kΩ, R_adc = 6 kΩ, C_s = 4 pF, N = 12:
 *   t_sample ≥ (16kΩ) × 4 pF × 14 × 0.693 = 0.62 µs
 *
 * Reference: STM32F4 §13.3.3 — Channel selection and sampling time
 * ────────────────────────────────────────────── */
typedef enum {
    ADC_SAMPLE_3_CYCLES   = 0x0,  /* ~0.1 µs @ 30 MHz ADC clock  */
    ADC_SAMPLE_15_CYCLES  = 0x1,  /* ~0.5 µs                     */
    ADC_SAMPLE_28_CYCLES  = 0x2,  /* ~0.9 µs                     */
    ADC_SAMPLE_56_CYCLES  = 0x3,  /* ~1.9 µs                     */
    ADC_SAMPLE_84_CYCLES  = 0x4,  /* ~2.8 µs                     */
    ADC_SAMPLE_112_CYCLES = 0x5,  /* ~3.7 µs                     */
    ADC_SAMPLE_144_CYCLES = 0x6,  /* ~4.8 µs                     */
    ADC_SAMPLE_480_CYCLES = 0x7   /* ~16.0 µs                    */
} adc_sample_time_t;

/* ──────────────────────────────────────────────
 * ADC Register Map (STM32F4 ADC1)
 *
 * ADC1 base: 0x40012000 (APB2 bus)
 * Up to 3 ADCs share trigger and DMA resources.
 * ────────────────────────────────────────────── */
typedef struct {
    volatile uint32_t SR;      /* 0x00: Status Register */
    volatile uint32_t CR1;     /* 0x04: Control Register 1 */
    volatile uint32_t CR2;     /* 0x08: Control Register 2 */
    volatile uint32_t SMPR1;   /* 0x0C: Sample Time Register 1 (ch 10-18) */
    volatile uint32_t SMPR2;   /* 0x10: Sample Time Register 2 (ch 0-9)  */
    volatile uint32_t JOFR[4]; /* 0x14–0x20: Injected Channel Data Offset */
    volatile uint32_t HTR;     /* 0x24: Watchdog Higher Threshold         */
    volatile uint32_t LTR;     /* 0x28: Watchdog Lower Threshold          */
    volatile uint32_t SQR1;    /* 0x2C: Regular Sequence Register 1       */
    volatile uint32_t SQR2;    /* 0x30: Regular Sequence Register 2       */
    volatile uint32_t SQR3;    /* 0x34: Regular Sequence Register 3       */
    volatile uint32_t JSQR;    /* 0x38: Injected Sequence Register        */
    volatile uint32_t JDR[4];  /* 0x3C–0x48: Injected Data Registers     */
    volatile uint32_t DR;      /* 0x4C: Regular Data Register             */
} adc_regs_t;

/* SR bits */
#define ADC_SR_EOC      (1U << 1)    /* Regular channel end of conversion */
#define ADC_SR_JEOC     (1U << 2)    /* Injected channel end of conversion */
#define ADC_SR_OVR      (1U << 5)    /* Overrun */
#define ADC_SR_STRT     (1U << 4)    /* Regular channel start flag */

/* CR2 bits */
#define ADC_CR2_ADON    (1U << 0)    /* A/D Converter ON */
#define ADC_CR2_CONT    (1U << 1)    /* Continuous conversion */
#define ADC_CR2_SWSTART (1U << 30)   /* Start conversion of regular channels */
#define ADC_CR2_EXTEN   (0x3U << 28) /* External trigger enable mask */

/* ──────────────────────────────────────────────
 * ADC Configuration Structure
 * ────────────────────────────────────────────── */
typedef struct {
    uint8_t              adc_index;     /* ADC1=0, ADC2=1, ADC3=2   */
    uint32_t             adc_clk_hz;    /* ADC clock frequency (APB2)*/
    adc_mode_t           mode;          /* Conversion mode           */
    adc_sample_time_t    sample_time;   /* Sampling duration         */
    uint8_t              num_channels;  /* Number of channels (scan) */
    uint8_t              channel_list[16]; /* Channel numbers (0-18)*/
    bool                 dma_enable;    /* Use DMA for data transfer */
    bool                 eoc_interrupt; /* Interrupt on end-of-conv  */
} adc_config_t;

/* ──────────────────────────────────────────────
 * Filter / Averaging Structures (L5 Algorithms)
 * ────────────────────────────────────────────── */

/* L5 Algorithm: Running (moving) average filter.
 * Maintains a circular buffer of the last N samples.
 * y[n] = (1/N) * Σ_{k=0}^{N-1} x[n-k]
 * Frequency response: |H(f)| = |sin(πfN/fs) / (N·sin(πf/fs))|
 * DC gain = 1, first null at f = fs/N. */
#define ADC_AVERAGE_WINDOW_SIZE  16

typedef struct {
    uint16_t buffer[ADC_AVERAGE_WINDOW_SIZE];
    uint8_t  index;
    uint8_t  count;
    uint32_t sum;
} adc_average_filter_t;

/*
 * L5 Algorithm: Exponential moving average (low-pass, single-pole IIR).
 * y[n] = α·x[n] + (1−α)·y[n−1]
 * where α = Δt / (RC + Δt) and cut-off frequency f_c = 1/(2πRC).
 *
 * Time-domain: faster α → faster response, more noise.
 *              slower α → slower response, more smoothing.
 *
 * Reference: Oppenheim & Schafer §6.5 — Digital filter design
 */
typedef struct {
    float    alpha;         /* Smoothing factor 0.0–1.0      */
    float    output;        /* Current filtered output       */
    bool     initialized;   /* Has first sample been loaded? */
} adc_ema_filter_t;

/* ──────────────────────────────────────────────
 * ADC API
 * ────────────────────────────────────────────── */

/*
 * adc_init — configure and enable the ADC peripheral
 *
 * Steps: calibrate (set ADCAL, wait for completion → ~100 µs),
 *        configure sampling time, sequence, mode, enable ADON.
 *
 * @param config: ADC configuration
 *
 * Complexity: O(1) + calibration time (~100 µs)
 */
void adc_init(const adc_config_t *config);

/*
 * adc_read — perform a single conversion and return the result
 *
 * For single mode: starts conversion (SWSTART), waits for EOC,
 * reads DR. Blocking function.
 *
 * @param adc_index: ADC peripheral index
 * @return 12-bit conversion result (0–4095)
 *
 * Complexity: O(1) + conversion time (~1–10 µs depending on sample time)
 */
uint16_t adc_read(uint8_t adc_index);

/*
 * adc_read_channel — read a specific channel
 *
 * Reconfigures the sequence to sample just one channel, then
 * performs a single conversion.
 *
 * @param adc_index: ADC index
 * @param channel:   analog input channel (0–18 for STM32F4)
 * @return 12-bit result
 */
uint16_t adc_read_channel(uint8_t adc_index, uint8_t channel);

/*
 * adc_read_mv — convert ADC reading to millivolts
 *
 * V_in (mV) = (reading / 4095) × V_ref (mV)
 *
 * @param reading: 12-bit ADC result
 * @param vref_mv: reference voltage in millivolts
 * @return input voltage in millivolts
 *
 * Complexity: O(1)
 *
 * L3 Math: Linear mapping with quantization.
 *   The uncertainty is ±0.5 LSB = ±V_ref / 8192.
 *   For V_ref = 3300 mV, uncertainty ≈ ±0.4 mV.
 */
uint32_t adc_to_millivolts(uint16_t reading, uint32_t vref_mv);

/*
 * adc_start_conversion — start conversion on regular channels
 *
 * Sets SWSTART (or JSWSTART for injected). In continuous mode,
 * conversions proceed automatically.
 *
 * @param adc_index: ADC index
 */
void adc_start_conversion(uint8_t adc_index);

/*
 * adc_is_conversion_done — check if the latest regular conversion is complete
 *
 * Polls the EOC flag. In continuous mode, EOC is set after each
 * conversion and must be cleared by reading DR.
 *
 * @param adc_index: ADC index
 * @return true if conversion complete
 */
bool adc_is_conversion_done(uint8_t adc_index);

/*
 * adc_avg_filter_init — initialise the running average filter
 *
 * @param filter: filter state structure
 */
void adc_avg_filter_init(adc_average_filter_t *filter);

/*
 * adc_avg_filter_update — feed a new sample into the running average
 *
 * Complexity: O(1) — circular buffer update with running sum
 *
 * @param filter: filter state
 * @param sample: new ADC reading
 * @return filtered value (average of last N samples)
 *
 * Theorem: For uncorrelated, zero-mean noise, averaging N samples
 *   reduces RMS noise by 1/√N.
 *   Averaging 16 samples → 4× noise reduction.
 *   Averaging 256 samples → 16× noise reduction (≈4 extra ENOB bits).
 */
uint16_t adc_avg_filter_update(adc_average_filter_t *filter, uint16_t sample);

/*
 * adc_ema_filter_init — initialise exponential moving average filter
 *
 * @param filter:   filter state
 * @param alpha:    smoothing factor (0.0 = infinite smoothing, 1.0 = no filtering)
 * @param initial:  first sample value to seed the filter
 */
void adc_ema_filter_init(adc_ema_filter_t *filter, float alpha, float initial);

/*
 * adc_ema_filter_update — feed a new sample into EMA filter
 *
 * @param filter: filter state
 * @param sample: new ADC reading
 * @return filtered value
 *
 * Complexity: O(1)
 */
float adc_ema_filter_update(adc_ema_filter_t *filter, float sample);

/*
 * adc_oversample_and_decimate — oversampling to increase effective resolution
 *
 * Theory: Averaging 4^N samples and right-shifting by N bits
 *   adds N bits of effective resolution.
 *   E.g., 16× oversampling → 2 extra bits (12→14 bit).
 *
 * Assumptions: noise dithers the LSB (≥1 LSB RMS), signal is
 *   band-limited, samples are uncorrelated.
 *
 * @param adc_index: ADC index
 * @param channel:   analog channel
 * @param oversample: oversampling factor (power of 4: 4, 16, 64, 256, ...)
 * @param extra_bits: number of extra bits to shift (log4(oversample))
 * @return oversampled and decimated result
 *
 * Complexity: O(oversample)
 * Reference: Atmel AVR121, "Enhancing ADC resolution by oversampling" (2005)
 */
uint32_t adc_oversample_and_decimate(uint8_t adc_index, uint8_t channel,
                                     uint32_t oversample, uint8_t extra_bits);

/*
 * adc_get_regs — return pointer to ADC register map
 *
 * @param adc_index: ADC index
 * @return pointer to adc_regs_t or NULL
 */
adc_regs_t *adc_get_regs(uint8_t adc_index);

#endif /* ADC_H */
