/**
 * @file    manufacturing.c
 * @brief   Design-for-manufacturing (DFM) and testability for IoT PCBs.
 *
 * Covers solder mask expansion, paste stencil design,
 * pad geometry optimization (IPC-7351), fiducial placement,
 * test point requirements, and panelization guidelines.
 * @author  mini-esp32-iot-board-layout
 * @date    2026-06-21
 */

#include "board_geometry.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <string.h>

/* Solder mask expansion (SME) calculation.
 * IPC-SM-840 recommends 0.05-0.10 mm expansion per side for standard.
 * Returns recommended expansion in mm. */
double solder_mask_expansion_mm(double pad_size_mm, int density_level)
{
    if (pad_size_mm <= 0.0) return -1.0;
    /* Density: 1=low, 2=medium, 3=high */
    switch (density_level) {
    case 1: return (pad_size_mm >= 0.5) ? 0.10 : 0.05;
    case 2: return (pad_size_mm >= 0.3) ? 0.075 : 0.05;
    case 3: return 0.05;
    default: return 0.075;
    }
}

/* Solder paste stencil aperture design (IPC-7525).
 * Area ratio = aperture_area / aperture_wall_area.
 * Target area ratio > 0.66 for good paste release.
 * Returns recommended aperture width reduction factor. */
double stencil_aperture_width_factor(double pad_width_mm,
                                      double pad_length_mm,
                                      double stencil_thickness_mm)
{
    if (pad_width_mm <= 0.0 || pad_length_mm <= 0.0 ||
        stencil_thickness_mm <= 0.0)
        return -1.0;
    /* Area ratio = (w*l) / (2*(w+l)*t) */
    double area_ratio = (pad_width_mm * pad_length_mm)
                      / (2.0 * (pad_width_mm + pad_length_mm)
                         * stencil_thickness_mm);
    /* If AR < 0.66, reduce aperture width by factor */
    if (area_ratio >= 0.66) return 1.0;
    double target_w = 0.66 * 2.0 * pad_length_mm * stencil_thickness_mm
                    / (pad_length_mm
                       - 0.66 * 2.0 * stencil_thickness_mm);
    if (target_w <= 0.0 || target_w > pad_width_mm) return 1.0;
    return target_w / pad_width_mm;
}

/* Fiducial marker placement recommendation.
 * IPC-7351 recommends 3 fiducials per board.
 * Global fiducials: at least 2, preferably 3 at corners.
 * Returns recommended number of fiducials. */
int fiducial_count_recommendation(double board_width_mm,
                                   double board_height_mm,
                                   int has_fine_pitch_components)
{
    if (board_width_mm <= 0.0 || board_height_mm <= 0.0) return -1;
    if (board_width_mm * board_height_mm < 2500.0) {
        return has_fine_pitch_components ? 3 : 2;
    }
    return has_fine_pitch_components ? 4 : 3;
}

/* Test point coverage ratio recommendation.
 * IPC recommends >= 85% node testability for adequate coverage.
 * For IoT boards with limited space, prioritize critical nets.
 * Returns minimum number of test points. */
int min_test_point_count(int num_nets, double coverage_ratio)
{
    if (num_nets <= 0 || coverage_ratio <= 0.0 || coverage_ratio > 1.0)
        return 0;
    return (int)ceil(num_nets * coverage_ratio);
}

/* Panelization: compute optimal panel utilization.
 * Given board size and panel size, compute how many boards fit.
 * Includes 5 mm border and 2 mm routing clearance between boards.
 * Returns number of boards per panel. */
int panel_utilization(double board_w_mm, double board_h_mm,
                       double panel_w_mm, double panel_h_mm)
{
    if (board_w_mm <= 0.0 || board_h_mm <= 0.0 ||
        panel_w_mm <= 0.0 || panel_h_mm <= 0.0)
        return 0;
    double border = 10.0;
    double clearance = 2.0;
    double usable_w = panel_w_mm - border;
    double usable_h = panel_h_mm - border;
    if (usable_w <= 0.0 || usable_h <= 0.0) return 0;
    int cols = (int)(usable_w / (board_w_mm + clearance));
    int rows = (int)(usable_h / (board_h_mm + clearance));
    if (cols < 1) cols = 1;
    if (rows < 1) rows = 1;
    return cols * rows;
}

/* Compute board thickness tolerance per IPC-6012.
 * Class 1 (general): +/- 10%
 * Class 2 (dedicated service): +/- 8%
 * Class 3 (high reliability): +/- 5%.
 * Returns tolerance range (min_thickness, stored in *t_min). */
double board_thickness_tolerance_mm(double nominal_thickness_mm,
                                     int class_level, double *t_min)
{
    double pct;
    switch (class_level) {
    case 1: pct = 0.10; break;
    case 2: pct = 0.08; break;
    case 3: pct = 0.05; break;
    default: pct = 0.10; break;
    }
    if (t_min) *t_min = nominal_thickness_mm * (1.0 - pct);
    return nominal_thickness_mm * (1.0 + pct);
}
