/**
 * @file    board_geometry.c
 * @brief   Board geometry calculations - area, keepout, board size estimation.
 * @author  mini-esp32-iot-board-layout
 * @date    2026-06-21
 */

#include "board_geometry.h"
#include <stdlib.h>
#include <math.h>

/* Compute board area using the shoelace formula.
 * area = 0.5 * |sum_{i=0}^{n-1} (x_i*y_{i+1} - x_{i+1}*y_i)|
 * Complexity: O(n) where n = number of vertices.
 * Reference: Surveying/geometry standard, also known as Gauss area formula. */
double board_area(const board_outline_t *outline)
{
    if (!outline || outline->corners < 3) return -1.0;
    if (!outline->vertex_x_mm || !outline->vertex_y_mm) return -1.0;
    double area = 0.0;
    int n = outline->corners;
    for (int i = 0; i < n; i++) {
        int j = (i + 1) % n;
        area += outline->vertex_x_mm[i] * outline->vertex_y_mm[j];
        area -= outline->vertex_x_mm[j] * outline->vertex_y_mm[i];
    }
    return fabs(area) * 0.5;
}

/* Validate ESP32 antenna keepout zones against Espressif requirements.
 * Each keepout zone must cover the antenna area with sufficient margin.
 * Returns number of violations (0 = pass).
 * Ref: Espressif ESP32 Hardware Design Guidelines Section 3.1.2 */
int validate_antenna_keepout(const keepout_zone_t *keepouts, int num_keepouts,
                              double ant_x, double ant_y)
{
    if (!keepouts || num_keepouts <= 0) return 1;
    int violations = 0;
    double req_w = ESP32_WROOM_ANT_KEEPOUT_W;
    double req_h = ESP32_WROOM_ANT_KEEPOUT_H;
    for (int i = 0; i < num_keepouts; i++) {
        const keepout_zone_t *k = &keepouts[i];
        if (ant_x < k->x0_mm || ant_x > k->x0_mm + k->width_mm ||
            ant_y < k->y0_mm || ant_y > k->y0_mm + k->height_mm)
            continue;
        double ml = ant_x - k->x0_mm;
        double mr = (k->x0_mm + k->width_mm) - (ant_x + req_w);
        double mt = (k->y0_mm + k->height_mm) - (ant_y + req_h);
        double mb = ant_y - k->y0_mm;
        if (ml < 0.0 || mr < 0.0 || mt < 0.0 || mb < 0.0)
            violations++;
        if (k->layers_bitmask == 0) violations++;
    }
    return violations;
}

/* Estimate minimum board area for ESP32 module + peripherals.
 * Factors in module footprint, connector real estate, routing overhead,
 * and antenna keepout margins. Returns area in mm^2.
 * Ref: IPC-2222A, Sectional Design Standard for Rigid Organic Printed Boards */
double min_board_area_estimate(double module_width, double module_length,
                                int num_connectors, int io_density)
{
    if (module_width <= 0.0 || module_length <= 0.0) return -1.0;
    double module_area = module_width * module_length;
    /* Each connector (USB, headers, barrel jack) needs ~150 mm^2 */
    double connector_area = num_connectors * 150.0;
    /* IO density increases routing area: sparse=2.0x, normal=2.5x, dense=3.0x */
    double routing_factor = 1.5 + 0.5 * io_density;
    /* 15 mm margin on each side of the module for antenna keepout */
    double margin_area = 2.0 * 15.0 * (module_width + module_length)
                       + 4.0 * 15.0 * 15.0;
    return (module_area + margin_area + connector_area) * routing_factor;
}

/* Estimate dielectric thickness to achieve target impedance.
 * Approximates inversion of microstrip Z0 formula (Hammerstad-Jensen).
 * Returns thickness in mm, or -1.0 on invalid input.
 * For w/h < 1: h = w/8 * exp(Z0*sqrt(er)/60)
 * For w/h >= 1: h = w / (120*pi/(Z0*sqrt(er)) - 2.42) */
double dielectric_thickness_for_z0(double er, double target_z0,
                                    double trace_width)
{
    if (er <= 1.0 || target_z0 <= 0.0 || trace_width <= 0.0) return -1.0;
    double zse = target_z0 * sqrt(er);
    if (zse <= 0.0) return -1.0;
    double h1 = trace_width / 8.0 * exp(zse / 60.0);
    double denom = 120.0 * M_PI / zse - 2.42;
    double h2 = (denom > 0.0) ? trace_width / denom : -1.0;
    double w_over_h1 = trace_width / h1;
    double result = (w_over_h1 < 1.0) ? h1
                  : (h2 > 0.0) ? h2 : h1;
    return (result > 0.0) ? result : -1.0;
}
