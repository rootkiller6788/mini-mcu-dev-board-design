/**
 * @file    sensor_filter.c
 * @brief   L5 Digital Filter Implementations for Sensor Data Cleaning
 *
 * @details Implements digital filters commonly used in MCU firmware:
 *          - Moving average (boxcar) filter with ring buffer
 *          - Exponential Moving Average (EMA/1st-order IIR LP)
 *          - Median filter with sliding window
 *          - FIR filter with configurable coefficients
 *          - IIR biquad filter (Direct Form I)
 *          - Hysteresis/deadband filter
 *          - Rate limiter
 *          - Filter chain for sequential processing
 *
 * Knowledge Mapping:
 *   L3 - FIR/IIR structures, z-transform, frequency response
 *   L4 - Nyquist-Shannon sampling theorem
 *   L5 - Circular buffer, sorting, coefficient design, bilinear transform
 *   L6 - Real-time sensor data filtering on Arduino/STM32 MCU
 *
 * Reference: Oppenheim & Schafer (2010), Smith (1997)
 */

#include "sensor_filter.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* ---- Moving Average Filter ---- */

void moving_average_init(moving_average_filter_t *maf, uint16_t window) {
    if (!maf) return;
    memset(maf, 0, sizeof(*maf));
    maf->window_size = window > FILTER_MAX_TAPS ? FILTER_MAX_TAPS : window;
}

double moving_average_update(moving_average_filter_t *maf, double sample) {
    if (!maf || maf->window_size == 0) return sample;
    double old = maf->buffer[maf->index];
    maf->buffer[maf->index] = sample;
    maf->index = (maf->index + 1) % maf->window_size;
    maf->sum += sample - old;
    if (maf->count < maf->window_size) {
        maf->count++;
        maf->sum = 0.0;
        for (uint16_t i = 0; i < maf->count; i++) maf->sum += maf->buffer[i];
    }
    if (maf->count >= maf->window_size) maf->is_full = true;
    return maf->sum / (double)maf->count;
}

double moving_average_get(const moving_average_filter_t *maf) {
    if (!maf || maf->count == 0) return 0.0;
    return maf->sum / (double)maf->count;
}

void moving_average_reset(moving_average_filter_t *maf) {
    if (!maf) return;
    maf->sum = 0.0; maf->index = 0; maf->count = 0; maf->is_full = false;
    memset(maf->buffer, 0, sizeof(maf->buffer));
}

double moving_average_cutoff_freq(const moving_average_filter_t *maf, double fs) {
    /* -3dB cutoff approx: fc = 0.443 * fs / N (for large N) */
    if (!maf || maf->window_size == 0) return fs;
    return 0.443 * fs / (double)maf->window_size;
}

/* ---- Exponential Moving Average ---- */

void ema_filter_init(ema_filter_t *ema, double alpha) {
    if (!ema) return;
    memset(ema, 0, sizeof(*ema));
    ema->alpha = alpha < 0.0 ? 0.0 : (alpha > 1.0 ? 1.0 : alpha);
}

double ema_filter_update(ema_filter_t *ema, double sample) {
    if (!ema) return sample;
    if (!ema->is_initialized) { ema->output = sample; ema->is_initialized = true; return sample; }
    ema->output = ema->alpha * sample + (1.0 - ema->alpha) * ema->output;
    return ema->output;
}

void ema_filter_init_from_tau(ema_filter_t *ema, double tau_s, double fs) {
    /* alpha = dt/(tau+dt) = 1/(tau*fs+1) */
    if (!ema) return;
    double alpha = (tau_s > 0.0) ? 1.0 / (tau_s * fs + 1.0) : 1.0;
    ema_filter_init(ema, alpha);
}

void ema_filter_init_from_cutoff(ema_filter_t *ema, double fc, double fs) {
    /* alpha = 2*pi*fc*dt for small fc relative to fs */
    if (!ema || fs <= 0.0) return;
    double alpha = 2.0 * M_PI * fc / fs;
    if (alpha > 1.0) alpha = 1.0;
    ema_filter_init(ema, alpha);
}

double ema_filter_get(const ema_filter_t *ema) {
    return ema ? ema->output : 0.0;
}

