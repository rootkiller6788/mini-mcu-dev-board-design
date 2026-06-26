/**
 * @file    sensor_calibration.c
 * @brief   L5 Sensor Calibration Algorithms - Linear Regression, Polynomial Fit, Lookup Tables
 *
 * @details Implements sensor calibration methods:
 *          - Two-point offset/gain calibration
 *          - Linear least squares regression
 *          - Polynomial fitting (up to 3rd order)
 *          - Steinhart-Hart 3-point thermistor calibration
 *          - Lookup table with linear interpolation
 *          - Temperature compensation
 *          - Gauss-Jordan elimination for solving linear systems
 *
 * Knowledge Mapping:
 *   L3 - Least squares, matrix operations, polynomial evaluation
 *   L4 - Steinhart-Hart equation, linear models
 *   L5 - Gauss-Jordan elimination, interpolation algorithms
 *   L6 - Factory calibration of sensor shield with reference standards
 *   L7 - Calibration data stored in MCU EEPROM for field deployment
 *
 * Reference: Press et al., "Numerical Recipes in C" (3rd ed., 2007)
 *            NIST Engineering Statistics Handbook
 */

#include "sensor_calibration.h"
#include <math.h>
#include <string.h>
#include <stdio.h>

/* ---- Calibration Result Init ---- */

void calibration_result_init(calibration_result_t *cr, calibration_type_t type) {
    if (!cr) return;
    memset(cr, 0, sizeof(*cr));
    cr->type = type;
    cr->is_valid = false;
}

/* ---- Two-Point Calibration ---- */

int calibration_two_point(double x1, double y1, double x2, double y2,
                           double *slope, double *intercept) {
    if (!slope || !intercept) return -1;
    double dx = x2 - x1;
    if (fabs(dx) < 1e-15) return -2;
    *slope = (y2 - y1) / dx;
    *intercept = y1 - (*slope) * x1;
    return 0;
}

/* ---- Linear Least Squares Regression (L5) ---- */

int calibration_linear_fit(const double *x, const double *y, size_t n,
                            double *slope, double *intercept, double *r2) {
    if (!x || !y || n < 2) return -1;
    double sum_x = 0.0, sum_y = 0.0, sum_xy = 0.0, sum_xx = 0.0, sum_yy = 0.0;
    for (size_t i = 0; i < n; i++) {
        sum_x += x[i]; sum_y += y[i];
        sum_xy += x[i] * y[i];
        sum_xx += x[i] * x[i];
        sum_yy += y[i] * y[i];
    }
    double denom = (double)n * sum_xx - sum_x * sum_x;
    if (fabs(denom) < 1e-15) return -2;
    double m = ((double)n * sum_xy - sum_x * sum_y) / denom;
    double b = (sum_y - m * sum_x) / (double)n;
    if (slope) *slope = m;
    if (intercept) *intercept = b;
    if (r2) {
        double ss_res = 0.0, ss_tot = 0.0;
        double y_mean = sum_y / (double)n;
        for (size_t i = 0; i < n; i++) {
            double y_pred = m * x[i] + b;
            ss_res += (y[i] - y_pred) * (y[i] - y_pred);
            ss_tot += (y[i] - y_mean) * (y[i] - y_mean);
        }
        *r2 = (ss_tot > 1e-15) ? 1.0 - ss_res / ss_tot : 0.0;
    }
    return 0;
}

/* ---- Polynomial Calibration (L5) ---- */

