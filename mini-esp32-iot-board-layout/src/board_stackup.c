/**
 * @file    board_stackup.c
 * @brief   Board stackup initialization for 2/4/6 layer ESP32 IoT boards.
 * @author  mini-esp32-iot-board-layout
 * @date    2026-06-21
 */

#include "board_geometry.h"
#include "thermal_design.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

board_stackup_t stackup_4layer_standard(double thickness_mm,
                                        copper_weight_t cu_outer,
                                        copper_weight_t cu_inner)
{
    board_stackup_t s;
    memset(&s, 0, sizeof(s));
    s.name = "4-Layer Standard (Sig/GND/PWR/Sig)";
    s.num_layers = 4;
    s.total_thickness = thickness_mm;
    s.is_controlled_impedance = 1;
    s.target_impedance = 50.0;
    s.target_diff_Z0 = 100.0;
    s.layers = (stackup_layer_t *)calloc(4, sizeof(stackup_layer_t));
    s.dielectrics = (dielectric_t *)calloc(3, sizeof(dielectric_t));
    if (!s.layers || !s.dielectrics) {
        board_stackup_free(&s);
        board_stackup_t empty;
        memset(&empty, 0, sizeof(empty));
        return empty;
    }
    double pp = thickness_mm * 0.20;
    double core = thickness_mm * 0.40;
    s.layers[0] = (stackup_layer_t){1, "Top Signal", LAYER_SIGNAL,
        cu_outer.thickness_um/1000.0, cu_outer, 1};
    s.dielectrics[0] = (dielectric_t){1, "FR-4 Prepreg", pp,
        FR4_ER_1GHZ, FR4_TAN_DELTA, FR4_THERMAL_K};
    s.layers[1] = (stackup_layer_t){2, "GND Plane", LAYER_PLANE_GND,
        cu_inner.thickness_um/1000.0, cu_inner, 0};
    s.dielectrics[1] = (dielectric_t){2, "FR-4 Core", core,
        FR4_ER_1GHZ, FR4_TAN_DELTA, FR4_THERMAL_K};
    s.layers[2] = (stackup_layer_t){3, "PWR Plane", LAYER_PLANE_PWR,
        cu_inner.thickness_um/1000.0, cu_inner, 0};
    s.dielectrics[2] = (dielectric_t){3, "FR-4 Prepreg", pp,
        FR4_ER_1GHZ, FR4_TAN_DELTA, FR4_THERMAL_K};
    s.layers[3] = (stackup_layer_t){4, "Bottom Signal", LAYER_SIGNAL,
        cu_outer.thickness_um/1000.0, cu_outer, 1};
    return s;
}

board_stackup_t stackup_2layer_standard(double thickness_mm,
                                        copper_weight_t cu_weight)
{
    board_stackup_t s;
    memset(&s, 0, sizeof(s));
    s.name = "2-Layer Standard (Sig/GND)";
    s.num_layers = 2;
    s.total_thickness = thickness_mm;
    s.is_controlled_impedance = 0;
    s.target_impedance = 50.0;
    s.target_diff_Z0 = 100.0;
    s.layers = (stackup_layer_t *)calloc(2, sizeof(stackup_layer_t));
    s.dielectrics = (dielectric_t *)calloc(1, sizeof(dielectric_t));
    if (!s.layers || !s.dielectrics) {
        board_stackup_free(&s);
        board_stackup_t empty;
        memset(&empty, 0, sizeof(empty));
        return empty;
    }
    double d_thk = thickness_mm - 2.0*cu_weight.thickness_um/1000.0;
    if (d_thk < 0.1) d_thk = 0.1;
    s.layers[0] = (stackup_layer_t){1, "Top Signal", LAYER_SIGNAL,
        cu_weight.thickness_um/1000.0, cu_weight, 1};
    s.dielectrics[0] = (dielectric_t){1, "FR-4 Core", d_thk,
        FR4_ER_1GHZ, FR4_TAN_DELTA, FR4_THERMAL_K};
    s.layers[1] = (stackup_layer_t){2, "Bottom GND", LAYER_PLANE_GND,
        cu_weight.thickness_um/1000.0, cu_weight, 1};
    return s;
}

board_stackup_t stackup_6layer_advanced(double thickness_mm)
{
    board_stackup_t s;
    memset(&s, 0, sizeof(s));
    s.name = "6-Layer Advanced";
    s.num_layers = 6;
    s.total_thickness = thickness_mm;
    s.is_controlled_impedance = 1;
    s.target_impedance = 50.0;
    s.target_diff_Z0 = 100.0;
    s.layers = (stackup_layer_t *)calloc(6, sizeof(stackup_layer_t));
    s.dielectrics = (dielectric_t *)calloc(5, sizeof(dielectric_t));
    if (!s.layers || !s.dielectrics) {
        board_stackup_free(&s);
        board_stackup_t empty;
        memset(&empty, 0, sizeof(empty));
        return empty;
    }
    copper_weight_t c_outer = CU_WEIGHT_1OZ;
    copper_weight_t c_inner = CU_WEIGHT_0_5OZ;
    double dk = thickness_mm / 8.0;
    const char *nm[] = {"Top Signal","GND 1","Inner Sig",
        "PWR Plane","GND 2","Bottom Sig"};
    layer_type_t lt[] = {LAYER_SIGNAL,LAYER_PLANE_GND,LAYER_SIGNAL,
        LAYER_PLANE_PWR,LAYER_PLANE_GND,LAYER_SIGNAL};
    int is_out[] = {1,0,0,0,0,1};
    for (int i = 0; i < 6; i++) {
        s.layers[i].layer_num = i + 1;
        s.layers[i].name = nm[i];
        s.layers[i].type = lt[i];
        s.layers[i].thickness_mm = (is_out[i]
            ? c_outer.thickness_um : c_inner.thickness_um)/1000.0;
        s.layers[i].copper = is_out[i] ? c_outer : c_inner;
        s.layers[i].is_outer = is_out[i];
    }
    for (int i = 0; i < 5; i++) {
        s.dielectrics[i].between_layers = i + 1;
        s.dielectrics[i].material = (i==1||i==3)?"FR-4 Core":"FR-4 Prepreg";
        s.dielectrics[i].thickness_mm = dk;
        s.dielectrics[i].er = FR4_ER_1GHZ;
        s.dielectrics[i].tan_delta = FR4_TAN_DELTA;
        s.dielectrics[i].thermal_k = FR4_THERMAL_K;
    }
    return s;
}

void board_stackup_free(board_stackup_t *s)
{
    if (s) {
        free(s->layers);
        free(s->dielectrics);
        s->layers = NULL;
        s->dielectrics = NULL;
        s->num_layers = 0;
    }
}

void board_outline_free(board_outline_t *o)
{
    if (o) {
        free(o->vertex_x_mm);
        free(o->vertex_y_mm);
        o->vertex_x_mm = NULL;
        o->vertex_y_mm = NULL;
        o->corners = 0;
    }
}
