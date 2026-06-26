/*
 * adc.c — ADC Implementation for ARM Cortex-M MCUs
 *
 * Knowledge points implemented (independent):
 *   1. adc_init — SAR ADC configuration (sampling time, mode, sequence)
 *   2. adc_read — single-shot conversion with EOC polling
 *   3. adc_read_channel — single-channel read with sequence reprogramming
 *   4. adc_to_millivolts — linear mapping from ADC counts to mV
 *   5. adc_avg_filter_init/update — moving-average filter with running sum
 *   6. adc_ema_filter_init/update — exponential moving average (IIR low-pass)
 *   7. adc_oversample_and_decimate — oversampling for extra bit resolution
 */

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "adc.h"
#include "arch_compat.h"

/* ──────────────────────────────────────────────
 * ADC Base Addresses (STM32F4)
 *
 * ADC1: 0x40012000 (APB2)
 * ADC2: 0x40012100 (APB2)
 * ADC3: 0x40012200 (APB2)
 * Common: 0x40012300 (shared across all ADCs)
 * ────────────────────────────────────────────── */

#define ADC_NUM_INSTANCES 3

static const uint32_t adc_base_addresses[ADC_NUM_INSTANCES] = {
    0x40012000U,  /* ADC1 */
    0x40012100U,  /* ADC2 */
    0x40012200U   /* ADC3 */
};

adc_regs_t *adc_get_regs(uint8_t adc_index)
{
    if (adc_index >= ADC_NUM_INSTANCES) {
        return NULL;
    }
    return (adc_regs_t *)(uintptr_t)adc_base_addresses[adc_index];
}

/* ──────────────────────────────────────────────
 * adc_init — configure the SAR ADC
 *
 * Knowledge: SAR ADC configuration.
 *
 * A Successive-Approximation Register (SAR) ADC converts an
 * analog voltage to a digital code by binary search:
 *   1. Sample the input voltage onto an internal capacitor (sample-and-hold).
 *   2. The DAC generates a trial voltage. The comparator tests whether
 *      the input is above or below the trial voltage.
 *   3. This is repeated N times for an N-bit ADC, each time halving
 *      the search range.
 *
 * Total conversion time:
 *   T_conv = T_sample + N × T_comparator
 *          = ADC_SMP × T_ADC_CLK + 12 × T_ADC_CLK  (for 12-bit)
 *          = (SMP + 12) × T_ADC_CLK
 *
 * For SMP=56 cycles and 30 MHz ADC clock:
 *   T_conv = (56 + 12) × 33.3 ns = 2.27 µs
 *
 * Reference: Maxim Integrated Tutorial 1080, "Understanding SAR ADCs"
 * ────────────────────────────────────────────── */

