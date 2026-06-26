/**
 * @file voltage_regulation.h
 * @brief Voltage regulation for coin cell applications - LDO, boost, buck-boost converters
 *
 * Knowledge Coverage: L1 Definitions, L2 Core Concepts, L4 Laws, L5 Algorithms
 *
 * Coin cell voltage ranges from ~3.2V (fresh CR2032) down to ~2.0V (depleted).
 * MCUs typically need 1.8V, 3.0V, or 3.3V. Regulation bridges this gap.
 *
 * References:
 * - Rincon-Mora, "Analog IC Design with Low-Dropout Regulators", McGraw-Hill, 2009
 * - Erickson & Maksimovic, "Fundamentals of Power Electronics", 2nd Ed, 2001
 * - Pressman, "Switching Power Supply Design", 3rd Ed, McGraw-Hill, 2009
 *
 * Course Alignment:
 * - Berkeley EE105 -> Linear regulators, feedback, op-amp error amplifiers
 * - MIT 6.630 -> Inductor and capacitor energy storage
 * - ETH 227-0455 -> Power conversion efficiency
 */

#ifndef VOLTAGE_REGULATION_H
#define VOLTAGE_REGULATION_H

#include <stdint.h>

/* ============================================================================
 * L1 Definitions - Regulator types, topologies, specifications
 * ============================================================================ */

/**
 * @brief Voltage regulator topology types for coin cell applications
 */
typedef enum {
    REG_LDO        = 0,  /* Low-Dropout linear regulator */
    REG_BOOST      = 1,  /* Step-up (boost) switching converter */
    REG_BUCK       = 2,  /* Step-down (buck) switching converter */
    REG_BUCK_BOOST = 3,  /* Buck-boost (step-up/down) converter */
    REG_CHARGE_PUMP = 4, /* Switched-capacitor voltage converter */
    REG_BYPASS     = 5,  /* Direct battery connection (no regulation) */
    REG_TYPE_COUNT = 6
} RegulatorType;

/**
 * @brief LDO (Low-Dropout) Regulator parameters
 *
 * LDO is the simplest regulator: Vout = Vref * (1 + R1/R2)
 * Efficiency = Vout / Vin (for Iout >> Iq)
 * Key trade-off: simplicity vs efficiency loss at high Vin/Vout ratio
 */
typedef struct {
    double   V_out_nominal_V;    /* Regulated output voltage */
    double   V_dropout_max_mV;   /* Maximum dropout voltage at I_max */
    double   I_q_typ_uA;         /* Quiescent current (always consumed) */
    double   I_q_max_uA;         /* Maximum quiescent current over temp */
    double   I_out_max_mA;       /* Maximum output current */
    double   PSRR_dB;            /* Power Supply Rejection Ratio at 100Hz */
    double   line_regulation_mV;  /* Output change per volt input change */
    double   load_regulation_pct; /* Output change from 0 to full load */
    double   efficiency_at_typical; /* Typical efficiency = Vout/Vin */
    int      has_enable_pin;     /* 1 if EN pin allows shutdown */
    double   I_shutdown_nA;      /* Current in shutdown mode */
} LDOParams;

/**
 * @brief Switching converter parameters (boost/buck/buck-boost)
 *
 * Switching converters store energy in an inductor and release it
 * at a different voltage. Efficiency can be >90% but they add
 * switching noise and require external LC components.
 */
typedef struct {
    double   V_in_min_V;          /* Minimum input voltage */
    double   V_in_max_V;          /* Maximum input voltage */
    double   V_out_V;             /* Output voltage */
    double   I_out_max_mA;        /* Maximum output current */
    double   efficiency_peak_pct; /* Peak efficiency */
    double   efficiency_at_10uA;  /* Efficiency at 10uA load (light load) */
    double   efficiency_at_1mA;   /* Efficiency at 1mA load */
    double   I_q_uA;              /* Quiescent current (always consumed) */
    double   f_sw_kHz;            /* Switching frequency */
    double   L_typ_uH;            /* Recommended inductor value */
    double   C_in_typ_uF;         /* Recommended input capacitor */
    double   C_out_typ_uF;        /* Recommended output capacitor */
    double   ripple_mV;           /* Output voltage ripple (peak-to-peak) */
    int      has_pfm_mode;        /* 1 if pulse-frequency modulation at light load */
} SwitchingRegulatorParams;