int calibration_polynomial_fit(const double *x, const double *y, size_t n,
                                uint8_t order, calibration_result_t *result) {
    if (!x || !y || !result || n < (size_t)(order + 1) || order > 3) return -1;
    /* Build normal equations: A^T A * c = A^T y
     * A_ij = x_i^j, where j = 0..order */
    uint8_t m = order + 1;
    double ATA[16] = {0}; /* max 4x4 */
    double ATy[4] = {0};
    for (size_t i = 0; i < n; i++) {
        double xp[4] = {1.0, x[i], x[i]*x[i], x[i]*x[i]*x[i]};
        for (int r = 0; r < m; r++) {
            ATy[r] += xp[r] * y[i];
            for (int c = 0; c < m; c++) {
                ATA[r*m + c] += xp[r] * xp[c];
            }
        }
    }
    /* Solve ATA * c = ATy using Gaussian elimination.
     * Augmented matrix: [ATA | ATy], size m x (m+1), stride = m+1 */
    double aug[20] = {0}; /* max 4x5 = 20 */
    uint8_t stride = m + 1;
    for (int i = 0; i < m; i++) {
        for (int j = 0; j < m; j++) aug[i*stride + j] = ATA[i*m + j];
        aug[i*stride + m] = ATy[i]; /* last column = RHS */
    }
    /* Forward elimination */
    for (int col = 0; col < m; col++) {
        double diag = aug[col*stride + col];
        if (fabs(diag) < 1e-15) return -2;
        for (int j = col; j <= m; j++) aug[col*stride + j] /= diag;
        for (int row = col+1; row < m; row++) {
            double factor = aug[row*stride + col];
            for (int j = col; j <= m; j++) aug[row*stride + j] -= factor * aug[col*stride + j];
        }
    }
    /* Back substitution */
    for (int i = m-1; i >= 0; i--) {
        double sum = aug[i*stride + m];
        for (int j = i+1; j < m; j++) sum -= aug[i*stride + j] * result->coefficients[j];
        result->coefficients[i] = sum;
    }
    result->type = CALIB_POLYNOMIAL;
    result->polynomial_order = order;
    result->num_points = (uint8_t)n;
    /* Calculate residuals and R^2 */
    double ss_res = 0.0, ss_tot = 0.0, y_mean = 0.0, max_err = 0.0;
    for (size_t i = 0; i < n; i++) y_mean += y[i];
    y_mean /= (double)n;
    for (size_t i = 0; i < n && i < 20; i++) {
        double y_pred = calibration_poly_evaluate(result, x[i]);
        double resid = y[i] - y_pred;
        result->residuals[i] = resid;
        ss_res += resid * resid;
        ss_tot += (y[i] - y_mean) * (y[i] - y_mean);
        if (fabs(resid) > max_err) max_err = fabs(resid);
    }
    result->r_squared = (ss_tot > 1e-15) ? 1.0 - ss_res / ss_tot : 0.0;
    result->max_error = max_err;
    result->rms_error = sqrt(ss_res / (double)n);
    result->is_valid = true;
    return 0;
}

double calibration_poly_evaluate(const calibration_result_t *cr, double x) {
    if (!cr) return x;
    double result = cr->coefficients[0];
    double xp = x;
    for (int i = 1; i <= cr->polynomial_order; i++) {
        result += cr->coefficients[i] * xp;
        xp *= x;
    }
    return result;
}

/* ---- Steinhart-Hart Calibration (L4-L5) ---- */

int calibration_steinhart_hart(const double temps_c[3], const double resistances[3],
                                double *A, double *B, double *C) {
    if (!temps_c || !resistances || !A || !B || !C) return -1;
    /* 3-point calibration: solve for A, B, C */
    double lnR[3], invT[3];
    for (int i = 0; i < 3; i++) {
        if (resistances[i] <= 0.0) return -2;
        lnR[i] = log(resistances[i]);
        invT[i] = 1.0 / (temps_c[i] + 273.15);
    }
    /* 3x3 system: for each point i, A + B*lnR_i + C*lnR_i^3 = 1/T_i
     * Matrix row i: [1, lnR_i, lnR_i^3] */
    double M[9] = {
        1.0, lnR[0], lnR[0]*lnR[0]*lnR[0],
        1.0, lnR[1], lnR[1]*lnR[1]*lnR[1],
        1.0, lnR[2], lnR[2]*lnR[2]*lnR[2]
    };
    double rhs[3] = {invT[0], invT[1], invT[2]};
    /* Gauss elimination with partial pivoting */
    for (int col = 0; col < 3; col++) {
        int pivot = col;
        double max_v = fabs(M[col*3 + col]);
        for (int row = col+1; row < 3; row++) {
            if (fabs(M[row*3 + col]) > max_v) { max_v = fabs(M[row*3 + col]); pivot = row; }
        }
        if (max_v < 1e-15) return -3;
        if (pivot != col) {
            for (int j=0; j<3; j++) { double t=M[col*3+j]; M[col*3+j]=M[pivot*3+j]; M[pivot*3+j]=t; }
            double t=rhs[col]; rhs[col]=rhs[pivot]; rhs[pivot]=t;
        }
        double diag = M[col*3 + col];
        for (int j=col; j<3; j++) M[col*3+j] /= diag;
        rhs[col] /= diag;
        for (int row=col+1; row<3; row++) {
            double f = M[row*3+col];
            for (int j=col; j<3; j++) M[row*3+j] -= f * M[col*3+j];
            rhs[row] -= f * rhs[col];
        }
    }
    for (int i=2; i>=0; i--) {
        for (int j=i+1; j<3; j++) rhs[i] -= M[i*3+j] * rhs[j];
    }
    *A = rhs[0]; *B = rhs[1]; *C = rhs[2];
    return 0;
}

