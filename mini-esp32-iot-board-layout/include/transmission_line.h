/**
 * @file    transmission_line.h
 * @brief   RF transmission line impedance & loss calculations.
 *
 * Closed-form impedance formulas for microstrip, stripline,
 * coplanar waveguide (CPW), and coupled differential pairs.
 * Includes dielectric and conductor loss models for IoT board traces.
 *
 * Knowledge mapping:
 *   L1 - Definitions:    Z0, effective er, propagation constant
 *   L2 - Core Concepts:  impedance control, dispersion, frequency-dependent loss
 *   L3 - Math:           Wheeler, Hammerstad-Jensen, Cohn formulas
 *   L4 - Fundamental:    Telegrapher's equations, wave equation
 *   L5 - Algorithms:     Newton-Raphson root finding for trace width
 *
 * References:
 *   - Wheeler, H.A. "Transmission-Line Properties of a Strip..."
 *     IEEE T-MTT, 1977
 *   - Hammerstad & Jensen, IEEE MTT-S, 1980
 *   - Cohn, S.B. IRE T-MTT, 1954
 *   - Johnson & Graham, "High-Speed Digital Design", 1993
 *
 * @author  mini-esp32-iot-board-layout
 * @date    2026-06-21
 */

#ifndef TRANSMISSION_LINE_H
#define TRANSMISSION_LINE_H

#include "board_geometry.h"

#ifdef __cplusplus
extern "C" {
#endif

/* --- Transmission Line Types ------------------------------------------- */

typedef enum {
    TL_MICROSTRIP,
    TL_EMBEDDED_MICROSTRIP,
    TL_STRIPLINE_SYMMETRIC,
    TL_STRIPLINE_ASYMMETRIC,
    TL_CPW,
    TL_CPWG,
    TL_DIFF_MICROSTRIP,
    TL_DIFF_STRIPLINE
} tl_type_t;

/** Design parameters for a single-ended transmission line */
typedef struct {
    tl_type_t   type;
    double      er;
    double      h_mm;
    double      w_mm;
    double      t_mm;
    double      gap_mm;
    double      h2_mm;
    double      freq_hz;
    double      tan_delta;
    double      roughness_um;
} tl_params_t;

/** Computed transmission line characteristics */
typedef struct {
    double  Z0_ohm;
    double  ereff;
    double  wavelength_mm;
    double  vp_m_per_s;
    double  tpd_ps_per_mm;
    double  alpha_d_db_per_mm;
    double  alpha_c_db_per_mm;
    double  alpha_total_db_per_mm;
} tl_result_t;

/** Differential pair parameters */
typedef struct {
    double  w_mm;
    double  s_mm;
    double  h_mm;
    double  t_mm;
    double  er;
} diff_pair_params_t;

/** Differential pair results */
typedef struct {
    double  Z0_odd_ohm;
    double  Z0_even_ohm;
    double  Z0_diff_ohm;
    double  Z0_common_ohm;
    double  coupling_coeff;
} diff_pair_result_t;

/* --- Core Functions ---------------------------------------------------- */

double microstrip_z0(double er, double h_mm, double w_mm, double t_mm);
double microstrip_ereff(double er, double h_mm, double w_mm, double t_mm);
double stripline_z0(double er, double h_mm, double w_mm, double t_mm);
double cpw_z0(double er, double h_mm, double w_mm, double gap_mm, double t_mm);
double cpwg_z0(double er, double h_mm, double w_mm, double gap_mm, double t_mm);

void tl_analyze(const tl_params_t *params, tl_result_t *result);

double microstrip_width_for_z0(double er, double h_mm, double t_mm,
                                double target_z0, double tolerance,
                                int max_iter);

double dielectric_loss_db_per_mm(double freq_hz, double ereff,
                                  double tan_delta);

double conductor_loss_db_per_mm(double freq_hz, double w_mm, double t_mm,
                                 double z0_ohm, double roughness_um);

double wavelength_in_substrate(double freq_hz, double ereff);

double propagation_delay_ps_per_mm(double ereff);

double critical_length_mm(double t_rise_ps, double tpd);

void diff_pair_analyze(const diff_pair_params_t *params,
                        diff_pair_result_t *result);

double diff_pair_max_mismatch(double t_budget_ps, double tpd);

double skin_depth_frequency(double t_mm, double resistivity_ohm_m);

double insertion_loss_db(double alpha_total, double length_mm);

#ifdef __cplusplus
}
#endif

#endif /* TRANSMISSION_LINE_H */
