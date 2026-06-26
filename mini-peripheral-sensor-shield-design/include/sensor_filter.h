/**
 * @file    sensor_filter.h
 * @brief   L5 Digital Filtering Algorithms for Sensor Data - Moving Average, EMA, Median, FIR, IIR
 *
 * @details Digital filters commonly used in MCU firmware for sensor data cleaning:
 *          - Moving average (boxcar) filter
 *          - Exponential Moving Average (EMA / 1st-order IIR low-pass)
 *          - Median filter (non-linear, spike removal)
 *          - FIR low-pass filter (windowed sinc)
 *          - IIR Butterworth filter (biquad cascade)
 *          - Hysteresis / deadband filter
 *          - Rate-of-change limiter
 *
 * Knowledge Mapping:
 *   L3 - FIR vs IIR structures, z-transform H(z), frequency response
 *   L4 - Nyquist-Shannon sampling theorem (fs > 2*fmax for anti-aliasing)
 *   L5 - FIR coefficient design (windowing), IIR bilinear transform,
 *        moving window ring buffer, median filter sliding window
 *   L6 - Real-time sensor data filtering on Arduino/STM32 with limited RAM
 *   L8 - Adaptive filtering based on signal statistics
 *
 * Reference: Oppenheim & Schafer "Discrete-Time Signal Processing" (2010)
 *            Smith "The Scientist and Engineer's Guide to DSP" (1997)
 */

#ifndef SENSOR_FILTER_H
#define SENSOR_FILTER_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#define FILTER_MAX_TAPS 64

/* ---- Moving Average Filter (L5) ----
 * y[n] = (1/N) * sum_{k=0}^{N-1} x[n-k]
 * Frequency response: H(f) = sin(pi*f*N/fs) / (N*sin(pi*f/fs))
 * -3dB cutoff: fc ~= 0.443 * fs / N (for large N) */
typedef struct {
    uint16_t window_size;
    double buffer[FILTER_MAX_TAPS];
    uint16_t index;
    double sum;
    uint16_t count;
    bool is_full;
} moving_average_filter_t;

void moving_average_init(moving_average_filter_t *maf, uint16_t window_size);
double moving_average_update(moving_average_filter_t *maf, double new_sample);
double moving_average_get(const moving_average_filter_t *maf);
void moving_average_reset(moving_average_filter_t *maf);
double moving_average_cutoff_freq(const moving_average_filter_t *maf, double fs_hz);

/* ---- Exponential Moving Average (EMA) (L5) ----
 * y[n] = alpha * x[n] + (1-alpha) * y[n-1]
 * alpha = dt / (tau + dt)  where tau = time constant
 * or alpha = 2/(N+1) for N-window equivalent
 * -3dB cutoff: fc = alpha/(2*pi*dt) for small alpha */
typedef struct {
    double alpha;                    /* smoothing factor (0-1) */
    double output;                   /* current filter output */
    bool is_initialized;
} ema_filter_t;

void ema_filter_init(ema_filter_t *ema, double alpha);
double ema_filter_update(ema_filter_t *ema, double new_sample);
void ema_filter_init_from_tau(ema_filter_t *ema, double tau_s, double fs_hz);
void ema_filter_init_from_cutoff(ema_filter_t *ema, double fc_hz, double fs_hz);
double ema_filter_get(const ema_filter_t *ema);
void ema_filter_reset(ema_filter_t *ema);

/* ---- Median Filter (L5) ----
 * y[n] = median{x[n-k], ..., x[n], ..., x[n+k]} for window_size=2k+1
 * Excellent for removing impulse/spike noise while preserving edges
 * Non-linear: no transfer function, but preserves monotonic edges */
typedef struct {
    uint16_t window_size;            /* must be odd */
    double buffer[FILTER_MAX_TAPS];
    double sorted[FILTER_MAX_TAPS];  /* for sorting */
    uint16_t index;
    uint16_t count;
} median_filter_t;

void median_filter_init(median_filter_t *mf, uint16_t window_size);
double median_filter_update(median_filter_t *mf, double new_sample);
double median_filter_get(const median_filter_t *mf);
void median_filter_reset(median_filter_t *mf);

/* ---- FIR Filter (L5) ----
 * y[n] = sum_{k=0}^{N-1} b_k * x[n-k]
 * Linear phase when coefficients are symmetric
 * Design by windowing ideal impulse response (sinc function) */
