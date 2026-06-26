#include "stm32_minimal_config.h"
#include "power_system.h"
#include "clock_system.h"
#include "pcb_layout.h"
#include "reset_boot.h"
#include "thermal.h"
#include <math.h>
#include <string.h>
#include <stdio.h>

typedef struct {
    int power_ok, clock_ok, reset_ok, layout_ok, thermal_ok, emc_ok;
    int overall_pass, total_checks, passed_checks;
    char warnings[32][256]; int warning_count;
    char errors[16][256]; int error_count;
} DesignValidationReport;

void validate_complete_design(const BoardConfig *cfg,
                               const ResetCircuit *reset,
                               double crystal_dist_mm,
                               double max_decoup_dist_mm,
                               double swd_trace_length_mm,
                               double copper_area_mm2,
                               int num_layers,
                               DesignValidationReport *report) {
    if (!cfg || !report) return;
    memset(report, 0, sizeof(*report));
    report->overall_pass = 1;

    /* Power validation */
    report->total_checks++;
    if (validate_power_spec(&cfg->power_specs[0], cfg->series)) {
        report->power_ok = 1; report->passed_checks++;
    } else {
        report->power_ok = 0; report->overall_pass = 0;
        sprintf(report->errors[report->error_count++], "Power spec failed");
    }

    if (cfg->vdda_pin_count > 0) {
        report->total_checks++;
        if (validate_power_spec(&cfg->power_specs[1], cfg->series)) {
            report->passed_checks++;
        } else {
            report->overall_pass = 0;
            sprintf(report->errors[report->error_count++], "VDDA spec failed");
        }
    }

    /* PLL validation */
    report->total_checks++;
    if (cfg->pll.enabled) {
        double vco, sys, usb;
        double input_freq = (cfg->pll.input_source == CLOCK_SOURCE_HSE)
                            ? cfg->hse_crystal.nominal_freq : 16e6;
        if (compute_pll_frequencies(input_freq, cfg->pll.input_divider,
                                     cfg->pll.multiplier, cfg->pll.sys_divider,
                                     cfg->pll.usb_divider, &vco, &sys, &usb) == 0) {
            report->clock_ok = 1; report->passed_checks++;
        } else {
            report->clock_ok = 0; report->overall_pass = 0;
            sprintf(report->errors[report->error_count++], "PLL failed");
        }
    } else {
        report->clock_ok = 1; report->passed_checks++;
    }

    /* Crystal validation */
    if (cfg->hse_crystal.nominal_freq > 0) {
        report->total_checks++;
        double gm = compute_gain_margin(&cfg->hse_crystal, 5e-3);
        if (gm >= 5.0) { report->passed_checks++; }
        else {
            report->overall_pass = 0;
            sprintf(report->errors[report->error_count++],
                    "HSE gain margin too low: %.1f", gm);
        }
    }

    /* Reset timing check */
    if (reset) {
        report->total_checks++;
        NRSTPinCharacteristics nrst;
        nrst.internal_pullup_ohm = 40000;
        nrst.vih_min = 0.7 * 3.3;
        nrst.vil_max = 0.3 * 3.3;
        nrst.output_low_vol = 0.4;
        nrst.min_reset_pulse_ns = 300;
        if (check_nrst_timing(reset, &nrst, 1.0)) {
            report->reset_ok = 1; report->passed_checks++;
        } else {
            report->reset_ok = 0; report->overall_pass = 0;
            sprintf(report->errors[report->error_count++], "NRST timing failed");
        }
    }

    /* Layout check */
    {
        LayoutValidation lr;
        validate_stm32_layout(crystal_dist_mm, max_decoup_dist_mm,
                              swd_trace_length_mm, 1, 0.5, 0.2, 1.0, &lr);
        report->total_checks++;
        if (lr.passes) { report->layout_ok = 1; report->passed_checks++; }
        else { report->layout_ok = 0; report->overall_pass = 0; }
    }

    /* Thermal check */
    report->total_checks++;
    if (copper_area_mm2 > 100 || num_layers >= 4) {
        report->thermal_ok = 1; report->passed_checks++;
    } else {
        sprintf(report->warnings[report->warning_count++],
                "Low copper area, consider increasing");
        report->thermal_ok = 1; report->passed_checks++;
    }

    /* EMC check */
    report->total_checks++;
    if (num_layers >= 2) {
        report->emc_ok = 1; report->passed_checks++;
    } else {
        report->emc_ok = 0; report->overall_pass = 0;
        sprintf(report->errors[report->error_count++],
                "Single layer board not recommended for EMC");
    }
}

typedef struct {
    double vdd_margin_percent, clock_margin_percent, thermal_margin_percent;
    double overall_margin_percent;
} DesignMargin;

void compute_design_margin(const BoardConfig *cfg, double ambient_temp_c,
                            double actual_vdd, double actual_sysclk,
                            DesignMargin *margin) {
    if (!cfg || !margin) return;
    memset(margin, 0, sizeof(*margin));
    double vdd_min = cfg->power_specs[0].min_voltage;
    if (vdd_min > 0)
        margin->vdd_margin_percent = (actual_vdd - vdd_min) / vdd_min * 100.0;
    if (cfg->core_max_freq_hz > 0)
        margin->clock_margin_percent = (cfg->core_max_freq_hz - actual_sysclk)
                                       / cfg->core_max_freq_hz * 100.0;
    if (cfg->max_temp_c > ambient_temp_c)
        margin->thermal_margin_percent = (cfg->max_temp_c - ambient_temp_c)
                                         / cfg->max_temp_c * 100.0;
    margin->overall_margin_percent = margin->vdd_margin_percent;
    if (margin->clock_margin_percent < margin->overall_margin_percent)
        margin->overall_margin_percent = margin->clock_margin_percent;
    if (margin->thermal_margin_percent < margin->overall_margin_percent)
        margin->overall_margin_percent = margin->thermal_margin_percent;
}

