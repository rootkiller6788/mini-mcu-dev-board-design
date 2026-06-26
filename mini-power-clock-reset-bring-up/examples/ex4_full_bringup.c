#include "../include/bring_up.h"
#include "../include/board_validation.h"
#include "../include/power_design.h"
#include <stdio.h>
#include <string.h>
int main(void) {
    printf("======== FULL BRING-UP ========\n");
    printf("Phase 1: Continuity\n");
    double r; int s;
    continuity_check(&r, &s);
    printf("  VDD-GND: %.1fk Short:%s\n", r/1000, s?"YES":"No");
    printf("Phase 2: First Power\n");
    first_power_measurement_t m;
    first_power_up(100.0, 3.3, &m);
    printf("  3.3V=%.3f 1.8V=%.3f 1.2V=%.3f P=%.1fmW\n", m.v_3v3_V, m.v_1v8_V, m.v_1v2_V, m.power_total_mW);
    printf("Phase 3: Clock+Reset\n");
    int h,l,p; clock_verification(8e6,32768.0,&h,&l,&p);
    int pd,nr; double b; reset_verification(&pd,&nr,&b);
    printf("  HSE:%s PLL:%s POR:%s\n", h?"OK":"FAIL", p?"LOCK":"UNLOCK", pd?"OK":"FAIL");
    printf("Phase 4: Signal Integrity\n");
    eye_diagram_t eye = {400.0,1200.0,80.0,0.7,5.0,70.0,8.0,8.0,7.0,1e-12,1};
    double mV,ps;
    usb20_eye_compliance(&eye,&mV,&ps);
    printf("  USB20: %.0fmV x %.0fps %s\n", eye.eye_height_mV, eye.eye_width_ps, mV>0&&ps>0?"PASS":"FAIL");
    printf("========================================\n");
    printf("  RESULT: %s\n", (s||!m.all_rails_ok||!h)?"ISSUES":"ALL PASS");
    printf("========================================\n");
    return 0;
}