double calibration_steinhart_forward(double A, double B, double C, double R) {
    if (R <= 0.0) return 25.0;
    double lnR = log(R);
    double invT = A + B*lnR + C*lnR*lnR*lnR;
    if (invT <= 0.0) return 300.0;
    return 1.0/invT - 273.15;
}

double calibration_steinhart_inverse(double A, double B, double C, double T_c) {
    /* Given T, find R using Newton's method on: A + B*lnR + C*(lnR)^3 - 1/(T+273.15) = 0 */
    double invT = 1.0 / (T_c + 273.15);
    double lnR = log(10000.0); /* initial guess: 10k at 25C */
    for (int i = 0; i < 20; i++) {
        double f = A + B*lnR + C*lnR*lnR*lnR - invT;
        double df = B + 3.0*C*lnR*lnR;
        if (fabs(df) < 1e-15) break;
        double dlnR = -f / df;
        lnR += dlnR;
        if (fabs(dlnR) < 1e-12) break;
    }
    return exp(lnR);
}

/* ---- General Calibration API ---- */

int calibration_perform(const calibration_point_t *pts, uint8_t n,
                        calibration_type_t type, calibration_result_t *result) {
    if (!pts || !result || n < 2) return -1;
    double x[32], y[32];
    for (int i = 0; i < n && i < 32; i++) { x[i] = pts[i].measured_value; y[i] = pts[i].reference_value; }
    switch (type) {
        case CALIB_GAIN_OFFSET:
            return calibration_two_point(x[0], y[0], x[1], y[1],
                   &result->coefficients[1], &result->coefficients[0]);
        case CALIB_MULTI_POINT_LINEAR:
            return calibration_linear_fit(x, y, n, &result->coefficients[1],
                   &result->coefficients[0], &result->r_squared);
        case CALIB_POLYNOMIAL:
            return calibration_polynomial_fit(x, y, n, 2, result);
        case CALIB_STEINHART_HART:
            if (n < 3) return -2;
            return calibration_steinhart_hart(
                (double[]){pts[0].reference_value, pts[1].reference_value, pts[2].reference_value},
                (double[]){pts[0].measured_value, pts[1].measured_value, pts[2].measured_value},
                &result->coefficients[0], &result->coefficients[1], &result->coefficients[2]);
        default:
            return -3;
    }
}

double calibration_apply(const calibration_result_t *cr, double measured) {
    if (!cr || !cr->is_valid) return measured;
    switch (cr->type) {
        case CALIB_GAIN_OFFSET:
        case CALIB_MULTI_POINT_LINEAR:
            return cr->coefficients[0] + cr->coefficients[1] * measured;
        case CALIB_POLYNOMIAL:
            return calibration_poly_evaluate(cr, measured);
        case CALIB_STEINHART_HART:
            return calibration_steinhart_forward(
                cr->coefficients[0], cr->coefficients[1], cr->coefficients[2], measured);
        default:
            return measured;
    }
}

