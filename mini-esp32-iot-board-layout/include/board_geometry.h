/**
 * @file    board_geometry.h
 * @brief   Board geometry & layer stackup definitions for PCB design.
 *
 * Defines the physical board parameters, layer stackup configurations,
 * copper weight, and material constants relevant to ESP32 IoT board layout.
 *
 * Knowledge mapping:
 *   L1 - Definitions:    board_outline, stackup_layer, copper_constants
 *   L2 - Core Concepts:  impedance control stackup, prepreg/core thickness
 *   L3 - Math:           plane capacitance model
 *   L4 - Fundamental:    IPC-2221 board thickness guidelines
 *
 * References:
 *   - IPC-2221A: Generic Standard on Printed Board Design
 *   - Johnson & Graham: High-Speed Digital Design (1993)
 *   - Ritchey: Right the First Time (2007)
 *
 * @author  mini-esp32-iot-board-layout
 * @date    2026-06-21
 */

#ifndef BOARD_GEOMETRY_H
#define BOARD_GEOMETRY_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* --- Unit Conversions ------------------------------------------------- */

#define MIL_TO_M(x)     ((x) * 2.54e-5)
#define M_TO_MIL(x)     ((x) / 2.54e-5)
#define INCH_TO_MM(x)   ((x) * 25.4)
#define MM_TO_INCH(x)   ((x) / 25.4)
#define OZ_TO_UM(x)     ((x) * 35.0)
#define UM_TO_OZ(x)     ((x) / 35.0)

/* --- Material Constants ------------------------------------------------ */

#define COPPER_CONDUCTIVITY      5.8e7
#define COPPER_RESISTIVITY       1.724e-8
#define COPPER_TEMPCO            0.00393
#define EPSILON_0                8.8541878128e-12
#define MU_0                     1.25663706212e-6
#define C_LIGHT                  2.99792458e8

/* M_PI fallback for strict C11 compliance */
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* --- FR-4 Material Properties ------------------------------------------ */

#define FR4_ER_DEFAULT           4.5
#define FR4_ER_1GHZ              4.2
#define FR4_TAN_DELTA            0.02
#define FR4_TG_DEFAULT           130.0
#define FR4_TG_HIGH              170.0
#define FR4_THERMAL_K            0.3

/* --- Rogers RO4350B (for RF sections) ---------------------------------- */

#define RO4350_ER                3.48
#define RO4350_TAN_DELTA         0.0037
#define RO4350_THERMAL_K         0.69

/* --- Copper Weight Presets --------------------------------------------- */

typedef struct {
    double oz_per_sqft;
    double thickness_um;
    double thickness_mil;
} copper_weight_t;

#define CU_WEIGHT_0_5OZ    ((copper_weight_t){0.5, 17.5, 0.69})
#define CU_WEIGHT_1OZ      ((copper_weight_t){1.0, 35.0, 1.38})
#define CU_WEIGHT_2OZ      ((copper_weight_t){2.0, 70.0, 2.76})
#define CU_WEIGHT_3OZ      ((copper_weight_t){3.0, 105.0, 4.13})

/* --- Layer Stackup Definitions ----------------------------------------- */

typedef enum {
    LAYER_SIGNAL,
    LAYER_PLANE_GND,
    LAYER_PLANE_PWR,
    LAYER_MIXED,
    LAYER_SOLDERMASK,
    LAYER_SILKSCREEN
} layer_type_t;

typedef struct {
    int            layer_num;
    const char    *name;
    layer_type_t   type;
    double         thickness_mm;
    copper_weight_t copper;
    int            is_outer;
} stackup_layer_t;

typedef struct {
    int         between_layers;
    const char *material;
    double      thickness_mm;
    double      er;
    double      tan_delta;
    double      thermal_k;
} dielectric_t;

typedef struct {
    const char      *name;
    int              num_layers;
    double           total_thickness;
    stackup_layer_t *layers;
    dielectric_t    *dielectrics;
    int              is_controlled_impedance;
    double           target_impedance;
    double           target_diff_Z0;
} board_stackup_t;

/* --- Board Outline / Mechanical ---------------------------------------- */

typedef struct {
    double  width_mm;
    double  height_mm;
    double  thickness_mm;
    int     num_layers;
    int     corners;
    double *vertex_x_mm;
    double *vertex_y_mm;
} board_outline_t;

typedef struct {
    double  x_mm;
    double  y_mm;
    double  hole_diam_mm;
    double  pad_diam_mm;
    int     is_plated;
    int     connected_to_gnd;
    double  clearance_mm;
} mounting_hole_t;

/* --- Keepout / Clearance Zones ----------------------------------------- */

typedef struct {
    double  x0_mm;
    double  y0_mm;
    double  width_mm;
    double  height_mm;
    int     layers_bitmask;
    const char *description;
} keepout_zone_t;

#define ESP32_WROOM_ANT_KEEPOUT_W    16.0
#define ESP32_WROOM_ANT_KEEPOUT_H    40.0
#define ESP32_PICO_ANT_KEEPOUT_W     10.0

/* --- Core Function Declarations ---------------------------------------- */

board_stackup_t stackup_4layer_standard(double thickness_mm,
                                        copper_weight_t cu_outer,
                                        copper_weight_t cu_inner);

board_stackup_t stackup_2layer_standard(double thickness_mm,
                                        copper_weight_t cu_weight);

board_stackup_t stackup_6layer_advanced(double thickness_mm);

double dielectric_thickness_for_z0(double er, double target_z0,
                                    double trace_width);

double board_area(const board_outline_t *outline);

int validate_antenna_keepout(const keepout_zone_t *keepouts, int num_keepouts,
                              double ant_x, double ant_y);

double min_board_area_estimate(double module_width, double module_length,
                                int num_connectors, int io_density);

int thermal_via_count(double power_w, double via_diam_mm,
                       double via_height_mm, double cu_plating_um,
                       double delta_t_c);

double trace_resistance_per_mm(double width_mm, double thickness_um,
                                double temperature_c);

void board_stackup_free(board_stackup_t *s);

void board_outline_free(board_outline_t *o);

#ifdef __cplusplus
}
#endif

#endif /* BOARD_GEOMETRY_H */
