#include "../include/power_design.h"
#include <stdio.h>
#include <assert.h>
#include <math.h>
int main(void) {
    assert(power_topology_select(5.0, 3.3, 0.1, 1) == 0);
    assert(power_topology_select(12.0, 3.3, 0.5, 0) == 1);
    dcdc_converter_t buck = {12.0, 12.0, 5.0, 2.0, 500e3, 0.9, 22.0, 47.0, 10.0, "buck"};
    double duty = buck_duty_cycle(&buck);
    assert(duty > 0.4 && duty < 0.5);
    double ripple = buck_inductor_ripple(&buck, duty);
    assert(ripple > 0.0);
    assert(fabs(rc_time_constant(1000.0, 10e-6) - 0.01) < 1e-6);
    double vdrop = trace_voltage_drop(0.1, 0.0005, 35e-6, 1.0);
    assert(vdrop > 0.05 && vdrop < 0.15);
    double ploss = regulator_power_loss(5.0, 3.3, 0.5, 0.80);
    assert(ploss > 0.0);
    double pm = ldo_phase_margin(100.0, 10000.0, 100000.0);
    assert(pm > 0.0 && pm < 90.0);
    assert(stm32_power_rail_check(3300, 3300, 2200, 3000) == 0);
    assert(stm32_power_rail_check(1500, 3300, 2200, 3000) != 0);
    double e1,e2,e3;
    double pin = mcu_power_budget(0.1, 0.05, 0.3, &e1, &e2, &e3);
    assert(pin > 0.0);
    decoupling_cap_t caps[2] = {{10e-6, 0.01, 1e-9, 1.6e6, 6.3, "X7R", "0805"}, {100e-9, 0.05, 0.5e-9, 22e6, 10.0, "X7R", "0402"}};
    double freqs[] = {1e3, 100e3, 10e6, 100e6}, z_out[4];
    decoupling_impedance_sweep(caps, 2, freqs, 4, z_out);
    for (int i=0;i<4;i++) assert(z_out[i]>0.0);
    double life = esp32_battery_life(3000.0, 200.0, 0.01, 10.0, 0.99);
    assert(life > 100.0);
    double fom, hs_fom;
    gan_vs_si_fom(10.0, 5.0, 1.0, 1, &fom, &hs_fom);
    assert(fom < 20.0);
    printf("ALL POWER TESTS PASSED\n");
    return 0;
}
