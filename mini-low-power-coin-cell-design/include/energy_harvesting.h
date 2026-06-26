/**
 * @file energy_harvesting.h
 * @brief Energy harvesting for coin cell applications - solar, thermal, RF, vibration
 *
 * Knowledge Coverage: L1 Definitions, L2 Concepts, L4 Laws, L5 Algorithms, L7 Applications
 *
 * Energy harvesting extends coin cell life or enables battery-free operation.
 * Even microwatts matter: a small solar cell in indoor light (~10uW) can
 * significantly extend a CR2032-powered sensor node's life.
 *
 * References:
 * - Priya & Inman, "Energy Harvesting Technologies", Springer, 2009
 * - Roundy, "Energy Harvesting for Wireless Sensor Networks", Ph.D., UC Berkeley, 2003
 * - Penella-Lopez & Gasulla-Forner, "Powering Autonomous Sensors", Springer, 2011
 *
 * Course Alignment:
 * - Berkeley EE16B -> Energy harvesting design project
 * - MIT 6.450 -> Energy as information-theoretic resource
 * - TU Munich -> Energy-autonomous systems
 */

#ifndef ENERGY_HARVESTING_H
#define ENERGY_HARVESTING_H

#include <stddef.h>
#include <stdint.h>

/* ============================================================================
 * L1 Definitions - Energy harvesting sources and transducers
 * ============================================================================ */

/**
 * @brief Energy harvesting source types
 */
typedef enum {
    HARVEST_SOLAR_INDOOR   = 0,  /* Indoor photovoltaic (100-1000 lux) */
    HARVEST_SOLAR_OUTDOOR  = 1,  /* Outdoor photovoltaic (1000-100000 lux) */
    HARVEST_THERMAL_TEG    = 2,  /* Thermoelectric generator (Seebeck effect) */
    HARVEST_RF_AMBIENT     = 3,  /* Ambient RF (WiFi, cellular, broadcast) */
    HARVEST_RF_DEDICATED   = 4,  /* Dedicated RF power transfer */
    HARVEST_PIEZO_VIBRATION = 5, /* Piezoelectric vibration harvesting */
    HARVEST_INDUCTIVE_VIB  = 6,  /* Electromagnetic vibration harvesting */
    HARVEST_TRIBOELECTRIC  = 7,  /* Triboelectric nanogenerator */
    HARVEST_SOURCE_COUNT   = 8
} HarvestingSource;

/**
 * @brief Energy harvesting transducer parameters
 *
 * Characterizes the harvester: what power it can deliver
 * under what environmental conditions.
 */
typedef struct {
    HarvestingSource source_type;
    double   P_density_uW_per_cm2;  /* Power density (uW/cm^2) */
    double   V_oc_V;                /* Open-circuit voltage */
    double   I_sc_uA;               /* Short-circuit current */
    double   V_mpp_V;               /* Voltage at maximum power point */
    double   I_mpp_uA;              /* Current at maximum power point */
    double   P_mpp_uW;              /* Maximum power point power */
    double   area_cm2;              /* Active transducer area */
    double   efficiency_pct;        /* Conversion efficiency */
    const char *typical_condition;  /* "Office lighting 500 lux", "dT=5C", etc. */
} HarvesterParams;

/**
 * @brief Energy buffer / storage element
 *
 * Harvested energy is intermittent - buffers smooth the supply.
 * Common options: supercapacitor, thin-film battery, or hybrid.
 */
typedef enum {
    BUFFER_NONE         = 0,  /* No buffer, direct use only */
    BUFFER_CERAMIC_CAP  = 1,  /* Ceramic capacitor (uF range) */
    BUFFER_ELEC_CAP     = 2,  /* Electrolytic capacitor (mF range) */
    BUFFER_SUPERCAP     = 3,  /* Supercapacitor (F range) */
    BUFFER_THIN_FILM_BAT = 4, /* Thin-film rechargeable battery */
    BUFFER_HYBRID       = 5,  /* Supercap + battery hybrid */
    BUFFER_TYPE_COUNT   = 6
} BufferType;

/**
 * @brief Energy buffer parameters
 */
