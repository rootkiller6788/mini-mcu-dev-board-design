#include "../include/bring_up.h"
#include "../include/board_validation.h"
#include <stdio.h>
#include <assert.h>
#include <math.h>
#include <string.h>
int main(void) {
    first_power_measurement_t power;
    first_power_up(100.0, 3.3, &power);
    uint32_t dev_id; int halted;
    assert(debug_connectivity_check("SWD", &dev_id, &halted) == 0);
    double v_actual = voltage_divider_actual(1.65, 100e3, 100e3);
    assert(fabs(v_actual - 3.3) < 0.1);
    assert(ohms_law_verify(3.3, 100.0, 0.033, 5.0) == 1);
    assert(voltage_within_tolerance(3.25, 3.3, 5.0) == 1);
    bringup_step_t steps[3];
    for (int i=0;i<3;i++) { steps[i].step_number=i+1; snprintf(steps[i].description,128,"Step %d",i+1); steps[i].is_automated=1; steps[i].blocking_on_fail=0; steps[i].timeout_seconds=10.0; }
    bringup_status_t status;
    automated_bringup_sequence(steps, 3, &status);
    assert(status.total_steps == 3);
    int results[]={1,1,1,1,1,1,1};
    assert(nucleo_bringup_checklist(results) == 0);
    int bl_ok,flash_ok,verify_ok;
    esp32_first_flash(0,1,&bl_ok,&flash_ok,&verify_ok);
    assert(bl_ok && flash_ok && verify_ok);
    eye_diagram_t eye = {400.0, 1200.0, 80.0, 0.7, 5.0, 70.0, 10.0, 10.0, 6.0, 1e-9, 1};
    double ber = ber_from_qfactor(7.0);
    assert(ber < 1e-10);
    double z0,gr,gi;
    telegrapher_equations(0.1, 250e-9, 1e-12, 100e-12, 1e9, &z0, &gr, &gi);
    assert(z0 > 30.0 && z0 < 80.0);
    printf("ALL BRINGUP TESTS PASSED\n");
    return 0;
}
