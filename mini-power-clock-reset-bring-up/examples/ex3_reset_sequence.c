#include "../include/reset_design.h"
#include <stdio.h>
#include <math.h>
int main(void) {
    printf("=== Reset Circuit Design ===\n");
    double t_por = por_delay_seconds(100e3, 0.1e-6, 3.3, 2.0);
    printf("POR delay (100k+100nF): %.2f ms\n", t_por*1000);
    supervisor_ic_t sup = {2.93, 50.0, 200.0, 10.0, 140.0, "MAX809"};
    int compat = supervisor_compatibility_check(&sup, 2100.0, 100.0);
    printf("MAX809 Vth=%.2fV compat: %s\n", sup.threshold_V, compat==0?"OK":"FAIL");
    reset_timing_t timing;
    reset_timing_calculate(100e3, 0.1e-6, &sup, 100.0, &timing);
    printf("Reset: assert=%.0fus hold=%.0fus recov=%.0fus total=%.0fus\n", timing.assertion_delay_us, timing.hold_time_us, timing.recovery_time_us, timing.total_reset_time_us);
    printf("Watchdog optimal: %.0f ms\n", watchdog_optimal_timeout(10.0, 1.0, 1.5));
    double wdg_period = watchdog_period_us(32, 4095, 32000.0);
    printf("IWDG period: %.0f ms\n", wdg_period/1000);
    int faults = stm32f4_reset_config(2.4, 1, 1000.0, 0, 0.0, 0.0);
    printf("STM32F4 reset: %s\n", faults==0?"OK":"FAULTS");
    int asil = automotive_reset_architecture(2, 1, 1, 100.0, 50.0);
    printf("Auto ASIL: %d\n", asil);
    double dc = sil_reset_diagnostic_coverage(2, 1, 2, 0);
    printf("SIL DC: %.0f%%\n", dc*100);
    printf("Example complete.\n");
    return 0;
}
