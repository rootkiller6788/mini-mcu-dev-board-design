/**
 * @file power_budget.h
 * @brief Power budget analysis, battery life estimation, and energy accounting
 *
 * Knowledge Coverage: L1 Definitions, L2 Core Concepts, L4 Fundamental Laws, L5 Algorithms
 *
 * A power budget is the fundamental tool for coin cell design.
 * It answers: "How long will this device run on a CR2032?"
 *
 * References:
 * - AN4747: Optimizing power with STM32L4, STMicroelectronics, 2017
 * - Nordic Power Profiler Kit documentation, 2020
 * - EEMBC ULPMark benchmark methodology, 2018
 *
 * Course Alignment:
 * - MIT 6.450 -> Link budget analogy (power budget = energy link budget)
 * - Berkeley EE105 -> Energy accounting in electronic systems
 * - TU Munich -> Systems engineering approach to power
 */

#ifndef POWER_BUDGET_H
#define POWER_BUDGET_H

#include <stddef.h>
#include <stdint.h>

/* Include required type definitions */
#include "coin_cell_battery.h"
#include "low_power_mcu.h"

/* ============================================================================
 * L1 Definitions - Power, energy, capacity units and conversions
 * ============================================================================ */

/**
 * @brief Power consumption component categorization
 *
 * Every microwatt counts in coin cell design.
 * Categorizing consumers helps identify optimization targets.
 */
typedef enum {
    CONSUMER_MCU_CORE     = 0,  /* CPU core dynamic power */
    CONSUMER_MCU_PERIPH   = 1,  /* On-chip peripherals (ADC, timers, etc.) */
    CONSUMER_RADIO_TX     = 2,  /* Radio transmitter (BLE, LoRa, etc.) */
    CONSUMER_RADIO_RX     = 3,  /* Radio receiver */
    CONSUMER_SENSOR       = 4,  /* External sensors */
    CONSUMER_DISPLAY      = 5,  /* Display (e-ink, OLED, LCD) */
    CONSUMER_STORAGE      = 6,  /* Non-volatile storage (flash write) */
    CONSUMER_REGULATOR    = 7,  /* Voltage regulator quiescent current */
    CONSUMER_LED          = 8,  /* Indicator LEDs */
    CONSUMER_LEAKAGE      = 9,  /* Unavoidable leakage/standby */
    CONSUMER_OTHER        = 10,
    CONSUMER_COUNT        = 11
} PowerConsumer;

/**
 * @brief Single power consumer entry in a budget
 */
typedef struct {
    PowerConsumer type;
    const char   *name;
    double        I_active_uA;      /* Current when active */
    double        I_standby_uA;     /* Current in standby (0 if fully gated) */
    double        duty_cycle_pct;   /* Percentage of time active [0,100] */
    double        duration_per_event_ms; /* Duration per activation */
    double        events_per_hour;  /* Activation frequency */
} PowerConsumerEntry;

/**
 * @brief Complete power budget for a design
 */
typedef struct {
    PowerConsumerEntry *entries;
    size_t              num_entries;
    double              V_supply_V;        /* Supply voltage */
    double              I_total_avg_uA;    /* Computed average current */
    double              P_total_avg_uW;    /* Computed average power */
    double              design_margin_pct; /* Safety margin (% above calculated) */
} PowerBudget;

/* ============================================================================
 * L2 Core Concepts - Battery capacity utilization, derating factors
 * ============================================================================ */

/**
 * @brief Battery capacity derating factors
 *
 * Real-world battery capacity is always less than nominal due to:
 * - Temperature effects (cold reduces capacity)
 * - Discharge rate (Peukert effect)
 * - Cutoff voltage (can't use full capacity if Vmin > Vcutoff)
 * - Aging (capacity fades over time for rechargeables)
 * - Self-discharge during shelf life
 */
typedef struct {
    double temperature_derate;    /* Factor [0,1] - temperature effect */
    double rate_derate;           /* Factor [0,1] - Peukert/rate effect */
    double cutoff_derate;         /* Factor [0,1] - usable voltage window */
    double aging_derate;          /* Factor [0,1] - cycle aging */
    double self_discharge_derate; /* Factor [0,1] - self-discharge losses */
    double overall_derate;        /* Product of all above = usable fraction */
} CapacityDerating;

/**
 * @brief Battery life estimation result
 */
typedef struct {
    double estimated_life_hours;
    double estimated_life_days;
    double estimated_life_months;
    double estimated_life_years;
    double usable_capacity_mAh;
    double avg_current_uA;
    double peak_current_mA;
    double min_voltage_V;
    int    is_voltage_limited;  /* 1 if voltage sag limits life before capacity */
    int    is_pulse_limited;    /* 1 if pulse current exceeds battery capability */
} BatteryLifeEstimate;

