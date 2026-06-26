/**
 * @file    thermal_design_cooling.c
 * @brief   Convection, radiation, power budget, and heatsink evaluation.
 * @author  mini-esp32-iot-board-layout
 * @date    2026-06-21
 */

#include "thermal_design.h"
#include <math.h>
#include <string.h>

/* Natural convection heat transfer power.
 * Q = h * A * (T_surface - T_ambient).
 * h ~ 10 W/(m^2*K) for natural convection.
 * Returns power in Watts. */
double natural_convection_power(double area_mm2, double T_surface_c,
                                 double T_ambient_c)
{
    if (area_mm2 <= 0.0) return -1.0;
    double delta_T = T_surface_c - T_ambient_c;
    if (delta_T < 0.0) return 0.0;
    return H_NATURAL_CONVECTION * area_mm2 * 1e-6 * delta_T;
}

/* Radiation heat transfer power (Stefan-Boltzmann law).
 * Q_rad = epsilon * sigma * A * (T_surface^4 - T_ambient^4).
 * sigma = 5.670367e-8 W/(m^2*K^4).
 * Temperatures in Celsius are converted to Kelvin.
 * Returns power in Watts. */
double radiation_heat_power(double area_mm2, double emissivity,
                              double T_surface_c, double T_ambient_c)
{
    if (area_mm2 <= 0.0 || emissivity <= 0.0 || emissivity > 1.0)
        return -1.0;
    double sigma = 5.670367e-8;
    double Ts_K = T_surface_c + 273.15;
    double Ta_K = T_ambient_c + 273.15;
    double Ts4 = Ts_K * Ts_K * Ts_K * Ts_K;
    double Ta4 = Ta_K * Ta_K * Ta_K * Ta_K;
    return emissivity * sigma * area_mm2 * 1e-6 * (Ts4 - Ta4);
}

/* Maximum power dissipation budget.
 * P_max = (T_j_max - T_ambient) / Rth_total.
 * Returns power in Watts. */
double max_power_budget(double T_j_max, double T_ambient,
                         double Rth_total)
{
    double delta_T = T_j_max - T_ambient;
    if (delta_T <= 0.0 || Rth_total <= 0.0) return 0.0;
    return delta_T / Rth_total;
}

/* Forced convection heatsink thermal resistance correction.
 * Rth_forced = Rth_natural / (1 + v * k_factor).
 * Simplistic model: airflow reduces Rth by factor ~(1 + 0.1*v).
 * v = airflow in m/s. Returns corrected Rth in K/W. */
double forced_convection_heatsink_Rth(double Rth_natural,
                                       double airflow_m_s)
{
    if (Rth_natural <= 0.0) return -1.0;
    if (airflow_m_s <= 0.0) return Rth_natural;
    return Rth_natural / (1.0 + 0.1 * airflow_m_s);
}

/* Check if a heatsink meets thermal requirements.
 * Compares total Rth_ja against the maximum allowed.
 * Returns 1 if heatsink is adequate, 0 otherwise. */
int heatsink_meets_requirement(const heatsink_params_t *hs,
                                const thermal_params_t *req,
                                double P_diss_w)
{
    if (!hs || !req || P_diss_w <= 0.0) return 0;
    /* Estimate Rth_sa from heatsink geometry.
     * Simplistic: Rth_sa ~ 1/(h*A_surface),
     * A_surface = base_area + fin_area */
    double base_area = hs->length_mm * hs->width_mm * 1e-6;
    double fin_area = hs->num_fins * hs->fin_height_mm
                    * hs->length_mm * 2.0 * 1e-6;
    double total_area = base_area + fin_area;
    if (total_area <= 0.0) return 0;
    double Rth_sa = 1.0 / (H_NATURAL_CONVECTION * total_area);
    double Rth_ja = req->Rth_jc + req->Rth_cs + Rth_sa;
    double T_j = junction_temperature(req->T_ambient, P_diss_w, Rth_ja);
    return (T_j <= req->T_junction_max) ? 1 : 0;
}
