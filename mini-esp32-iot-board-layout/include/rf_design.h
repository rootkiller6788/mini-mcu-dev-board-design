/**
 * @file    rf_design.h
 * @brief   RF matching network design and Smith chart computations.
 *
 * Implements L-match, Pi-match, T-match network synthesis,
 * reflection coefficient, VSWR, return/insertion loss calculations.
 * Critical for ESP32 antenna matching and RF front-end design.
 *
 * Knowledge mapping:
 *   L1 - Definitions:    Z0, Gamma, VSWR, return loss, insertion loss
 *   L2 - Core Concepts:  impedance matching, conjugate match, bandwidth
 *   L3 - Math:           bilinear transform, Smith chart geometry
 *   L4 - Fundamental:    maximum power transfer theorem
 *   L5 - Algorithms:     L-match, Pi-match, T-match synthesis
 *   L6 - Canonical:      antenna impedance matching problem
 *
 * References:
 *   - Pozar, D.M. "Microwave Engineering", 4th Ed., 2012
 *   - Ludwig & Bogdanov, "RF Circuit Design", 2nd Ed., 2009
 *   - Bowick, C. "RF Circuit Design", 2nd Ed., 2008
 *
 * @author  mini-esp32-iot-board-layout
 * @date    2026-06-21
 */

#ifndef RF_DESIGN_H
#define RF_DESIGN_H

#include "board_geometry.h"
#include <complex.h>

#ifdef __cplusplus
extern "C" {
#endif

/* --- Complex impedance type ------------------------------------------- */

typedef struct {
    double real;
    double imag;
} complex_z_t;

typedef struct {
    double real;
    double imag;
} complex_y_t;

/* --- Matching network types ------------------------------------------- */

typedef enum {
    MATCH_L_SHUNT_C_SERIES_L,
    MATCH_L_SERIES_C_SHUNT_L,
    MATCH_L_SHUNT_L_SERIES_C,
    MATCH_L_SERIES_L_SHUNT_C,
    MATCH_PI,
    MATCH_T,
    MATCH_SINGLE_STUB_SERIES,
    MATCH_SINGLE_STUB_SHUNT,
    MATCH_QUARTER_WAVE
} match_type_t;

typedef struct {
    match_type_t type;
    int     num_components;
    int     is_series[6];
    double  component_value[6];
    int     component_is_L[6];
    double  bandwidth_hz;
    double  center_freq_hz;
    double  insertion_loss_db;
} match_network_t;

/* --- S-parameter structures ------------------------------------------- */

typedef struct {
    complex_z_t s11;
    complex_z_t s12;
    complex_z_t s21;
    complex_z_t s22;
    double      freq_hz;
} s_params_2port_t;

typedef struct {
    int     n_ports;
    complex_z_t *matrix;
    double  freq_hz;
} s_params_t;

/* --- Core Functions ---------------------------------------------------- */

complex_z_t reflection_coefficient(complex_z_t ZL, double Z0);
double vswr(complex_z_t gamma);
double return_loss_db(complex_z_t gamma);
double mismatch_loss_db(complex_z_t gamma);
double insertion_loss_db_s11(complex_z_t s11);
complex_z_t impedance_from_gamma(complex_z_t gamma, double Z0);
complex_z_t admittance_from_impedance(complex_z_t Z);

match_network_t l_match_synthesize(complex_z_t Z_source, complex_z_t Z_load,
                                    double freq_hz);
match_network_t pi_match_synthesize(complex_z_t Z_source, complex_z_t Z_load,
                                     double Q, double freq_hz);
match_network_t t_match_synthesize(complex_z_t Z_source, complex_z_t Z_load,
                                    double Q, double freq_hz);
match_network_t quarter_wave_transformer(double ZL_real, double Z0, double freq_hz,
                                          double er);

double stub_oc_input_impedance(double Z0, double length_mm, double lambda_g_mm);
double stub_sc_input_impedance(double Z0, double length_mm, double lambda_g_mm);
double stub_length_for_admittance(double Z0, double Y_target, double lambda_g_mm,
                                    int is_open_circuit);

double max_matching_bandwidth(complex_z_t Z_source, complex_z_t Z_load,
                               double freq_hz, double target_vswr);

double antenna_link_budget(double tx_power_dbm, double tx_gain_dbi,
                             double rx_gain_dbi, double distance_m,
                             double freq_hz);
double free_space_path_loss_db(double distance_m, double freq_hz);

#ifdef __cplusplus
}
#endif

#endif /* RF_DESIGN_H */
