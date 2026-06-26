/**
 * @file stm32_minimal_config.h
 * @brief STM32 minimal system board configuration parameters and core definitions.
 *
 * Knowledge Level: L1 (Definitions), L4 (Fundamental Laws)
 *
 * A STM32 minimal system must provide:
 *   - Stable power supply (VDD: 1.8V ~ 3.6V, VDDA for analog)
 *   - Clock source (external crystal or internal oscillator)
 *   - Reset circuit (external RC or supervisor IC)
 *   - Boot mode configuration (BOOT0/BOOT1 pins)
 *   - Debug/programming interface (SWD: SWCLK + SWDIO)
 *   - Decoupling capacitors for each power pin pair
 *
 * Reference: STM32F103x8/xB datasheet DS5319, AN2586 (hardware getting started)
 * Course mapping: Berkeley EE16A/B (circuits), Michigan EECS 411 (embedded)
 */

#ifndef STM32_MINIMAL_CONFIG_H
#define STM32_MINIMAL_CONFIG_H

#include <stdint.h>
#include <stddef.h>

/* =========================================================================
 * L1: Core Definitions — STM32 Minimal System Components
 * ========================================================================= */

/** Voltage domain enumeration for STM32 power architecture */
typedef enum {
    VOLTAGE_DOMAIN_VDD   = 0,
    VOLTAGE_DOMAIN_VDDA  = 1,
    VOLTAGE_DOMAIN_VBAT  = 2,
    VOLTAGE_DOMAIN_VREF  = 3,
    VOLTAGE_DOMAIN_VDD_USB = 4,
    VOLTAGE_DOMAIN_COUNT = 5
} VoltageDomain;

/** Clock source type enumeration */
typedef enum {
    CLOCK_SOURCE_HSI     = 0,
    CLOCK_SOURCE_HSE     = 1,
    CLOCK_SOURCE_LSI     = 2,
    CLOCK_SOURCE_LSE     = 3,
    CLOCK_SOURCE_PLL     = 4,
    CLOCK_SOURCE_COUNT   = 5
} ClockSource;

/** Boot mode configuration */
typedef enum {
    BOOT_MODE_MAIN_FLASH   = 0,
    BOOT_MODE_SYSTEM_MEM   = 1,
    BOOT_MODE_SRAM         = 2,
    BOOT_MODE_UNKNOWN      = 3
} BootMode;

/** Reset source type */
typedef enum {
    RESET_SOURCE_POR      = 0,
    RESET_SOURCE_BOR      = 1,
    RESET_SOURCE_EXTERNAL = 2,
    RESET_SOURCE_WATCHDOG = 3,
    RESET_SOURCE_SOFTWARE = 4,
    RESET_SOURCE_LOW_POWER= 5,
    RESET_SOURCE_COUNT    = 6
} ResetSource;

/** Capacitor type for decoupling network */
typedef enum {
    CAP_TYPE_CERAMIC     = 0,
    CAP_TYPE_TANTALUM    = 1,
    CAP_TYPE_ELECTROLYTIC= 2,
    CAP_TYPE_FILM        = 3,
    CAP_TYPE_COUNT       = 4
} CapacitorType;

/* =========================================================================
 * L1: Key Parameter Structures
 * ========================================================================= */

/** Power supply specification for a voltage domain */
typedef struct {
    VoltageDomain domain;
    double nominal_voltage;
    double min_voltage;
    double max_voltage;
    double max_current;
    double ripple_tolerance;
    int    requires_filtering;
} PowerSpec;

/** Decoupling capacitor specification */
typedef struct {
    double capacitance;
    double esr;
    double esl;
    double voltage_rating;
    CapacitorType type;
    double self_resonant_freq;
    double target_impedance;
} DecouplingCapSpec;

/** Crystal oscillator specification (Pierce topology) */
typedef struct {
    double nominal_freq;
    double freq_tolerance_ppm;
    double load_capacitance;
    double shunt_capacitance;
    double esr_max;
    double drive_level_max;
    double gm_crit;
} CrystalSpec;

/** PLL configuration */
typedef struct {
    int    enabled;
    ClockSource input_source;
    int    input_divider;
    int    multiplier;
    int    sys_divider;
    int    usb_divider;
    double vco_freq;
    double output_freq;
} PLLConfig;

