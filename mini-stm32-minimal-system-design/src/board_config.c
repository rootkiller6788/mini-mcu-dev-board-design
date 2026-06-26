#include "stm32_minimal_config.h"
#include "power_system.h"
#include "clock_system.h"
#include "reset_boot.h"
#include <string.h>
#include <math.h>
#include <stdio.h>

void stm32f103c8_bluepill_config(BoardConfig *cfg) {
    if (!cfg) return;
    memset(cfg, 0, sizeof(*cfg));
    cfg->series = STM32_SERIES_F1;
    cfg->package = PACKAGE_LQFP48;
    cfg->pin_count = 48;
    cfg->vdd_pin_count = 3;
    cfg->vss_pin_count = 3;
    cfg->vdda_pin_count = 1;
    cfg->core_max_freq_hz = 72e6;
    cfg->flash_size_kb = 64;
    cfg->sram_size_kb = 20;
    cfg->max_temp_c = 85.0;
    cfg->power_specs[0].domain = VOLTAGE_DOMAIN_VDD;
    cfg->power_specs[0].nominal_voltage = 3.3;
    cfg->power_specs[0].min_voltage = 2.0;
    cfg->power_specs[0].max_voltage = 3.6;
    cfg->power_specs[0].max_current = 0.15;
    cfg->power_specs[0].ripple_tolerance = 0.050;
    cfg->power_specs[0].requires_filtering = 0;
    cfg->power_specs[1].domain = VOLTAGE_DOMAIN_VDDA;
    cfg->power_specs[1].nominal_voltage = 3.3;
    cfg->power_specs[1].min_voltage = 2.0;
    cfg->power_specs[1].max_voltage = 3.6;
    cfg->power_specs[1].max_current = 0.005;
    cfg->power_specs[1].ripple_tolerance = 0.010;
    cfg->power_specs[1].requires_filtering = 1;
    cfg->power_specs[2].domain = VOLTAGE_DOMAIN_VBAT;
    cfg->power_specs[2].nominal_voltage = 3.0;
    cfg->power_specs[2].min_voltage = 1.8;
    cfg->power_specs[2].max_voltage = 3.6;
    cfg->power_specs[2].max_current = 0.001;
    cfg->power_specs[2].ripple_tolerance = 0.050;
    cfg->power_specs[2].requires_filtering = 0;
    cfg->hse_crystal.nominal_freq = 8e6;
    cfg->hse_crystal.freq_tolerance_ppm = 30;
    cfg->hse_crystal.load_capacitance = 18e-12;
    cfg->hse_crystal.shunt_capacitance = 7e-12;
    cfg->hse_crystal.esr_max = 80.0;
    cfg->hse_crystal.drive_level_max = 500e-6;
    cfg->lse_crystal.nominal_freq = 32768;
    cfg->lse_crystal.freq_tolerance_ppm = 20;
    cfg->lse_crystal.load_capacitance = 12.5e-12;
    cfg->lse_crystal.shunt_capacitance = 1.1e-12;
    cfg->lse_crystal.esr_max = 35000.0;
    cfg->lse_crystal.drive_level_max = 1.0e-6;
    cfg->pll.enabled = 1;
    cfg->pll.input_source = CLOCK_SOURCE_HSE;
    cfg->pll.input_divider = 1;
    cfg->pll.multiplier = 9;
    cfg->pll.sys_divider = 1;
    cfg->pll.usb_divider = 1;
    cfg->pll.vco_freq = 72e6;
    cfg->pll.output_freq = 72e6;
    cfg->reset.nrst_pullup_resistance = 40000;
    cfg->reset.nrst_capacitance = 100e-9;
    cfg->reset.reset_time_required = 300e-9;
    cfg->reset.has_external_supervisor = 0;
    cfg->reset.bor_threshold = 2.0;
}

