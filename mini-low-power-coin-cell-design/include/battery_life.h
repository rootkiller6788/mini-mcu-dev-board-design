/**
 * @file battery_life.h
 * @brief Advanced battery life estimation, aging models, and reliability analysis
 *
 * Knowledge Coverage: L1 Definitions, L2 Core Concepts, L4 Laws, L5 Algorithms, L6 Problems
 *
 * Beyond simple C/I calculations: this module handles variable load profiles,
 * temperature cycling effects, calendar aging, and statistical lifetime prediction.
 *
 * References:
 * - IEEE 1625-2008: Standard for Rechargeable Batteries for Portable Computing
 * - Weicker, "A Systems Approach to Lithium-Ion Battery Management", Artech, 2014
 * - Plett, "Battery Management Systems Vol I-II", Artech, 2015
 *
 * Course Alignment:
 * - Stanford EE359 -> Battery models in wireless sensor networks
 * - Michigan EECS 411 -> Automotive battery reliability
 * - Georgia Tech ECE 6601 -> Statistical communication channel (battery as source)
 */

#ifndef BATTERY_LIFE_H
#define BATTERY_LIFE_H

#include <stdint.h>
#include <stddef.h>

#include "coin_cell_battery.h"

/* ============================================================================
 * L1 Definitions - Aging modes, failure mechanisms
 * ============================================================================ */

/**
 * @brief Battery aging/failure mechanisms
 */
typedef enum {
    AGING_CALENDAR      = 0,  /* Time-dependent degradation (SEI growth) */
    AGING_CYCLE         = 1,  /* Cycle-dependent degradation */
    AGING_TEMPERATURE   = 2,  /* High-temperature accelerated aging */
    AGING_OVERDISCHARGE = 3,  /* Damage from deep discharge */
    AGING_OVERVOLTAGE   = 4,  /* Damage from overcharge */
    AGING_MECHANICAL    = 5,  /* Physical damage, tab welding failure */
    AGING_ELECTROLYTE   = 6,  /* Electrolyte dry-out (high temp, seal leak) */
    AGING_LITHIUM_PLATING = 7, /* Li plating at low temp / high rate charge */
    AGING_COUNT         = 8
} AgingMechanism;

/**
 * @brief Calendar aging model parameters
 *
 * Calendar aging is primarily driven by SEI (Solid Electrolyte Interphase)
 * growth, which consumes active lithium and increases internal resistance.
 *
 * Capacity loss follows sqrt(t) kinetics for diffusion-limited SEI growth:
 *   C_loss(t) = k_cal * sqrt(t)
 */
typedef struct {
    double   k_cal_sqrt_hr;    /* Calendar aging rate (capacity loss per sqrt(hour)) */
    double   Ea_eV;            /* Activation energy for SEI growth */
    double   T_ref_K;          /* Reference temperature */
} CalendarAgingModel;

/**
 * @brief Cycle aging model parameters
 *
 * Cycle aging is primarily from mechanical stress on electrode particles.
 * Each cycle causes micro-cracking and loss of electrical contact.
 *
 * C_loss(N) = C_0 * (1 - exp(-k_cycle * N))
 * where N is cycle count (relevant for LIR2032 rechargeable)
 */
typedef struct {
    double   k_cycle;          /* Cycle aging rate constant */
    double   irreversible_pct; /* Irreversible capacity loss per cycle (%) */
} CycleAgingModel;

/* ============================================================================
 * L2 Core Concepts - Load profile analysis, statistical lifetime
 * ============================================================================ */

/**
 * @brief Variable load profile segment
 *
 * Represents a period with constant current draw.
 * A complete profile is a sequence of these segments.
 */
typedef struct {
    double   duration_hours;
    double   I_load_uA;
    double   temperature_C;
    const char *description;
} LoadSegment;

/**
 * @brief Complete variable load profile
 *
 * Instead of a single I_avg, models the actual usage pattern:
 * - Daytime: active sensing and reporting (higher current)
 * - Nighttime: deep sleep (lower current)
 * - Periodic high-current events (BLE advertising bursts)
 */
typedef struct {
    LoadSegment *segments;
    size_t       num_segments;
    double       total_period_hours;  /* Sum of all segment durations */
    const char  *profile_name;
} LoadProfile;

