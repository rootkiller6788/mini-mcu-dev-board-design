/**
 * @file    shield_interface.h
 * @brief   L2 Shield Interface - Form Factors, Bus Protocols, Pin Mapping for Sensor Shields
 *
 * @details Defines standard MCU development board shield form factors (Arduino
 *          R3, Nano, Mega, Nucleo-64, Feather, etc.), digital bus configurations
 *          (I2C, SPI, UART, 1-Wire), pin mapping, and power rail specifications.
 *
 * Knowledge Mapping:
 *   L1 - Shield form factor dimensions, pinout standards, connector types
 *   L2 - Digital bus protocols, addressing, timing, multi-drop topologies
 *   L4 - I2C bus capacitance limits, SPI signal integrity, pull-up sizing
 *   L6 - Designing a multi-sensor shield with shared I2C bus (address conflict)
 *   L7 - Arduino R3 sensor shield with I2C mux for >8 sensors
 *
 * Reference: Arduino R3 specification, ST Nucleo user manual,
 *            NXP I2C-bus specification (UM10204), SPI Block Guide V03.06
 */

#ifndef SHIELD_INTERFACE_H
#define SHIELD_INTERFACE_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "sensor_types.h"

/* ---- Shield Form Factor Definitions (L1) ---- */
typedef enum {
    SHIELD_FORM_ARDUINO_UNO_R3 = 0,
    SHIELD_FORM_ARDUINO_MEGA_R3,
    SHIELD_FORM_ARDUINO_NANO,
    SHIELD_FORM_NUCLEO_64,
    SHIELD_FORM_NUCLEO_144,
    SHIELD_FORM_FEATHER,
    SHIELD_FORM_FEATHERWING,
    SHIELD_FORM_RASPBERRY_PI_HAT,
    SHIELD_FORM_QWIIC,
    SHIELD_FORM_STEMMA_QT,
    SHIELD_FORM_CUSTOM_2_54MM,
    SHIELD_FORM_MIKROBUS
} shield_form_factor_t;

typedef struct {
    shield_form_factor_t form;
    double pcb_width_mm; double pcb_height_mm;
    uint8_t num_pins; uint8_t rows; /* 1=single, 2=dual inline */
    double pin_pitch_mm;            /* typically 2.54mm */
    bool has_isp_header;
    bool has_swd_header;
    bool has_usb_connector;
    char compatibility_list[128];   /* e.g., "Uno,Leonardo,Mega" */
} shield_form_spec_t;

void shield_form_spec_init(shield_form_spec_t *spec, shield_form_factor_t form);

/* ---- Shield Pin Definition (L1-L2) ---- */
#define MAX_SHIELD_PINS 128
#define MAX_PIN_LABEL 16

typedef enum {
    PIN_FUNC_POWER_5V = 0, PIN_FUNC_POWER_3V3, PIN_FUNC_GND, PIN_FUNC_VIN,
    PIN_FUNC_GPIO_DIGITAL, PIN_FUNC_GPIO_ANALOG, PIN_FUNC_PWM,
    PIN_FUNC_I2C_SDA, PIN_FUNC_I2C_SCL, PIN_FUNC_SPI_MOSI, PIN_FUNC_SPI_MISO,
    PIN_FUNC_SPI_SCK, PIN_FUNC_SPI_CS, PIN_FUNC_UART_TX, PIN_FUNC_UART_RX,
    PIN_FUNC_SWD_CLK, PIN_FUNC_SWD_IO, PIN_FUNC_RESET, PIN_FUNC_AREF,
    PIN_FUNC_INTERRUPT, PIN_FUNC_TIMER_CAPTURE, PIN_FUNC_DAC_OUTPUT,
    PIN_FUNC_ONEWIRE, PIN_FUNC_CAN_TX, PIN_FUNC_CAN_RX
} pin_function_t;

typedef struct {
    uint8_t pin_number;              /* physical pin on header */
    char label[MAX_PIN_LABEL];       /* e.g., "D13", "A0", "SCL" */
    pin_function_t primary_func;
    pin_function_t alt_func;         /* alternate function */
    uint8_t mcu_port;                /* MCU GPIO port (e.g., PORTB) */
    uint8_t mcu_pin;                 /* MCU GPIO pin bit */
    uint8_t adc_channel;             /* if analog-capable */
    uint8_t pwm_timer;               /* if PWM-capable */
    bool is_5v_tolerant;             /* true if 3.3V pin accepts 5V */
    bool has_internal_pullup;        /* internal MCU pull-up available */
    double max_current_ma;           /* max source/sink current */
} shield_pin_t;

