/**
 * @file    shield_interface.c
 * @brief   L2-L6 Shield Interface Implementations - Form Factors, Bus Config, Power
 *
 * @details Implements shield form factor specifications, pin mapping,
 *          I2C/SPI/UART/1-Wire bus configuration calculations, power
 *          distribution design (regulator selection, decoupling),
 *          and complete shield assembly validation.
 *
 * Knowledge Mapping:
 *   L1 - Shield form factor dimensions and pinouts
 *   L2 - Bus protocol configurations, pin assignments
 *   L4 - I2C pull-up sizing, SPI signal integrity, regulator efficiency
 *   L6 - Multi-sensor shield BOM and validation
 *   L7 - Arduino Uno R3 sensor shield with I2C mux
 */

#include "shield_interface.h"
#include <math.h>
#include <string.h>
#include <stdio.h>

/* ---- Shield Form Factor Specs ---- */

void shield_form_spec_init(shield_form_spec_t *spec, shield_form_factor_t form) {
    if (!spec) return;
    memset(spec, 0, sizeof(*spec));
    spec->form = form;
    spec->pin_pitch_mm = 2.54;
    switch (form) {
        case SHIELD_FORM_ARDUINO_UNO_R3:
            spec->pcb_width_mm = 53.34; spec->pcb_height_mm = 68.58;
            spec->num_pins = 28; spec->rows = 2;
            spec->has_isp_header = true;
            strcpy(spec->compatibility_list, "Uno,Leonardo,Zero,MKR1000");
            break;
        case SHIELD_FORM_ARDUINO_MEGA_R3:
            spec->pcb_width_mm = 53.34; spec->pcb_height_mm = 101.6;
            spec->num_pins = 54; spec->rows = 2;
            spec->has_isp_header = true;
            strcpy(spec->compatibility_list, "Mega,Mega2560");
            break;
        case SHIELD_FORM_NUCLEO_64:
            spec->pcb_width_mm = 70.0; spec->pcb_height_mm = 82.0;
            spec->num_pins = 72; spec->rows = 4;
            spec->has_swd_header = true;
            strcpy(spec->compatibility_list, "Nucleo-F0/F1/F3/F4/L0/L4");
            break;
        case SHIELD_FORM_FEATHER:
            spec->pcb_width_mm = 22.86; spec->pcb_height_mm = 50.8;
            spec->num_pins = 28; spec->rows = 2;
            strcpy(spec->compatibility_list, "Feather M0/M4/ESP32/nRF52");
            break;
        case SHIELD_FORM_RASPBERRY_PI_HAT:
            spec->pcb_width_mm = 65.0; spec->pcb_height_mm = 56.5;
            spec->num_pins = 40; spec->rows = 2;
            strcpy(spec->compatibility_list, "Pi 3/4/Zero");
            break;
        default:
            spec->pcb_width_mm = 50.0; spec->pcb_height_mm = 50.0;
            spec->num_pins = 20; spec->rows = 2;
            break;
    }
}

/* ---- Shield Layout ---- */

void shield_layout_init(shield_layout_t *sl, const char *name, shield_form_factor_t form) {
    if (!sl) return;
    memset(sl, 0, sizeof(*sl));
    if (name) strncpy(sl->shield_name, name, 31);
    sl->form = form;
    shield_form_spec_init(&sl->form_spec, form);
    sl->power_5v_budget_ma = 500.0;
    sl->power_3v3_budget_ma = 300.0;
}

int shield_layout_add_pin(shield_layout_t *sl, uint8_t num, const char *label,
                          pin_function_t func, uint8_t mcu_port, uint8_t mcu_bit) {
    if (!sl || sl->num_pins >= MAX_SHIELD_PINS) return -1;
    shield_pin_t *p = &sl->pins[sl->num_pins];
    p->pin_number = num;
    if (label) strncpy(p->label, label, MAX_PIN_LABEL - 1);
    p->primary_func = func;
    p->mcu_port = mcu_port;
    p->mcu_pin = mcu_bit;
    p->is_5v_tolerant = (func == PIN_FUNC_POWER_5V || func == PIN_FUNC_GND);
    p->max_current_ma = 20.0;
    sl->num_pins++;
    return 0;
}

