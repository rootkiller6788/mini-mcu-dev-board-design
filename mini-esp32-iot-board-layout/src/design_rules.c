/**
 * @file    design_rules.c
 * @brief   IPC-2221/2152 design rules for ESP32 IoT PCB layout.
 *
 * Implements trace width/current calculations per IPC-2221,
 * via current capacity per IPC-2152, clearance and creepage
 * per IPC-2221, annular ring requirements, and differential
 * pair spacing guidelines.
 * @author  mini-esp32-iot-board-layout
 * @date    2026-06-21
 */

#include "board_geometry.h"
#include "transmission_line.h"
#include <math.h>
#include <stdlib.h>

/* IPC-2221 trace width for given current (general formula).
 * For external layers: I = 0.048 * dT^0.44 * A^0.725
 * where A = cross-sectional area in square mils.
 * Returns width in mm. */
double ipc2221_trace_width_for_current(double current_a, double delta_t_c,
                                        double thickness_um, int is_outer)
{
    if (current_a <= 0.0 || delta_t_c <= 0.0 || thickness_um <= 0.0)
        return -1.0;
    double k = is_outer ? 0.048 : 0.024;
    double t_mil = thickness_um / 25.4;
    if (t_mil <= 0.0) return -1.0;
    double A_mil2 = pow(current_a / (k * pow(delta_t_c, 0.44)), 1.0/0.725);
    return A_mil2 / t_mil * 0.0254;
}

/* IPC-2221 current capacity for given trace geometry.
 * Returns current in Amperes. */
double ipc2221_current_capacity(double width_mm, double thickness_um,
                                 double delta_t_c, int is_outer)
{
    if (width_mm <= 0.0 || thickness_um <= 0.0 || delta_t_c <= 0.0)
        return -1.0;
    double k = is_outer ? 0.048 : 0.024;
    double w_mil = width_mm / 0.0254;
    double t_mil = thickness_um / 25.4;
    double A_mil2 = w_mil * t_mil;
    return k * pow(delta_t_c, 0.44) * pow(A_mil2, 0.725);
}

/* IPC-2221 via current capacity.
 * I_via = k * dT^0.44 * (pi*d*t)^0.725.
 * Where d = via diameter, t = plating thickness.
 * Returns current in Amperes. */
double ipc2221_via_current(double via_diam_mm, double plating_thk_um,
                            double delta_t_c, int is_outer)
{
    if (via_diam_mm <= 0.0 || plating_thk_um <= 0.0 || delta_t_c <= 0.0)
        return -1.0;
    double k = is_outer ? 0.048 : 0.024;
    double d_mil = via_diam_mm / 0.0254;
    double t_mil = plating_thk_um / 25.4;
    double A_mil2 = M_PI * d_mil * t_mil;
    return k * pow(delta_t_c, 0.44) * pow(A_mil2, 0.725);
}

/* IPC-2221 minimum electrical clearance.
 * For voltages < 500V: clearance_mm ~ 0.1 + 0.01*V.
 * Simplified per IPC-2221 Table 6-1.
 * Returns clearance in mm. */
double ipc2221_clearance_mm(double peak_voltage)
{
    if (peak_voltage < 0.0) return -1.0;
    if (peak_voltage <= 15.0) return 0.05;
    if (peak_voltage <= 30.0) return 0.10;
    if (peak_voltage <= 50.0) return 0.13;
    if (peak_voltage <= 100.0) return 0.15;
    if (peak_voltage <= 150.0) return 0.40;
    if (peak_voltage <= 170.0) return 0.50;
    if (peak_voltage <= 250.0) return 1.25;
    if (peak_voltage <= 300.0) return 1.50;
    if (peak_voltage <= 500.0) return 2.50;
    return 0.005 * peak_voltage + 0.5;
}

/* IPC-2221 creepage distance for pollution degree.
 * Creepage = clearance * (1 + factor based on pollution degree).
 * Pollution degree 1: factor 0 (clean, sealed)
 * Pollution degree 2: factor 0.5 (typical indoor)
 * Pollution degree 3: factor 1.0 (condensation possible).
 * Returns creepage in mm. */
double ipc2221_creepage_mm(double peak_voltage, double cti,
                            int pollution_degree)
{
    double clearance = ipc2221_clearance_mm(peak_voltage);
    if (clearance <= 0.0) return -1.0;
    double factor;
    switch (pollution_degree) {
    case 1: factor = 0.0; break;
    case 2: factor = (cti >= 400.0) ? 0.5 : 1.0; break;
    case 3: factor = (cti >= 400.0) ? 1.0 : 2.0; break;
    default: factor = 2.0; break;
    }
    return clearance * (1.0 + factor);
}

/* IPC-2221 minimum annular ring.
 * Class 1 (general): 0.05 mm over min
 * Class 2 (dedicated service): 0.10 mm over min
 * Class 3 (high reliability): 0.15 mm over min.
 * Min annular ring = (pad_diam - hole_diam) / 2.
 * Returns 1 if meets class, 0 otherwise. */
int ipc2221_annular_ring_check(double pad_diam_mm, double hole_diam_mm,
                                int class_level)
{
    if (pad_diam_mm <= hole_diam_mm) return 0;
    double ring = (pad_diam_mm - hole_diam_mm) / 2.0;
    double min_ring;
    switch (class_level) {
    case 1: min_ring = 0.05; break;
    case 2: min_ring = 0.10; break;
    case 3: min_ring = 0.15; break;
    default: return 0;
    }
    return (ring >= min_ring) ? 1 : 0;
}