void ema_filter_reset(ema_filter_t *ema) {
    if (!ema) return;
    ema->output = 0.0; ema->is_initialized = false;
}

/* ---- Median Filter ---- */

void median_filter_init(median_filter_t *mf, uint16_t window) {
    if (!mf) return;
    memset(mf, 0, sizeof(*mf));
    if (window % 2 == 0) window++; /* ensure odd */
    mf->window_size = window > FILTER_MAX_TAPS ? FILTER_MAX_TAPS : window;
}

static int cmp_double(const void *a, const void *b) {
    double da = *(const double *)a, db = *(const double *)b;
    return (da > db) - (da < db);
}

double median_filter_update(median_filter_t *mf, double sample) {
    if (!mf || mf->window_size == 0) return sample;
    mf->buffer[mf->index] = sample;
    mf->index = (mf->index + 1) % mf->window_size;
    if (mf->count < mf->window_size) mf->count++;
    /* Copy to sorted array and find median */
    memcpy(mf->sorted, mf->buffer, mf->count * sizeof(double));
    qsort(mf->sorted, mf->count, sizeof(double), cmp_double);
    return mf->sorted[mf->count / 2];
}

double median_filter_get(const median_filter_t *mf) {
    if (!mf || mf->count == 0) return 0.0;
    return mf->sorted[mf->count / 2];
}

void median_filter_reset(median_filter_t *mf) {
    if (!mf) return;
    mf->index = 0; mf->count = 0;
    memset(mf->buffer, 0, sizeof(mf->buffer));
}

/* ---- FIR Filter ---- */

void fir_filter_init(fir_filter_t *fir, const double *coeffs, uint16_t taps) {
    if (!fir) return;
    memset(fir, 0, sizeof(*fir));
    fir->num_taps = taps > FILTER_MAX_TAPS ? FILTER_MAX_TAPS : taps;
    if (coeffs) memcpy(fir->coefficients, coeffs, fir->num_taps * sizeof(double));
}

double fir_filter_update(fir_filter_t *fir, double sample) {
    if (!fir || fir->num_taps == 0) return sample;
    fir->buffer[fir->index] = sample;
    fir->index = (fir->index + 1) % fir->num_taps;
    double result = 0.0;
    for (uint16_t i = 0; i < fir->num_taps; i++) {
        uint16_t buf_idx = (fir->index + fir->num_taps - 1 - i) % fir->num_taps;
        result += fir->coefficients[i] * fir->buffer[buf_idx];
    }
    return result;
}

void fir_filter_reset(fir_filter_t *fir) {
    if (!fir) return;
    fir->index = 0;
    memset(fir->buffer, 0, sizeof(fir->buffer));
}

int fir_filter_design_lowpass(fir_filter_t *fir, uint16_t taps, double fc, double fs) {
    /* Windowed sinc design: h[n] = 2*fc/fs * sinc(2*fc/fs*(n-M/2)) * window[n] */
    if (!fir || taps < 3 || taps > FILTER_MAX_TAPS || fc >= fs/2.0) return -1;
    double norm_fc = fc / fs;
    int M = taps - 1;
    double sum = 0.0;
    for (int n = 0; n < taps; n++) {
        if (n == M/2) {
            fir->coefficients[n] = 2.0 * norm_fc;
        } else {
            double x = 2.0 * M_PI * norm_fc * (n - M/2.0);
            fir->coefficients[n] = sin(x) / (M_PI * (n - M/2.0));
        }
        /* Hamming window: w[n] = 0.54 - 0.46*cos(2*pi*n/M) */
        double w = 0.54 - 0.46 * cos(2.0 * M_PI * n / M);
        fir->coefficients[n] *= w;
        sum += fir->coefficients[n];
    }
    /* Normalize for unity gain at DC */
    if (sum > 1e-15) for (int n = 0; n < taps; n++) fir->coefficients[n] /= sum;
    fir->num_taps = taps;
    fir_filter_reset(fir);
    return 0;
}

