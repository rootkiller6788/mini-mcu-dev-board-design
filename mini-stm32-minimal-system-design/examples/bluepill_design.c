/**
 * @example bluepill_design.c
 * @brief STM32F103C8T6 Blue Pill minimal system design example.
 *
 * Knowledge: L6 Canonical Problem — STM32F103 minimal system
 *
 * Demonstrates a complete design flow for a Blue Pill-like board:
 * 1. Configure board parameters
 * 2. Validate power, clock, reset subsystems
 * 3. Compute decoupling requirements
 * 4. Estimate PCB area and BOM
 * 5. Generate design report
 */
#include "stm32_minimal_config.h"
#include "power_system.h"
#include "clock_system.h"
#include "decoupling.h"
#include "pcb_layout.h"
#include "reset_boot.h"
#include "thermal.h"
#include "board_validation.h"
#include <stdio.h>
#include <math.h>

int main(void) {
    printf("=== STM32F103C8T6 Blue Pill Minimal System Design ===\n\n");

    /* Step 1: Configure the board */
    BoardConfig cfg;
    stm32f103c8_bluepill_config(&cfg);
    printf("Board: STM32F103C8T6, LQFP48, 72MHz, 64KB Flash\n");

    /* Step 2: Validate power supply */
    printf("\n--- Power Validation ---\n");
    int power_ok = validate_power_spec(&cfg.power_specs[0], cfg.series);
    printf("VDD spec: %s\n", power_ok ? "PASS" : "FAIL");

    double mcu_power = estimate_mcu_power(3.3, 72e6, 3);
    printf("Estimated MCU power at 72MHz: %.0f mW\n", mcu_power * 1000);

    double bulk_cap = size_bulk_capacitance(0.05, 100, 100);
    printf("Recommended bulk capacitance: %.1f uF\n", bulk_cap * 1e6);

    /* Step 3: Clock system analysis */
    printf("\n--- Clock Analysis ---\n");
    double cl1, cl2;
    if (compute_load_capacitors(&cfg.hse_crystal, 5e-12, &cl1, &cl2) == 0) {
        printf("HSE load caps: CL1=CL2=%.1f pF\n", cl1 * 1e12);
    }

    double gm = compute_gain_margin(&cfg.hse_crystal, 5e-3);
    printf("HSE gain margin: %.1f (>= 5 recommended)\n", gm);

    /* Step 4: Decoupling network */
    printf("\n--- Decoupling Design ---\n");
    DecouplingCap cap_100nf = {0.1e-6, 0.003, 0.4e-9, 16, 603};
    double z_target = compute_target_impedance(3.3, 5, 0.05);
    printf("Target PDN impedance: %.3f ohm\n", z_target);
    int n_caps = min_caps_for_impedance(&cap_100nf, z_target, 0.7);
    printf("Minimum 100nF caps per rail: %d\n", n_caps);

    /* Step 5: PCB estimation */
    printf("\n--- PCB Estimation ---\n");
    double area = compute_pcb_area_estimate(&cfg);
    printf("Estimated minimum PCB area: %.0f mm2 (%.0fx%.0f mm)\n",
           area, sqrt(area), sqrt(area));

    /* Step 6: Reset timing */
    printf("\n--- Reset Timing ---\n");
    double t_vih = nrst_time_to_vih(3.3, 2.31, 40000, 100e-9);
    printf("NRST rise to VIH: %.3f ms\n", t_vih * 1000);

    /* Step 7: Thermal check */
    printf("\n--- Thermal ---\n");
    ThermalPoint tp;
    full_thermal_analysis(mcu_power, 25, 85, 20, 50, 200, 2, NULL, &tp);
    printf("Junction temp at 25C ambient: %.1f C (margin: %.1f C)\n",
           tp.junction_temp_c, tp.margin_c);

    /* Step 8: Layout rules */
    printf("\n--- Layout Guidelines ---\n");
    double trace_w = ipc2221_trace_width(0.2, 10, 1.0, 0);
    printf("Power trace width for 200mA: %.2f mm\n", trace_w);

    double z0_50 = microstrip_width_for_impedance(4.4, 50, 0.035, 0.2);
    printf("50-ohm microstrip width (FR-4, 0.2mm height): %.2f mm\n", z0_50);

    /* Step 9: BOM summary */
    printf("\n--- BOM Summary ---\n");
    BOMEstimate bom;
    compute_board_bom(&cfg, &bom);
    printf("100nF caps: %d\n", bom.cap_100nf_count);
    printf("4.7uF caps: %d\n", bom.cap_4u7_count);
    printf("Load capacitors: %d\n", bom.cap_cl1_cl2);
    printf("Ferrite beads: %d\n", bom.ferrite_bead);

    /* Step 10: Design report */
    printf("\n--- Design Report ---\n");
    DesignValidationReport report;
    ResetCircuit reset = {0, 100e-9, 0.004, 0, {0}, 1, 10};
    validate_complete_design(&cfg, &reset, 8.0, 1.5, 30, 200, 2, &report);

    DesignMargin margin;
    compute_design_margin(&cfg, 25, 3.3, 72e6, &margin);

    char report_buf[2048];
    generate_design_report(&cfg, &report, &margin, report_buf, 2048);
    printf("%s\n", report_buf);

    printf("\n=== Design Complete ===\n");
    return 0;
}
