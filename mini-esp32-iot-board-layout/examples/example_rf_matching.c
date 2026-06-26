/**
 * @file    example_rf_matching.c
 * @brief   Example: Design RF matching network for ESP32 antenna.
 *
 * Demonstrates computing reflection coefficient, return loss, VSWR,
 * designing an L-match network for antenna impedance matching,
 * and calculating the Friis link budget for a BLE connection.
 *
 * Usage: ./examples/example_rf_matching.ex
 */

#include "rf_design.h"
#include "transmission_line.h"
#include <stdio.h>
#include <math.h>

int main(void)
{
    printf("=== ESP32 Antenna Matching & RF Link Budget ===\n\n");

    /* ESP32 antenna typical impedance: 35 + j10 ohm at 2.45 GHz */
    complex_z_t Z_antenna = {35.0, 10.0};
    double Z0 = 50.0;
    double freq = 2.45e9;

    /* Compute reflection coefficient */
    complex_z_t gamma = reflection_coefficient(Z_antenna, Z0);
    printf("Antenna impedance: %.1f + j%.1f ohm\n",
           Z_antenna.real, Z_antenna.imag);
    printf("Reflection coefficient: %.3f + j%.3f\n",
           gamma.real, gamma.imag);
    printf("  |Gamma| = %.3f\n",
           sqrt(gamma.real*gamma.real + gamma.imag*gamma.imag));

    /* VSWR and return loss */
    double VSWR = vswr(gamma);
    double RL = return_loss_db(gamma);
    printf("VSWR: %.1f:1\n", VSWR);
    printf("Return loss: %.1f dB\n", RL);
    printf("Mismatch loss: %.2f dB\n", mismatch_loss_db(gamma));

    /* Design L-match to transform antenna to 50 ohm */
    printf("\n--- L-Match Network Design ---\n");
    complex_z_t Z_source_Z0 = {50.0, 0.0};
    match_network_t match = l_match_synthesize(Z_source_Z0,
        Z_antenna, freq);

    printf("Match type: %d\n", match.type);
    printf("Components: %d\n", match.num_components);
    for (int i = 0; i < match.num_components; i++) {
        printf("  %s %s = %.2f %s\n",
               match.is_series[i] ? "Series" : "Shunt",
               match.component_is_L[i] ? "L" : "C",
               match.component_value[i],
               match.component_is_L[i] ? "H" : "F");
    }
    printf("Bandwidth: %.1f MHz\n", match.bandwidth_hz / 1e6);

    /* Pi-match alternative */
    printf("\n--- Pi-Match Network (Q=2) ---\n");
    match_network_t pi = pi_match_synthesize(Z_source_Z0,
        Z_antenna, 2.0, freq);
    printf("Components: %d\n", pi.num_components);
    for (int i = 0; i < pi.num_components; i++) {
        printf("  %s %s = %.2f %s\n",
               pi.is_series[i] ? "Series" : "Shunt",
               pi.component_is_L[i] ? "L" : "C",
               pi.component_value[i],
               pi.component_is_L[i] ? "H" : "F");
    }

    /* Link budget: ESP32 BLE @ 1m range */
    printf("\n--- BLE Link Budget (1m range) ---\n");
    double tx_power = 4.0;   /* dBm, ESP32 max for BLE */
    double tx_gain = 2.0;    /* dBi, PCB antenna */
    double rx_gain = 2.0;    /* dBi */
    double distance = 1.0;   /* m */

    double fspl = free_space_path_loss_db(distance, freq);
    printf("Free-space path loss @ 1m, 2.45GHz: %.1f dB\n", fspl);

    double rx_power = antenna_link_budget(tx_power, tx_gain,
        rx_gain, distance, freq);
    printf("Received power: %.1f dBm\n", rx_power);

    /* BLE sensitivity ~ -93 dBm */
    double sensitivity = -93.0;
    printf("Link margin: %.1f dB (sensitivity = %.0f dBm)\n",
           rx_power - sensitivity, sensitivity);

    return 0;
}