int fir_filter_design_highpass(fir_filter_t *fir, uint16_t taps, double fc, double fs) {
    /* Design LP first, then spectral inversion: h_HP[n] = -h_LP[n] for n!=M/2, h_HP[M/2]=1-h_LP[M/2] */
    if (!fir || fir_filter_design_lowpass(fir, taps, fc, fs) != 0) return -1;
    int mid = (taps - 1) / 2;
    for (int i = 0; i < taps; i++) {
        if (i == mid) fir->coefficients[i] = 1.0 - fir->coefficients[i];
        else fir->coefficients[i] = -fir->coefficients[i];
    }
    return 0;
}

/* ---- IIR Biquad Filter ---- */

void biquad_filter_init(biquad_filter_t *bq) {
    if (!bq) return;
    memset(bq, 0, sizeof(*bq));
    bq->b0 = 1.0; bq->b1 = 0.0; bq->b2 = 0.0;
    bq->a1 = 0.0; bq->a2 = 0.0;
}

double biquad_filter_update(biquad_filter_t *bq, double sample) {
    if (!bq) return sample;
    double y = bq->b0 * sample + bq->b1 * bq->x1 + bq->b2 * bq->x2
               - bq->a1 * bq->y1 - bq->a2 * bq->y2;
    bq->x2 = bq->x1; bq->x1 = sample;
    bq->y2 = bq->y1; bq->y1 = y;
    return y;
}

void biquad_filter_reset(biquad_filter_t *bq) {
    if (!bq) return;
    bq->x1 = bq->x2 = bq->y1 = bq->y2 = 0.0;
}

int biquad_design_lowpass(biquad_filter_t *bq, double fc, double fs, double Q) {
    if (!bq || fc <= 0.0 || fs <= 0.0) return -1;
    double w0 = 2.0 * M_PI * fc / fs;
    double cw = cos(w0);
    double alpha = sin(w0) / (2.0 * Q);
    double b0 = (1.0 - cw) / 2.0;
    double b1 = 1.0 - cw;
    double b2 = (1.0 - cw) / 2.0;
    double a0 = 1.0 + alpha;
    double a1 = -2.0 * cw;
    double a2 = 1.0 - alpha;
    bq->b0 = b0/a0; bq->b1 = b1/a0; bq->b2 = b2/a0;
    bq->a1 = a1/a0; bq->a2 = a2/a0;
    biquad_filter_reset(bq);
    return 0;
}

int biquad_design_highpass(biquad_filter_t *bq, double fc, double fs, double Q) {
    if (!bq || fc <= 0.0 || fs <= 0.0) return -1;
    double w0 = 2.0 * M_PI * fc / fs;
    double cw = cos(w0);
    double alpha = sin(w0) / (2.0 * Q);
    double b0 = (1.0 + cw) / 2.0;
    double b1 = -(1.0 + cw);
    double b2 = (1.0 + cw) / 2.0;
    double a0 = 1.0 + alpha;
    double a1 = -2.0 * cw;
    double a2 = 1.0 - alpha;
    bq->b0 = b0/a0; bq->b1 = b1/a0; bq->b2 = b2/a0;
    bq->a1 = a1/a0; bq->a2 = a2/a0;
    biquad_filter_reset(bq);
    return 0;
}

int biquad_design_notch(biquad_filter_t *bq, double fn, double fs, double Q) {
    if (!bq || fn <= 0.0 || fs <= 0.0) return -1;
    double w0 = 2.0 * M_PI * fn / fs;
    double cw = cos(w0);
    double alpha = sin(w0) / (2.0 * Q);
    double b0 = 1.0;
    double b1 = -2.0 * cw;
    double b2 = 1.0;
    double a0 = 1.0 + alpha;
    double a1 = -2.0 * cw;
    double a2 = 1.0 - alpha;
    bq->b0 = b0/a0; bq->b1 = b1/a0; bq->b2 = b2/a0;
    bq->a1 = a1/a0; bq->a2 = a2/a0;
    biquad_filter_reset(bq);
    return 0;
}