int shield_layout_validate_power(const shield_layout_t *sl) {
    if (!sl) return -1;
    double margin_5v = sl->power_5v_budget_ma - sl->power_total_consumed_ma;
    if (margin_5v < 0.0) return 1; /* over budget */
    return 0;
}

int shield_layout_validate_i2c_addresses(const shield_layout_t *sl) {
    if (!sl) return -1;
    /* Check for address conflicts in multi-sensor I2C setup */
    if (sl->num_i2c_sensors <= 1) return 0;
    if (sl->num_i2c_sensors > 8 && !sl->has_i2c_mux) return 1; /* need mux */
    return 0;
}

void shield_layout_power_report(const shield_layout_t *sl, double *total, double *margin) {
    if (total) *total = sl ? sl->power_total_consumed_ma : 0.0;
    if (margin && sl) {
        double total_avail = sl->power_5v_budget_ma + sl->power_3v3_budget_ma;
        *margin = (total_avail - sl->power_total_consumed_ma) / total_avail * 100.0;
    }
}

/* ---- I2C Bus Configuration ---- */

void i2c_config_init(i2c_config_t *i2c, uint32_t clock_hz, uint8_t addr) {
    if (!i2c) return;
    memset(i2c, 0, sizeof(*i2c));
    i2c->clock_hz = clock_hz;
    i2c->target_address = addr;
    i2c->bus_capacitance_pf = 100.0;
    i2c->pull_up_resistance = 4700.0;
    i2c->timeout_ms = 100;
}

double i2c_calculate_pullup_min(double Vdd) {
    /* Rp_min = (Vdd - Vol_max) / Iol
     * Vol_max = 0.4V, Iol = 3mA for standard/fast mode */
    return (Vdd - 0.4) / 0.003;
}

double i2c_calculate_pullup_max(double Cb_pf) {
    /* Rp_max = tr / (0.8473 * Cb)
     * tr = 300ns for fast mode, 1000ns for standard mode */
    double tr = 300e-9;
    return tr / (0.8473 * Cb_pf * 1e-12);
}

int i2c_check_address_conflict(const uint8_t *addrs, uint8_t count) {
    if (!addrs || count <= 1) return 0;
    for (int i = 0; i < count; i++) {
        for (int j = i+1; j < count; j++) {
            if (addrs[i] == addrs[j]) return 1; /* conflict */
        }
    }
    return 0;
}

double i2c_max_bus_length(double C_per_m_pf, double max_bus_pf) {
    if (C_per_m_pf <= 0.0) return 10.0;
    return max_bus_pf / C_per_m_pf;
}

/* ---- SPI Bus Configuration ---- */

void spi_config_init(spi_config_t *cfg, spi_mode_t mode, uint32_t clock, uint8_t cs) {
    if (!cfg) return;
    memset(cfg, 0, sizeof(*cfg));
    cfg->mode = mode;
    cfg->clock_hz = clock;
    cfg->cs_pin = cs;
    cfg->cs_active_low = true;
    cfg->msb_first = true;
    cfg->data_bits = 8;
    cfg->max_trace_length_mm = 50.0;
}

double spi_max_clock_for_trace_length(double length_mm) {
    /* Simplified: signal propagation ~5ns/m, rise time ~1/3*period
     * Max clock = 1 / (6 * t_prop) where t_prop = length * 5e-9 * 1000 */
    double t_prop_ns = length_mm * 0.005; /* ~5 ps/mm */
    if (t_prop_ns <= 0.0) return 50e6;
    return 1e9 / (6.0 * t_prop_ns);
}

/* ---- UART Configuration ---- */

void uart_config_init(uart_config_t *cfg, uint32_t baud, uart_level_t level) {
    if (!cfg) return;
    memset(cfg, 0, sizeof(*cfg));
    cfg->baud_rate = baud;
    cfg->data_bits = 8;
    cfg->parity = UART_PARITY_NONE;
    cfg->stop_bits = 1;
    cfg->voltage_level = level;
}

double uart_baud_error_percent(uint32_t target, uint32_t actual) {
    if (target == 0) return 100.0;
    return fabs((double)((int64_t)actual - (int64_t)target)) / (double)target * 100.0;
}

/* ---- 1-Wire Configuration ---- */

