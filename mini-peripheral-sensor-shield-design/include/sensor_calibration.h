/**
 * @file    sensor_calibration.h
 * @brief   L5 Sensor Calibration Algorithms - Linearization, Polynomial Fit, Lookup Tables
 *
 * @details Implements calibration methods for sensor shields:
 *          - Steinhart-Hart 3-point thermistor calibration
 *          - Linear regression (least squares) for linear sensors
 *          - Polynomial calibration with order selection
 *          - Multi-point calibration with interpolation
 *          - Lookup table generation and interpolation
 *          - Two-point offset/gain calibration
 *          - Temperature compensation for non-temperature sensors
 *
 * Knowledge Mapping:
 *   L1 - Calibration types: offset, gain, linearity, temperature
 *   L3 - Least squares, matrix inversion (2x2, 3x3), polynomial fitting
 *   L4 - Steinhart-Hart equation, Callendar-Van Dusen, Seebeck polynomial
 *   L5 - Linear regression algorithms, Gauss-Jordan elimination, interpolation
 *   L6 - Multi-point calibration of a thermistor shield with Arduino
 *   L7 - Factory calibration data stored in MCU EEPROM
 *
 * Reference: NIST calibration guidelines
 *            ISO/IEC 17025 General requirements for test and calibration
 *            Press et al., "Numerical Recipes in C" (3rd ed., 2007)
 */

#ifndef SENSOR_CALIBRATION_H
#define SENSOR_CALIBRATION_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "sensor_types.h"

/* ---- Calibration Point (L1) ---- */
typedef struct {
    double reference_value;          /* known true value (standard) */
    double measured_value;           /* raw sensor reading */
    double temperature_c;            /* ambient temp during measurement */
    double timestamp_s;              /* relative time of measurement */
    bool is_valid;
} calibration_point_t;

/* ---- Calibration Type (L1) ---- */
typedef enum {
    CALIB_OFFSET_ONLY = 0,           /* single point, add constant */
    CALIB_GAIN_OFFSET,               /* two point, slope + intercept */
    CALIB_MULTI_POINT_LINEAR,        /* multiple points, least squares */
    CALIB_POLYNOMIAL,                /* polynomial fit of order N */
    CALIB_STEINHART_HART,            /* 3-point S-H for thermistors */
    CALIB_CALLENDAR_VAN_DUSEN,       /* RTD calibration */
    CALIB_LOOKUP_TABLE,              /* discrete points + interpolation */
    CALIB_TEMP_COMPENSATION          /* compensate for temp drift */
} calibration_type_t;

/* ---- Calibration Result (L3-L5) ---- */
typedef struct {
    calibration_type_t type;
    uint8_t num_points;               /* number of calibration points used */
    uint8_t polynomial_order;         /* order if polynomial fit */
    double coefficients[10];          /* cal coefficients (offset, gain, poly...) */
    double residuals[20];             /* per-point fitting residuals */
    double r_squared;                 /* goodness of fit (0-1) */
    double max_error;                 /* maximum residual error */
    double rms_error;                 /* root-mean-square error */
    bool is_valid;
    uint64_t calibration_timestamp;   /* when calibration was performed */
    char calibration_standard[32];    /* reference standard used */
} calibration_result_t;

/* ---- Lookup Table for Non-Linear Sensors (L5) ---- */
#define LUT_MAX_ENTRIES 256
typedef struct {
    uint16_t num_entries;
    double input[LUT_MAX_ENTRIES];    /* measured values (sorted ascending) */
    double output[LUT_MAX_ENTRIES];   /* corresponding true values */
    double input_min; double input_max;
    bool is_sorted;
} lookup_table_t;

/* ---- Temperature Compensation Model (L5-L7) ----
 * Compensates a sensor reading R(T) at temperature T to
 * equivalent reading R(T_ref) at reference temperature T_ref.
 * Model: R_corrected = R_measured / (1 + alpha*(T-T_ref) + beta*(T-T_ref)^2) */
typedef struct {
    double T_ref_c;                   /* reference temperature (typically 25C) */
    double alpha;                     /* linear temp coefficient */
    double beta;                      /* quadratic temp coefficient */
    double temp_range_min_c;          /* valid compensation range */
    double temp_range_max_c;
} temp_compensation_model_t;

/* ---- API: Calibration Functions (L5) ---- */

/* General calibration */
void calibration_result_init(calibration_result_t *cr, calibration_type_t type);
int calibration_perform(const calibration_point_t *points, uint8_t num_points,
                        calibration_type_t type, calibration_result_t *result);
double calibration_apply(const calibration_result_t *cr, double measured_value);
double calibration_inverse(const calibration_result_t *cr, double true_value);

/* Linear regression (least squares) */
int calibration_linear_fit(const double *x, const double *y, size_t n,
                            double *slope, double *intercept, double *r_squared);
int calibration_two_point(double x1, double y1, double x2, double y2,
                           double *slope, double *intercept);

/* Polynomial calibration */
int calibration_polynomial_fit(const double *x, const double *y, size_t n,
                                uint8_t order, calibration_result_t *result);
double calibration_poly_evaluate(const calibration_result_t *cr, double x);

/* Steinhart-Hart thermistor calibration (3-point) */
int calibration_steinhart_hart(const double temps_c[3], const double resistances[3],
                                double *A, double *B, double *C);
double calibration_steinhart_forward(double A, double B, double C, double R);
double calibration_steinhart_inverse(double A, double B, double C, double T_c);

/* Lookup table */
void lookup_table_init(lookup_table_t *lut);
int lookup_table_add_point(lookup_table_t *lut, double input, double output);
int lookup_table_sort(lookup_table_t *lut);
double lookup_table_interpolate(const lookup_table_t *lut, double input);
double lookup_table_linear_interp(const lookup_table_t *lut, double input);
int lookup_table_from_calibration(lookup_table_t *lut, const calibration_result_t *cr,
                                   double min_val, double max_val, uint16_t steps);

/* Temperature compensation */
void temp_compensation_init(temp_compensation_model_t *tc, double T_ref);
double temp_compensation_apply(const temp_compensation_model_t *tc,
                                double measured_value, double current_temp_c);
int temp_compensation_calibrate(temp_compensation_model_t *tc,
                                 const double *values, const double *temps, size_t n);

/* Gauss-Jordan elimination for solving linear systems (L5) */
int gauss_jordan_solve(double *A, double *b, size_t n, double *x);
int matrix_invert_2x2(const double A[4], double A_inv[4]);
int matrix_invert_3x3(const double A[9], double A_inv[9]);

#endif /* SENSOR_CALIBRATION_H */