void stm32f407vet6_black_config(BoardConfig *cfg) {
    if (!cfg) return;
    memset(cfg, 0, sizeof(*cfg));
    cfg->series = STM32_SERIES_F4;
    cfg->package = PACKAGE_LQFP100;
    cfg->pin_count = 100;
    cfg->vdd_pin_count = 7;
    cfg->vss_pin_count = 7;
    cfg->vdda_pin_count = 1;
    cfg->core_max_freq_hz = 168e6;
    cfg->flash_size_kb = 512;
    cfg->sram_size_kb = 192;
    cfg->max_temp_c = 85.0;
    cfg->power_specs[0].domain = VOLTAGE_DOMAIN_VDD;
    cfg->power_specs[0].nominal_voltage = 3.3;
    cfg->power_specs[0].min_voltage = 1.8;
    cfg->power_specs[0].max_voltage = 3.6;
    cfg->power_specs[0].max_current = 0.25;
    cfg->power_specs[0].ripple_tolerance = 0.040;
    cfg->power_specs[0].requires_filtering = 0;
    cfg->power_specs[1].domain = VOLTAGE_DOMAIN_VDDA;
    cfg->power_specs[1].nominal_voltage = 3.3;
    cfg->power_specs[1].min_voltage = 1.8;
    cfg->power_specs[1].max_voltage = 3.6;
    cfg->power_specs[1].max_current = 0.005;
    cfg->power_specs[1].ripple_tolerance = 0.005;
    cfg->power_specs[1].requires_filtering = 1;
    cfg->hse_crystal.nominal_freq = 25e6;
    cfg->hse_crystal.freq_tolerance_ppm = 20;
    cfg->hse_crystal.load_capacitance = 16e-12;
    cfg->hse_crystal.shunt_capacitance = 5e-12;
    cfg->hse_crystal.esr_max = 50.0;
    cfg->hse_crystal.drive_level_max = 300e-6;
    cfg->lse_crystal.nominal_freq = 32768;
    cfg->lse_crystal.freq_tolerance_ppm = 20;
    cfg->lse_crystal.load_capacitance = 12.5e-12;
    cfg->lse_crystal.shunt_capacitance = 1.1e-12;
    cfg->lse_crystal.esr_max = 35000.0;
    cfg->lse_crystal.drive_level_max = 1.0e-6;
    cfg->pll.enabled = 1;
    cfg->pll.input_source = CLOCK_SOURCE_HSE;
    cfg->pll.input_divider = 25;
    cfg->pll.multiplier = 336;
    cfg->pll.sys_divider = 2;
    cfg->pll.usb_divider = 7;
    cfg->pll.vco_freq = 336e6;
    cfg->pll.output_freq = 168e6;
    cfg->reset.nrst_pullup_resistance = 40000;
    cfg->reset.nrst_capacitance = 100e-9;
    cfg->reset.reset_time_required = 300e-9;
    cfg->reset.has_external_supervisor = 0;
    cfg->reset.bor_threshold = 2.0;
}