typedef struct {
    BufferType type;
    double     C_F;             /* Capacitance (Farads) for capacitors */
    double     V_rated_V;       /* Rated voltage */
    double     V_min_V;         /* Minimum useful voltage */
    double     ESR_ohm;         /* Equivalent series resistance */
    double     I_leakage_uA;    /* Leakage current */
    double     energy_capacity_J; /* Total energy storage capacity */
    double     usable_energy_J;   /* Energy between V_rated and V_min */
} EnergyBuffer;

/* ============================================================================
 * L2 Core Concepts - MPPT, power management, energy neutrality
 * ============================================================================ */

/**
 * @brief Maximum Power Point Tracking (MPPT) algorithm type
 *
 * Harvesters have a nonlinear I-V curve. MPPT ensures the harvester
 * operates at the point of maximum power transfer.
 */
typedef enum {
    MPPT_NONE            = 0,  /* No MPPT, direct connection */
    MPPT_FRACTIONAL_VOC  = 1,  /* V_mpp = k * V_oc (simple, low power) */
    MPPT_PERTURB_OBSERVE = 2,  /* Perturb & Observe (hill-climbing) */
    MPPT_INCREMENTAL_CONDUCTANCE = 3, /* IncCond (more accurate) */
    MPPT_CONSTANT_VOLTAGE = 4,  /* Fixed V_mpp for known conditions */
    MPPT_COUNT           = 5
} MPPTAlgorithm;

/**
 * @brief MPPT controller state
 */
typedef struct {
    MPPTAlgorithm algorithm;
    double   V_mpp_estimated;   /* Current MPP voltage estimate */
    double   P_harvested_uW;    /* Current harvested power */
    double   step_size_mV;      /* Perturbation step size */
    double   update_interval_ms; /* MPPT update period */
    double   k_voc;             /* Fractional VOC coefficient (typ 0.7-0.8) */
    int      converged;         /* 1 if at MPP */
} MPPTController;

/**
 * @brief Energy neutrality state
 *
 * Energy-neutral operation: E_harvested >= E_consumed over a period.
 * This is the fundamental requirement for perpetual operation.
 */
typedef struct {
    double   E_harvested_J;    /* Energy harvested in current period */
    double   E_consumed_J;     /* Energy consumed in current period */
    double   E_stored_J;       /* Energy currently in buffer */
    double   E_buffer_max_J;   /* Maximum buffer capacity */
    double   period_hours;     /* Neutrality check period */
    int      is_energy_neutral; /* 1 if harvested >= consumed (long-term) */
    double   deficit_J;        /* Energy deficit (negative = surplus) */
} EnergyNeutralityState;

/* ============================================================================
 * L3 Math Structures - Solar irradiance models, thermal models
 * ============================================================================ */

/**
 * @brief Solar irradiance model for indoor/outdoor prediction
 *
 * Indoor light levels:
 *   - Dark room: 50 lux
 *   - Office: 500 lux
 *   - Bright office: 1000 lux
 *   - Overcast day: 5000 lux
 *   - Full sun: 100000 lux
 *
 * Approximate conversion: 1 lux ~ 0.0079 W/m^2 for fluorescent
 *                         1 lux ~ 0.0046 W/m^2 for LED
 *                         1 lux ~ 0.0039 W/m^2 for sunlight
 */
typedef struct {
    double   lux_level;
    double   irradiance_W_per_m2;
    double   cell_efficiency_pct;  /* PV cell efficiency (typ 5-25%) */
    double   P_out_W_per_cm2;     /* Output power per unit area */
} SolarIrradianceModel;

/**
 * @brief Thermoelectric generator (TEG) model
 *
 * Seebeck effect: V = alpha * dT
 * where alpha is the Seebeck coefficient (typ 40-400 uV/K per junction)
 *
 * Power output depends on dT and thermal resistance:
 *   P_max = (alpha^2 * dT^2) / (4 * R_internal)
 */
typedef struct {
    double   alpha_uV_per_K;   /* Seebeck coefficient */
    double   R_internal_ohm;   /* Internal electrical resistance */
    double   R_thermal_K_per_W; /* Thermal resistance */
    double   num_junctions;    /* Number of thermocouple junctions */
    double   dT_K;             /* Temperature difference across TEG */
} TEGModel;

