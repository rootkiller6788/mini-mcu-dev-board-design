/**
 * @file    test_rf_pdn.c
 * @brief   RF and PDN tests: matching, decoupling, reflection, thermal.
 */

#include "rf_design.h"
#include "power_integrity.h"
#include "thermal_design.h"
#include <stdio.h>
#include <math.h>

static int passed = 0, failed = 0;

#define TEST(n) do{printf("  TEST %-50s",n);fflush(stdout);}while(0)
#define CHECK(c) do{if(c){printf("PASS\n");passed++;}else{printf("FAIL\n");failed++;}}while(0)
#define CHECK_NEAR(a,b,t) do{if(fabs((a)-(b))<=(t)){printf("PASS\n");passed++;}else{printf("FAIL: %g vs %g\n",(double)(a),(double)(b));failed++;}}while(0)

int main(void)
{
    printf("=== Test RF and PDN ===\n");

    TEST("reflection_coefficient(100, 50) ~ 0.333");
    { complex_z_t ZL = {100.0, 0.0};
      complex_z_t gamma = reflection_coefficient(ZL, 50.0);
      CHECK_NEAR(gamma.real, 1.0/3.0, 0.001); }

    TEST("reflection_coefficient(50, 50) = 0");
    { complex_z_t ZL = {50.0, 0.0};
      complex_z_t gamma = reflection_coefficient(ZL, 50.0);
      CHECK_NEAR(gamma.real, 0.0, 0.001); }

    TEST("vswr for Gamma=0 returns 1");
    { complex_z_t g = {0.0, 0.0};
      CHECK_NEAR(vswr(g), 1.0, 0.001); }

    TEST("return_loss for Gamma=0 is infinite");
    { complex_z_t g = {0.0, 0.0};
      CHECK(isinf(return_loss_db(g))); }

    TEST("admittance_from_impedance(50,0) -> (0.02,0)");
    { complex_z_t Z = {50.0, 0.0};
      complex_z_t Y = admittance_from_impedance(Z);
      CHECK_NEAR(Y.real, 0.02, 0.0001); }

    TEST("pdn_target_impedance(3.3V, 5%, 0.5A)");
    { pdn_spec_t spec = {3.3, 5.0, 1.0, 0.5, 10.0, 100.0, 1e9};
      double Zt = pdn_target_impedance(&spec);
      CHECK_NEAR(Zt, 3.3*0.05/0.5, 0.001); }

    TEST("decap_srf(100nF, 0.4nH) ~ 25 MHz");
    { decap_model_t cap = DECAP_100NF_0402;
      double fsrf = decap_srf(&cap);
      CHECK(fsrf > 1e6 && fsrf < 100e6); }

    TEST("decap_impedance at SRF equals ESR");
    { decap_model_t cap = {10e-6, 1e-9, 0.01, 10.0, "0603", "X5R"};
      double fsrf = decap_srf(&cap);
      if (fsrf > 0 && isfinite(fsrf)) {
        double Z = decap_impedance(&cap, fsrf);
        CHECK_NEAR(Z, cap.esr_ohm, cap.esr_ohm * 0.3);
      } else { CHECK(0); } }

    TEST("plane_capacitance(100x100mm, 0.2mm, FR4)");
    { plane_params_t pp = {100.0, 100.0, 0.2, 4.2, 0.02};
      double C = plane_capacitance(&pp);
      CHECK(C > 1e-9 && C < 1e-6); }

    TEST("via_inductance(1.6mm, 0.3mm) ~ 1.2 nH");
    { double L = via_inductance(1.6, 0.3);
      CHECK_NEAR(L, 1.2e-9, 0.5e-9); }

    TEST("junction_temperature(25C, 0.5W, 50K/W) = 50C");
    CHECK_NEAR(junction_temperature(25.0, 0.5, 50.0), 50.0, 0.01);

    TEST("heatsink_thermal_resistance(85C, 25C, 1W, 5, 1)");
    CHECK_NEAR(heatsink_thermal_resistance(85.0, 25.0, 1.0, 5.0, 1.0), 54.0, 0.01);

    TEST("parallel_thermal_resistance(10, 10) = 5");
    CHECK_NEAR(parallel_thermal_resistance(10.0, 10.0), 5.0, 0.001);

    TEST("series_thermal_resistance(10, 20) = 30");
    CHECK_NEAR(series_thermal_resistance(10.0, 20.0), 30.0, 0.001);

    TEST("free_space_path_loss_db(1m, 2.45GHz) ~ 40 dB");
    { double fspl = free_space_path_loss_db(1.0, 2.45e9);
      CHECK_NEAR(fspl, 40.2, 1.0); }

    TEST("antenna_link_budget basic");
    { double pr = antenna_link_budget(20.0, 3.0, 3.0, 1.0, 2.45e9);
      CHECK(pr < 20.0); }



    printf("\n=== Results: %d passed, %d failed ===\n", passed, failed);
    return (failed > 0) ? 1 : 0;
}