double biquad_frequency_response(const biquad_filter_t *bq, double freq, double fs) {
    if (!bq || fs <= 0.0) return 1.0;
    double w = 2.0 * M_PI * freq / fs;
    double num_re = bq->b0 + bq->b1*cos(w) + bq->b2*cos(2.0*w);
    double num_im = -bq->b1*sin(w) - bq->b2*sin(2.0*w);
    double den_re = 1.0 + bq->a1*cos(w) + bq->a2*cos(2.0*w);
    double den_im = -bq->a1*sin(w) - bq->a2*sin(2.0*w);
    return sqrt((num_re*num_re + num_im*num_im) / (den_re*den_re + den_im*den_im));
}

/* ---- Hysteresis Filter ---- */

void hysteresis_filter_init(hysteresis_filter_t *hf, double threshold) {
    if (!hf) return;
    memset(hf, 0, sizeof(*hf));
    hf->threshold = threshold;
}

double hysteresis_filter_update(hysteresis_filter_t *hf, double sample) {
    if (!hf) return sample;
    if (!hf->is_initialized) { hf->output = sample; hf->is_initialized = true; return sample; }
    if (fabs(sample - hf->output) >= hf->threshold) hf->output = sample;
    return hf->output;
}

double hysteresis_filter_get(const hysteresis_filter_t *hf) {
    return hf ? hf->output : 0.0;
}

/* ---- Rate Limiter ---- */

void rate_limiter_init(rate_limiter_t *rl, double max_rate) {
    if (!rl) return;
    memset(rl, 0, sizeof(*rl));
    rl->max_rate_per_second = max_rate;
}

double rate_limiter_update(rate_limiter_t *rl, double sample, double dt_s) {
    if (!rl) return sample;
    if (!rl->is_initialized) { rl->output = sample; rl->prev_input = sample; rl->is_initialized = true; return sample; }
    double max_delta = rl->max_rate_per_second * dt_s;
    double delta = sample - rl->prev_input;
    if (delta > max_delta) sample = rl->prev_input + max_delta;
    else if (delta < -max_delta) sample = rl->prev_input - max_delta;
    rl->prev_input = sample;
    rl->output = sample;
    return sample;
}

double rate_limiter_get(const rate_limiter_t *rl) {
    return rl ? rl->output : 0.0;
}

/* ---- Filter Chain ---- */

#define FC_BIT_MA       0x01
#define FC_BIT_EMA      0x02
#define FC_BIT_MEDIAN   0x04
#define FC_BIT_FIR      0x08
#define FC_BIT_BIQUAD   0x10
#define FC_BIT_HYST     0x20
#define FC_BIT_RATE     0x40

void filter_chain_init(filter_chain_t *fc) {
    if (!fc) return;
    memset(fc, 0, sizeof(*fc));
    moving_average_init(&fc->ma, 8);
    ema_filter_init_from_cutoff(&fc->ema, 10.0, 100.0);
    median_filter_init(&fc->median, 5);
    hysteresis_filter_init(&fc->hysteresis, 0.01);
    rate_limiter_init(&fc->rate_limiter, 100.0);
    fc->active_filters = FC_BIT_MA | FC_BIT_EMA;
}

double filter_chain_process(filter_chain_t *fc, double sample, double dt_s) {
    if (!fc) return sample;
    double y = sample;
    if (fc->active_filters & FC_BIT_MEDIAN)  y = median_filter_update(&fc->median, y);
    if (fc->active_filters & FC_BIT_RATE)    y = rate_limiter_update(&fc->rate_limiter, y, dt_s);
    if (fc->active_filters & FC_BIT_HYST)    y = hysteresis_filter_update(&fc->hysteresis, y);
    if (fc->active_filters & FC_BIT_MA)      y = moving_average_update(&fc->ma, y);
    if (fc->active_filters & FC_BIT_EMA)     y = ema_filter_update(&fc->ema, y);
    if (fc->active_filters & FC_BIT_FIR)     y = fir_filter_update(&fc->fir, y);
    if (fc->active_filters & FC_BIT_BIQUAD)  y = biquad_filter_update(&fc->biquad, y);
    return y;
}

void filter_chain_reset(filter_chain_t *fc) {
    if (!fc) return;
    moving_average_reset(&fc->ma);
    ema_filter_reset(&fc->ema);
    median_filter_reset(&fc->median);
    fir_filter_reset(&fc->fir);
    biquad_filter_reset(&fc->biquad);
}