/* ============================================================================
 * L4 Laws - Maximum power transfer, energy conservation
 * ============================================================================ */

/**
 * @brief Maximum power transfer theorem applied to harvesters
 *
 * For MPPT, the load impedance should match the source impedance.
 * For photovoltaic: V_mpp ~ 0.75-0.85 * V_oc
 * For TEG: R_load = R_internal
 */
typedef struct {
    double   R_source_ohm;      /* Harvester source impedance */
    double   R_load_ohm;        /* Load impedance */
    double   power_match_ratio; /* Actual / max possible power ratio */
} PowerTransferMatch;

/* ============================================================================
 * L5 Algorithms - Energy prediction, adaptive duty cycling, buffer sizing
 * ============================================================================ */

/**
 * @brief Energy prediction model
 *
 * Predicts available energy based on historical harvesting data.
 * Uses exponential smoothing for adaptive prediction.
 */
typedef struct {
    double   predicted_energy_J_per_day;
    double   alpha_smoothing;    /* Smoothing factor (0-1), higher = more responsive */
    double   variance_J2;        /* Prediction variance */
    double   confidence_interval_J; /* 95% confidence interval */
    int      num_samples;         /* Number of historical samples */
} EnergyPrediction;

/**
 * @brief Adaptive duty cycle controller
 *
 * Adjusts device duty cycle based on available harvested energy.
 * More energy -> higher duty cycle (more measurements, more reports).
 * Less energy -> lower duty cycle to maintain energy neutrality.
 */
typedef struct {
    double   base_duty_cycle_pct;   /* Nominal duty cycle */
    double   min_duty_cycle_pct;    /* Minimum acceptable duty */
    double   max_duty_cycle_pct;    /* Maximum possible duty */
    double   current_duty_cycle;    /* Currently active duty */
    double   E_buffer_J;            /* Current buffer energy */
    double   E_buffer_target_J;     /* Target buffer energy level */
    double   gain_P;                /* Proportional gain for control */
    double   gain_I;                /* Integral gain for control */
    double   integral_error;        /* Accumulated energy error */
} AdaptiveDutyController;

/* ============================================================================
 * L7 Applications - Real-world harvesting scenarios
 * ============================================================================ */

/**
 * @brief Complete energy harvesting system design
 */
typedef struct {
    HarvestingSource  source;
    HarvesterParams   harvester;
    MPPTController    mppt;
    EnergyBuffer      buffer;
    EnergyPrediction  prediction;
    EnergyNeutralityState neutrality;
    double            system_load_uW;  /* Average system power consumption */
    double            autonomy_hours;   /* Runtime without any harvest */
    int               is_perpetual;     /* 1 if energy-neutral design achieved */
} HarvestingSystem;

/* ============================================================================
 * API Functions
 * ============================================================================ */

/* --- Harvester Characterization --- */

/**
 * @brief Get typical harvester parameters for a given source and condition
 * @param source Harvesting source type
 * @param condition Description of environmental condition
 * @param area_cm2 Active harvester area
 * @return HarvesterParams with estimated performance
 */
HarvesterParams harvester_estimate(HarvestingSource source,
                                    const char *condition, double area_cm2);

/**
 * @brief Compute solar cell power output
 * @param model Solar irradiance model
 * @param area_cm2 Cell area
 * @return Power output (uW)
 *
 * P_out = irradiance * area * efficiency
 * Indoor (500 lux, 10cm^2, 10% eff): 500 * 0.0079 * 10e-4 * 0.10 = 3.95 uW
 * Outdoor (50000 lux, 10cm^2, 20% eff): 100000 * 0.0039 * 10e-4 * 0.20 = 7800 uW
 */
double solar_power_output(const SolarIrradianceModel *model, double area_cm2);

/**
 * @brief Compute TEG power output
 * @param teg TEG model parameters
 * @return Maximum power output (uW)
 *
 * P_max = (alpha * dT)^2 / (4 * R_internal)
 * Example: dT=5K, alpha=200uV/K, 100 junctions, R=10ohm
 *   V_oc = 200e-6 * 5 * 100 = 0.1V
 *   P_max = 0.1^2 / (4*10) = 250 uW
 */
double teg_power_output(const TEGModel *teg);