/**
 * @brief Monte Carlo simulation parameters for battery life
 *
 * Battery life has inherent variability due to:
 * - Manufacturing variation in capacity (+/- 10%)
 * - Temperature variation over deployment
 * - Load current variation
 *
 * Monte Carlo simulation accounts for these distributions.
 */
typedef struct {
    double   C_nominal_mAh;
    double   C_stddev_mAh;       /* Capacity variation (sigma) */
    double   I_mean_uA;
    double   I_stddev_uA;        /* Current variation */
    double   T_mean_C;
    double   T_stddev_C;         /* Temperature variation */
    int      num_simulations;    /* Number of Monte Carlo runs */
} MonteCarloParams;

/**
 * @brief Statistical battery life result
 */
typedef struct {
    double   mean_life_hours;
    double   median_life_hours;
    double   stddev_life_hours;
    double   min_life_hours;     /* Worst case (e.g., P1) */
    double   max_life_hours;     /* Best case (e.g., P99) */
    double   percentile_10;      /* P10 life */
    double   percentile_90;      /* P90 life */
    int      num_simulations;
} StatisticalLife;

/* ============================================================================
 * L3 Math Structures - Probability distributions for reliability
 * ============================================================================ */

/**
 * @brief Weibull distribution parameters for failure analysis
 *
 * Weibull is the standard model for lifetime/reliability:
 *   F(t) = 1 - exp(-(t/eta)^beta)
 *   beta < 1: infant mortality
 *   beta = 1: random failures (exponential)
 *   beta > 1: wear-out failures
 */
typedef struct {
    double   eta;    /* Scale parameter (characteristic life) */
    double   beta;   /* Shape parameter */
} WeibullParams;

/**
 * @brief Reliability metrics at a given time
 */
typedef struct {
    double   reliability;       /* R(t) = probability of survival to time t */
    double   failure_rate;      /* h(t) = instantaneous failure rate */
    double   MTTF_hours;        /* Mean Time To Failure */
    double   B10_life_hours;    /* Time at which 10% have failed */
} ReliabilityMetrics;

/* ============================================================================
 * L4 Laws - Arrhenius for aging acceleration, Weibull reliability
 * ============================================================================ */

/**
 * @brief Temperature acceleration factor (Arrhenius)
 *
 * AF = exp((Ea/kB) * (1/T_use - 1/T_stress))
 *
 * Used to relate accelerated life test results to field conditions.
 * E.g., testing at 60C to predict behavior at 25C.
 */
typedef struct {
    double   Ea_eV;
    double   T_stress_K;
    double   T_use_K;
    double   acceleration_factor;
} ArrheniusAcceleration;

/* ============================================================================
 * L5 Algorithms - Life prediction, reliability analysis
 * ============================================================================ */

/**
 * @brief Rainflow cycle counting for irregular load profiles
 *
 * Rainflow algorithm extracts equivalent full cycles from
 * irregular time series (e.g., temperature cycles).
 * Standard method for fatigue analysis (ASTM E1049).
 */
typedef struct {
    double  *cycle_depths;      /* Array of extracted cycle depths */
    size_t   num_cycles;
    size_t   capacity;          /* Allocated array capacity */
} RainflowResult;

/**
 * @brief State-of-Health (SoH) estimation parameters
 *
 * SoH = current_capacity / initial_capacity * 100%
 * Combines multiple indicators for robust estimation.
 */
typedef struct {
    double   SoH_capacity_pct;  /* Based on capacity fade */
    double   SoH_resistance_pct; /* Based on resistance increase */
    double   SoH_combined_pct;  /* Weighted combination */
    double   confidence;        /* Estimation confidence [0,1] */
} StateOfHealth;

/* ============================================================================
 * API Functions
 * ============================================================================ */

/* --- Load Profile Analysis --- */

/**
 * @brief Compute average current from a variable load profile
 * @param profile Load profile
 * @return Average current (uA) over full period
 *
 * I_avg = sum(I_i * T_i) / sum(T_i)
 */
double load_profile_avg_current(const LoadProfile *profile);

/**
 * @brief Compute battery life for a variable load profile
 * @param profile Load profile
 * @param battery Battery parameters
 * @param initial_SoC Starting state of charge (typ 1.0)
 * @param V_min Minimum allowable voltage (regulator dropout)
 * @return Estimated life in hours
 *
 * Simulates the battery through each load segment, tracking:
 * - Cumulative capacity extraction
 * - Voltage sag under each load
 * - Temperature-dependent self-discharge
 */
