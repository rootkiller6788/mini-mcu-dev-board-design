/**
 * @file    thermal_design.h
 * @brief   Thermal analysis and management for ESP32 IoT board layout.
 *
 * Junction temperature estimation, heatsink sizing, copper area for
 * cooling, thermal via arrays, and convection/radiation models.
 *
 * Knowledge mapping:
 *   L1 - Definitions:    Rth_ja, Rth_jc, Theta_JA, junction temperature
 *   L2 - Core Concepts:  thermal resistance network, parallel heat paths
 *   L3 - Math:           Fourier heat equation, Newton cooling law
 *   L4 - Fundamental:    conservation of energy, thermal Ohm law
 *   L5 - Algorithms:     heatsink selection, thermal via optimization
 *
 * References:
 *   - JESD51-1: Integrated Circuit Thermal Measurement Method
 *   - Lienhard & Lienhard, A Heat Transfer Textbook, 5th Ed., 2019
 *   - Espressif ESP32 Datasheet, Thermal Characteristics
 *
 * @author  mini-esp32-iot-board-layout
 * @date    2026-06-21
 */

#ifndef THERMAL_DESIGN_H
#define THERMAL_DESIGN_H

#include "board_geometry.h"

#ifdef __cplusplus
extern "C" {
#endif

/* --- Thermal resistance types ----------------------------------------- */

typedef struct {
    double  Rth_jc;
    double  Rth_cs;
    double  Rth_sa;
    double  Rth_ja;
    double  T_junction_max;
    double  T_ambient;
} thermal_params_t;

typedef struct {
    double  length_mm;
    double  width_mm;
    double  height_mm;
    double  thermal_conductivity;
    int     num_fins;
    double  fin_height_mm;
    double  fin_thickness_mm;
} heatsink_params_t;

typedef struct {
    int     num_vias;
    double  via_diam_mm;
    double  via_pitch_mm;
    double  pad_size_mm;
    double  cu_plating_um;
    double  board_thickness_mm;
    double  effective_Rth;
} thermal_via_array_t;

typedef struct {
    double  area_mm2;
    double  copper_thickness_um;
    double  temp_rise_c;
    double  max_power_w;
} copper_pour_cooling_t;

#define K_COPPER        398.0
#define K_ALUMINUM      205.0
#define K_FR4           0.3
#define K_ALUMINA       25.0
#define K_SILICON       148.0
#define K_SOLDER_MASK   0.2
#define K_THERMAL_PASTE 1.0

#define H_NATURAL_CONVECTION   10.0
#define H_FORCED_CONVECTION_1MS  25.0
#define H_FORCED_CONVECTION_2MS  45.0

double junction_temperature(double T_ambient, double P_diss_w, double Rth_ja);
double heatsink_thermal_resistance(double T_j_max, double T_ambient,
                                     double P_diss_w, double Rth_jc,
                                     double Rth_cs);
double copper_area_for_cooling(double P_diss_w, double delta_T_c,
                                double copper_thickness_um, int is_internal);
double thermal_via_resistance(thermal_via_array_t *via_array);
double thermal_resistance_plane(double area_mm2, double thickness_mm,
                                 double conductivity);
double parallel_thermal_resistance(double R1, double R2);
double series_thermal_resistance(double R1, double R2);
double natural_convection_power(double area_mm2, double T_surface_c,
                                 double T_ambient_c);
double radiation_heat_power(double area_mm2, double emissivity,
                              double T_surface_c, double T_ambient_c);
double max_power_budget(double T_j_max, double T_ambient, double Rth_total);
double forced_convection_heatsink_Rth(double Rth_natural, double airflow_m_s);
int heatsink_meets_requirement(const heatsink_params_t *hs,
                                const thermal_params_t *req, double P_diss_w);

#ifdef __cplusplus
}
#endif

#endif /* THERMAL_DESIGN_H */