double calibration_inverse(const calibration_result_t *cr, double true_val) {
    if (!cr || !cr->is_valid) return true_val;
    if (cr->type == CALIB_GAIN_OFFSET || cr->type == CALIB_MULTI_POINT_LINEAR) {
        if (cr->coefficients[1] == 0.0) return true_val;
        return (true_val - cr->coefficients[0]) / cr->coefficients[1];
    }
    /* Newton iteration for inverse */
    double x = true_val;
    for (int i = 0; i < 30; i++) {
        double fx = calibration_apply(cr, x);
        double dx = x * 0.001 + 0.001;
        double df = (calibration_apply(cr, x+dx) - fx) / dx;
        if (fabs(df) < 1e-15) break;
        double step = (true_val - fx) / df;
        x += step;
        if (fabs(step) < 1e-9) break;
    }
    return x;
}

/* ---- Lookup Table (L5) ---- */

void lookup_table_init(lookup_table_t *lut) {
    if (!lut) return;
    memset(lut, 0, sizeof(*lut));
    lut->is_sorted = true;
}

int lookup_table_add_point(lookup_table_t *lut, double in, double out) {
    if (!lut || lut->num_entries >= LUT_MAX_ENTRIES) return -1;
    lut->input[lut->num_entries] = in;
    lut->output[lut->num_entries] = out;
    lut->num_entries++;
    lut->is_sorted = false;
    if (lut->num_entries == 1) { lut->input_min = in; lut->input_max = in; }
    else { if (in < lut->input_min) lut->input_min = in; if (in > lut->input_max) lut->input_max = in; }
    return 0;
}

int lookup_table_sort(lookup_table_t *lut) {
    if (!lut || lut->num_entries <= 1) return 0;
    /* Simple insertion sort */
    for (uint16_t i = 1; i < lut->num_entries; i++) {
        double key_in = lut->input[i], key_out = lut->output[i];
        int j = i - 1;
        while (j >= 0 && lut->input[j] > key_in) {
            lut->input[j+1] = lut->input[j];
            lut->output[j+1] = lut->output[j];
            j--;
        }
        lut->input[j+1] = key_in; lut->output[j+1] = key_out;
    }
    lut->is_sorted = true;
    return 0;
}

double lookup_table_interpolate(const lookup_table_t *lut, double in) {
    return lookup_table_linear_interp(lut, in);
}

double lookup_table_linear_interp(const lookup_table_t *lut, double in) {
    if (!lut || lut->num_entries == 0) return in;
    if (!lut->is_sorted) return in;
    if (in <= lut->input[0]) return lut->output[0];
    if (in >= lut->input[lut->num_entries-1]) return lut->output[lut->num_entries-1];
    for (uint16_t i = 0; i < lut->num_entries - 1; i++) {
        if (in >= lut->input[i] && in <= lut->input[i+1]) {
            double t = (in - lut->input[i]) / (lut->input[i+1] - lut->input[i]);
            return lut->output[i] + t * (lut->output[i+1] - lut->output[i]);
        }
    }
    return in;
}

int lookup_table_from_calibration(lookup_table_t *lut, const calibration_result_t *cr,
                                   double min_v, double max_v, uint16_t steps) {
    if (!lut || !cr || steps < 2 || steps > LUT_MAX_ENTRIES) return -1;
    lookup_table_init(lut);
    double step = (max_v - min_v) / (double)(steps - 1);
    for (uint16_t i = 0; i < steps; i++) {
        double in = min_v + step * i;
        double out = calibration_apply(cr, in);
        lookup_table_add_point(lut, in, out);
    }
    lookup_table_sort(lut);
    return 0;
}

/* ---- Temperature Compensation (L5-L7) ---- */

void temp_compensation_init(temp_compensation_model_t *tc, double T_ref) {
    if (!tc) return;
    memset(tc, 0, sizeof(*tc));
    tc->T_ref_c = T_ref;
    tc->temp_range_min_c = -40.0;
    tc->temp_range_max_c = 85.0;
}

