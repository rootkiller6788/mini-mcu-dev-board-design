/**
 * @file    transmission_line_diff.c
 * @brief   Differential pair and width optimization calculations.
 *
 * Implements Newton-Raphson trace width optimization for target Z0,
 * differential pair impedance analysis (odd/even/diff/common mode),
 * and length matching tolerance computation.
 * @author  mini-esp32-iot-board-layout
 * @date    2026-06-21
 */

#include "transmission_line.h"
#include <math.h>
#include <string.h>

/* Newton-Raphson iteration to find microstrip trace width for target Z0.
 * Solves f(w) = microstrip_z0(er, h, w, t) - target_z0 = 0.
 * Uses numerical derivative: df/dw ~ (f(w+h) - f(w)) / h.
 * Returns width in mm, or negative value on convergence failure.
 * Complexity: O(max_iter). */
double microstrip_width_for_z0(double er, double h_mm, double t_mm,
                                double target_z0, double tolerance,
                                int max_iter)
{
    if (er < 1.0 || h_mm <= 0.0 || target_z0 <= 0.0 || max_iter <= 0)
        return -1.0;
    /* Initial guess: w ~ h for typical 50 ohm on FR4 */
    double w = h_mm * 1.8;
    double delta = 1e-6;
    for (int i = 0; i < max_iter; i++) {
        double z0 = microstrip_z0(er, h_mm, w, t_mm);
        if (z0 < 0.0) return -1.0;
        double err = z0 - target_z0;
        if (fabs(err) < tolerance) return w;
        /* Numerical derivative */
        double z0_plus = microstrip_z0(er, h_mm, w + delta, t_mm);
        if (z0_plus < 0.0) return -1.0;
        double deriv = (z0_plus - z0) / delta;
        if (fabs(deriv) < 1e-15) return -1.0;
        w -= err / deriv;
        if (w <= 0.0) w = delta;
    }
    return -1.0;
}

/* Edge-coupled differential microstrip analysis.
 * Uses IPC-2141A approximations for odd/even mode impedances.
 * Z0_odd = Z0 * sqrt((1 - k)/(1 + k)), Z0_even = Z0 * sqrt((1 + k)/(1 - k))
 * where k is the coupling coefficient determined by s/h ratio.
 * Ref: IPC-2141A, Controlled Impedance Circuit Boards */
void diff_pair_analyze(const diff_pair_params_t *params,
                        diff_pair_result_t *result)
{
    if (!params || !result) return;
    memset(result, 0, sizeof(*result));
    double w = params->w_mm, s = params->s_mm;
    double h = params->h_mm, t = params->t_mm, er = params->er;
    double Z0_se = microstrip_z0(er, h, w, t);
    if (Z0_se <= 0.0) return;
    double s_over_h = s / h;
    /* Coupling coefficient model */
    double k = exp(-0.8 * s_over_h);
    if (k > 0.99) k = 0.99;
    if (k < 0.01) k = 0.01;
    result->Z0_odd_ohm = Z0_se * sqrt((1.0 - k) / (1.0 + k));
    result->Z0_even_ohm = Z0_se * sqrt((1.0 + k) / (1.0 - k));
    result->Z0_diff_ohm = 2.0 * result->Z0_odd_ohm;
    result->Z0_common_ohm = result->Z0_even_ohm / 2.0;
    result->coupling_coeff = k;
}

/* Maximum allowable length mismatch in a differential pair.
 * max_mismatch = t_budget_ps / tpd.
 * The timing budget is typically 10% of the bit period.
 * Ref: Hall & Heck, Advanced Signal Integrity, 2009 */
double diff_pair_max_mismatch(double t_budget_ps, double tpd)
{
    if (t_budget_ps <= 0.0 || tpd <= 0.0) return -1.0;
    return t_budget_ps / tpd;
}