/**
 * @brief Compute rectified RF power (Friis equation for energy harvesting)
 * @param P_tx_dBm Transmitter power (dBm)
 * @param G_tx_dBi Transmitter antenna gain (dBi)
 * @param G_rx_dBi Receiver antenna gain (dBi)
 * @param frequency_MHz Operating frequency (MHz)
 * @param distance_m Distance from transmitter (m)
 * @param rectifier_efficiency RF-DC conversion efficiency (typ 0.3-0.7)
 * @return Received DC power (uW)
 *
 * Uses Friis transmission equation:
 *   P_rx = P_tx * G_tx * G_rx * (lambda/(4*pi*d))^2 * eta_rect
 */
double rf_harvested_power(double P_tx_dBm, double G_tx_dBi, double G_rx_dBi,
                           double frequency_MHz, double distance_m,
                           double rectifier_efficiency);

/* --- MPPT Control --- */

/**
 * @brief Initialize MPPT controller
 * @param mppt Controller structure
 * @param algorithm MPPT algorithm type
 */
void mppt_init(MPPTController *mppt, MPPTAlgorithm algorithm);

/**
 * @brief Run one iteration of MPPT algorithm
 * @param mppt Controller state
 * @param V_measured Measured harvester voltage
 * @param I_measured Measured harvester current
 * @return New target voltage for harvester
 *
 * Perturb & Observe:
 *   1. Measure P_now = V * I
 *   2. If P_now > P_prev, keep direction; else reverse
 *   3. Adjust V_target by step_size
 */
double mppt_step(MPPTController *mppt, double V_measured, double I_measured);

/**
 * @brief Get current harvested power
 * @param mppt Controller state
 * @return Current power estimate (uW)
 */
double mppt_get_power(const MPPTController *mppt);

/* --- Energy Buffer Management --- */

/**
 * @brief Initialize energy buffer parameters
 * @param buffer Buffer structure
 * @param type Buffer type
 * @param C_F Capacitance (F), ignored for batteries
 * @param V_rated Rated voltage
 * @param V_min Minimum useful voltage
 *
 * For capacitors: E = 0.5 * C * (V_rated^2 - V_min^2)
 * For batteries: E = C_mAh * V_nominal * 3.6 (convert mAh to Joules)
 */
void energy_buffer_init(EnergyBuffer *buffer, BufferType type, double C_F,
                        double V_rated, double V_min);

/**
 * @brief Compute energy stored in buffer at given voltage
 * @param buffer Buffer parameters
 * @param V_current Current buffer voltage
 * @return Stored energy (Joules)
 *
 * E = 0.5 * C * V^2 (capacitor)
 * For batteries, uses linear approximation: E = capacity * SoC * V_nominal
 */
double energy_buffer_stored(const EnergyBuffer *buffer, double V_current);

/**
 * @brief Compute time to charge buffer from empty given harvest power
 * @param buffer Buffer parameters
 * @param P_harvest_uW Available harvest power
 * @return Time to full charge (seconds)
 *
 * t_charge = E_usable / P_harvest (ignoring leakage)
 */
double energy_buffer_charge_time(const EnergyBuffer *buffer, double P_harvest_uW);

/**
 * @brief Size buffer for required autonomy
 * @param P_load_uW Average load power
 * @param autonomy_hours Required autonomy without harvesting
 * @param V_rated Rated buffer voltage
 * @param V_min Minimum buffer voltage
 * @return Required capacitance (Farads) or battery capacity (mAh)
 *
 * For capacitor: C = 2 * P * t / (V_rated^2 - V_min^2)
 * For battery:  C_mAh = P * t / (V_nominal * 3.6)
 */
double size_buffer_for_autonomy(double P_load_uW, double autonomy_hours,
                                 double V_rated, double V_min);

/* --- Energy Neutrality --- */

/**
 * @brief Initialize energy neutrality tracking
 * @param state Neutrality state
 * @param buffer_max_J Maximum buffer energy capacity
 * @param period_hours Neutrality check period
 */
void energy_neutrality_init(EnergyNeutralityState *state,
                            double buffer_max_J, double period_hours);

/**
 * @brief Update energy neutrality tracking
 * @param state Neutrality state
 * @param E_harvested_this_step Energy harvested in this step (J)
 * @param E_consumed_this_step Energy consumed in this step (J)
 * @param dt_hours Time step duration (hours)
 */
