/**
 * @file power_system.h
 * @brief STM32 power supply system design.
 *
 * Knowledge Level: L1 (Definitions), L2 (Core Concepts), L4 (Fundamental Laws)
 * Reference: ST AN4488 / AN2586
 * Course mapping: Berkeley EE105, MIT 6.003
 */

#ifndef POWER_SYSTEM_H
#define POWER_SYSTEM_H

#include "stm32_minimal_config.h"

/** Internal voltage regulator modes */
typedef enum {
    REGULATOR_RUN       = 0,
    REGULATOR_LP        = 1,
    REGULATOR_POWERDOWN = 2,
    REGULATOR_BYPASS    = 3
} RegulatorMode;

/** Power supply topologies for the board */
typedef enum {
    POWER_TOPO_LDO_ONLY       = 0,
    POWER_TOPO_DCDC_BUCK      = 1,
    POWER_TOPO_BOOST          = 2,
    POWER_TOPO_BATTERY_DIRECT = 3,
    POWER_TOPO_DUAL_RAIL      = 4
} PowerTopology;

/** LDO (Low Drop-Out) regulator specification */
typedef struct {
    double input_voltage;        /**< Input voltage (V) */
    double output_voltage;       /**< Regulated output (V) */
    double dropout_voltage;      /**< Dropout voltage (V) */
    double max_current;          /**< Maximum output current (A) */
    double quiescent_current;    /**< Quiescent current (A) */
    double psrr_db;             /**< PSRR at 100Hz (dB) */
    double output_noise_uv_rms;  /**< RMS output noise (uV) */
    int    requires_heatsink;
} LDOSpec;

/** Bulk capacitor bank specification */
typedef struct {
    double total_capacitance;    /**< Total bulk capacitance (F) */
    double esr_parallel;         /**< Equivalent parallel ESR (ohm) */
    double max_ripple_current;   /**< Total ripple current capacity (A) */
    int    num_caps;
    DecouplingCapSpec caps[16];
} BulkCapBank;

/** Power rail analysis result */
typedef struct {
    double nominal_voltage;      /**< Target voltage (V) */
    double actual_voltage;       /**< Estimated actual (V) */
    double voltage_drop;         /**< IR drop (V) */
    double ripple_mv;            /**< Ripple amplitude (mVpp) */
    double noise_mv;             /**< Total noise (mV RMS) */
    double efficiency;           /**< Power efficiency (0..1) */
    int    passes_spec;
    double margin_percent;
} PowerRailAnalysis;

/* =========================================================================
 * L2: Core Concepts
 * ========================================================================= */

/**
 * Verify power supply meets STM32 requirements.
 * STM32 VDD range: typically 2.0V ~ 3.6V (varies by series).
 */
int validate_power_spec(const PowerSpec *spec, STM32Series series);

/**
 * Calculate power dissipation of the MCU core.
 * P_core = VDD * I_DD(run), I_DD = I_static + C*V*f
 */
double estimate_mcu_power(double vdd, double core_freq_hz, int peripheral_on);

/**
 * Size bulk capacitance for holdup during transient load steps.
 * L4: C_bulk = I_step * delta_t / delta_V
 */
double size_bulk_capacitance(double step_current_a, double regulator_resp_us,
                             double max_droop_mv);

/**
 * Compute PSRR attenuation at frequency for VDD->VDDA filter.
 * L4: Attenuation_dB = PSRR + 20*log10(Z_cap / (Z_cap + R_ferrite))
 */
double compute_psrr_attenuation(double psrr_db, double filter_cap_f,
                                double filter_ferrite_ohm, double target_freq_hz);

/**
 * Check if LDO has sufficient headroom.
 * vin_min > vout + dropout
 */
int ldo_headroom_check(const LDOSpec *ldo, double vin_min);

/**
 * Calculate LDO power dissipation and required PCB copper area.
 * P_ldo = (VIN - VOUT)*I_load + VIN*I_q
 */
double ldo_required_copper_area(const LDOSpec *ldo, double load_current,
                                double ambient_temp, double max_junction);

/**
 * Select input bypass capacitance for LDO.
 * Switching pre-reg: C_in = I_load / (f_sw * delta_V_ripple)
 * Linear pre-regulation: C_in >= 1 uF
 */
double ldo_input_capacitance(const LDOSpec *ldo, int pre_reg_switching,
                             double pre_reg_freq_hz, double load_current_a);

/**
 * Compute RC delay for NRST supervisor trigger.
 * L4: tau = R*C, V(t) = VDD*(1 - e^(-t/tau))
 */
double nrst_rc_delay(double pullup_ohm, double cap_farad, double vdd, double vth);

#endif /* POWER_SYSTEM_H */

double estimate_vdd_rail_voltage(double v_supply, double i_load,
                                  double r_trace, double r_connector);
double compute_power_efficiency(double v_in, double i_in,
                                 double v_out, double i_out);
double compute_inrush_current(double bulk_capacitance,
                               double voltage_rise_time_s, double v_nominal);
double compute_ripple_voltage(double load_current, double switching_freq,
                               double output_cap, double esr);
double compute_vbat_lifetime(double battery_capacity_mah,
                              double load_current_ua,
                              double self_discharge_percent_per_year);
double compute_decoupling_impedance_vs_frequency(double capacitance,
                                                   double esr, double esl,
                                                   double freq_hz);