/** Reset circuit configuration */
typedef struct {
    double nrst_pullup_resistance;
    double nrst_capacitance;
    double reset_time_required;
    int    has_external_supervisor;
    double bor_threshold;
} ResetConfig;

/** PCB trace specification for layout rules */
typedef struct {
    double trace_width_mm;
    double copper_weight_oz;
    double trace_thickness_mm;
    double max_current;
    double resistance_per_mm;
    double temp_rise;
} PCBTraceSpec;

/* =========================================================================
 * L1: Chip Series & Package Type
 * ========================================================================= */

typedef enum {
    STM32_SERIES_F0 = 0,
    STM32_SERIES_F1,
    STM32_SERIES_F2,
    STM32_SERIES_F3,
    STM32_SERIES_F4,
    STM32_SERIES_F7,
    STM32_SERIES_H7,
    STM32_SERIES_G0,
    STM32_SERIES_G4,
    STM32_SERIES_L0,
    STM32_SERIES_L1,
    STM32_SERIES_L4,
    STM32_SERIES_L5,
    STM32_SERIES_U5,
    STM32_SERIES_WB,
    STM32_SERIES_WL,
    STM32_SERIES_COUNT
} STM32Series;

typedef enum {
    PACKAGE_LQFP48  = 48,
    PACKAGE_LQFP64  = 64,
    PACKAGE_LQFP100 = 100,
    PACKAGE_LQFP144 = 144,
    PACKAGE_LQFP176 = 176,
    PACKAGE_LQFP208 = 208,
    PACKAGE_BGA100  = 200,
    PACKAGE_BGA144  = 144,
    PACKAGE_BGA176  = 276,
    PACKAGE_UFQFPN32= 32,
    PACKAGE_UFQFPN48= 48,
    PACKAGE_WLCSP   = 0
} PackageType;

/** Complete STM32 minimal system board configuration */
typedef struct {
    STM32Series series;
    PackageType package;
    int         pin_count;
    int         vdd_pin_count;
    int         vss_pin_count;
    int         vdda_pin_count;
    double      core_max_freq_hz;
    int         flash_size_kb;
    int         sram_size_kb;
    double      max_temp_c;
    PowerSpec   power_specs[5];
    CrystalSpec hse_crystal;
    CrystalSpec lse_crystal;
    PLLConfig   pll;
    ResetConfig reset;
} BoardConfig;

/* =========================================================================
 * L4: Fundamental Laws — Applied to Board Design
 * ========================================================================= */

/**
 * Compute required decoupling cap count for target impedance.
 * Z_target = ΔV / ΔI
 */
int compute_decoupling_cap_count(double icc_max, double ripple_max, double freq_max);

/**
 * Voltage drop across PCB trace (Ohm's Law).
 * V_drop = I * R, where R = rho * L / (W * t)
 */
double trace_voltage_drop(double trace_width_mm, double trace_length_mm,
                          double copper_weight_oz, double current_a,
                          double ambient_temp_c);

/**
 * Self-resonant frequency of decoupling capacitor.
 * f0 = 1 / (2 * pi * sqrt(L * C))
 */
double capacitor_self_resonant_freq(double capacitance, double esl);

/* Board config functions declared in src/board_config.c */
void stm32f103c8_bluepill_config(BoardConfig *cfg);
void stm32f407vet6_black_config(BoardConfig *cfg);
void stm32h743vit6_config(BoardConfig *cfg);
void stm32g070rb_config(BoardConfig *cfg);
void stm32l452re_config(BoardConfig *cfg);

typedef struct {
    int cap_100nf_count, cap_10uf_count, cap_4u7_count, cap_cl1_cl2;
    int resistor_pulldown, ferrite_bead, crystal_hse, crystal_lse;
    int push_button, ldo;
} BOMEstimate;

void compute_board_bom(const BoardConfig *cfg, BOMEstimate *bom);
double compute_pcb_area_estimate(const BoardConfig *cfg);
int validate_board_config(const BoardConfig *cfg);

#endif /* STM32_MINIMAL_CONFIG_H */
