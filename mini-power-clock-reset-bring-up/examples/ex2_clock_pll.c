#include "../include/clock_design.h"
#include <stdio.h>
#include <math.h>
int main(void) {
    printf("=== Clock & PLL Design ===\n");
    crystal_spec_t xtal = {8e6, 20.0, 30.0, 12.5, 5.0, 5.0, 10.0, 50.0, 100.0};
    double c_ext;
    crystal_load_capacitor(&xtal, 3.5, &c_ext);
    printf("8MHz crystal CL=12.5pF -> C_ext=%.1f pF\n", c_ext);
    pierce_oscillator_t osc;
    barkhausen_check(&xtal, 12.5, 5e-3, &osc);
    printf("Pierce osc: gm_crit=%.1fuS margin=%.1f\n", osc.gm_critical_S*1e6, osc.gain_margin);
    pll_config_t pll;
    pll_frequency_synthesis(8e6, 168e6, 100e6, 432e6, &pll);
    printf("STM32F4 PLL: 8MHz->%.0fMHz SYSCLK\n", pll.vco_freq_Hz/2e6);
    printf("  PLLM=%d PLLN=%d\n", pll.ref_div, pll.feedback_div);
    pll.charge_pump_current_A = 100e-6; pll.kvco_Hz_per_V = 50e6;
    double c1, r2, c2;
    pll_loop_filter_design(&pll, &c1, &r2, &c2);
    printf("Loop filter: C1=%.2fnF R2=%.1fk\n", c1*1e9, r2*1e-3);
    printf("Phase noise(10kHz)=%.1f dBc/Hz\n", leeson_phase_noise(10000,100e6,50000,1000,2.0,0.001));
    printf("ADC jitter limit: %.2f ps\n", adc_jitter_limit(12,1e6)*1e12);
    printf("Example complete.\n");
    return 0;
}