/**
 * @brief Charge pump (switched-capacitor) parameters
 *
 * Charge pumps use capacitors to multiply/divide voltage.
 * No inductors needed - smaller BOM but lower current capability.
 * Common in coin cell designs: 3V -> 1.8V with ~90% efficiency.
 */
typedef struct {
    double   V_in_min_V;
    double   V_in_max_V;
    double   V_out_V;
    double   I_out_max_mA;
    double   efficiency_pct;
    double   I_q_uA;
    double   f_sw_kHz;
    double   C_fly_typ_uF;     /* Flying capacitor value */
    double   C_out_typ_uF;
    double   ripple_mV;
} ChargePumpParams;

/**
 * @brief Complete power supply design for coin cell application
 */
typedef struct {
    RegulatorType type;
    union {
        LDOParams              ldo;
        SwitchingRegulatorParams sw;
        ChargePumpParams        cp;
    } params;
    double   V_in_min_V;      /* Minimum input from battery */
    double   V_in_max_V;      /* Maximum input from battery */
    double   V_out_V;         /* Regulated output */
    double   I_out_avg_uA;    /* Average load current */
    double   I_out_peak_mA;   /* Peak load current */
} PowerSupplyDesign;

/* ============================================================================
 * L2 Core Concepts - Efficiency, dropout, power loss analysis
 * ============================================================================ */

/**
 * @brief Operating point analysis for a regulator
 */
typedef struct {
    double   V_in_V;             /* Input voltage at operating point */
    double   V_out_V;            /* Output voltage */
    double   I_out_mA;           /* Load current */
    double   efficiency_pct;     /* Efficiency at this point */
    double   power_loss_mW;      /* Power lost as heat */
    double   battery_life_hours; /* Estimated battery life with this regulator */
    int      in_regulation;      /* 1 if input > dropout threshold */
    int      in_current_limit;   /* 1 if load exceeds regulator capability */
} RegulatorOperatingPoint;

/**
 * @brief Efficiency map data point
 */
typedef struct {
    double   I_out_mA;
    double   V_in_V;
    double   efficiency_pct;
} EfficiencyPoint;

/* ============================================================================
 * L3 Math Structures - Efficiency curves, interpolation
 * ============================================================================ */

/**
 * @brief Piecewise linear efficiency model
 *
 * Efficiency = f(I_out, V_in) interpolated from measured data points.
 * Used to accurately estimate regulator losses across operating range.
 */
typedef struct {
    EfficiencyPoint *points;
    size_t           num_points;
} EfficiencyMap;

/* ============================================================================
 * L4 Laws - Conservation of energy, power balance
 * ============================================================================ */

/**
 * @brief Power balance for a regulator
 *
 * Pin = Pout + Ploss
 * Pin = Vin * Iin
 * Pout = Vout * Iout
 * Ploss = conduction_loss + switching_loss + quiescent_loss
 *
 * Efficiency = Pout / Pin = Vout*Iout / (Vout*Iout + Ploss)
 */
typedef struct {
    double   P_in_mW;
    double   P_out_mW;
    double   P_conduction_mW;   /* I^2*R losses in pass element */
    double   P_switching_mW;    /* Gate drive / switching losses */
    double   P_quiescent_mW;    /* Control circuit consumption */
    double   P_total_loss_mW;
    double   efficiency_pct;
} RegulatorPowerBalance;

/* ============================================================================
 * L5 Algorithms - Component selection, startup analysis, brown-out handling
 * ============================================================================ */

/**
 * @brief BOD (Brown-Out Detection) configuration
 *
 * Protects MCU from operating below minimum voltage.
 * Critical for coin cell designs where voltage gradually decays.
 */
typedef struct {
    double   V_threshold_V;      /* Brown-out reset threshold */
    double   V_hysteresis_mV;    /* Hysteresis to prevent oscillation */
    double   response_time_us;   /* Detection to reset delay */
    int      enabled_in_sleep;   /* 1 if active during sleep modes */
    int      generate_interrupt; /* 1 to warn before reset */
} BODConfig;

/**
 * @brief Voltage supervisor/monitor for coin cell
 *
 * Monitors battery voltage and takes action at predefined thresholds.
 * Critical for preventing data corruption during battery depletion.
 */