int check_power_integrity(double vdd_actual, double vdd_nominal,
                           double vdd_ripple_mv, double ripple_max_mv,
                           double bulk_cap_uf, double min_bulk_cap_uf) {
    int score = 100;
    double vdd_error = fabs(vdd_actual - vdd_nominal) / vdd_nominal * 100.0;
    if (vdd_error > 5.0) score -= 20;
    else if (vdd_error > 3.0) score -= 10;
    else if (vdd_error > 1.0) score -= 5;
    if (ripple_max_mv > 0) {
        double ripple_ratio = vdd_ripple_mv / ripple_max_mv;
        if (ripple_ratio > 1.0) score -= 30;
        else if (ripple_ratio > 0.8) score -= 15;
        else if (ripple_ratio > 0.5) score -= 5;
    }
    if (min_bulk_cap_uf > 0) {
        double cap_ratio = bulk_cap_uf / min_bulk_cap_uf;
        if (cap_ratio < 0.5) score -= 20;
        else if (cap_ratio < 1.0) score -= 10;
    }
    if (score < 0) score = 0;
    return score;
}

int check_signal_integrity(double crystal_dist_mm, double swd_trace_length_mm,
                            double swd_clock_freq_hz, double epsilon_r_eff) {
    int ok = 1;
    if (crystal_dist_mm > 10.0) ok = 0;
    double crit_len = compute_critical_length(10000.0, epsilon_r_eff);
    if (swd_trace_length_mm > crit_len) ok = 0;
    if (swd_clock_freq_hz > 10e6 && swd_trace_length_mm > 30.0) ok = 0;
    return ok;
}

void generate_design_report(const BoardConfig *cfg,
                             const DesignValidationReport *validation,
                             const DesignMargin *margin,
                             char *report_buffer, int buffer_size) {
    if (!cfg || !validation || !margin || !report_buffer || buffer_size <= 0) return;
    int pos = 0;
    pos += snprintf(report_buffer + pos, buffer_size - pos,
                    "=== STM32 Minimal System Design Report ===\n\n");
    pos += snprintf(report_buffer + pos, buffer_size - pos,
                    "MCU: series %d, %d pins, %.1f MHz\n",
                    cfg->series, cfg->pin_count, cfg->core_max_freq_hz / 1e6);
    pos += snprintf(report_buffer + pos, buffer_size - pos,
                    "Flash: %d KB, SRAM: %d KB\n",
                    cfg->flash_size_kb, cfg->sram_size_kb);
    pos += snprintf(report_buffer + pos, buffer_size - pos,
                    "VDD: %.1fV nominal, %.0f mA max\n",
                    cfg->power_specs[0].nominal_voltage,
                    cfg->power_specs[0].max_current * 1000.0);
    pos += snprintf(report_buffer + pos, buffer_size - pos,
                    "Power=%s Clock=%s Reset=%s Layout=%s Thermal=%s EMC=%s\n",
                    validation->power_ok ? "PASS" : "FAIL",
                    validation->clock_ok ? "PASS" : "FAIL",
                    validation->reset_ok ? "PASS" : "FAIL",
                    validation->layout_ok ? "PASS" : "FAIL",
                    validation->thermal_ok ? "PASS" : "FAIL",
                    validation->emc_ok ? "PASS" : "FAIL");
    pos += snprintf(report_buffer + pos, buffer_size - pos,
                    "Checks passed: %d / %d\n",
                    validation->passed_checks, validation->total_checks);
    pos += snprintf(report_buffer + pos, buffer_size - pos,
                    "Margins: VDD %.1f%%, Clock %.1f%%, Thermal %.1f%%\n",
                    margin->vdd_margin_percent, margin->clock_margin_percent,
                    margin->thermal_margin_percent);
    pos += snprintf(report_buffer + pos, buffer_size - pos,
                    "Overall: %s\n",
                    validation->overall_pass ? "PASS" : "FAIL");
}

/*
 * quick_checklist — minimalist go/no-go checklist for bringup.
 * L5: Practical bringup checklist derived from ST AN4488.
 * Returns 0 if critical issues found, 1 if board is ready for power-on.
 */
int quick_checklist(double vdd_to_gnd_resistance,
                     double vdda_to_gnd_resistance,
                     int visual_inspection_ok,
                     int polarity_check_ok,
                     int solder_bridge_check_ok) {
    if (!visual_inspection_ok) return 0;
    if (!polarity_check_ok) return 0;
    if (!solder_bridge_check_ok) return 0;
    /* Typical VDD-GND resistance: 10k-100k ohm (through internal circuits)
     * A dead short (< 10 ohm) indicates a solder bridge or damaged chip */
    if (vdd_to_gnd_resistance < 10.0) return 0;
    if (vdda_to_gnd_resistance < 10.0) return 0;
    /* Open circuit (> 1M) may indicate missing power connection */
    if (vdd_to_gnd_resistance > 1e6) return 0;
    return 1;
}
