/**
 * @file    rf_matching.c
 * @brief   RF impedance matching network synthesis and link budget.
 *
 * L-match, Pi-match, T-match network synthesis for conjugate matching.
 * Stub matching, quarter-wave transformer, and Friis free-space path loss.
 * @author  mini-esp32-iot-board-layout
 * @date    2026-06-21
 */

#include "rf_design.h"
#include "transmission_line.h"
#include <math.h>
#include <string.h>
#include <string.h>

/* L-match network synthesis.
 * For Z_source (real) to Z_load (real): two-element LC network.
 * Computes shunt and series element values.
 * If ZL > ZS: shunt C at load, series L.
 * If ZL < ZS: shunt L at load, series C.
 * Returns network with component values.
 * Ref: Bowick, RF Circuit Design, 2nd Ed., Ch.4 */
match_network_t l_match_synthesize(complex_z_t Z_source,
                                    complex_z_t Z_load, double freq_hz)
{
    match_network_t net;
    memset(&net, 0, sizeof(net));
    net.type = MATCH_L_SHUNT_C_SERIES_L;
    net.center_freq_hz = freq_hz;
    net.num_components = 2;

    double Rs = Z_source.real;
    double RL = Z_load.real;
    if (Rs <= 0.0 || RL <= 0.0 || freq_hz <= 0.0) return net;

    double w = 2.0 * M_PI * freq_hz;
    double Q = sqrt(RL/Rs - 1.0);
    if (Q <= 0.0) Q = 0.1;

    if (RL > Rs) {
        /* Shunt C at load, series L */
        net.type = MATCH_L_SHUNT_C_SERIES_L;
        double C_shunt = Q / (w * RL);
        double L_series = Q * Rs / w;
        net.is_series[0] = 0;
        net.component_value[0] = C_shunt;
        net.component_is_L[0] = 0;
        net.is_series[1] = 1;
        net.component_value[1] = L_series;
        net.component_is_L[1] = 1;
    } else {
        /* Series C, shunt L */
        net.type = MATCH_L_SERIES_C_SHUNT_L;
        double Q2 = sqrt(Rs/RL - 1.0);
        if (Q2 <= 0.0) Q2 = 0.1;
        double C_series = 1.0 / (w * Q2 * Rs);
        double L_shunt = Q2 * RL / w;
        net.is_series[0] = 1;
        net.component_value[0] = C_series;
        net.component_is_L[0] = 0;
        net.is_series[1] = 0;
        net.component_value[1] = L_shunt;
        net.component_is_L[1] = 1;
    }

    /* Bandwidth estimate: BW = f0 / Q (loaded Q = sqrt(Q)) */
    double Q_loaded = sqrt(Q);
    if (Q_loaded > 0.0) net.bandwidth_hz = freq_hz / Q_loaded;
    return net;
}

/* Pi-match network synthesis (three-element: shunt-series-shunt).
 * Uses loaded Q to set bandwidth. Higher Q = narrower band, better harmonics.
 * Ref: Bowick, Ch.5 */
match_network_t pi_match_synthesize(complex_z_t Z_source,
                                     complex_z_t Z_load,
                                     double Q, double freq_hz)
{
    match_network_t net;
    memset(&net, 0, sizeof(net));
    net.type = MATCH_PI;
    net.center_freq_hz = freq_hz;
    net.num_components = 3;

    double Rs = Z_source.real;
    double RL = Z_load.real;
    if (Rs <= 0.0 || RL <= 0.0 || Q <= 0.0 || freq_hz <= 0.0) return net;

    double w = 2.0 * M_PI * freq_hz;
    double R_int = (Rs < RL ? Rs : RL) / (Q*Q + 1.0);
    if (R_int <= 0.0) return net;

    double C1 = Q / (w * Rs);
    double C2 = Q / (w * RL);
    double L_series = (Q * Rs + Q * RL) / (w * (Q*Q + 1.0));

    net.is_series[0] = 0; net.component_value[0] = C1;
    net.component_is_L[0] = 0;
    net.is_series[1] = 1; net.component_value[1] = L_series;
    net.component_is_L[1] = 1;
    net.is_series[2] = 0; net.component_value[2] = C2;
    net.component_is_L[2] = 0;
    net.bandwidth_hz = freq_hz / Q;
    return net;
}

/* T-match network synthesis (three-element: series-shunt-series).
 * Dual of Pi-match. Uses loaded Q for bandwidth control.
 * Ref: Bowick, Ch.5 */
match_network_t t_match_synthesize(complex_z_t Z_source,
                                    complex_z_t Z_load,
                                    double Q, double freq_hz)
{
    match_network_t net;
    memset(&net, 0, sizeof(net));
    net.type = MATCH_T;
    net.center_freq_hz = freq_hz;
    net.num_components = 3;

    double Rs = Z_source.real;
    double RL = Z_load.real;
    if (Rs <= 0.0 || RL <= 0.0 || Q <= 0.0 || freq_hz <= 0.0) return net;

    double w = 2.0 * M_PI * freq_hz;
    double R_int = (Rs > RL ? Rs : RL) * (Q*Q + 1.0);

    double L1 = Q * Rs / w;
    double L2 = Q * RL / w;
    double C_shunt = (Q * Rs + Q * RL) / (w * R_int);

    net.is_series[0] = 1; net.component_value[0] = L1;
    net.component_is_L[0] = 1;
    net.is_series[1] = 0; net.component_value[1] = C_shunt;
    net.component_is_L[1] = 0;
    net.is_series[2] = 1; net.component_value[2] = L2;
    net.component_is_L[2] = 1;
    net.bandwidth_hz = freq_hz / Q;
    return net;
}