typedef struct {
    double   V_warning_V;    /* Low battery warning threshold */
    double   V_critical_V;   /* Critical - prepare for shutdown */
    double   V_shutdown_V;   /* Emergency shutdown threshold */
    double   V_hysteresis_mV; /* Prevent threshold chatter */
    int      warning_active;
    int      critical_active;
    int      shutdown_active;
} VoltageSupervisor;

/* ============================================================================
 * API Functions
 * ============================================================================ */

/* --- Regulator Selection --- */

/**
 * @brief Select optimal regulator type for given requirements
 * @param V_battery_min Minimum battery voltage (V)
 * @param V_battery_max Maximum battery voltage (V)
 * @param V_out_required Required output voltage (V)
 * @param I_out_avg_uA Average output current (uA)
 * @param I_out_peak_mA Peak output current (mA)
 * @return Recommended regulator type
 *
 * Decision logic:
 * - V_battery_min > V_out + 0.3V: LDO (simplest)
 * - V_battery always < V_out: Boost
 * - V_battery crosses V_out: Buck-boost
 * - Very low current (<100uA): LDO even if inefficient
 */
RegulatorType select_regulator_type(double V_battery_min, double V_battery_max,
                                     double V_out_required, double I_out_avg_uA,
                                     double I_out_peak_mA);

/**
 * @brief Get default LDO parameters for common coin-cell LDOs
 * @param part_number Part number string (e.g., "TPS783", "MCP1700", "XC6206")
 * @return Pointer to static LDOParams, or NULL if unknown
 */
const LDOParams* ldo_get_params(const char *part_number);

/**
 * @brief Get default switching regulator parameters
 * @param part_number Part number string (e.g., "TPS62740", "TPS61099")
 * @return Pointer to static SwitchingRegulatorParams, or NULL if unknown
 */
const SwitchingRegulatorParams* swreg_get_params(const char *part_number);

/* --- Efficiency Analysis --- */

/**
 * @brief Compute LDO efficiency at an operating point
 * @param ldo LDO parameters
 * @param V_in Input voltage (V)
 * @param I_out Output current (mA)
 * @return Efficiency percentage (0-100)
 *
 * For LDO: efficiency = (Vout * Iout) / (Vin * (Iout + Iq)) * 100
 * At very light loads, Iq dominates and efficiency drops significantly.
 */
double ldo_efficiency(const LDOParams *ldo, double V_in, double I_out);

/**
 * @brief Compute switching regulator efficiency
 * @param sw Switching regulator parameters
 * @param V_in Input voltage (V)
 * @param I_out Output current (mA)
 * @return Efficiency percentage
 *
 * Uses piecewise model: constant loss + proportional loss + quadratic loss
 * Ploss = P_fixed + k1*Iout + k2*Iout^2 (derived from datasheet curves)
 */
double switching_regulator_efficiency(const SwitchingRegulatorParams *sw,
                                       double V_in, double I_out);

/**
 * @brief Analyze regulator at a specific operating point
 * @param design Power supply design
 * @param V_battery Current battery voltage
 * @param I_load Load current (mA)
 * @param op Output operating point analysis
 */
void regulator_analyze_operating_point(const PowerSupplyDesign *design,
                                        double V_battery, double I_load,
                                        RegulatorOperatingPoint *op);

/**
 * @brief Compute regulator power balance
 * @param design Power supply design
 * @param V_in Input voltage
 * @param I_out Output current (mA)
 * @param balance Output power balance breakdown
 */
void regulator_power_balance(const PowerSupplyDesign *design,
                              double V_in, double I_out,
                              RegulatorPowerBalance *balance);

/* --- Battery Voltage Range Analysis --- */

/**
 * @brief Determine if battery can directly power the load (bypass mode)
 * @param V_battery_min Minimum battery voltage over life
 * @param V_battery_max Maximum battery voltage (fresh cell)
 * @param V_load_min Minimum voltage tolerated by load
 * @param V_load_max Maximum voltage tolerated by load
 * @return 1 if direct connection feasible, 0 if regulation needed
 *
 * Direct connection (bypass) is always most efficient.
 * Feasible when battery voltage range is fully within load tolerance.
 */
int can_use_direct_battery(double V_battery_min, double V_battery_max,
                            double V_load_min, double V_load_max);