void stm32h743vit6_config(BoardConfig *cfg) {
    if (!cfg) return;
    memset(cfg, 0, sizeof(*cfg));
    cfg->series = STM32_SERIES_H7;
    cfg->package = PACKAGE_LQFP100;
    cfg->pin_count = 100;
    cfg->vdd_pin_count = 9;
    cfg->vss_pin_count = 9;
    cfg->vdda_pin_count = 1;
    cfg->core_max_freq_hz = 480e6;
    cfg->flash_size_kb = 2048;
    cfg->sram_size_kb = 1024;
    cfg->max_temp_c = 85.0;
    cfg->power_specs[0].domain = VOLTAGE_DOMAIN_VDD;
    cfg->power_specs[0].nominal_voltage = 3.3;
    cfg->power_specs[0].min_voltage = 1.62;
    cfg->power_specs[0].max_voltage = 3.6;
    cfg->power_specs[0].max_current = 0.40;
    cfg->power_specs[0].ripple_tolerance = 0.030;
    cfg->power_specs[0].requires_filtering = 0;
    cfg->power_specs[1].domain = VOLTAGE_DOMAIN_VDDA;
    cfg->power_specs[1].nominal_voltage = 3.3;
    cfg->power_specs[1].min_voltage = 1.62;
    cfg->power_specs[1].max_voltage = 3.6;
    cfg->power_specs[1].max_current = 0.010;
    cfg->power_specs[1].ripple_tolerance = 0.005;
    cfg->power_specs[1].requires_filtering = 1;
    cfg->hse_crystal.nominal_freq = 25e6;
    cfg->hse_crystal.freq_tolerance_ppm = 15;
    cfg->hse_crystal.load_capacitance = 10e-12;
    cfg->hse_crystal.shunt_capacitance = 3e-12;
    cfg->hse_crystal.esr_max = 40.0;
    cfg->hse_crystal.drive_level_max = 200e-6;
    cfg->lse_crystal.nominal_freq = 32768;
    cfg->lse_crystal.freq_tolerance_ppm = 20;
    cfg->lse_crystal.load_capacitance = 7e-12;
    cfg->lse_crystal.shunt_capacitance = 1.1e-12;
    cfg->lse_crystal.esr_max = 70000.0;
    cfg->lse_crystal.drive_level_max = 0.5e-6;
    cfg->pll.enabled = 1;
    cfg->pll.input_source = CLOCK_SOURCE_HSE;
    cfg->pll.input_divider = 5;
    cfg->pll.multiplier = 192;
    cfg->pll.sys_divider = 2;
    cfg->pll.usb_divider = 10;
    cfg->pll.vco_freq = 960e6;
    cfg->pll.output_freq = 480e6;
    cfg->reset.nrst_pullup_resistance = 40000;
    cfg->reset.nrst_capacitance = 100e-9;
    cfg->reset.reset_time_required = 300e-9;
    cfg->reset.has_external_supervisor = 1;
    cfg->reset.bor_threshold = 2.0;
}

void compute_board_bom(const BoardConfig *cfg, BOMEstimate *bom) {
    if (!cfg || !bom) return;
    memset(bom, 0, sizeof(*bom));
    bom->cap_100nf_count = cfg->vdd_pin_count + cfg->vdda_pin_count;
    bom->cap_4u7_count = (cfg->vdd_pin_count + 1) / 2;
    bom->cap_10uf_count = 1;
    if (cfg->hse_crystal.nominal_freq > 0) bom->cap_cl1_cl2 += 2;
    if (cfg->lse_crystal.nominal_freq > 0) bom->cap_cl1_cl2 += 2;
    bom->resistor_pulldown = 1;
    bom->ferrite_bead = 1;
    if (cfg->hse_crystal.nominal_freq > 0) bom->crystal_hse = 1;
    if (cfg->lse_crystal.nominal_freq > 0) bom->crystal_lse = 1;
    bom->push_button = 1;
    bom->ldo = 1;
}

double compute_pcb_area_estimate(const BoardConfig *cfg) {
    if (!cfg) return 0.0;
    double component_area;
    switch (cfg->package) {
        case PACKAGE_LQFP48:  component_area = 400.0; break;
        case PACKAGE_LQFP64:  component_area = 576.0; break;
        case PACKAGE_LQFP100: component_area = 900.0; break;
        case PACKAGE_LQFP144: component_area = 1600.0; break;
        case PACKAGE_LQFP176: component_area = 2025.0; break;
        case PACKAGE_BGA100:  component_area = 400.0; break;
        default:              component_area = 600.0; break;
    }
    double total_area = component_area * 2.5;
    if (total_area < 900.0) total_area = 900.0;
    return total_area;
}