typedef struct {
    uint16_t num_taps;
    double coefficients[FILTER_MAX_TAPS];
    double buffer[FILTER_MAX_TAPS];  /* circular delay line */
    uint16_t index;
} fir_filter_t;

void fir_filter_init(fir_filter_t *fir, const double *coeffs, uint16_t num_taps);
double fir_filter_update(fir_filter_t *fir, double new_sample);
void fir_filter_reset(fir_filter_t *fir);
int fir_filter_design_lowpass(fir_filter_t *fir, uint16_t num_taps,
                                double fc_hz, double fs_hz);
int fir_filter_design_highpass(fir_filter_t *fir, uint16_t num_taps,
                                 double fc_hz, double fs_hz);

/* ---- IIR Biquad Filter (L5) ----
 * Direct Form I:
 *   y[n] = b0*x[n] + b1*x[n-1] + b2*x[n-2] - a1*y[n-1] - a2*y[n-2]
 *
 * Bilinear Transform maps analog prototype to digital:
 *   s = (2/T) * (1 - z^{-1}) / (1 + z^{-1})
 *
 * Butterworth LP coefficients (pre-warped):
 *   w0 = 2*pi*fc/fs, c = cos(w0), alpha = sin(w0)/(2*Q)
 *   b0 = (1-c)/2, b1 = 1-c, b2 = (1-c)/2
 *   a0 = 1+alpha, a1 = -2*c, a2 = 1-alpha
 *   (normalize by dividing all by a0) */
typedef struct {
    double b0, b1, b2;               /* numerator coefficients */
    double a1, a2;                   /* denominator coeffs (a0=1) */
    double x1, x2;                   /* input delay line */
    double y1, y2;                   /* output delay line */
} biquad_filter_t;

void biquad_filter_init(biquad_filter_t *bq);
double biquad_filter_update(biquad_filter_t *bq, double new_sample);
void biquad_filter_reset(biquad_filter_t *bq);
int biquad_design_lowpass(biquad_filter_t *bq, double fc_hz, double fs_hz, double Q);
int biquad_design_highpass(biquad_filter_t *bq, double fc_hz, double fs_hz, double Q);
int biquad_design_notch(biquad_filter_t *bq, double fn_hz, double fs_hz, double Q);
double biquad_frequency_response(const biquad_filter_t *bq, double freq_hz, double fs_hz);

/* ---- Hysteresis / Deadband Filter (L5) ----
 * Only updates output when input changes by more than threshold
 * Used for eliminating small fluctuations/noise on slow signals */
typedef struct {
    double threshold;                /* minimum change to trigger update */
    double output;                   /* last stable output */
    bool is_initialized;
} hysteresis_filter_t;

void hysteresis_filter_init(hysteresis_filter_t *hf, double threshold);
double hysteresis_filter_update(hysteresis_filter_t *hf, double new_sample);
double hysteresis_filter_get(const hysteresis_filter_t *hf);

/* ---- Rate Limiter (L5) ----
 * Limits the maximum rate of change between consecutive samples
 * y[n] = clamp(x[n], y[n-1]-max_rate*dt, y[n-1]+max_rate*dt) */
typedef struct {
    double max_rate_per_second;      /* maximum change per second */
    double output; double prev_input;
    bool is_initialized;
} rate_limiter_t;

void rate_limiter_init(rate_limiter_t *rl, double max_rate);
double rate_limiter_update(rate_limiter_t *rl, double new_sample, double dt_s);
double rate_limiter_get(const rate_limiter_t *rl);

/* ---- Filter Chain (L6) ----
 * Applies multiple filters in sequence for sensor data pipeline */
typedef struct {
    moving_average_filter_t ma;
    ema_filter_t ema;
    median_filter_t median;
    fir_filter_t fir;
    biquad_filter_t biquad;
    hysteresis_filter_t hysteresis;
    rate_limiter_t rate_limiter;
    uint8_t active_filters;          /* bitmap of enabled filters */
} filter_chain_t;

void filter_chain_init(filter_chain_t *fc);
double filter_chain_process(filter_chain_t *fc, double sample, double dt_s);
void filter_chain_reset(filter_chain_t *fc);

#endif /* SENSOR_FILTER_H */