void energy_neutrality_update(EnergyNeutralityState *state,
                               double E_harvested_this_step,
                               double E_consumed_this_step,
                               double dt_hours);

/**
 * @brief Check if system is energy-neutral over the tracking period
 * @param state Neutrality state
 * @return 1 if energy-neutral (harvested >= consumed), 0 otherwise
 */
int energy_neutrality_check(const EnergyNeutralityState *state);

/* --- Energy Prediction --- */

/**
 * @brief Initialize energy predictor
 * @param pred Prediction state
 * @param alpha Smoothing factor (0-1)
 */
void energy_prediction_init(EnergyPrediction *pred, double alpha);

/**
 * @brief Update energy prediction with new observation
 * @param pred Prediction state
 * @param energy_today_J Energy harvested today (J)
 *
 * Uses exponential smoothing:
 *   pred_{t+1} = alpha * obs_t + (1-alpha) * pred_t
 */
void energy_prediction_update(EnergyPrediction *pred, double energy_today_J);

/**
 * @brief Get predicted energy for next period
 * @param pred Prediction state
 * @return Predicted energy (J) for next period
 */
double energy_prediction_get(const EnergyPrediction *pred);

/* --- Adaptive Duty Cycling --- */

/**
 * @brief Initialize adaptive duty cycle controller
 * @param adc Controller state
 * @param base_duty Base duty cycle (%)
 * @param min_duty Minimum duty cycle (%)
 * @param max_duty Maximum duty cycle (%)
 */
void adaptive_duty_init(AdaptiveDutyController *adc,
                        double base_duty, double min_duty, double max_duty);

/**
 * @brief Update adaptive duty cycle based on buffer energy
 * @param adc Controller state
 * @param E_buffer_current Current buffer energy (J)
 * @param E_buffer_target Target buffer energy (J)
 * @param dt_hours Time step (hours)
 * @return New duty cycle recommendation (%)
 *
 * PI controller: adjusts duty cycle to maintain buffer at target level.
 * Above target -> increase duty (more sampling)
 * Below target -> decrease duty (conserve energy)
 */
double adaptive_duty_update(AdaptiveDutyController *adc,
                             double E_buffer_current, double E_buffer_target,
                             double dt_hours);

/* --- System-Level Design --- */

/**
 * @brief Design a complete energy harvesting system
 * @param sys System structure to populate
 * @param source Harvesting source type
 * @param condition Environmental condition description
 * @param area_cm2 Harvester area
 * @param P_load_uW Average system load power
 * @param autonomy_hours Required autonomy without harvesting
 *
 * Selects appropriate buffer size, configures MPPT, and
 * initializes energy neutrality tracking.
 */
void harvesting_system_design(HarvestingSystem *sys,
                               HarvestingSource source,
                               const char *condition,
                               double area_cm2,
                               double P_load_uW,
                               double autonomy_hours);

/**
 * @brief Simulate one day of harvesting system operation
 * @param sys System state
 * @param hourly_harvest_uW Array of 24 hourly harvest power values
 * @param hourly_load_uW Array of 24 hourly load power values
 *
 * Simulates energy buffer charge/discharge over 24 hours with
 * hourly-resolution harvest and load profiles.
 * Updates energy neutrality state.
 */
void harvesting_system_simulate_day(HarvestingSystem *sys,
                                     const double *hourly_harvest_uW,
                                     const double *hourly_load_uW);

/**
 * @brief Check if perpetual operation is achievable
 * @param sys Harvesting system
 * @return 1 if system is energy-autonomous, 0 if battery-dependent
 */
int harvesting_system_is_perpetual(const HarvestingSystem *sys);

/**
 * @brief Compare harvester options for a given application
 * @param options Array of HarvesterParams to compare
 * @param num_options Number of options
 * @param P_load_uW System load power
 * @param scores Output array of scores (higher = better, caller allocates)
 *
 * Scores based on: power margin, size, cost proxy, reliability.
 */
void compare_harvesters(const HarvesterParams *options, int num_options,
                        double P_load_uW, double *scores);

#endif /* ENERGY_HARVESTING_H */