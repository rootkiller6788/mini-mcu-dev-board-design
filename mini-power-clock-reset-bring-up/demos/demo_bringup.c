#include "../include/power_design.h"
#include "../include/clock_design.h"
#include <stdio.h>
int main(void) {
    printf("=== Power & Clock Demo ===\n");
    int topo = power_topology_select(5.0, 3.3, 0.2, 1);
    printf("5V->3.3V @200mA noise-sens: %s\n", topo==0?"LDO":"DC-DC");
    dcdc_converter_t buck = {12.0,12.0,5.0,2.0,500e3,0.92,15.0,47.0,20.0,"buck"};
    double d = buck_duty_cycle(&buck);
    double rip = buck_inductor_ripple(&buck, d);
    printf("Buck 12V->5V: D=%.2f dI=%.2fA\n", d, rip);
    crystal_spec_t xtal = {8e6,20.0,30.0,12.5,5.0,5.0,10.0,50.0,100.0};
    double c_ext;
    crystal_load_capacitor(&xtal, 3.5, &c_ext);
    printf("8MHz crystal: C_ext=%.1f pF\n", c_ext);
    double life = esp32_battery_life(2000.0, 200.0, 0.02, 10.0, 0.98);
    printf("ESP32 2000mAh: %.0f hours\n", life);
    printf("Demo complete.\n");
    return 0;
}