/* ---- Shield Layout Configuration (L2) ---- */
typedef struct {
    char shield_name[32];
    char revision[8];
    shield_form_factor_t form;
    shield_form_spec_t form_spec;
    uint8_t num_pins;
    shield_pin_t pins[MAX_SHIELD_PINS];
    /* Power budget */
    double power_5v_budget_ma;       /* available from 5V rail */
    double power_3v3_budget_ma;      /* available from 3.3V rail */
    double power_total_consumed_ma;   /* total shield consumption */
    /* Sensor bus topology */
    uint8_t num_i2c_sensors;
    uint8_t num_spi_sensors;
    uint8_t num_uart_sensors;
    uint8_t num_analog_sensors;
    uint8_t num_onewire_sensors;
    bool has_i2c_mux;                /* TCA9548A or similar */
    bool has_spi_demux;              /* for multiple CS lines */
} shield_layout_t;

void shield_layout_init(shield_layout_t *sl, const char *name, shield_form_factor_t form);
int shield_layout_add_pin(shield_layout_t *sl, uint8_t num, const char *label,
                          pin_function_t func, uint8_t mcu_port, uint8_t mcu_bit);
int shield_layout_validate_power(const shield_layout_t *sl);
int shield_layout_validate_i2c_addresses(const shield_layout_t *sl);
void shield_layout_power_report(const shield_layout_t *sl,
                                 double *total_ma, double *margin_percent);

/* ---- I2C Bus Configuration (L2-L4) ----
 * Standard mode: 100 kHz, max bus capacitance 400 pF
 * Fast mode: 400 kHz, max bus capacitance 400 pF
 * Fast mode plus: 1 MHz, max bus capacitance 550 pF
 * Pull-up: Rp_min = (Vdd - 0.4) / 3mA, Rp_max = 300ns / (0.8473 * Cb)
 * Address: 7-bit (0x00-0x7F) or 10-bit (0x78-0x7B reserved) */
typedef struct {
    uint32_t clock_hz;               /* SCL frequency */
    uint8_t own_address;             /* if shield is I2C target */
    uint8_t target_address;          /* sensor address */
    bool is_10bit_addressing;
    double pull_up_resistance;       /* calculated optimal value */
    double bus_capacitance_pf;       /* estimated total bus C */
    double rise_time_ns;             /* actual rise time */
    bool enable_clock_stretching;
    uint16_t timeout_ms;
} i2c_config_t;

void i2c_config_init(i2c_config_t *i2c, uint32_t clock_hz, uint8_t addr);
double i2c_calculate_pullup_min(double Vdd);
double i2c_calculate_pullup_max(double bus_capacitance_pf);
int i2c_check_address_conflict(const uint8_t *addresses, uint8_t count);
double i2c_max_bus_length(double capacitance_per_meter_pf, double max_bus_pf);

/* ---- SPI Bus Configuration (L2-L4) ----
 * Mode 0: CPOL=0, CPHA=0 (data sampled on rising edge)
 * Mode 1: CPOL=0, CPHA=1
 * Mode 2: CPOL=1, CPHA=0
 * Mode 3: CPOL=1, CPHA=1
 * Max speed limited by: signal integrity, trace length, device spec */
typedef enum { SPI_MODE_0 = 0, SPI_MODE_1, SPI_MODE_2, SPI_MODE_3 } spi_mode_t;

typedef struct {
    spi_mode_t mode;
    uint32_t clock_hz;               /* SCK frequency */
    bool msb_first;                   /* data bit order */
    uint8_t data_bits;               /* typically 8, sometimes 16 */
    uint8_t cs_pin;                   /* chip select GPIO */
    bool cs_active_low;               /* true for most devices */
    double max_trace_length_mm;       /* for signal integrity */
    bool use_hardware_nss;            /* hardware vs software CS */
} spi_config_t;

void spi_config_init(spi_config_t *cfg, spi_mode_t mode, uint32_t clock_hz, uint8_t cs);
double spi_max_clock_for_trace_length(double trace_length_mm);

