/**
 * @file    thermal_via.c
 * @brief   Thermal via count, trace resistance, and temperature-aware design.
 *
 * Implements thermal via count estimation per IPC-2152,
 * trace resistance with temperature coefficient correction,
 * and current density limits for PCB traces.
 * @author  mini-esp32-iot-board-layout
 * @date    2026-06-21
 */

#include "board_geometry.h"
#include "thermal_design.h"
#include <math.h>
#include <stdlib.h>

/* Compute number of thermal vias needed under a QFN/DFN pad.
 * Uses Fourier heat conduction model: Rth = L / (k * A)
 * where A is the cross-sectional copper area of the via barrel wall.
 * Adds 50% margin per IPC-2152 recommendations.
 * Ref: IPC-2152, Standard for Determining Current-Carrying Capacity */
int thermal_via_count(double power_w, double via_diam_mm,
                       double via_height_mm, double cu_plating_um,
                       double delta_t_c)
{
    if (power_w <= 0.0 || via_diam_mm <= 0.0 || via_height_mm <= 0.0 ||
        cu_plating_um <= 0.0 || delta_t_c <= 0.0)
        return 0;
    double d = via_diam_mm * 1e-3;
    double h = via_height_mm * 1e-3;
    double t = cu_plating_um * 1e-6;
    double A_cu = M_PI * d * t;
    if (A_cu <= 0.0) return 0;
    double Rth_one = h / (K_COPPER * A_cu);
    if (Rth_one <= 0.0) return 0;
    double Rth_req = delta_t_c / power_w;
    int n = (int)ceil(Rth_one / Rth_req * 1.5);
    return (n < 1) ? 1 : n;
}

/* Compute trace DC resistance per unit length.
 * R = rho(T) * L / A, with rho(T) = rho_20[1 + alpha*(T - 20)].
 * Returns ohm/mm. Returns -1.0 on invalid input.
 * Ref: IPC-2221A, generic standard on printed board design */
double trace_resistance_per_mm(double width_mm, double thickness_um,
                                double temperature_c)
{
    if (width_mm <= 0.0 || thickness_um <= 0.0) return -1.0;
    double rho_T = COPPER_RESISTIVITY
                 * (1.0 + COPPER_TEMPCO * (temperature_c - 20.0));
    double area_m2 = width_mm * 1e-3 * thickness_um * 1e-6;
    if (area_m2 <= 0.0) return -1.0;
    return rho_T * 1e-3 / area_m2;
}

/* Compute trace current capacity using IPC-2221 external conductor formula.
 * I = k * dT^0.44 * (W * t)^0.725
 * where k = 0.048 for outer layers, 0.024 for inner layers.
 * Returns current in Amperes. */
double ipc2221_trace_current(double width_mm, double thickness_um,
                              double delta_t_c, int is_outer)
{
    if (width_mm <= 0.0 || thickness_um <= 0.0 || delta_t_c <= 0.0)
        return -1.0;
    double k = is_outer ? 0.048 : 0.024;
    double area_mil2 = (width_mm / 0.0254) * (thickness_um / 25.4);
    return k * pow(delta_t_c, 0.44) * pow(area_mil2, 0.725);
}

/* Compute required trace width for a target current using IPC-2221.
 * Inverts I = k * dT^0.44 * (W_cross)^0.725
 * Returns width in mm, or -1.0 on error. */
double ipc2221_trace_width(double current_a, double delta_t_c,
                            double thickness_um, int is_outer)
{
    if (current_a <= 0.0 || delta_t_c <= 0.0 || thickness_um <= 0.0)
        return -1.0;
    double k = is_outer ? 0.048 : 0.024;
    double t_mil = thickness_um / 25.4;
    if (t_mil <= 0.0) return -1.0;
    double area_mil2 = pow(current_a / (k * pow(delta_t_c, 0.44)),
                           1.0 / 0.725);
    double w_mil = area_mil2 / t_mil;
    return w_mil * 0.0254;
}

/* Compute skin depth at a given frequency for copper.
 * delta = sqrt(rho / (pi * f * mu0)).
 * At frequencies where skin depth < conductor thickness,
 * current flows primarily on the surface. */
double skin_depth_um(double freq_hz, double resistivity_ohm_m,
                      double relative_permeability)
{
    if (freq_hz <= 0.0 || resistivity_ohm_m <= 0.0) return -1.0;
    double mu = MU_0 * relative_permeability;
    return sqrt(resistivity_ohm_m / (M_PI * freq_hz * mu)) * 1e6;
}

/* Compute AC resistance increase factor due to skin effect.
 * For t/delta > 1: factor ~ t/(2*delta) * (1 + correction)
 * Simplified model: factor = 1 + (t/delta)^2 / 3 for t/delta < 1,
 * factor = t/(2*delta) + 0.25 for t/delta >= 1 */
double ac_dc_resistance_ratio(double thickness_um, double skin_depth_um)
{
    if (thickness_um <= 0.0 || skin_depth_um <= 0.0) return 1.0;
    double ratio = thickness_um / skin_depth_um;
    if (ratio < 1.0) {
        return 1.0 + ratio * ratio / 3.0;
    } else {
        return ratio / 2.0 + 0.25;
    }
}