/* ============================================================================
 * L3 Math Structures - Energy integration, optimization models
 * ============================================================================ */

/**
 * @brief Time-series power measurement point
 */
typedef struct {
    double timestamp_s;
    double current_uA;
    double voltage_V;
} PowerSample;

/**
 * @brief Aggregate energy statistics over a measurement window
 */
typedef struct {
    double total_energy_uJ;
    double avg_power_uW;
    double min_current_uA;
    double max_current_uA;
    double rms_current_uA;
    double peak_to_avg_ratio;
    size_t num_samples;
    double duration_s;
} EnergyStatistics;

/**
 * @brief Pareto-optimal power-performance point
 *
 * For multi-objective optimization: minimize power while
 * meeting performance constraints.
 */
typedef struct {
    double avg_power_uW;
    double performance_metric;  /* Application-specific (e.g., updates/day) */
    double battery_life_days;
    int    feasible;            /* 1 if meets all constraints */
} ParetoPoint;

/* ============================================================================
 * L4 Laws - Conservation of energy, power summation
 * ============================================================================ */

/**
 * @brief Kirchhoff-style current summation for power budget
 *
 * Total average current = sum over all consumers of:
 *   I_avg_i = I_active_i * duty_i/100 + I_standby_i * (1 - duty_i/100)
 *
 * This is the fundamental power budget equation.
 * All consumers sum linearly (KCL applied to average currents).
 */
typedef struct {
    double I_from_kcl_uA;       /* Sum of component currents */
    double I_measured_uA;       /* If available, actual measurement */
    double discrepancy_pct;     /* (measured - calculated) / measured * 100 */
    int    measurement_valid;   /* 1 if I_measured is from actual hardware */
} PowerBudgetVerification;

/* ============================================================================
 * L5 Algorithms - Budget optimization, what-if analysis
 * ============================================================================ */

/**
 * @brief Power optimization scenario for what-if analysis
 */
typedef struct {
    const char *description;
    double      I_savings_uA;       /* Expected current reduction */
    double      implementation_cost; /* Engineering effort / BOM cost estimate */
    double      battery_life_gain_days;
    int         priority;            /* 1=highest, 5=lowest */
} OptimizationScenario;

/* ============================================================================
 * API Functions
 * ============================================================================ */

/* --- Power Budget Construction and Analysis --- */

/**
 * @brief Initialize an empty power budget
 * @param budget Budget structure to initialize
 * @param V_supply Supply voltage (V)
 * @param design_margin_pct Safety margin percentage (typ 20%)
 */
void power_budget_init(PowerBudget *budget, double V_supply,
                       double design_margin_pct);

/**
 * @brief Add a consumer to the power budget
 * @param budget Power budget
 * @param type Consumer category
 * @param name Human-readable name
 * @param I_active_uA Current when active (uA)
 * @param I_standby_uA Current in standby (uA)
 * @param duty_cycle_pct Duty cycle percentage [0-100]
 * @param duration_per_event_ms Duration per activation (ms)
 * @param events_per_hour Activation frequency (per hour)
 */
void power_budget_add_consumer(PowerBudget *budget, PowerConsumer type,
                               const char *name, double I_active_uA,
                               double I_standby_uA, double duty_cycle_pct,
                               double duration_per_event_ms,
                               double events_per_hour);

/**
 * @brief Compute total average current for the budget
 * @param budget Power budget (reads entries, writes I_total_avg_uA and P_total_avg_uW)
 *
 * I_avg = sum over all entries of:
 *   I_active * duty/100 + I_standby * (1 - duty/100)
 *
 * P_avg = I_avg * V_supply
 *
 * Includes design margin: I_total = I_avg * (1 + margin/100)
 */
void power_budget_compute(PowerBudget *budget);

/**
 * @brief Print power budget summary to stdout
 * @param budget Computed power budget
 *
 * Prints a formatted table of all consumers and their contributions.
 */
void power_budget_print(const PowerBudget *budget);

/**
 * @brief Find the top N power consumers by contribution
 * @param budget Computed power budget
 * @param top_n Number of top consumers to find
 * @param indices Output array of indices into budget->entries (caller allocates)
 *
 * Sorts consumers by I_avg contribution and returns indices of largest.
 */
void power_budget_top_consumers(const PowerBudget *budget, int top_n,
                                int *indices);

/* --- Battery Life Estimation --- */

/**
 * @brief Estimate battery life using a complete power budget
 * @param budget Computed power budget (must call power_budget_compute first)
 * @param battery Battery parameters
 * @param derating Capacity derating factors
 * @param estimate Output: battery life estimate
 *
 * Computes estimated life considering:
 * 1. Usable capacity = C_nominal * overall_derate
 * 2. Life = usable_capacity / I_avg
 * 3. Voltage sag check: I_peak causes V_drop < V_cutoff?
 * 4. Pulse capability check: can battery deliver peak load?
 */