double load_profile_battery_life(const LoadProfile *profile,
                                  const CoinCellParams *battery,
                                  double initial_SoC, double V_min);

/**
 * @brief Find the load segment that dominates battery drain
 * @param profile Load profile
 * @return Index of segment with highest capacity consumption rate
 */
int load_profile_dominant_segment(const LoadProfile *profile);

/* --- Aging Models --- */

/**
 * @brief Compute calendar aging capacity loss
 * @param model Calendar aging model
 * @param time_hours Elapsed time
 * @param temperature_C Storage/operating temperature
 * @return Fraction of initial capacity lost [0, 1]
 *
 * C_loss = k_cal * sqrt(t) * exp((Ea/kB)*(1/T_ref - 1/T))
 */
double calendar_aging_loss(const CalendarAgingModel *model,
                           double time_hours, double temperature_C);

/**
 * @brief Compute cycle aging capacity loss
 * @param model Cycle aging model
 * @param num_cycles Number of full equivalent cycles
 * @return Fraction of initial capacity lost [0, 1]
 */
double cycle_aging_loss(const CycleAgingModel *model, uint32_t num_cycles);

/**
 * @brief Compute total aging loss (calendar + cycle, superposition)
 * @param cal_model Calendar aging model
 * @param cycle_model Cycle aging model
 * @param time_hours Operating time
 * @param num_cycles Number of cycles
 * @param temperature_C Temperature
 * @return Total fraction of capacity lost
 */
double total_aging_loss(const CalendarAgingModel *cal_model,
                        const CycleAgingModel *cycle_model,
                        double time_hours, uint32_t num_cycles,
                        double temperature_C);

/* --- Statistical Battery Life --- */

/**
 * @brief Monte Carlo battery life simulation
 * @param params Simulation parameters (distributions)
 * @param result Output statistical life result
 *
 * Runs N simulations with random samples from specified distributions.
 * Each simulation computes battery life = C_sample / I_sample.
 * Results are sorted to compute percentiles.
 */
void monte_carlo_battery_life(const MonteCarloParams *params,
                               StatisticalLife *result);

/**
 * @brief Simple statistical life from known distributions
 * @param C_mean_mAh Mean capacity
 * @param C_std_mAh Capacity standard deviation
 * @param I_mean_uA Mean current
 * @param I_std_uA Current standard deviation
 * @param N_sims Number of simulations
 * @param result Output statistical life
 *
 * Simplified version that uses analytical formulas where possible.
 */
void statistical_battery_life(double C_mean_mAh, double C_std_mAh,
                               double I_mean_uA, double I_std_uA,
                               int N_sims, StatisticalLife *result);

/* --- Weibull Reliability --- */

/**
 * @brief Compute Weibull reliability at time t
 * @param params Weibull parameters
 * @param time_hours Time at which to evaluate
 * @param metrics Output reliability metrics
 *
 * R(t) = exp(-(t/eta)^beta)
 * h(t) = (beta/eta) * (t/eta)^(beta-1)
 * MTTF = eta * Gamma(1 + 1/beta)
 */
void weibull_reliability(const WeibullParams *params, double time_hours,
                          ReliabilityMetrics *metrics);

/**
 * @brief Fit Weibull parameters from lifetime data
 * @param lifetimes Array of observed lifetimes (hours)
 * @param N Number of observations
 * @param params Output fitted Weibull parameters
 *
 * Uses maximum likelihood estimation (MLE) for Weibull distribution.
 * Solves using iterative method (Newton-Raphson for beta).
 */
void weibull_fit(const double *lifetimes, size_t N, WeibullParams *params);

/**
 * @brief Compute B10 life (time at which 10% failure probability)
 * @param params Weibull parameters
 * @return B10 life in hours
 *
 * B10 = eta * (-ln(0.9))^(1/beta)
 */
double weibull_b10_life(const WeibullParams *params);

/* --- Arrhenius Acceleration --- */

/**
 * @brief Compute Arrhenius acceleration factor
 * @param Ea_eV Activation energy (eV)
 * @param T_stress_K Stress temperature (K)
 * @param T_use_K Use temperature (K)
 * @return Acceleration factor (AF > 1 means stress accelerates aging)
 *
 * AF = exp((Ea/kB) * (1/T_use - 1/T_stress))
 * kB = 8.617333262145e-5 eV/K
 */