/* ---- UART Configuration (L2) ----
 * Baud rate: standard 9600, 19200, 38400, 57600, 115200, 230400, 460800, 921600
 * Voltage levels: TTL (3.3V/5V), RS-232 (+/-12V), RS-485 differential
 * Error: 8N1 = 8 data, No parity, 1 stop (most common) */
typedef enum { UART_PARITY_NONE = 0, UART_PARITY_EVEN, UART_PARITY_ODD } uart_parity_t;
typedef enum { UART_LEVEL_TTL = 0, UART_LEVEL_RS232, UART_LEVEL_RS485 } uart_level_t;

typedef struct {
    uint32_t baud_rate;
    uint8_t data_bits;
    uart_parity_t parity;
    uint8_t stop_bits;
    uart_level_t voltage_level;
    bool use_flow_control;           /* RTS/CTS */
    double baud_rate_error_percent;  /* should be <2% */
} uart_config_t;

void uart_config_init(uart_config_t *cfg, uint32_t baud, uart_level_t level);
double uart_baud_error_percent(uint32_t target_baud, uint32_t actual_baud);

/* ---- 1-Wire Interface (L2-L4) ----
 * Single data line + GND, parasitic power option
 * Standard speed: 15.4 kbps, Overdrive: 125 kbps
 * Pull-up: 4.7K typical; strong pull-up during temperature conversion
 * Each device has unique 64-bit ROM ID (8-bit family code + 48-bit serial + 8-bit CRC) */
typedef struct {
    uint8_t gpio_pin;                /* MCU pin for 1-Wire bus */
    double pull_up_resistance;       /* typically 4.7K */
    bool use_parasitic_power;        /* true = 2-wire, false = 3-wire */
    bool use_strong_pullup;          /* for high-current operations */
    uint32_t reset_time_us;          /* typically 480 us */
    uint32_t slot_time_us;           /* typically 60-120 us */
} onewire_config_t;

void onewire_config_init(onewire_config_t *cfg, uint8_t pin, bool parasitic);

/* ---- Shield Power Distribution (L2-L4) ----
 * Voltage regulator selection: LDO vs buck converter
 * LDO: simple, low noise, but P_loss = (Vin-Vout)*I_load
 * Buck: efficient (85-95%), but switching noise
 * Decoupling: 100nF ceramic per IC + 10uF bulk per rail
 * PTC fuse for overcurrent protection */
typedef enum { REGULATOR_LDO = 0, REGULATOR_BUCK, REGULATOR_BOOST, REGULATOR_SEPIC } regulator_type_t;

typedef struct {
    regulator_type_t type;
    double Vin_nominal; double Vout;
    double Iout_max_ma; double efficiency_percent;
    double dropout_voltage_mv;       /* LDO only */
    double ripple_mv_pp;             /* output ripple */
    double switching_freq_khz;       /* buck/boost only */
    double power_dissipation_mw;
} voltage_regulator_t;

void voltage_regulator_init(voltage_regulator_t *vr, regulator_type_t type,
                             double Vin, double Vout, double Iout);
double voltage_regulator_efficiency(const voltage_regulator_t *vr);
bool voltage_regulator_check_thermal(const voltage_regulator_t *vr,
                                      double ambient_temp_c, double max_junction_c);

/* ---- Shield Complete Assembly (L6) ---- */
typedef struct {
    char name[32]; char revision[8];
    shield_layout_t layout;
    voltage_regulator_t regulator_5v;
    voltage_regulator_t regulator_3v3;
    i2c_config_t i2c_bus;
    spi_config_t spi_bus[2];         /* up to 2 SPI buses */
    uart_config_t uart_debug;
    onewire_config_t onewire_bus;
    double ambient_temp_max_c;
    double total_current_estimate_ma;
    bool is_valid;
} shield_assembly_t;

void shield_assembly_init(shield_assembly_t *sa, const char *name,
                          shield_form_factor_t form);
int shield_assembly_add_i2c_sensor(shield_assembly_t *sa, uint8_t addr);
int shield_assembly_add_spi_sensor(shield_assembly_t *sa, uint8_t bus, uint8_t cs_pin);
int shield_assembly_add_analog_sensor(shield_assembly_t *sa);
int shield_assembly_validate(shield_assembly_t *sa);
void shield_assembly_bom_summary(const shield_assembly_t *sa);

#endif /* SHIELD_INTERFACE_H */
