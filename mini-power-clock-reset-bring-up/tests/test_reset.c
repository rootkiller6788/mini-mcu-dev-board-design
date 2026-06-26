#include "../include/reset_design.h"
#include <stdio.h>
#include <assert.h>
#include <math.h>
int main(void) {
    double t_por = por_delay_seconds(100e3, 0.1e-6, 3.3, 2.0);
    assert(t_por > 0.0 && t_por < 0.01);
    assert(decode_reset_source(1u << 17) == RESET_SRC_POR);
    double v = rc_charge_voltage(3.3, 100e3, 0.1e-6, 5e-3);
    assert(v > 0.0 && v < 3.3);
    double tw = watchdog_period_us(32, 4095, 32000.0);
    assert(tw > 0.0);
    double vd = exponential_decay(3.3, 0.01, 0.02);
    assert(vd > 0.0 && vd < 3.3);
    int counter = 0;
    for (int i = 0; i < 20; i++) debounce_button(1, &counter, 10);
    assert(counter >= 10);
    double wdt_opt = watchdog_optimal_timeout(10.0, 1.0, 1.5);
    assert(wdt_opt > 10.0);
    int faults = stm32f4_reset_config(2.4, 1, 1000.0, 0, 0.0, 0.0);
    assert(faults == 0);
    assert(stm32_reset_cause_get(1u << 26) == RESET_SRC_WATCHDOG);
    assert(nrf52_reset_cause_get(1u << 0) == RESET_SRC_EXTERNAL);
    int asil = automotive_reset_architecture(2, 1, 1, 100.0, 50.0);
    assert(asil == 3);
    double dc = sil_reset_diagnostic_coverage(2, 1, 2, 0);
    assert(dc > 0.7);
    int sys_ok, wdg_disagree;
    dual_watchdog_check(1, 0, 1, 0, &sys_ok, &wdg_disagree);
    assert(sys_ok == 1 && wdg_disagree == 1);
    printf("ALL RESET TESTS PASSED\n");
    return 0;
}
