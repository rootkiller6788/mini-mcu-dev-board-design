/**
 * @file    signal_integrity.h
 * @brief   Signal integrity analysis for high-speed traces on ESP32 IoT boards.
 *
 * Crosstalk estimation, impedance discontinuity, ground bounce,
 * simultaneous switching noise (SSN), eye diagram margin estimation.
 *
 * Knowledge mapping:
 *   L1 - Definitions:    crosstalk, NEXT, FEXT, ground bounce, SSN, eye diagram
 *   L2 - Core Concepts:  mutual inductance/capacitance, return path discontinuity
 *   L3 - Math:           coupled transmission line equations, modal decomposition
 *   L4 - Fundamental:    Faraday law (mutual L), Gauss law (mutual C)
 *   L5 - Algorithms:     NEXT/FEXT computation, eye margin estimation
 *
 * References:
 *   - Johnson and Graham, High-Speed Digital Design, 1993
 *   - Bogatin, E. Signal and Power Integrity Simplified, 3rd Ed., 2018
 *   - Hall and Heck, Advanced Signal Integrity, 2009
 *
 * @author  mini-esp32-iot-board-layout
 * @date    2026-06-21
 */

#ifndef SIGNAL_INTEGRITY_H
#define SIGNAL_INTEGRITY_H

#include "board_geometry.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    double  L_self_nH;
    double  L_mutual_nH;
    double  C_self_pF;
    double  C_mutual_pF;
    double  Z0_ohm;
    double  length_mm;
    double  t_rise_ps;
    double  v_prop_mm_per_ps;
} coupled_line_t;

typedef struct {
    double  NEXT_mv;
    double  FEXT_mv;
    double  NEXT_coefficient;
    double  FEXT_coefficient;
    double  signal_amplitude_v;
} crosstalk_result_t;

typedef struct {
    double  L_package_nH;
    double  L_gnd_nH;
    int     num_switching;
    double  di_dt_A_per_ns;
    double  V_dd;
} ssn_params_t;

typedef struct {
    double  Z1_ohm;
    double  Z2_ohm;
    double  incident_v;
} discontinuity_t;

typedef struct {
    double  eye_height_v;
    double  eye_width_ps;
    double  jitter_ps_rms;
    double  noise_v_rms;
    double  BER_estimate;
} eye_diagram_t;

typedef struct {
    int     num_vias;
    double  via_diam_mm;
    double  pad_diam_mm;
    double  antipad_diam_mm;
    double  board_thickness_mm;
    double  er;
} via_transition_t;

double reflection_coefficient_Z(double Z1, double Z2);
double reflected_voltage(double incident_v, double Z1, double Z2);
double transmitted_voltage(double incident_v, double Z1, double Z2);

void near_end_crosstalk(const coupled_line_t *line, double input_v,
                         crosstalk_result_t *result);
void far_end_crosstalk(const coupled_line_t *line, double input_v,
                        crosstalk_result_t *result);

double ground_bounce_voltage(double L_gnd_nH, double di_dt_A_per_ns);
double ssn_voltage(const ssn_params_t *params);
double ssn_quiet_low_noise(double L_pkg_nH, int N, double di_dt_A_per_ns);

double impedance_discontinuity_reflection(double Z1, double Z2);
double tdr_impedance_from_reflection(double Z0, double reflection_coeff);

void eye_diagram_estimate(double bit_period_ps, double v_swing,
                           double noise_rms, double jitter_rms,
                           eye_diagram_t *eye);

double rise_time_bandwidth(double t_rise_ps);
double via_impedance_discontinuity(const via_transition_t *via, double freq_hz);
double maximum_stub_length(double t_rise_ps, double er);
double bit_error_rate_q(double Q_factor);

#ifdef __cplusplus
}
#endif

#endif /* SIGNAL_INTEGRITY_H */