void adc_init(const adc_config_t *config)
{
    adc_regs_t *regs;
    uint32_t cr1_val;
    uint32_t cr2_val;
    uint8_t i;

    if (config == NULL) {
        return;
    }

    regs = adc_get_regs(config->adc_index);
    if (regs == NULL) {
        return;
    }

    /* 1. Power up ADC: set ADON in CR2 (requires a stabilization time ~10 µs) */
    cr2_val = regs->CR2;
    cr2_val |= ADC_CR2_ADON;
    regs->CR2 = cr2_val;

    /* Wait for ADC stabilization (t_STAB = ~10 µs). On real hardware,
     * poll or use a fixed delay. Here we use a simple spin loop. */
    for (i = 0; i < 100; i++) {
        NOP();
    }

    /* 2. Configure sampling time for all channels.
     * SMPR2 covers channels 0–9 (3 bits per channel, 10 channels = 30 bits).
     * SMPR1 covers channels 10–18 (9 channels = 27 bits). */
    {
        uint32_t smp_val = (uint32_t)config->sample_time & 0x7U;
        uint32_t smp_word = 0;
        uint8_t ch;

        /* Build 30-bit SMPR2 value: each channel gets smp_val */
        for (ch = 0; ch < 10U; ch++) {
            smp_word |= (smp_val << (ch * 3U));
        }

        regs->SMPR2 = smp_word;

        /* Build 27-bit SMPR1 value for channels 10–18 */
        smp_word = 0;
        for (ch = 0; ch < 9U; ch++) {
            smp_word |= (smp_val << (ch * 3U));
        }

        regs->SMPR1 = smp_word;
    }

    /* 3. Configure sequence registers (SQR1–SQR3).
     * SQR1[23:20] = L[3:0] — number of conversions in regular sequence (0 = 1 conv).
     * SQR3[4:0] through SQR1[19:15] — channel numbers in order.
     *
     * For simplicity: configure a single channel. */
    {
        uint8_t num_ch = config->num_channels;
        if (num_ch == 0) num_ch = 1;
        if (num_ch > 16) num_ch = 16;

        /* SQR1: L = num_ch - 1, and SQ16–SQ14 */
        regs->SQR1 = ((uint32_t)(num_ch - 1) << 20U);

        /* Fill channels into SQR3 (SQ1), then SQR2 (SQ7–SQ12), then SQR1 (SQ13–SQ16).
         * Simple case: first channel in SQ1 (SQR3[4:0]). */
        regs->SQR3 = ((uint32_t)(config->channel_list[0]) & 0x1FU);

        /* Additional channels in higher sequence positions if num_ch > 1 */
        if (num_ch > 1 && config->channel_list[1] < 19U) {
            regs->SQR3 |= ((uint32_t)(config->channel_list[1]) & 0x1FU) << 5U;
        }
        if (num_ch > 2 && config->channel_list[2] < 19U) {
            regs->SQR3 |= ((uint32_t)(config->channel_list[2]) & 0x1FU) << 10U;
        }
        if (num_ch > 3 && config->channel_list[3] < 19U) {
            regs->SQR3 |= ((uint32_t)(config->channel_list[3]) & 0x1FU) << 15U;
        }
        if (num_ch > 4 && config->channel_list[4] < 19U) {
            regs->SQR3 |= ((uint32_t)(config->channel_list[4]) & 0x1FU) << 20U;
        }
        if (num_ch > 5 && config->channel_list[5] < 19U) {
            regs->SQR3 |= ((uint32_t)(config->channel_list[5]) & 0x1FU) << 25U;
        }
    }

    /* 4. CR2: mode settings */
    cr2_val = regs->CR2;
    if (config->mode == ADC_MODE_CONTINUOUS) {
        cr2_val |= ADC_CR2_CONT;
    } else {
        cr2_val &= ~ADC_CR2_CONT;
    }
    regs->CR2 = cr2_val;

    /* 5. CR1: no scan mode by default (SCAN=0 for single channel) */
    cr1_val = regs->CR1;
    if (config->mode == ADC_MODE_SCAN) {
        cr1_val |= (1U << 8U);  /* SCAN mode */
    } else {
        cr1_val &= ~(1U << 8U);
    }
    if (config->eoc_interrupt) {
        cr1_val |= (1U << 5U);  /* EOCIE */
    }
    regs->CR1 = cr1_val;
}

/* ──────────────────────────────────────────────
 * adc_read — single-shot conversion
 *
 * Knowledge: ADC conversion flow.
 *
 * Steps:
 *   1. Set SWSTART in CR2 to trigger conversion.
 *   2. Wait for EOC (End of Conversion) flag in SR.
 *   3. Read DR (Data Register) for the 12-bit result.
 *
 * DR[15:0] contains the result in bits[11:0] (right-aligned
 * by default). Bits[15:12] are zero.
 *
 * For a scan sequence, each channel in the sequence is converted
 * in order, and DR updates with each result. Use DMA to transfer
 * results to RAM without CPU intervention in scan+continuous mode.
 * ────────────────────────────────────────────── */

uint16_t adc_read(uint8_t adc_index)
{
    adc_regs_t *regs = adc_get_regs(adc_index);

    if (regs == NULL) {
        return 0;
    }

    /* Start conversion */
    regs->CR2 |= ADC_CR2_SWSTART;

    /* Wait for conversion to complete */
    while ((regs->SR & ADC_SR_EOC) == 0U) {
        /* Spin-wait */
    }

    /* Read result (EOC is cleared by reading DR on some MCUs,
     * or requires explicit clear on others. STM32F4 auto-clears
     * EOC when DR is read.) */
    return (uint16_t)(regs->DR & 0xFFFU);
}

/* ──────────────────────────────────────────────
 * adc_read_channel — read a specific channel
 *
 * Reprograms SQR3 to sample only the specified channel,
 * starts a conversion, and returns the result.
 * ────────────────────────────────────────────── */

uint16_t adc_read_channel(uint8_t adc_index, uint8_t channel)
{
    adc_regs_t *regs = adc_get_regs(adc_index);

    if (regs == NULL || channel > 18U) {
        return 0;
    }

    /* Set single channel in SQR1 (L=0 = 1 conversion) and SQR3 */
    regs->SQR1 = 0U;  /* L[3:0]=0 means 1 conversion */
    regs->SQR3 = (uint32_t)(channel & 0x1FU);

    /* Start conversion and wait */
    regs->CR2 |= ADC_CR2_SWSTART;

    while ((regs->SR & ADC_SR_EOC) == 0U) {
        /* Spin-wait */
    }

    return (uint16_t)(regs->DR & 0xFFFU);
}

