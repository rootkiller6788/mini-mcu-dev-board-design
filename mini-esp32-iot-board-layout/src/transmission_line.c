/**
 * @file    transmission_line.c
 * @brief   RF transmission line impedance and loss calculations.
 *
 * Implements microstrip (Hammerstad-Jensen), stripline (Cohn),
 * CPW/CPWG (conformal mapping), differential pairs, loss models,
 * and Newton-Raphson trace width optimization.
 *
 * Key references:
 *   - Hammerstad & Jensen, IEEE MTT-S, 1980
 *   - Wheeler, IEEE T-MTT, 1977
 *   - Cohn, IRE T-MTT, 1954
 *   - Wadell, Transmission Line Design Handbook, 1991
 * @author  mini-esp32-iot-board-layout
 * @date    2026-06-21
 */

#include "transmission_line.h"
#include <math.h>
#include <stdio.h>

/* === Microstrip (Hammerstad-Jensen) ================================= */

/* Microstrip effective dielectric constant.
 * ereff = (er+1)/2 + (er-1)/2 * (1 + 12*h/w)^(-0.5)
 * With thickness correction when w/h < 1/(2*pi).
 * Ref: Hammerstad & Jensen, 1980, Eq. 2-3 */
double microstrip_ereff(double er, double h_mm, double w_mm, double t_mm)
{
    if (er < 1.0 || h_mm <= 0.0 || w_mm <= 0.0) return -1.0;
    double w_over_h = w_mm / h_mm;
    /* Effective width correction for thickness */
    double w_eff = w_mm;
    if (t_mm > 0.0) {
        if (w_over_h < 1.0 / (2.0 * M_PI)) {
            w_eff = w_mm + t_mm / M_PI * (1.0 + log(2.0 * h_mm / t_mm));
        } else {
            w_eff = w_mm + t_mm / M_PI * (1.0 + log(4.0 * M_PI * w_mm / t_mm));
        }
    }
    double a = 1.0 + 12.0 * h_mm / w_eff;
    double ereff = (er + 1.0) / 2.0 + (er - 1.0) / 2.0 * 1.0 / sqrt(a);
    return ereff;
}

/* Microstrip characteristic impedance.
 * For w/h <= 1: Z0 = 60/sqrt(ereff) * ln(8h/w_eff + w_eff/(4h))
 * For w/h > 1:  Z0 = 120*pi/(sqrt(ereff)*(w_eff/h + 1.393 + 0.667*ln(w_eff/h+1.444)))
 * Ref: Hammerstad & Jensen, 1980 */
double microstrip_z0(double er, double h_mm, double w_mm, double t_mm)
{
    if (er < 1.0 || h_mm <= 0.0 || w_mm <= 0.0) return -1.0;
    double ereff = microstrip_ereff(er, h_mm, w_mm, t_mm);
    if (ereff <= 0.0) return -1.0;
    double w_eff = w_mm;
    if (t_mm > 0.0) {
        if (w_mm / h_mm < 1.0 / (2.0 * M_PI)) {
            w_eff = w_mm + t_mm / M_PI * (1.0 + log(2.0 * h_mm / t_mm));
        } else {
            w_eff = w_mm + t_mm / M_PI * (1.0 + log(4.0 * M_PI * w_mm / t_mm));
        }
    }
    double w_over_h = w_eff / h_mm;
    double z0;
    if (w_over_h <= 1.0) {
        z0 = 60.0 / sqrt(ereff) * log(8.0 / w_over_h + w_over_h / 4.0);
    } else {
        z0 = 120.0 * M_PI / (sqrt(ereff) * (w_over_h + 1.393
             + 0.667 * log(w_over_h + 1.444)));
    }
    return z0;
}

/* === Stripline (Cohn) ================================================ */

/* Symmetric stripline characteristic impedance.
 * Z0 = 60/sqrt(er) * ln(4h / (0.67*pi*w*(0.8 + t/w)))
 * Valid for w/(h-t) < 0.35 and t/h < 0.25.
 * Ref: Cohn, IRE T-MTT, 1954 */
double stripline_z0(double er, double h_mm, double w_mm, double t_mm)
{
    if (er < 1.0 || h_mm <= 0.0 || w_mm <= 0.0) return -1.0;
    if (t_mm >= h_mm * 0.25) return -1.0;
    double b = h_mm - t_mm;
    double w_over_b = w_mm / b;
    if (w_over_b >= 0.35) return -1.0;
    double x = t_mm / w_mm;
    return 60.0 / sqrt(er) * log(4.0 * b
           / (0.67 * M_PI * w_mm * (0.8 + x)));
}

/* === Coplanar Waveguide ============================================== */

/* Elliptic integral ratio approximation K(k)/K(k') for CPW.
 * For 0 < k < 1/sqrt(2): K/K' = pi / ln(2*(1+sqrt(k'))/(1-sqrt(k')))
 * For 1/sqrt(2) <= k <= 1: K'/K = ln(2*(1+sqrt(k))/(1-sqrt(k))) / pi
 * Ref: Hilberg, IEEE T-MTT, 1969 */
static double ellip_ratio(double k)
{
    if (k <= 0.0 || k >= 1.0) return 1.0;
    double kp = sqrt(1.0 - k * k);
    if (k < 1.0 / sqrt(2.0)) {
        return M_PI / log(2.0 * (1.0 + sqrt(kp)) / (1.0 - sqrt(kp)));
    } else {
        return log(2.0 * (1.0 + sqrt(k)) / (1.0 - sqrt(k))) / M_PI;
    }
}

/* CPW characteristic impedance (no ground plane).
 * Z0 = 30*pi/sqrt(ereff) * K(k')/K(k)
 * where k = w/(w+2*gap).
 * Ref: Wadell, 1991, Ch.4 */
double cpw_z0(double er, double h_mm, double w_mm, double gap_mm,
               double t_mm)
{
    (void)h_mm;
    (void)t_mm;
    if (er < 1.0 || w_mm <= 0.0 || gap_mm <= 0.0) return -1.0;
    double k = w_mm / (w_mm + 2.0 * gap_mm);
    if (k <= 0.0 || k >= 1.0) return -1.0;
    double ereff = (er + 1.0) / 2.0;
    return 30.0 * M_PI / sqrt(ereff) * ellip_ratio(k);
}

/* Grounded CPW (CPWG) impedance with backside ground plane.
 * Ref: Wadell, 1991 */
double cpwg_z0(double er, double h_mm, double w_mm, double gap_mm,
                double t_mm)
{
    (void)t_mm;
    if (er < 1.0 || h_mm <= 0.0 || w_mm <= 0.0 || gap_mm <= 0.0)
        return -1.0;
    double k1 = w_mm / (w_mm + 2.0 * gap_mm);
    double k2 = tanh(M_PI * w_mm / (4.0 * h_mm))
              / tanh(M_PI * (w_mm + 2.0 * gap_mm) / (4.0 * h_mm));
    if (k1 <= 0.0 || k1 >= 1.0 || k2 <= 0.0 || k2 >= 1.0)
        return -1.0;
    double ereff = (er + 1.0) / 2.0;
    return 60.0 * M_PI / sqrt(ereff)
         / (ellip_ratio(k1) + ellip_ratio(k2));
}