int validate_board_config(const BoardConfig *cfg) {
    if (!cfg) return 0;
    if (!validate_power_spec(&cfg->power_specs[VOLTAGE_DOMAIN_VDD], cfg->series))
        return 0;
    if (cfg->pll.enabled) {
        if (cfg->pll.multiplier < 1 || cfg->pll.multiplier > 432) return 0;
        if (cfg->pll.input_divider < 1 || cfg->pll.input_divider > 63) return 0;
    }
    if (cfg->hse_crystal.nominal_freq > 0) {
        if (cfg->hse_crystal.nominal_freq < 1e6
            || cfg->hse_crystal.nominal_freq > 50e6) return 0;
    }
    if (cfg->pin_count <= 0 || cfg->vdd_pin_count <= 0 || cfg->vss_pin_count <= 0)
        return 0;
    if (cfg->core_max_freq_hz <= 0 || cfg->core_max_freq_hz > 1e9) return 0;
    if (cfg->max_temp_c < 70.0 || cfg->max_temp_c > 150.0) return 0;
    return 1;
}

/* STM32G070RB minimal system config.
 * L7: Cortex-M0+, 64MHz, LQFP64, 128KB Flash, 36KB SRAM
 * Cost-optimized value line, popular in IoT and consumer */
void stm32g070rb_config(BoardConfig *cfg) {
    if (!cfg) return;
    memset(cfg, 0, sizeof(*cfg));
    cfg->series = STM32_SERIES_G0;
    cfg->package = PACKAGE_LQFP64;
    cfg->pin_count = 64;
    cfg->vdd_pin_count = 2;
    cfg->vss_pin_count = 2;
    cfg->vdda_pin_count = 1;
    cfg->core_max_freq_hz = 64e6;
    cfg->flash_size_kb = 128;
    cfg->sram_size_kb = 36;
    cfg->max_temp_c = 85.0;
    cfg->power_specs[0].domain = VOLTAGE_DOMAIN_VDD;
    cfg->power_specs[0].nominal_voltage = 3.3;
    cfg->power_specs[0].min_voltage = 1.7;
    cfg->power_specs[0].max_voltage = 3.6;
    cfg->power_specs[0].max_current = 0.10;
    cfg->power_specs[0].ripple_tolerance = 0.050;
    cfg->power_specs[0].requires_filtering = 0;
    cfg->power_specs[1].domain = VOLTAGE_DOMAIN_VDDA;
    cfg->power_specs[1].nominal_voltage = 3.3;
    cfg->power_specs[1].min_voltage = 1.7;
    cfg->power_specs[1].max_voltage = 3.6;
    cfg->power_specs[1].max_current = 0.005;
    cfg->power_specs[1].ripple_tolerance = 0.010;
    cfg->power_specs[1].requires_filtering = 1;
    cfg->hse_crystal.nominal_freq = 8e6;
    cfg->hse_crystal.freq_tolerance_ppm = 30;
    cfg->hse_crystal.load_capacitance = 18e-12;
    cfg->hse_crystal.shunt_capacitance = 7e-12;
    cfg->hse_crystal.esr_max = 80.0;
    cfg->hse_crystal.drive_level_max = 500e-6;
    cfg->lse_crystal.nominal_freq = 32768;
    cfg->lse_crystal.freq_tolerance_ppm = 20;
    cfg->lse_crystal.load_capacitance = 12.5e-12;
    cfg->lse_crystal.shunt_capacitance = 1.1e-12;
    cfg->lse_crystal.esr_max = 35000.0;
    cfg->lse_crystal.drive_level_max = 1.0e-6;
    cfg->pll.enabled = 1;
    cfg->pll.input_source = CLOCK_SOURCE_HSE;
    cfg->pll.input_divider = 1;
    cfg->pll.multiplier = 8;
    cfg->pll.sys_divider = 1;
    cfg->pll.usb_divider = 1;
    cfg->pll.vco_freq = 64e6;
    cfg->pll.output_freq = 64e6;
    cfg->reset.nrst_pullup_resistance = 40000;
    cfg->reset.nrst_capacitance = 100e-9;
    cfg->reset.reset_time_required = 300e-9;
    cfg->reset.has_external_supervisor = 0;
    cfg->reset.bor_threshold = 2.0;
}

