/**
 * @file    thermal_design.c
 * @brief   Thermal analysis: junction temp, heatsink sizing, cooling.
 *
 * Implements junction temperature computation, required heatsink
 * thermal resistance, copper pour cooling area estimation,
 * thermal via array resistance, convection and radiation power,
 * and maximum power budget analysis per JESD51-1.
 * @author  mini-esp32-iot-board-layout
 * @date    2026-06-21
 */

#include "thermal_design.h"
#include <math.h>
#include <string.h>

/* Junction temperature: Tj = Ta + P * Rth_ja.
 * Fundamental thermal Ohm law: delta_T = P * Rth.
 * Ref: JESD51-1 */
double junction_temperature(double T_ambient, double P_diss_w,
                             double Rth_ja)
{
    if (P_diss_w < 0.0 || Rth_ja < 0.0) return -1.0;
    return T_ambient + P_diss_w * Rth_ja;
}

/* Heatsink thermal resistance required to keep Tj below Tj_max.
 * Rth_sa_req = (Tj_max - Ta) / P - Rth_jc - Rth_cs.
 * Returns negative if no heatsink needed (package alone suffices). */
double heatsink_thermal_resistance(double T_j_max, double T_ambient,
                                     double P_diss_w, double Rth_jc,
                                     double Rth_cs)
{
    if (P_diss_w <= 0.0) return INFINITY;
    double Rth_total = (T_j_max - T_ambient) / P_diss_w;
    double Rth_sa = Rth_total - Rth_jc - Rth_cs;
    return (Rth_sa > 0.0) ? Rth_sa : 0.0;
}

/* Copper area needed for PCB cooling (natural convection).
 * Approximate: A = P / (h * delta_T), where h ~ 10 W/(m^2*K).
 * Internal layers have ~3x worse cooling due to FR-4 insulation.
 * Returns area in mm^2. */
double copper_area_for_cooling(double P_diss_w, double delta_T_c,
                                double copper_thickness_um, int is_internal)
{
    (void)copper_thickness_um;
    if (P_diss_w <= 0.0 || delta_T_c <= 0.0) return -1.0;
    double h = is_internal ? 3.0 : 10.0;
    double area_m2 = P_diss_w / (h * delta_T_c);
    return area_m2 * 1e6;
}

/* Effective thermal resistance of a thermal via array.
 * Each via: Rth = h/(k_cu * A_cu), N vias in parallel.
 * Returns effective Rth in K/W. */
double thermal_via_resistance(thermal_via_array_t *va)
{
    if (!va || va->num_vias <= 0 || va->via_diam_mm <= 0.0 ||
        va->board_thickness_mm <= 0.0 || va->cu_plating_um <= 0.0)
        return -1.0;
    double d = va->via_diam_mm * 1e-3;
    double t = va->cu_plating_um * 1e-6;
    double h = va->board_thickness_mm * 1e-3;
    double A_per_via = M_PI * d * t;
    if (A_per_via <= 0.0) return -1.0;
    double Rth_per_via = h / (K_COPPER * A_per_via);
    va->effective_Rth = Rth_per_via / va->num_vias;
    return va->effective_Rth;
}

/* Thermal resistance of a plane slab.
 * Rth = thickness / (k * area). Fourier conduction in 1D.
 * Returns thermal resistance in K/W. */
double thermal_resistance_plane(double area_mm2, double thickness_mm,
                                 double conductivity)
{
    if (area_mm2 <= 0.0 || thickness_mm < 0.0 || conductivity <= 0.0)
        return -1.0;
    return thickness_mm * 1e-3
         / (conductivity * area_mm2 * 1e-6);
}

/* Parallel thermal resistance.
 * R_parallel = 1 / (1/R1 + 1/R2). */
double parallel_thermal_resistance(double R1, double R2)
{
    if (R1 <= 0.0 || R2 <= 0.0) return -1.0;
    return 1.0 / (1.0/R1 + 1.0/R2);
}

/* Series thermal resistance.
 * R_series = R1 + R2. */
double series_thermal_resistance(double R1, double R2)
{
    return R1 + R2;
}