void battery_life_estimate_from_budget(const PowerBudget *budget,
                                        const CoinCellParams *battery,
                                        const CapacityDerating *derating,
                                        BatteryLifeEstimate *estimate);

/**
 * @brief Compute capacity derating factors
 * @param battery Battery parameters
 * @param temperature_C Operating temperature (C)
 * @param I_peak_mA Peak current draw (mA)
 * @param V_min_required Minimum required voltage (V)
 * @param shelf_months Months in storage before deployment
 * @param cycle_count Number of discharge cycles (1 for primary cells)
 * @param derating Output: all derating factors computed
 *
 * Computes the five derating factors and their product.
 * Each factor independently reduces usable capacity.
 */
void compute_capacity_derating(const CoinCellParams *battery,
                                double temperature_C, double I_peak_mA,
                                double V_min_required, double shelf_months,
                                uint32_t cycle_count,
                                CapacityDerating *derating);

/**
 * @brief Simple battery life calculation (no budget needed)
 * @param C_usable_mAh Usable battery capacity (mAh)
 * @param I_avg_uA Average current draw (uA)
 * @return Estimated life in hours
 *
 * t = C_usable / I_avg (in consistent units)
 * This is the most basic battery life equation.
 */
double simple_battery_life_hours(double C_usable_mAh, double I_avg_uA);

/**
 * @brief Estimate battery life using duty cycle profile
 * @param dc Duty cycle definition
 * @param profile MCU power profile
 * @param battery Battery parameters
 * @param derating Capacity derating
 * @return Estimated life in hours
 *
 * Combines duty_cycle_avg_current with battery capacity.
 */
double battery_life_from_duty_cycle(const DutyCycle *dc,
                                     const McuPowerProfile *profile,
                                     const CoinCellParams *battery,
                                     const CapacityDerating *derating);

/* --- Optimization --- */

/**
 * @brief Perform what-if optimization analysis
 * @param budget Base power budget
 * @param scenarios Array of optimization scenarios to evaluate
 * @param num_scenarios Number of scenarios
 * @param battery Battery parameters
 * @param derating Capacity derating factors
 *
 * For each scenario, computes the resulting battery life gain
 * and sorts by priority (best gain/effort ratio first).
 */
void power_budget_optimize(const PowerBudget *budget,
                           OptimizationScenario *scenarios,
                           int num_scenarios,
                           const CoinCellParams *battery,
                           const CapacityDerating *derating);

/**
 * @brief Find break-even duty cycle for a consumer
 * @param entry Consumer entry
 * @param target_I_uA Target average current contribution (uA)
 * @return Required duty cycle percentage to meet target
 *
 * Solves: target_I = I_active * duty/100 + I_standby * (1 - duty/100)
 *   => duty = (target_I - I_standby) / (I_active - I_standby) * 100
 */
double power_budget_breakeven_duty(const PowerConsumerEntry *entry,
                                    double target_I_uA);

/* --- Energy Statistics --- */

/**
 * @brief Compute energy statistics from time-series samples
 * @param samples Array of power samples
 * @param N Number of samples
 * @param stats Output statistics
 *
 * Integrates power samples using trapezoidal rule.
 * Computes total energy, average power, min/max/rms current.
 */
void compute_energy_statistics(const PowerSample *samples, size_t N,
                               EnergyStatistics *stats);

/**
 * @brief Estimate energy per operation for a given consumer
 * @param entry Consumer entry
 * @param V_supply Supply voltage
 * @return Energy per event in microjoules (uJ)
 *
 * E_per_event = I_active * V_supply * duration_per_event_ms / 1000
 * Total daily energy = E_per_event * events_per_hour * 24
 */
double energy_per_event(const PowerConsumerEntry *entry, double V_supply);

/**
 * @brief Verify power budget against measurements
 * @param budget Computed budget
 * @param I_measured_uA Actual measured average current
 * @param verification Output verification result
 *
 * Useful for validating that the budget model matches real hardware.
 * Discrepancy > 20% suggests missing consumers or inaccurate estimates.
 */
void power_budget_verify(const PowerBudget *budget, double I_measured_uA,
                         PowerBudgetVerification *verification);

/* --- Pareto Optimization --- */

/**
 * @brief Generate Pareto frontier of power vs performance
 * @param budget Base budget
 * @param battery Battery parameters
 * @param derating Capacity derating
 * @param points Output array (caller allocates, max N)
 * @param max_points Maximum points to generate
 * @return Number of points generated
 *
 * Sweeps across possible duty cycle reductions and computes
 * the resulting power-performance trade-off curve.
 */
int generate_pareto_frontier(const PowerBudget *budget,
                             const CoinCellParams *battery,
                             const CapacityDerating *derating,
                             ParetoPoint *points, int max_points);

#endif /* POWER_BUDGET_H */