/* ──────────────────────────────────────────────
 * adc_to_millivolts — convert ADC reading to millivolts
 *
 * Knowledge: Linear voltage mapping with quantization error.
 *
 * V_in = (reading / 2^N) × V_ref
 *
 * For N=12, V_ref=3300 mV:
 *   V_in(mV) = reading × 3300 / 4096
 *            = reading × 0.8057 mV
 *
 * The formula uses integer arithmetic to avoid floating-point
 * (often unavailable or slow on Cortex-M without FPU).
 *
 * Quantization error:
 *   ε_q = ±0.5 LSB = ±V_ref / (2 × 2^N) = ±V_ref / 8192
 *   For V_ref=3300 mV: ±0.40 mV
 * ────────────────────────────────────────────── */

uint32_t adc_to_millivolts(uint16_t reading, uint32_t vref_mv)
{
    /* V = reading * V_ref / 4096
     * Multiply first to preserve precision, then divide.
     * (uint64_t to avoid overflow for 4095 × 3300 = 13,513,500 which fits in 32 bits,
     *  but large vref could overflow 32-bit). */
    return (uint32_t)(((uint64_t)reading * (uint64_t)vref_mv) / 4096ULL);
}

/* ──────────────────────────────────────────────
 * adc_start_conversion — start ADC conversion
 * ────────────────────────────────────────────── */

void adc_start_conversion(uint8_t adc_index)
{
    adc_regs_t *regs = adc_get_regs(adc_index);
    if (regs == NULL) {
        return;
    }
    regs->CR2 |= ADC_CR2_SWSTART;
}

/* ──────────────────────────────────────────────
 * adc_is_conversion_done — check EOC flag
 * ────────────────────────────────────────────── */

bool adc_is_conversion_done(uint8_t adc_index)
{
    adc_regs_t *regs = adc_get_regs(adc_index);
    if (regs == NULL) {
        return false;
    }
    return (regs->SR & ADC_SR_EOC) != 0U;
}

/* ──────────────────────────────────────────────
 * adc_avg_filter_init — initialise moving average filter
 *
 * Knowledge: Running (moving) average filter.
 *
 * The moving average filter is a finite impulse response (FIR) filter
 * with all coefficients equal to 1/N:
 *
 *   y[k] = (1/N) × Σ_{i=0}^{N-1} x[k − i]
 *
 * Frequency response:
 *   |H(e^(jω))| = |sin(ωN/2) / (N × sin(ω/2))|
 *
 * This is a low-pass filter with:
 *   - DC gain = 1 (0 dB)
 *   - First null at f = f_s / N
 *   - Side-lobes decaying as ~1/f (slow, only 13 dB first side-lobe)
 *
 * With a running sum implementation, each update costs O(1):
 *   sum = sum − oldest + new_sample
 *   y = sum / N
 *
 * ────────────────────────────────────────────── */

void adc_avg_filter_init(adc_average_filter_t *filter)
{
    uint8_t i;

    if (filter == NULL) {
        return;
    }

    for (i = 0; i < ADC_AVERAGE_WINDOW_SIZE; i++) {
        filter->buffer[i] = 0;
    }
    filter->index = 0;
    filter->count = 0;
    filter->sum = 0;
}

/* ──────────────────────────────────────────────
 * adc_avg_filter_update — feed new sample into moving average
 *
 * O(1) per sample using running sum.
 * ────────────────────────────────────────────── */

uint16_t adc_avg_filter_update(adc_average_filter_t *filter, uint16_t sample)
{
    if (filter == NULL) {
        return sample;
    }

    /* If buffer not yet full, just accumulate */
    if (filter->count < ADC_AVERAGE_WINDOW_SIZE) {
        filter->buffer[filter->index] = sample;
        filter->sum += (uint32_t)sample;
        filter->index++;
        filter->count++;
        return (uint16_t)(filter->sum / (uint32_t)filter->count);
    }

    /* Buffer full: subtract oldest, add newest */
    filter->sum -= (uint32_t)filter->buffer[filter->index];
    filter->sum += (uint32_t)sample;
    filter->buffer[filter->index] = sample;

    filter->index++;
    if (filter->index >= ADC_AVERAGE_WINDOW_SIZE) {
        filter->index = 0;
    }

    return (uint16_t)(filter->sum / ADC_AVERAGE_WINDOW_SIZE);
}

