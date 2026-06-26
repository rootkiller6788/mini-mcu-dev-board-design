/**
 * @file    power_integrity_plane.c
 * @brief   Power plane modeling, via inductance, IR drop, and PDN sweep.
 *
 * Continues the PDN analysis with plane capacitance, spreading inductance,
 * plane resonance modes, via inductance, PDN frequency sweep evaluation,
 * and IR drop computation.
 * @author  mini-esp32-iot-board-layout
 * @date    2026-06-21
 */

#include "power_integrity.h"
#include <math.h>
#include <string.h>

/* Plane capacitance (parallel plate model).
 * C = er * epsilon0 * W * L / d.
 * Power-ground plane pairs form distributed decoupling capacitors.
 * Returns capacitance in Farads. */
double plane_capacitance(const plane_params_t *pp)
{
    if (!pp || pp->separation_mm <= 0.0 || pp->er <= 0.0) return -1.0;
    double A = pp->width_mm * pp->length_mm * 1e-6;
    double d = pp->separation_mm * 1e-3;
    return EPSILON_0 * pp->er * A / d;
}

/* Plane spreading inductance (approximate).
 * For a rectangular plane pair with current entering at one edge:
 * L_spread = mu0 * d * L / W (simplified).
 * The actual spreading inductance depends on via placement.
 * Returns inductance in Henries. */
double plane_spreading_inductance(const plane_params_t *pp)
{
    if (!pp || pp->width_mm <= 0.0 || pp->length_mm <= 0.0)
        return -1.0;
    return MU_0 * pp->separation_mm * 1e-3
         * pp->length_mm / pp->width_mm;
}

/* Plane cavity resonant frequencies.
 * f_mn = c0 / (2*sqrt(er)) * sqrt((m/W)^2 + (n/L)^2).
 * These resonances can cause high PDN impedance peaks.
 * Returns frequency in Hz. */
double plane_resonance_freq(const plane_params_t *pp, int m, int n)
{
    if (!pp || pp->er <= 0.0 || pp->width_mm <= 0.0 ||
        pp->length_mm <= 0.0 || m < 0 || n < 0)
        return -1.0;
    if (m == 0 && n == 0) return 0.0;
    double W = pp->width_mm * 1e-3;
    double L = pp->length_mm * 1e-3;
    return C_LIGHT / (2.0 * sqrt(pp->er))
         * sqrt((m*m)/(W*W) + (n*n)/(L*L));
}

/* Partial inductance of a cylindrical via.
 * L_via = mu0*h/(2*pi) * [ln(4h/d) + 0.5].
 * For typical PCB vias (h=1.6mm, d=0.3mm): L ~ 1.3 nH.
 * Ref: Johnson & Graham, 1993, Ch.7 */
double via_inductance(double h_mm, double d_mm)
{
    if (h_mm <= 0.0 || d_mm <= 0.0) return -1.0;
    double h = h_mm * 1e-3;
    double d = d_mm * 1e-3;
    double ratio = 4.0 * h / d;
    if (ratio <= 0.0) return -1.0;
    return MU_0 * h / (2.0 * M_PI) * (log(ratio) + 0.5);
}

/* Loop inductance of a via pair (signal + return via).
 * L_loop = mu0*h/pi * ln(s/d) [approximate, s >> d].
 * This is the total inductance seen by the current loop.
 * Returns inductance in Henries. */
double via_pair_inductance(double h_mm, double d_mm, double s_mm)
{
    if (h_mm <= 0.0 || d_mm <= 0.0 || s_mm <= d_mm) return -1.0;
    double h = h_mm * 1e-3;
    return MU_0 * h / M_PI * log(s_mm / d_mm);
}