double arrhenius_acceleration_factor(double Ea_eV, double T_stress_K,
                                      double T_use_K);

/**
 * @brief Project lifetime from accelerated test to field conditions
 * @param life_at_stress_hours Observed life at stress temperature
 * @param Ea_eV Activation energy
 * @param T_stress_K Stress temperature
 * @param T_use_K Field use temperature
 * @return Projected life at use temperature (hours)
 *
 * t_use = t_stress * AF
 */
double arrhenius_project_lifetime(double life_at_stress_hours, double Ea_eV,
                                   double T_stress_K, double T_use_K);

/* --- Rainflow Counting --- */

/**
 * @brief Initialize rainflow cycle counter
 * @param result Rainflow result structure to initialize
 * @param max_cycles Maximum number of cycles to store
 */
void rainflow_init(RainflowResult *result, size_t max_cycles);

/**
 * @brief Process a data point in rainflow counting
 * @param result Rainflow state
 * @param value Current data point value
 *
 * Implements the standard 4-point rainflow algorithm (ASTM E1049).
 * Extracts closed hysteresis loops as full cycles.
 */
void rainflow_process_point(RainflowResult *result, double value);

/**
 * @brief Finalize rainflow counting (process remaining half-cycles)
 * @param result Rainflow state
 *
 * After all points are processed, remaining open reversals
 * are counted as half-cycles.
 */
void rainflow_finalize(RainflowResult *result);

/**
 * @brief Get total damage from rainflow results
 * @param result Rainflow results
 * @param S_N_curve Array of [stress_amplitude, cycles_to_failure] pairs
 * @param num_sn_points Number of S-N curve points
 * @return Cumulative damage (Miner's rule: sum(n_i/N_i))
 *
 * Miner's linear damage accumulation: D = sum(n_i / N_i)
 * D >= 1.0 indicates predicted failure.
 */
double rainflow_damage(const RainflowResult *result,
                       const double *S_N_curve, size_t num_sn_points);

/* --- State of Health --- */

/**
 * @brief Estimate State of Health from capacity and resistance measurements
 * @param C_current_mAh Current measured capacity
 * @param C_initial_mAh Initial (fresh) capacity
 * @param R_current_ohm Current internal resistance
 * @param R_initial_ohm Initial internal resistance
 * @param soh Output StateOfHealth estimate
 */
void estimate_state_of_health(double C_current_mAh, double C_initial_mAh,
                               double R_current_ohm, double R_initial_ohm,
                               StateOfHealth *soh);

/**
 * @brief Predict remaining useful life from SoH trend
 * @param soh_history Array of SoH measurements over time
 * @param times_hours Array of measurement times
 * @param N Number of measurements
 * @param soh_threshold_pct SoH threshold for end-of-life (typ 70-80%)
 * @return Predicted remaining hours until threshold is reached
 *
 * Fits a linear or exponential trend to SoH history
 * and extrapolates to the end-of-life threshold.
 */
double predict_remaining_life(const StateOfHealth *soh_history,
                               const double *times_hours, size_t N,
                               double soh_threshold_pct);

/* --- Practical Life Estimation --- */

/**
 * @brief Comprehensive battery life estimation with all factors
 * @param profile Load profile (can be NULL for constant current)
 * @param I_constant_uA Constant current if profile is NULL
 * @param battery Battery parameters
 * @param temperature_C Operating temperature
 * @param cal_model Calendar aging model (can be NULL)
 * @param cycle_model Cycle aging model (can be NULL)
 * @param V_min Minimum operating voltage
 * @param lifetime_hours Output: estimated lifetime
 * @return 0 on success, -1 on parameter error
 *
 * This is the "kitchen sink" function that accounts for:
 * - Variable load profile
 * - Temperature effects on capacity and self-discharge
 * - Calendar and cycle aging
 * - Voltage cutoff limitations
 *
 * It simulates the battery in discrete time steps, tracking
 * cumulative capacity loss and voltage evolution.
 */
int comprehensive_battery_life(const LoadProfile *profile,
                                double I_constant_uA,
                                const CoinCellParams *battery,
                                double temperature_C,
                                const CalendarAgingModel *cal_model,
                                const CycleAgingModel *cycle_model,
                                double V_min,
                                double *lifetime_hours);

#endif /* BATTERY_LIFE_H */