/**
 * @brief Estimate usable fraction of battery capacity with given regulator
 * @param battery_min_V Minimum battery voltage
 * @param battery_max_V Maximum battery voltage
 * @param regulator_min_input_V Minimum input voltage for regulation
 * @param battery_discharge_curve Array of V vs SoC (N pairs)
 * @param N Number of discharge curve points
 * @return Fraction of capacity usable [0.0, 1.0]
 *
 * Integrates the SoC range where V_battery >= regulator_min_input.
 * Below this voltage, the regulator drops out and output falls.
 */
double usable_capacity_fraction(double battery_min_V, double battery_max_V,
                                 double regulator_min_input_V,
                                 const double *discharge_V, const double *discharge_SoC,
                                 size_t N);

/* --- LDO Component Selection --- */

/**
 * @brief Compute required LDO output capacitor for stability
 * @param ldo LDO parameters
 * @param I_out_max Maximum output current (mA)
 * @param required_transient_mV Maximum allowable transient drop (mV)
 * @return Required capacitance (uF)
 *
 * C_out >= I_step * dt / dV
 * Also checks LDO datasheet requirements (ESR range for stability).
 */
double ldo_output_capacitor(const LDOParams *ldo, double I_out_max,
                             double required_transient_mV);

/**
 * @brief Compute LDO power dissipation and check thermal limits
 * @param ldo LDO parameters
 * @param V_in Input voltage
 * @param I_out Output current (mA)
 * @param T_ambient_C Ambient temperature
 * @param R_theta_JA Thermal resistance junction-ambient (C/W)
 * @return Junction temperature (C). Returns -1 if exceeds Tj_max.
 */
double ldo_junction_temperature(const LDOParams *ldo, double V_in,
                                 double I_out, double T_ambient_C,
                                 double R_theta_JA);

/* --- BOD and Voltage Supervision --- */

/**
 * @brief Configure brown-out detection for coin cell application
 * @param V_system_min Minimum safe operating voltage for the system
 * @param V_hysteresis Hysteresis voltage
 * @param config Output BOD configuration
 *
 * Sets BOD threshold above system minimum with appropriate hysteresis
 * to prevent oscillation near the threshold.
 */
void bod_configure(double V_system_min, double V_hysteresis, BODConfig *config);

/**
 * @brief Initialize voltage supervisor
 * @param sup Supervisor structure to initialize
 * @param V_nominal Nominal battery voltage
 * @param V_min_system Minimum system operating voltage
 */
void voltage_supervisor_init(VoltageSupervisor *sup, double V_nominal,
                              double V_min_system);

/**
 * @brief Check voltage against supervisor thresholds
 * @param sup Voltage supervisor
 * @param V_measured Measured battery voltage
 * @return 0 = normal, 1 = warning, 2 = critical, 3 = shutdown
 *
 * Updates supervisor state flags based on measured voltage.
 * Implements hysteresis to prevent rapid state changes.
 */
int voltage_supervisor_check(VoltageSupervisor *sup, double V_measured);

/**
 * @brief Get recommended action string for current supervisor state
 * @param sup Voltage supervisor
 * @return Static string with recommended action
 */
const char* voltage_supervisor_action(const VoltageSupervisor *sup);

/* --- Energy Loss Analysis --- */

/**
 * @brief Compute total energy lost in regulation over battery life
 * @param design Power supply design
 * @param C_battery_mAh Battery capacity
 * @param I_load_avg_uA Average load current
 * @return Total energy lost (Joules)
 *
 * Integrates regulator losses over the battery discharge curve.
 * Useful for comparing regulation strategies.
 */
double regulator_total_energy_loss(const PowerSupplyDesign *design,
                                    double C_battery_mAh, double I_load_avg_uA);

/**
 * @brief Compare two regulator options for a given application
 * @param opt_a First regulator design
 * @param opt_b Second regulator design
 * @param C_battery_mAh Battery capacity
 * @param I_load_avg_uA Average load current
 * @return Positive if opt_a is better (longer battery life),
 *         negative if opt_b is better, 0 if equal
 *
 * Returns difference in estimated battery life (hours).
 */
double regulator_compare(const PowerSupplyDesign *opt_a,
                          const PowerSupplyDesign *opt_b,
                          double C_battery_mAh, double I_load_avg_uA);

#endif /* VOLTAGE_REGULATION_H */