/* STM32L452RE ultra-low-power config.
 * L7: Cortex-M4, 80MHz, LQFP64, 512KB Flash, 160KB SRAM
 * ULP series with multiple power modes down to 30nA in standby */
void stm32l452re_config(BoardConfig *cfg) {
    if (!cfg) return;
    memset(cfg, 0, sizeof(*cfg));
    cfg->series = STM32_SERIES_L4;
    cfg->package = PACKAGE_LQFP64;
    cfg->pin_count = 64;
    cfg->vdd_pin_count = 3;
    cfg->vss_pin_count = 3;
    cfg->vdda_pin_count = 1;
    cfg->core_max_freq_hz = 80e6;
    cfg->flash_size_kb = 512;
    cfg->sram_size_kb = 160;
    cfg->max_temp_c = 85.0;
    cfg->power_specs[0].domain = VOLTAGE_DOMAIN_VDD;
    cfg->power_specs[0].nominal_voltage = 3.3;
    cfg->power_specs[0].min_voltage = 1.71;
    cfg->power_specs[0].max_voltage = 3.6;
    cfg->power_specs[0].max_current = 0.08;
    cfg->power_specs[0].ripple_tolerance = 0.050;
    cfg->power_specs[0].requires_filtering = 0;
    cfg->power_specs[1].domain = VOLTAGE_DOMAIN_VDDA;
    cfg->power_specs[1].nominal_voltage = 3.3;
    cfg->power_specs[1].min_voltage = 1.71;
    cfg->power_specs[1].max_voltage = 3.6;
    cfg->power_specs[1].max_current = 0.005;
    cfg->power_specs[1].ripple_tolerance = 0.005;
    cfg->power_specs[1].requires_filtering = 1;
    cfg->hse_crystal.nominal_freq = 8e6;
    cfg->hse_crystal.freq_tolerance_ppm = 20;
    cfg->hse_crystal.load_capacitance = 12e-12;
    cfg->hse_crystal.shunt_capacitance = 5e-12;
    cfg->hse_crystal.esr_max = 60.0;
    cfg->hse_crystal.drive_level_max = 300e-6;
    cfg->lse_crystal.nominal_freq = 32768;
    cfg->lse_crystal.freq_tolerance_ppm = 20;
    cfg->lse_crystal.load_capacitance = 7e-12;
    cfg->lse_crystal.shunt_capacitance = 1.1e-12;
    cfg->lse_crystal.esr_max = 70000.0;
    cfg->lse_crystal.drive_level_max = 0.5e-6;
    cfg->pll.enabled = 1;
    cfg->pll.input_source = CLOCK_SOURCE_HSE;
    cfg->pll.input_divider = 1;
    cfg->pll.multiplier = 10;
    cfg->pll.sys_divider = 1;
    cfg->pll.usb_divider = 1;
    cfg->pll.vco_freq = 80e6;
    cfg->pll.output_freq = 80e6;
    cfg->reset.nrst_pullup_resistance = 40000;
    cfg->reset.nrst_capacitance = 100e-9;
    cfg->reset.reset_time_required = 300e-9;
    cfg->reset.has_external_supervisor = 0;
    cfg->reset.bor_threshold = 1.8;
}

/*
 * get_bom_summary_string — format BOM as a human-readable string.
 * L5: Practical BOM formatting for design documentation.
 */
void get_bom_summary_string(const BoardConfig *cfg, char *buf, int bufsize) {
    BOMEstimate bom;
    compute_board_bom(cfg, &bom);
    snprintf(buf, bufsize,
        "BOM: %dx100nF %dx4.7uF %dx10uF %dxCL1/2 %dxFerrite %dxButton",
        bom.cap_100nf_count, bom.cap_4u7_count, bom.cap_10uf_count,
        bom.cap_cl1_cl2, bom.ferrite_bead, bom.push_button);
}
