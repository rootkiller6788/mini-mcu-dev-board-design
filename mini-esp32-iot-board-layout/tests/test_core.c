/**
 * @file    test_core.c
 * @brief   Core tests: board geometry, stackup, transmission line impedance.
 * @author  mini-esp32-iot-board-layout
 */

#include "board_geometry.h"
#include "transmission_line.h"
#include <stdio.h>
#include <math.h>
#include <assert.h>
#include <stdlib.h>

static int passed = 0, failed = 0;

#define TEST(n) do{printf("  TEST %-50s",n);fflush(stdout);}while(0)
#define CHECK(c) do{if(c){printf("PASS\n");passed++;}else{printf("FAIL\n");failed++;}}while(0)
#define CHECK_NEAR(a,b,t) do{if(fabs((a)-(b))<=(t)){printf("PASS\n");passed++;}else{printf("FAIL: %g vs %g\n",(double)(a),(double)(b));failed++;}}while(0)

int main(void)
{
    printf("=== Test Core: Board Geometry and Transmission Lines ===\n");

    TEST("MIL_TO_M(1) ~ 2.54e-5");
    CHECK(fabs(MIL_TO_M(1.0) - 2.54e-5) < 1e-10);

    TEST("OZ_TO_UM(1) = 35");
    CHECK(fabs(OZ_TO_UM(1.0) - 35.0) < 0.01);

    TEST("stackup_4layer_standard allocates");
    { board_stackup_t s = stackup_4layer_standard(1.6, CU_WEIGHT_1OZ, CU_WEIGHT_0_5OZ);
      CHECK(s.num_layers == 4 && s.layers != NULL);
      board_stackup_free(&s); }

    TEST("stackup_2layer_standard allocates");
    { board_stackup_t s = stackup_2layer_standard(1.6, CU_WEIGHT_1OZ);
      CHECK(s.num_layers == 2 && s.layers != NULL);
      board_stackup_free(&s); }

    TEST("microstrip_z0(FR4, 0.2mm, 0.35mm) ~ 50 ohm");
    { double z0 = microstrip_z0(FR4_ER_1GHZ, 0.2, 0.35, 0.035);
      CHECK(fabs(z0 - 50.0) < 15.0); }

    TEST("microstrip_z0 rejects invalid input");
    CHECK(microstrip_z0(-1.0, 0.2, 0.35, 0.035) < 0.0);

    TEST("microstrip_ereff(4.2, 0.2, 0.35) > 1.0");
    { double ereff = microstrip_ereff(FR4_ER_1GHZ, 0.2, 0.35, 0.035);
      CHECK(ereff > 1.0 && ereff < FR4_ER_1GHZ); }

    TEST("stripline_z0(4.2, 1.0, 0.2, 0.017) valid");
    CHECK(stripline_z0(FR4_ER_1GHZ, 1.0, 0.2, 0.017) > 0.0);

    TEST("cpw_z0(4.2, 1.6, 0.5, 0.2, 0.035) valid");
    CHECK(cpw_z0(FR4_ER_1GHZ, 1.6, 0.5, 0.2, 0.035) > 0.0);

    TEST("wavelength_in_substrate(2.45GHz, 3.0) ~ 70.7mm");
    { double lam = wavelength_in_substrate(2.45e9, 3.0);
      CHECK(fabs(lam - C_LIGHT/(2.45e9*sqrt(3.0))*1000.0) < 2.0); }

    TEST("propagation_delay_ps_per_mm(3.0) ~ 5.77");
    CHECK_NEAR(propagation_delay_ps_per_mm(3.0), 5.77, 0.2);

    TEST("board_area 10x10mm square = 100");
    { double vx[] = {0,10,10,0}, vy[] = {0,0,10,10};
      board_outline_t o = {10, 10, 1.6, 4, 4, vx, vy};
      CHECK_NEAR(board_area(&o), 100.0, 0.001); }

    TEST("trace_resistance_per_mm(0.254, 35, 25) > 0");
    CHECK(trace_resistance_per_mm(0.254, 35.0, 25.0) > 0.0);

    TEST("thermal_via_count(1W, 0.3, 1.6, 25, 20) >= 1");
    CHECK(thermal_via_count(1.0, 0.3, 1.6, 25.0, 20.0) >= 1);

    TEST("dielectric_thickness_for_z0 valid");
    { double h = dielectric_thickness_for_z0(4.2, 50.0, 0.35);
      CHECK(h > 0.0); }

    TEST("min_board_area_estimate(18, 25.5, 2, 2) > 0");
    CHECK(min_board_area_estimate(18.0, 25.5, 2, 2) > 0.0);

    printf("\n=== Results: %d passed, %d failed ===\n", passed, failed);
    return (failed > 0) ? 1 : 0;
}