/* ──────────────────────────────────────────────
 * adc_ema_filter_init — initialise exponential moving average
 *
 * Knowledge: IIR first-order low-pass filter.
 *
 * y[n] = α·x[n] + (1−α)·y[n−1]
 *
 * Equivalent to an RC low-pass filter with:
 *   f_c = α / (2π × T_sample) for small α
 *
 * Impulse response:
 *   h[n] = α × (1−α)^n × u[n]  (infinite, decaying)
 *
 * Step response time constant τ = −T_sample / ln(1−α) ≈ T_sample / α
 *   for small α.
 *
 * For α = 0.1, T_sample = 1 ms:
 *   τ ≈ 1 ms / 0.1 = 10 ms
 *   f_c ≈ 0.1 / (2π × 0.001) ≈ 15.9 Hz
 * ────────────────────────────────────────────── */

void adc_ema_filter_init(adc_ema_filter_t *filter, float alpha, float initial)
{
    if (filter == NULL) {
        return;
    }

    /* Clamp alpha to [0, 1] */
    if (alpha < 0.0f) alpha = 0.0f;
    if (alpha > 1.0f) alpha = 1.0f;

    filter->alpha = alpha;
    filter->output = initial;
    filter->initialized = true;
}

/* ──────────────────────────────────────────────
 * adc_ema_filter_update — apply EMA filter to new sample
 * ────────────────────────────────────────────── */

float adc_ema_filter_update(adc_ema_filter_t *filter, float sample)
{
    if (filter == NULL) {
        return sample;
    }

    if (!filter->initialized) {
        filter->output = sample;
        filter->initialized = true;
        return sample;
    }

    filter->output = filter->alpha * sample + (1.0f - filter->alpha) * filter->output;
    return filter->output;
}

/* ──────────────────────────────────────────────
 * adc_oversample_and_decimate — oversampling for extra bits
 *
 * Knowledge: Oversampling and decimation to increase resolution.
 *
 * Theory (Atmel AVR121):
 *   - The quantization noise is approximately white and uniform
 *     over ±0.5 LSB, with RMS = LSB / √12.
 *   - Averaging N samples preserves the signal (coherent addition
 *     → amplitude grows ×N) while the noise adds in RMS (power adds
 *     → amplitude grows ×√N).
 *   - SNR improvement: ΔSNR = 10×log₁₀(N) dB.
 *   - Each ×4 oversampling adds ~1 bit of effective resolution:
 *     N = 4^(extra_bits).
 *
 * Requirements:
 *   - The ADC must have > 1 LSB of RMS noise to dither the LSB.
 *   - The signal must be band-limited (no frequency components
 *     above f_s / (2 × N)).
 *   - Samples must be uncorrelated (no fixed DC offset).
 *
 * Practical example:
 *   - 16× oversampling of a 12-bit ADC: average 16 readings,
 *     right-shift by 2 → 14-bit effective result.
 *   - 256× oversampling → 16-bit effective result.
 *
 * Trade-off: oversampling reduces throughput.
 *   16× oversampling → maximum sample rate = ADC_rate / 16.
 * ────────────────────────────────────────────── */

uint32_t adc_oversample_and_decimate(uint8_t adc_index, uint8_t channel,
                                     uint32_t oversample, uint8_t extra_bits)
{
    uint64_t accumulator = 0ULL;
    uint32_t i;
    uint16_t reading;

    if (oversample == 0) {
        return 0;
    }

    /* Accumulate N samples */
    for (i = 0; i < oversample; i++) {
        reading = adc_read_channel(adc_index, channel);
        accumulator += (uint64_t)reading;
    }

    /* Decimate: divide by N, then shift by extra_bits.
     * (accumulator / oversample) >> extra_bits = (accumulator) / (oversample × 2^extra_bits)
     * But oversample = 4^extra_bits = 2^(2×extra_bits), so total shift = 2×extra_bits + extra_bits
     * Wait, no: we oversample and average. The average gives us the original bits plus
     * fractional bits derived from the noise. Right-shifting by extra_bits keeps the
     * integer part plus the extra fractional precision.
     *
     * accumulator / oversample = original value + fractional noise average.
     * (accumulator / oversample) << extra_bits = expanded value with extra bits.
     *
     * Equivalent: (accumulator << extra_bits) / oversample
     */
    return (uint32_t)((accumulator << (uint64_t)extra_bits) / (uint64_t)oversample);
}
