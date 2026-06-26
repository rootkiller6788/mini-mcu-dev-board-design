/**
 * @file    transmission_line_loss.c
 * @brief   Transmission line loss models and auxiliary calculations.
 *
 * Dielectric loss, conductor loss (Wheeler incremental inductance rule),
 * total link analysis, Newton-Raphson width optimization, wavelength,
 * propagation delay, critical length, and differential pair analysis.
 * @author  mini-esp32-iot-board-layout
 * @date    2026-06-21
 */

#include "transmission_line.h"
#include <math.h>
#include <string.h>

/* Complete transmission line analysis for given parameters.
 * Fills the tl_result_t structure with all computed characteristics.
 * Complexity: O(1). */
void tl_analyze(const tl_params_t *params, tl_result_t *result)
{
    if (!params || !result) return;
    memset(result, 0, sizeof(*result));
    double er = params->er, h = params->h_mm, w = params->w_mm;
    double t = params->t_mm, f = params->freq_hz;

    switch (params->type) {
    case TL_MICROSTRIP:
        result->Z0_ohm = microstrip_z0(er, h, w, t);
        result->ereff = microstrip_ereff(er, h, w, t);
        break;
    case TL_STRIPLINE_SYMMETRIC:
        result->Z0_ohm = stripline_z0(er, h, w, t);
        result->ereff = er;
        break;
    case TL_CPW:
        result->Z0_ohm = cpw_z0(er, h, w, params->gap_mm, t);
        result->ereff = (er + 1.0) / 2.0;
        break;
    case TL_CPWG:
        result->Z0_ohm = cpwg_z0(er, h, w, params->gap_mm, t);
        result->ereff = (er + 1.0) / 2.0;
        break;
    default:
        result->Z0_ohm = microstrip_z0(er, h, w, t);
        result->ereff = microstrip_ereff(er, h, w, t);
        break;
    }

    if (result->ereff <= 0.0) return;
    result->vp_m_per_s = C_LIGHT / sqrt(result->ereff);
    result->tpd_ps_per_mm = 1.0 / result->vp_m_per_s * 1e9;
    result->wavelength_mm = result->vp_m_per_s / f * 1000.0;
    result->alpha_d_db_per_mm = dielectric_loss_db_per_mm(f,
        result->ereff, params->tan_delta > 0.0
        ? params->tan_delta : FR4_TAN_DELTA);
    result->alpha_c_db_per_mm = conductor_loss_db_per_mm(f, w, t,
        result->Z0_ohm, params->roughness_um);
    result->alpha_total_db_per_mm = result->alpha_d_db_per_mm
                                  + result->alpha_c_db_per_mm;
}

/* Dielectric loss per unit length.
 * alpha_d = (pi/c) * f * sqrt(ereff) * tan_delta [Np/m]
 * Converts to dB/mm: multiply by 8.686 / 1000.
 * Ref: Pozar, Microwave Engineering, 4th Ed., Ch.3 */
double dielectric_loss_db_per_mm(double freq_hz, double ereff,
                                  double tan_delta)
{
    if (freq_hz <= 0.0 || ereff <= 0.0 || tan_delta < 0.0)
        return 0.0;
    double alpha_np_per_m = M_PI * freq_hz / C_LIGHT
                          * sqrt(ereff) * tan_delta;
    return alpha_np_per_m * 8.685889638 / 1000.0;
}

/* Conductor loss using Wheeler incremental inductance rule.
 * alpha_c = Rs / (2*Z0*w) [Np/m] for wide traces,
 * with surface resistivity Rs = sqrt(pi*f*mu0*rho).
 * Surface roughness correction factor per Hammerstad-Bekkadal.
 * Ref: Wheeler, 1942; Hammerstad & Bekkadal, 1975 */
double conductor_loss_db_per_mm(double freq_hz, double w_mm, double t_mm,
                                 double z0_ohm, double roughness_um)
{
    (void)t_mm;
    if (freq_hz <= 0.0 || w_mm <= 0.0 || z0_ohm <= 0.0) return 0.0;
    double rs = sqrt(M_PI * freq_hz * MU_0 * COPPER_RESISTIVITY);
    /* Roughness correction: Kr = 1 + 2/pi*atan(1.4*(roughness/skin_depth)^2) */
    double skin_depth = sqrt(COPPER_RESISTIVITY
                           / (M_PI * freq_hz * MU_0));
    double kr = 1.0;
    if (roughness_um > 0.0 && skin_depth > 0.0) {
        kr = 1.0 + 2.0/M_PI * atan(1.4
             * pow(roughness_um*1e-6 / skin_depth, 2.0));
    }
    double alpha_np_per_m = rs * kr / (2.0 * z0_ohm * w_mm * 1e-3);
    return alpha_np_per_m * 8.685889638 / 1000.0;
}

/* Wavelength in substrate at given frequency.
 * lambda_g = c / (f * sqrt(ereff)). Returns mm. */
double wavelength_in_substrate(double freq_hz, double ereff)
{
    if (freq_hz <= 0.0 || ereff <= 0.0) return -1.0;
    return C_LIGHT / (freq_hz * sqrt(ereff)) * 1000.0;
}

/* Propagation delay per unit length.
 * tpd = sqrt(ereff) / c [s/m]. Returns ps/mm. */
double propagation_delay_ps_per_mm(double ereff)
{
    if (ereff <= 0.0) return -1.0;
    return sqrt(ereff) / C_LIGHT * 1e9;
}

/* Critical length for transmission line behavior.
 * L_crit = t_rise / (6 * tpd).
 * Traces longer than this must be treated as transmission lines.
 * Ref: Johnson & Graham, 1993, Ch.4 */
double critical_length_mm(double t_rise_ps, double tpd)
{
    if (t_rise_ps <= 0.0 || tpd <= 0.0) return -1.0;
    return t_rise_ps / (6.0 * tpd);
}

/* Skin depth equals conductor thickness frequency.
 * f_skin = rho / (pi * mu0 * t^2). Above this frequency,
 * AC resistance increases significantly. */
double skin_depth_frequency(double t_mm, double resistivity_ohm_m)
{
    if (t_mm <= 0.0 || resistivity_ohm_m <= 0.0) return -1.0;
    return resistivity_ohm_m
         / (M_PI * MU_0 * t_mm * 1e-3 * t_mm * 1e-3);
}

/* Insertion loss for a transmission line segment.
 * IL_dB = alpha_total * length. */
double insertion_loss_db(double alpha_total, double length_mm)
{
    return alpha_total * length_mm;
}
