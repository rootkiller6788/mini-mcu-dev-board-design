/**
 * @file    rf_link_budget.c
 * @brief   RF link budget: stub matching, quarter-wave, Friis equation.
 * @author  mini-esp32-iot-board-layout
 * @date    2026-06-21
 */

#include "rf_design.h"
#include "transmission_line.h"
#include <math.h>
#include <string.h>

/* Quarter-wave transformer for matching real impedances.
 * Z0_tx = sqrt(ZL * Z0). Length = lambda_g / 4.
 * Only works for purely real load impedances.
 * Ref: Pozar, Microwave Engineering, Ch.2 */
match_network_t quarter_wave_transformer(double ZL_real, double Z0,
                                          double freq_hz, double er)
{
    match_network_t net;
    memset(&net, 0, sizeof(net));
    net.type = MATCH_QUARTER_WAVE;
    net.center_freq_hz = freq_hz;
    net.num_components = 1;
    if (ZL_real <= 0.0 || Z0 <= 0.0 || freq_hz <= 0.0 || er <= 1.0)
        return net;
    double Z0_tx = sqrt(ZL_real * Z0);
    net.component_value[0] = Z0_tx;
    net.component_is_L[0] = 0;
    return net;
}

/* Open-circuit stub input impedance.
 * Z_in_oc = -j * Z0 * cot(beta * length).
 * Behaves as capacitor for length < lambda/4.
 * Returns reactance in ohms (imaginary part). */
double stub_oc_input_impedance(double Z0, double length_mm,
                                double lambda_g_mm)
{
    if (Z0 <= 0.0 || lambda_g_mm <= 0.0) return 0.0;
    double beta_l = 2.0 * M_PI * length_mm / lambda_g_mm;
    return -Z0 / tan(beta_l);
}

/* Short-circuit stub input impedance.
 * Z_in_sc = j * Z0 * tan(beta * length).
 * Behaves as inductor for length < lambda/4.
 * Returns reactance in ohms (imaginary part). */
double stub_sc_input_impedance(double Z0, double length_mm,
                                double lambda_g_mm)
{
    if (Z0 <= 0.0 || lambda_g_mm <= 0.0) return 0.0;
    double beta_l = 2.0 * M_PI * length_mm / lambda_g_mm;
    return Z0 * tan(beta_l);
}

/* Compute stub length needed to achieve a target admittance.
 * For OC stub: B_target = Y0 * tan(beta*l).
 * For SC stub: B_target = -Y0 * cot(beta*l).
 * Returns length in mm. */
double stub_length_for_admittance(double Z0, double Y_target,
                                    double lambda_g_mm, int is_open_circuit)
{
    if (Z0 <= 0.0 || lambda_g_mm <= 0.0) return -1.0;
    double Y0 = 1.0 / Z0;
    double ratio = Y_target / Y0;
    double beta_l;
    if (is_open_circuit) {
        beta_l = atan(ratio);
        if (beta_l < 0.0) beta_l += M_PI;
    } else {
        beta_l = M_PI / 2.0 - atan(ratio);
        if (beta_l < 0.0) beta_l += M_PI;
    }
    return beta_l * lambda_g_mm / (2.0 * M_PI);
}

/* Free-space path loss (Friis equation).
 * FSPL = 20*log10(4*pi*d*f/c) [dB].
 * Reference: Friis, Proc. IRE, 1946 */
double free_space_path_loss_db(double distance_m, double freq_hz)
{
    if (distance_m <= 0.0 || freq_hz <= 0.0) return -1.0;
    return 20.0 * log10(4.0 * M_PI * distance_m * freq_hz / C_LIGHT);
}

/* Link budget calculation (Friis transmission equation).
 * Pr = Pt + Gt + Gr - FSPL [all in dB].
 * Returns received power in dBm.
 * Ref: Friis, Proc. IRE, 1946 */
double antenna_link_budget(double tx_power_dbm, double tx_gain_dbi,
                             double rx_gain_dbi, double distance_m,
                             double freq_hz)
{
    double fspl = free_space_path_loss_db(distance_m, freq_hz);
    if (fspl < 0.0) return -INFINITY;
    return tx_power_dbm + tx_gain_dbi + rx_gain_dbi - fspl;
}