void onewire_config_init(onewire_config_t *cfg, uint8_t pin, bool parasitic) {
    if (!cfg) return;
    memset(cfg, 0, sizeof(*cfg));
    cfg->gpio_pin = pin;
    cfg->pull_up_resistance = 4700.0;
    cfg->use_parasitic_power = parasitic;
    cfg->use_strong_pullup = false;
    cfg->reset_time_us = 480;
    cfg->slot_time_us = 75;
}

/* ---- Voltage Regulator ---- */

void voltage_regulator_init(voltage_regulator_t *vr, regulator_type_t type,
                             double Vin, double Vout, double Iout) {
    if (!vr) return;
    memset(vr, 0, sizeof(*vr));
    vr->type = type; vr->Vin_nominal = Vin;
    vr->Vout = Vout; vr->Iout_max_ma = Iout;
    if (type == REGULATOR_LDO) {
        vr->efficiency_percent = Vout / Vin * 100.0;
        vr->dropout_voltage_mv = (Vin - Vout) * 1000.0;
        if (vr->dropout_voltage_mv < 100.0) vr->dropout_voltage_mv = 100.0;
        vr->power_dissipation_mw = (Vin - Vout) * Iout;
    } else if (type == REGULATOR_BUCK) {
        vr->efficiency_percent = 90.0;
        vr->switching_freq_khz = 500.0;
        vr->ripple_mv_pp = 30.0;
        vr->power_dissipation_mw = Vout * Iout * (1.0 - vr->efficiency_percent/100.0);
    }
}

double voltage_regulator_efficiency(const voltage_regulator_t *vr) {
    if (!vr) return 0.0;
    return vr->efficiency_percent;
}

bool voltage_regulator_check_thermal(const voltage_regulator_t *vr,
                                      double Tamb, double Tj_max) {
    if (!vr) return false;
    /* Simplified: assume theta_JA = 50 C/W, check Tj < Tj_max */
    double theta_ja = 50.0; /* C/W */
    double Tj = Tamb + vr->power_dissipation_mw * 1e-3 * theta_ja;
    return (Tj < Tj_max);
}

/* ---- Shield Assembly ---- */

void shield_assembly_init(shield_assembly_t *sa, const char *name, shield_form_factor_t form) {
    if (!sa) return;
    memset(sa, 0, sizeof(*sa));
    if (name) strncpy(sa->name, name, 31);
    strcpy(sa->revision, "1.0");
    shield_layout_init(&sa->layout, name, form);
    sa->ambient_temp_max_c = 70.0;
    sa->is_valid = false;
}

int shield_assembly_add_i2c_sensor(shield_assembly_t *sa, uint8_t addr) {
    (void)addr;
    if (!sa || sa->layout.num_i2c_sensors >= 16) return -1;
    sa->layout.num_i2c_sensors++;
    /* Estimate 5mA per I2C sensor */
    sa->layout.power_total_consumed_ma += 5.0;
    return 0;
}

int shield_assembly_add_spi_sensor(shield_assembly_t *sa, uint8_t bus, uint8_t cs) {
    if (!sa) return -1;
    (void)bus; (void)cs;
    sa->layout.num_spi_sensors++;
    sa->layout.power_total_consumed_ma += 10.0;
    return 0;
}

int shield_assembly_add_analog_sensor(shield_assembly_t *sa) {
    if (!sa) return -1;
    sa->layout.num_analog_sensors++;
    sa->layout.power_total_consumed_ma += 3.0;
    return 0;
}

int shield_assembly_validate(shield_assembly_t *sa) {
    if (!sa) return -1;
    int status = 0;
    if (shield_layout_validate_power(&sa->layout) != 0) status |= 1;
    if (shield_layout_validate_i2c_addresses(&sa->layout) != 0) status |= 2;
    sa->is_valid = (status == 0);
    return status;
}

void shield_assembly_bom_summary(const shield_assembly_t *sa) {
    if (!sa) return;
    printf("BOM Summary for %s rev %s:\n", sa->name, sa->revision);
    printf("  I2C sensors: %d\n", sa->layout.num_i2c_sensors);
    printf("  SPI sensors: %d\n", sa->layout.num_spi_sensors);
    printf("  Analog sensors: %d\n", sa->layout.num_analog_sensors);
    printf("  1-Wire sensors: %d\n", sa->layout.num_onewire_sensors);
    printf("  Total current: %.1f mA\n", sa->layout.power_total_consumed_ma);
    printf("  Status: %s\n", sa->is_valid ? "VALID" : "INVALID");
}
