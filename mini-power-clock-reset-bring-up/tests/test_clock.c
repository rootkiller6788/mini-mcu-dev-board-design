#include "../include/clock_design.h"
#include <stdio.h>
#include <assert.h>
#include <math.h>
int main(void) {
    crystal_spec_t xtal = {8e6, 20.0, 30.0, 12.5, 5.0, 5.0, 10.0, 50.0, 100.0};
    double c_ext;
    crystal_load_capacitor(&xtal, 3.5, &c_ext);
    assert(c_ext > 10.0 && c_ext < 25.0);
    pierce_oscillator_t osc;
    barkhausen_check(&xtal, 12.5, 1e-3, &osc);
    pll_config_t pll;
    int pll_ok = pll_frequency_synthesis(8e6, 168e6, 100e6, 432e6, &pll);
    assert(pll_ok == 0);
    double pn = leeson_phase_noise(10000.0, 100e6, 50000.0, 1000.0, 2.0, 0.001);
    assert(pn < 0.0);
    double fs, fp;
    crystal_resonance_frequencies(&xtal, &fs, &fp);
    assert(fp > fs);
    double tj = adc_jitter_limit(12, 1e6);
    assert(tj > 0.0 && tj < 1e-9);
    phase_noise_t pn_spec = {100e6, -80.0, -100.0, -120.0, -140.0, -155.0, -160.0};
    double rms_jitter = phase_noise_to_jitter(&pn_spec, 1000.0, 1e6);
    assert(rms_jitter > 0.0);
    double c1, r2, c2;
    pll.charge_pump_current_A = 100e-6;
    pll.kvco_Hz_per_V = 50e6;
    pll_loop_filter_design(&pll, &c1, &r2, &c2);
    assert(c1 > 0.0 && r2 > 0.0);
    double rj, dj;
    jitter_decomposition(10.0, 140.0, &rj, &dj);
    assert(rj > 0.0);
    int m,n,p,q;
    int stm_ok = stm32f4_pll_config(8e6, 168e6, &m, &n, &p, &q);
    assert(stm_ok == 0);
    int pll_mult, cpu_div;
    int esp_ok = esp32_clock_config(40e6, 240e6, &pll_mult, &cpu_div);
    assert(esp_ok == 0);
    int pcie_ok = pcie_refclk_budget(0.3, 0.5, 0.2, 1.0);
    assert(pcie_ok == 0);
    double score = mems_vs_quartz_comparison(-120.0, 50.0, -140.0, 20.0);
    double emi_red = sscg_emi_reduction(100e6, 0.5, 120e3);
    assert(emi_red > 0.0);
    printf("ALL CLOCK TESTS PASSED\n");
    return 0;
}
