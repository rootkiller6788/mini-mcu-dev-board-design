/**
 * @file    power_integrity.h
 * @brief   Power Delivery Network (PDN) and decoupling design for ESP32 IoT boards.
 *
 * Target impedance method, plane capacitance, decoupling capacitor
 * selection and anti-resonance analysis, via inductance, IR drop.
 *
 * Knowledge mapping:
 *   L1 - Definitions:    PDN, target impedance, plane capacitance, ESL, ESR
 *   L2 - Core Concepts:  decoupling hierarchy, anti-resonance, via inductance
 *   L3 - Math:           RLC network impedance, parallel resonance
 *   L4 - Fundamental:    Kirchhoff laws in PDN, maximum power transfer
 *   L5 - Algorithms:     multi-stage decoupling optimization
 *
 * References:
 *   - Smith, L.D. "Power Distribution System Design Methodology..."
 *     IEEE EMC Symposium, 1999
 *   - Swaminathan & Engin, "Power Integrity Modeling and Design", 2007
 *   - Bogatin, E. "Signal and Power Integrity - Simplified", 3rd Ed., 2018
 *
 * @author  mini-esp32-iot-board-layout
 * @date    2026-06-21
 */

#ifndef POWER_INTEGRITY_H
#define POWER_INTEGRITY_H

#include "board_geometry.h"

#ifdef __cplusplus
extern "C" {
#endif

/* --- Decoupling Capacitor Model ---------------------------------------- */

typedef struct {
    double  capacitance_f;
    double  esl_h;
    double  esr_ohm;
    double  voltage_rating;
    const char *package;
    const char *dielectric;
} decap_model_t;

typedef struct {
    double  freq_hz;
    double  Z_mag_ohm;
    double  Z_phase_rad;
} imp_point_t;

typedef struct {
    double  Vdd;
    double  ripple_pct;
    double  I_max_a;
    double  I_transient_a;
    double  t_rise_ns;
    double  f_min_hz;
    double  f_max_hz;
} pdn_spec_t;

typedef struct {
    double  width_mm;
    double  length_mm;
    double  separation_mm;
    double  er;
    double  tan_delta;
} plane_params_t;

typedef struct {
    int     num_stages;
    decap_model_t *stages;
    int     *quantities;
    double  max_z_ohm;
    double  target_z_ohm;
    double  margin_db;
} pdn_result_t;

/* Standard decap presets */
#define DECAP_100NF_0402  ((decap_model_t){100e-9, 0.4e-9, 0.05, 16.0, "0402", "X7R"})
#define DECAP_10NF_0402   ((decap_model_t){10e-9,  0.3e-9, 0.08, 16.0, "0402", "X7R"})
#define DECAP_1UF_0402    ((decap_model_t){1e-6,   0.45e-9, 0.03, 10.0, "0402", "X5R"})
#define DECAP_10UF_0603   ((decap_model_t){10e-6,  0.8e-9, 0.01, 10.0, "0603", "X5R"})
#define DECAP_4_7UF_0603  ((decap_model_t){4.7e-6, 0.7e-9, 0.02, 10.0, "0603", "X5R"})
#define DECAP_22UF_0805   ((decap_model_t){22e-6,  1.2e-9, 0.005, 10.0,"0805", "X5R"})

/* --- Core Functions ---------------------------------------------------- */

double pdn_target_impedance(const pdn_spec_t *spec);
double decap_impedance(const decap_model_t *cap, double freq_hz);
double decap_srf(const decap_model_t *cap);
double decap_bank_impedance(const decap_model_t *cap, int n, double freq_hz);
double decap_anti_resonance_freq(const decap_model_t *cap1, const decap_model_t *cap2);
double plane_capacitance(const plane_params_t *pp);
double plane_spreading_inductance(const plane_params_t *pp);
double plane_resonance_freq(const plane_params_t *pp, int m, int n);
double via_inductance(double h_mm, double d_mm);
double via_pair_inductance(double h_mm, double d_mm, double s_mm);
void pdn_impedance_sweep(const decap_model_t *stages, const int *quantities,
                          int num_stages, double f_start_hz, double f_stop_hz,
                          int points_per_decade, double *max_z, double *f_max_z);
double ir_drop(double current_a, double resistivity_ohm_m,
               double length_mm, double width_mm, double thickness_um);
int pdn_meets_spec(const pdn_result_t *result, const pdn_spec_t *spec);

#ifdef __cplusplus
}
#endif

#endif /* POWER_INTEGRITY_H */
