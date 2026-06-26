/**
 * @file    antenna_design.c
 * @brief   PCB antenna design for ESP32 IoT boards.
 *
 * Implements inverted-F antenna (IFA) dimension calculations,
 * meander antenna optimization, chip antenna matching,
 * and PCB trace antenna design rules per Espressif guidelines.
 *
 * Knowledge mapping:
 *   L1 - Definitions:    antenna gain, directivity, efficiency, EIRP
 *   L2 - Core Concepts:  resonant frequency, bandwidth, impedance matching
 *   L3 - Math:           antenna radiation integrals, duality
 *   L4 - Fundamental:    reciprocity theorem, Poynting vector
 *   L5 - Algorithms:     IFA dimension synthesis, meander optimization
 *   L7 - Applications:   ESP32 WiFi/Bluetooth PCB antenna (2.4 GHz)
 *
 * References:
 *   - Balanis, C.A. Antenna Theory: Analysis and Design, 4th Ed., 2016
 *   - Espressif AN205796: ESP32 PCB Antenna Design Guide
 *   - Orban & Moernaut, The Inverted-F Antenna, RF Globalnet, 2007
 * @author  mini-esp32-iot-board-layout
 * @date    2026-06-21
 */

#include "board_geometry.h"
#include "transmission_line.h"
#include <math.h>

/* --- Inverted-F Antenna (IFA) Design ----------------------------- */

/* Compute resonant length of IFA quarter-wave element.
 * L = c / (4 * f * sqrt(ereff)).
 * For 2.45 GHz on FR4 (er_eff ~ 3.5): L ~ 16.4 mm.
 * Returns length in mm. */
double ifa_resonant_length_mm(double freq_hz, double ereff)
{
    if (freq_hz <= 0.0 || ereff <= 1.0) return -1.0;
    return C_LIGHT / (4.0 * freq_hz * sqrt(ereff)) * 1000.0;
}

/* IFA height above ground plane.
 * Higher height = wider bandwidth but larger size.
 * Typical: 3-8 mm for 2.45 GHz.
 * BW ~ (h/lambda)^2 * 100 [%]. */
double ifa_bandwidth_percent(double height_mm, double freq_hz,
                              double ereff)
{
    if (height_mm <= 0.0 || freq_hz <= 0.0 || ereff <= 1.0) return -1.0;
    double lambda = C_LIGHT / (freq_hz * sqrt(ereff)) * 1000.0;
    double h_over_lambda = height_mm / lambda;
    return h_over_lambda * h_over_lambda * 100.0;
}

/* --- Meander Antenna (Space-Saving) ------------------------------ */

/* Meander line effective length factor.
 * Meandering reduces the physical length by factor M.
 * M = L_physical / L_electrical.
 * For ESP32 module meander: M ~ 0.5-0.7.
 * Returns effective electrical length in mm. */
double meander_electrical_length(double physical_length_mm,
                                  double meander_factor)
{
    if (physical_length_mm <= 0.0 || meander_factor <= 0.0 ||
        meander_factor >= 1.0)
        return -1.0;
    return physical_length_mm / meander_factor;
}

/* Minimum trace width for meander antenna.
 * Wider traces = lower ohmic loss but larger size.
 * For 50 ohm input: w_mm > 0.2 mm on FR4.
 * Returns recommended minimum width in mm. */
double meander_min_trace_width(double freq_hz, double impedance_ohm)
{
    (void)freq_hz;
    if (impedance_ohm <= 0.0) return -1.0;
    /* Approximation: wider for lower impedance */
    if (impedance_ohm < 30.0) return 0.5;
    if (impedance_ohm < 50.0) return 0.3;
    return 0.2;
}

/* --- Chip Antenna Matching --------------------------------------- */

/* Chip antenna keepout area requirement.
 * Typical ceramic chip antenna: 5-10 mm clearance all around.
 * Larger clearance = better radiation efficiency.
 * Returns required keepout area in mm^2. */
double chip_antenna_keepout_area(double antenna_length_mm,
                                  double clearance_mm)
{
    if (antenna_length_mm <= 0.0 || clearance_mm <= 0.0) return -1.0;
    double total_w = antenna_length_mm + 2.0 * clearance_mm;
    return total_w * total_w;
}

/* --- Regulatory Limits -------------------------------------------- */

/* EIRP (Equivalent Isotropically Radiated Power).
 * EIRP_dBm = P_tx_dBm + G_antenna_dBi - L_cable_dB.
 * FCC limit for 2.4 GHz ISM: +36 dBm EIRP.
 * CE/ETSI limit for 2.4 GHz: +20 dBm EIRP.
 * Returns EIRP in dBm. */
double eirp_dbm(double tx_power_dbm, double antenna_gain_dbi,
                 double cable_loss_db)
{
    return tx_power_dbm + antenna_gain_dbi - cable_loss_db;
}

/* Check if EIRP complies with regulatory limit.
 * Returns 1 if compliant, 0 if exceeds limit. */
int eirp_regulatory_check(double eirp_dbm, double limit_dbm)
{
    return (eirp_dbm <= limit_dbm) ? 1 : 0;
}

/* Compute antenna efficiency from radiation resistance and loss.
 * eta = R_rad / (R_rad + R_loss) * 100 [%].
 * Typical PCB trace antenna: 60-80% efficiency.
 * Returns efficiency in percent. */
double antenna_efficiency_pct(double R_radiation_ohm,
                               double R_loss_ohm)
{
    if (R_radiation_ohm <= 0.0 || R_loss_ohm < 0.0) return -1.0;
    return R_radiation_ohm / (R_radiation_ohm + R_loss_ohm) * 100.0;
}

/* Antenna near-field / far-field boundary.
 * Far-field distance: R_ff = 2*D^2 / lambda.
 * Where D = largest antenna dimension.
 * For ESP32 module (D ~ 25 mm, 2.45 GHz): R_ff ~ 10 mm.
 * Returns far-field distance in mm. */
double antenna_far_field_distance(double D_mm, double freq_hz)
{
    if (D_mm <= 0.0 || freq_hz <= 0.0) return -1.0;
    double lambda_mm = C_LIGHT / freq_hz * 1000.0;
    if (lambda_mm <= 0.0) return -1.0;
    return 2.0 * D_mm * D_mm / lambda_mm;
}