double temp_compensation_apply(const temp_compensation_model_t *tc,
                                double value, double current_temp_c) {
    if (!tc) return value;
    double dT = current_temp_c - tc->T_ref_c;
    double denom = 1.0 + tc->alpha * dT + tc->beta * dT * dT;
    if (fabs(denom) < 1e-15) return value;
    return value / denom;
}

int temp_compensation_calibrate(temp_compensation_model_t *tc,
                                 const double *values, const double *temps, size_t n) {
    /* Fit: value(T) = value_ref * (1 + alpha*(T-Tref) + beta*(T-Tref)^2)
     * Simplified: fit alpha only (linear), set beta=0 */
    if (!tc || !values || !temps || n < 3) return -1;
    double sum_x=0, sum_y=0, sum_xy=0, sum_xx=0;
    for (size_t i = 0; i < n; i++) {
        double dT = temps[i] - tc->T_ref_c;
        double ratio = values[i] / values[0]; /* normalize to first value */
        sum_x += dT; sum_y += (ratio - 1.0);
        sum_xy += dT * (ratio - 1.0); sum_xx += dT * dT;
    }
    double denom = (double)n * sum_xx - sum_x * sum_x;
    if (fabs(denom) < 1e-15) return -2;
    tc->alpha = ((double)n * sum_xy - sum_x * sum_y) / denom;
    tc->beta = 0.0;
    return 0;
}

/* ---- Gauss-Jordan Elimination (L5) ---- */

int gauss_jordan_solve(double *A, double *b, size_t n, double *x) {
    if (!A || !b || !x || n < 1 || n > 10) return -1;
    /* Augment A with b, then eliminate to identity */
    double aug[100] = {0}; /* max 10x11 */
    for (size_t i = 0; i < n; i++) {
        for (size_t j = 0; j < n; j++) aug[i*(n+1)+j] = A[i*n+j];
        aug[i*(n+1)+n] = b[i];
    }
    for (size_t col = 0; col < n; col++) {
        double diag = aug[col*(n+1)+col];
        if (fabs(diag) < 1e-15) return -2;
        for (size_t j = 0; j <= n; j++) aug[col*(n+1)+j] /= diag;
        for (size_t row = 0; row < n; row++) {
            if (row == col) continue;
            double factor = aug[row*(n+1)+col];
            for (size_t j = 0; j <= n; j++) aug[row*(n+1)+j] -= factor * aug[col*(n+1)+j];
        }
    }
    for (size_t i = 0; i < n; i++) x[i] = aug[i*(n+1)+n];
    return 0;
}

int matrix_invert_2x2(const double A[4], double A_inv[4]) {
    if (!A || !A_inv) return -1;
    double det = A[0]*A[3] - A[1]*A[2];
    if (fabs(det) < 1e-15) return -2;
    A_inv[0] =  A[3]/det; A_inv[1] = -A[1]/det;
    A_inv[2] = -A[2]/det; A_inv[3] =  A[0]/det;
    return 0;
}

int matrix_invert_3x3(const double A[9], double A_inv[9]) {
    if (!A || !A_inv) return -1;
    double det = A[0]*(A[4]*A[8]-A[5]*A[7]) - A[1]*(A[3]*A[8]-A[5]*A[6]) + A[2]*(A[3]*A[7]-A[4]*A[6]);
    if (fabs(det) < 1e-15) return -2;
    A_inv[0] = (A[4]*A[8]-A[5]*A[7])/det;
    A_inv[1] = (A[2]*A[7]-A[1]*A[8])/det;
    A_inv[2] = (A[1]*A[5]-A[2]*A[4])/det;
    A_inv[3] = (A[5]*A[6]-A[3]*A[8])/det;
    A_inv[4] = (A[0]*A[8]-A[2]*A[6])/det;
    A_inv[5] = (A[2]*A[3]-A[0]*A[5])/det;
    A_inv[6] = (A[3]*A[7]-A[4]*A[6])/det;
    A_inv[7] = (A[1]*A[6]-A[0]*A[7])/det;
    A_inv[8] = (A[0]*A[4]-A[1]*A[3])/det;
    return 0;
}
