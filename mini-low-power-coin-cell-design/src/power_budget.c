/**
 * @file power_budget.c
 * @brief Implementation of power budget analysis and battery life estimation
 *
 * Implements: Power budget construction, consumer contribution analysis,
 * battery life estimation, capacity derating, energy statistics,
 * optimization analysis, and Pareto frontier generation.
 */

#include "power_budget.h"
#include "coin_cell_battery.h"
#include "low_power_mcu.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <math.h>
#include <float.h>

/* ============================================================================
 * Power Budget Construction
 *
 * A power budget is the fundamental planning tool for coin cell design.
 * It answers the question: "Given my component choices and usage pattern,
 * how long will this device run on a single coin cell?"
 *
 * The process:
 *   1. List all current-consuming components
 *   2. For each, estimate active current, standby current, and duty cycle
 *   3. Compute per-component average current
 *   4. Sum all contributions
 *   5. Add design margin for unknowns
 *   6. Divide usable battery capacity by total average current
 * ============================================================================ */

void power_budget_init(PowerBudget *budget, double V_supply,
                       double design_margin_pct)
{
    assert(budget != NULL);
    assert(V_supply > 0.0);
    assert(design_margin_pct >= 0.0);

    memset(budget, 0, sizeof(PowerBudget));
    budget->V_supply_V = V_supply;
    budget->design_margin_pct = design_margin_pct;
}

void power_budget_add_consumer(PowerBudget *budget, PowerConsumer type,
                               const char *name, double I_active_uA,
                               double I_standby_uA, double duty_cycle_pct,
                               double duration_per_event_ms,
                               double events_per_hour)
{
    assert(budget != NULL);
    assert(name != NULL);
    assert(I_active_uA >= 0.0);
    assert(I_standby_uA >= 0.0);
    assert(duty_cycle_pct >= 0.0 && duty_cycle_pct <= 100.0);
    assert(duration_per_event_ms >= 0.0);
    assert(events_per_hour >= 0.0);

    /* Resize entries array */
    size_t new_size = (budget->num_entries + 1) * sizeof(PowerConsumerEntry);
    budget->entries = (PowerConsumerEntry*)realloc(budget->entries, new_size);
    assert(budget->entries != NULL);

    PowerConsumerEntry *entry = &budget->entries[budget->num_entries];
    entry->type = type;
    entry->name = name;
    entry->I_active_uA = I_active_uA;
    entry->I_standby_uA = I_standby_uA;
    entry->duty_cycle_pct = duty_cycle_pct;
    entry->duration_per_event_ms = duration_per_event_ms;
    entry->events_per_hour = events_per_hour;

    budget->num_entries++;
}

void power_budget_compute(PowerBudget *budget)
{
    assert(budget != NULL);

    double I_total = 0.0;

    for (size_t i = 0; i < budget->num_entries; i++) {
        const PowerConsumerEntry *e = &budget->entries[i];
        double duty = e->duty_cycle_pct / 100.0;

        /* I_avg = I_active * duty + I_standby * (1 - duty) */
        double I_avg = e->I_active_uA * duty + e->I_standby_uA * (1.0 - duty);
        I_total += I_avg;
    }

    /* Apply design margin */
    I_total *= (1.0 + budget->design_margin_pct / 100.0);

    budget->I_total_avg_uA = I_total;
    budget->P_total_avg_uW = I_total * budget->V_supply_V;
}

void power_budget_print(const PowerBudget *budget)
{
    assert(budget != NULL);

    printf("\n============================================\n");
    printf("  POWER BUDGET ANALYSIS\n");
    printf("  Supply Voltage: %.2f V\n", budget->V_supply_V);
    printf("  Design Margin:  %.1f%%\n", budget->design_margin_pct);
    printf("============================================\n");
    printf("%-20s %8s %8s %8s %8s\n",
           "Consumer", "I_act(uA)", "I_stby(uA)", "Duty(%)", "I_avg(uA)");
    printf("--------------------------------------------\n");

    for (size_t i = 0; i < budget->num_entries; i++) {
        const PowerConsumerEntry *e = &budget->entries[i];
        double duty = e->duty_cycle_pct / 100.0;
        double I_avg = e->I_active_uA * duty + e->I_standby_uA * (1.0 - duty);

        printf("%-20s %8.1f %8.1f %7.2f%% %8.2f\n",
               e->name, e->I_active_uA, e->I_standby_uA,
               e->duty_cycle_pct, I_avg);
    }

    printf("--------------------------------------------\n");
    printf("  Total Avg Current: %.2f uA\n", budget->I_total_avg_uA);
    printf("  Total Avg Power:   %.2f uW\n", budget->P_total_avg_uW);
    printf("============================================\n\n");
}

/**
 * @brief Compute average current for a consumer entry
 */
static double entry_avg_current(const PowerBudget *budget, int idx)
{
    const PowerConsumerEntry *e = &budget->entries[idx];
    double duty = e->duty_cycle_pct / 100.0;
    return e->I_active_uA * duty + e->I_standby_uA * (1.0 - duty);
}

void power_budget_top_consumers(const PowerBudget *budget, int top_n,
                                int *indices)
{
    assert(budget != NULL);
    assert(indices != NULL);
    assert(top_n > 0);

    size_t N = budget->num_entries;

    /* Create index array */
    int *idx = (int*)malloc(N * sizeof(int));
    assert(idx != NULL);
    for (size_t i = 0; i < N; i++) idx[i] = (int)i;

    /* Insertion sort by I_avg descending (N is typically small, <50) */
    for (size_t i = 1; i < N; i++) {
        int key = idx[i];
        double key_val = entry_avg_current(budget, key);
        size_t j = i;
        while (j > 0 && entry_avg_current(budget, idx[j-1]) < key_val) {
            idx[j] = idx[j-1];
            j--;
        }
        idx[j] = key;
    }

    /* Copy top N */
    int n = (top_n < (int)N) ? top_n : (int)N;
    for (int i = 0; i < n; i++) indices[i] = idx[i];

    free(idx);
}

/* ============================================================================
 * Battery Life Estimation
 *
 * Basic battery life: t = C_usable / I_avg
 *
 * However, real battery life is limited by multiple factors:
 *   - Capacity derating (temperature, rate, cutoff voltage)
 *   - Voltage limitations (regulator dropout)
 *   - Pulse current capability
 *
 * The estimate should be conservative: report the minimum of
 * capacity-limited life and voltage-limited life.
 * ============================================================================ */

double simple_battery_life_hours(double C_usable_mAh, double I_avg_uA)
{
    assert(C_usable_mAh > 0.0);
    assert(I_avg_uA > 0.0);

    /* t(hours) = C(mAh) / I(mA) = C(mAh) * 1000 / I(uA) */
    return C_usable_mAh * 1000.0 / I_avg_uA;
}

void compute_capacity_derating(const CoinCellParams *battery,
                                double temperature_C, double I_peak_mA,
                                double V_min_required, double shelf_months,
                                uint32_t cycle_count,
                                CapacityDerating *derating)
{
    assert(battery != NULL);
    assert(derating != NULL);

    memset(derating, 0, sizeof(CapacityDerating));

    /* 1. Temperature derating
     * Li/MnO2 capacity varies with temperature:
     *   - At 0C: ~90% of 25C capacity
     *   - At -20C: ~60% of 25C capacity
     *   - At 60C: ~105% of 25C capacity (temporarily higher, but faster aging)
     *
     * Model: linear interpolation between known points
     */
    if (temperature_C >= 25.0) {
        derating->temperature_derate = 1.0;  /* No capacity gain above 25C (aging dominates) */
    } else if (temperature_C >= 0.0) {
        /* Linear from 0.90 at 0C to 1.00 at 25C */
        derating->temperature_derate = 0.90 + (temperature_C / 25.0) * 0.10;
    } else if (temperature_C >= -20.0) {
        /* Linear from 0.60 at -20C to 0.90 at 0C */
        derating->temperature_derate = 0.60 + ((temperature_C + 20.0) / 20.0) * 0.30;
    } else {
        derating->temperature_derate = 0.30;  /* Below -20C, very limited */
    }

    /* 2. Rate derating (Peukert effect)
     * For Li/MnO2, the Peukert constant k is very close to 1.0,
     * so rate derating is minor except at very high currents.
     */
    double k = 1.02;  /* Li/MnO2 Peukert constant */
    if (I_peak_mA <= battery->I_std_mA) {
        derating->rate_derate = 1.0;
    } else {
        double C_eff = peukert_capacity(battery->C_nominal_mAh,
                                         battery->I_std_mA, I_peak_mA, k);
        derating->rate_derate = C_eff / battery->C_nominal_mAh;
    }

    /* 3. Cutoff voltage derating
     * If the system requires 2.3V minimum but the battery is rated to 2.0V,
     * not all capacity is usable. Estimate the usable fraction from the
     * OCV-SoC curve shape.
     *
     * For CR2032 (Li/MnO2):
     *   - V > 2.8V for first ~80% of capacity
     *   - V drops from 2.8V to 2.0V over last ~20%
     *   So if V_min = 2.3V, approximately 90% of capacity is usable.
     */
    if (V_min_required <= battery->V_cutoff_V) {
        derating->cutoff_derate = 1.0;
    } else {
        /* Linear approximation: V drops from V_nom to V_cutoff over capacity */
        double V_range = battery->V_nominal_V - battery->V_cutoff_V;
        double V_above_cutoff = battery->V_nominal_V - V_min_required;
        if (V_range <= 0.0) {
            derating->cutoff_derate = 1.0;
        } else {
            derating->cutoff_derate = V_above_cutoff / V_range;
            if (derating->cutoff_derate > 1.0) derating->cutoff_derate = 1.0;
            if (derating->cutoff_derate < 0.0) derating->cutoff_derate = 0.0;
        }
    }

    /* 4. Aging derating (cycle count for rechargeable)
     * Primary (non-rechargeable) cells: no cycle aging
     * Rechargeable (LIR): ~0.05% capacity loss per cycle
     */
    if (battery->chemistry == CHEMISTRY_LITHIUM_ION) {
        double loss_per_cycle = 0.0005;  /* 0.05% per cycle */
        derating->aging_derate = 1.0 - loss_per_cycle * cycle_count;
        if (derating->aging_derate < 0.7) derating->aging_derate = 0.7;  /* Floor */
    } else {
        derating->aging_derate = 1.0;  /* Primary cells, negligible cycle aging */
    }

    /* 5. Self-discharge derating
     * Capacity lost during shelf storage before deployment.
     */
    if (shelf_months > 0.0) {
        double remaining = self_discharge_remaining(
            1.0, battery->self_discharge_pct_per_month, shelf_months);
        derating->self_discharge_derate = remaining;
    } else {
        derating->self_discharge_derate = 1.0;
    }

    /* Overall derating = product of all factors */
    derating->overall_derate = derating->temperature_derate
                             * derating->rate_derate
                             * derating->cutoff_derate
                             * derating->aging_derate
                             * derating->self_discharge_derate;
}

void battery_life_estimate_from_budget(const PowerBudget *budget,
                                        const CoinCellParams *battery,
                                        const CapacityDerating *derating,
                                        BatteryLifeEstimate *estimate)
{
    assert(budget != NULL);
    assert(battery != NULL);
    assert(derating != NULL);
    assert(estimate != NULL);

    memset(estimate, 0, sizeof(BatteryLifeEstimate));

    /* Usable capacity */
    double C_usable = battery->C_nominal_mAh * derating->overall_derate;
    estimate->usable_capacity_mAh = C_usable;
    estimate->avg_current_uA = budget->I_total_avg_uA;

    /* Basic life estimate */
    double life_hours = simple_battery_life_hours(C_usable, budget->I_total_avg_uA);
    estimate->estimated_life_hours = life_hours;
    estimate->estimated_life_days = life_hours / 24.0;
    estimate->estimated_life_months = life_hours / (24.0 * 30.4375); /* Avg month */
    estimate->estimated_life_years = life_hours / (24.0 * 365.25);

    /* Find peak current from budget */
    double I_peak = 0.0;
    for (size_t i = 0; i < budget->num_entries; i++) {
        if (budget->entries[i].I_active_uA > I_peak) {
            I_peak = budget->entries[i].I_active_uA;
        }
    }
    estimate->peak_current_mA = I_peak / 1000.0;

    /* Voltage sag check */
    double V_sag = terminal_voltage_under_load(
        battery->V_nominal_V, estimate->peak_current_mA,
        battery->R_internal_0_ohm);
    estimate->min_voltage_V = V_sag;
    estimate->is_voltage_limited = (V_sag < battery->V_cutoff_V) ? 1 : 0;

    /* Pulse capability check */
    BatteryState state = {
        .SoC = 0.5,
        .V_terminal_V = battery->V_nominal_V,
        .R_internal_ohm = battery->R_internal_0_ohm,
        .temperature_C = 25.0
    };
    int can_pulse = battery_can_deliver_pulse(
        &state, estimate->peak_current_mA, 10.0, battery->V_cutoff_V);
    estimate->is_pulse_limited = can_pulse ? 0 : 1;
}

double battery_life_from_duty_cycle(const DutyCycle *dc,
                                     const McuPowerProfile *profile,
                                     const CoinCellParams *battery,
                                     const CapacityDerating *derating)
{
    assert(dc != NULL);
    assert(profile != NULL);
    assert(battery != NULL);
    assert(derating != NULL);

    double I_avg = duty_cycle_avg_current(dc, profile);
    double C_usable = battery->C_nominal_mAh * derating->overall_derate;

    return simple_battery_life_hours(C_usable, I_avg);
}

/* ============================================================================
 * Power Budget Optimization
 *
 * What-if analysis systematically evaluates potential power savings
 * and ranks them by impact-to-effort ratio.
 *
 * Common optimization scenarios for coin cell designs:
 *   1. Increase sleep duration (reduce duty cycle)
 *   2. Use lower-power sleep mode (STOP -> STANDBY)
 *   3. Reduce radio TX power
 *   4. Batch sensor readings to reduce wakeup frequency
 *   5. Use more efficient voltage regulator
 *   6. Remove indicator LED
 *   7. Lower CPU clock frequency
 * ============================================================================ */

void power_budget_optimize(const PowerBudget *budget,
                           OptimizationScenario *scenarios,
                           int num_scenarios,
                           const CoinCellParams *battery,
                           const CapacityDerating *derating)
{
    assert(budget != NULL);
    assert(scenarios != NULL);
    assert(battery != NULL);
    assert(derating != NULL);

    double C_usable = battery->C_nominal_mAh * derating->overall_derate;
    double base_life_days = C_usable * 1000.0
                            / budget->I_total_avg_uA / 24.0;

    for (int i = 0; i < num_scenarios; i++) {
        OptimizationScenario *s = &scenarios[i];

        /* New average current after savings */
        double I_new = budget->I_total_avg_uA - s->I_savings_uA;
        if (I_new < 0.1) I_new = 0.1;  /* Minimum current */

        /* New battery life */
        double new_life_days = C_usable * 1000.0 / I_new / 24.0;
        s->battery_life_gain_days = new_life_days - base_life_days;

        /* Priority: higher gain with lower effort = better */
        if (s->implementation_cost > 0.0) {
            double roi = s->battery_life_gain_days / s->implementation_cost;
            /* Map ROI to priority: higher ROI = lower priority number */
            if (roi > 10.0) s->priority = 1;
            else if (roi > 5.0) s->priority = 2;
            else if (roi > 2.0) s->priority = 3;
            else if (roi > 0.5) s->priority = 4;
            else s->priority = 5;
        } else {
            s->priority = 1;  /* No-cost optimizations are always top priority */
        }
    }
}

double power_budget_breakeven_duty(const PowerConsumerEntry *entry,
                                    double target_I_uA)
{
    assert(entry != NULL);
    assert(target_I_uA >= 0.0);

    /* target_I = I_active * duty + I_standby * (1 - duty)
     * target_I = I_standby + duty * (I_active - I_standby)
     * duty = (target_I - I_standby) / (I_active - I_standby)
     */
    double I_diff = entry->I_active_uA - entry->I_standby_uA;

    if (I_diff <= 0.0) {
        /* Active current equals standby - can't reduce by duty cycling */
        return 100.0;
    }

    double duty = (target_I_uA - entry->I_standby_uA) / I_diff * 100.0;

    if (duty < 0.0) return 0.0;
    if (duty > 100.0) return 100.0;

    return duty;
}

/* ============================================================================
 * Energy Statistics
 *
 * Computes aggregate statistics from time-series current/voltage
 * measurements. Useful for profiling real hardware.
 *
 * RMS current is particularly important for coin cell design because
 * battery capacity derating depends on the effective (RMS) current,
 * not the average. High peak-to-average ratios cause additional
 * losses through the battery's internal resistance.
 *
 * I_rms^2 * R_internal = average power lost in battery ESR.
 * ============================================================================ */

void compute_energy_statistics(const PowerSample *samples, size_t N,
                               EnergyStatistics *stats)
{
    assert(samples != NULL);
    assert(stats != NULL);
    assert(N >= 2);

    memset(stats, 0, sizeof(EnergyStatistics));

    double sum_I = 0.0, sum_I2 = 0.0;
    double total_E_uJ = 0.0;
    double min_I = DBL_MAX, max_I = -DBL_MAX;

    stats->duration_s = samples[N-1].timestamp_s - samples[0].timestamp_s;

    for (size_t i = 0; i < N; i++) {
        double I = samples[i].current_uA;
        double V = samples[i].voltage_V;

        sum_I += I;
        sum_I2 += I * I;

        if (I < min_I) min_I = I;
        if (I > max_I) max_I = I;

        /* Energy integration (trapezoidal) */
        if (i > 0) {
            double dt = samples[i].timestamp_s - samples[i-1].timestamp_s;
            if (dt > 0.0) {
                double I_avg = (I + samples[i-1].current_uA) / 2.0;
                double V_avg = (V + samples[i-1].voltage_V) / 2.0;
                total_E_uJ += V_avg * I_avg * dt;
            }
        }
    }

    stats->total_energy_uJ = total_E_uJ;
    stats->min_current_uA = min_I;
    stats->max_current_uA = max_I;
    stats->num_samples = N;

    double mean_I = sum_I / N;
    double mean_I2 = sum_I2 / N;
    double variance = mean_I2 - mean_I * mean_I;
    if (variance < 0.0) variance = 0.0;

    stats->rms_current_uA = sqrt(mean_I2);
    stats->avg_power_uW = total_E_uJ / stats->duration_s;

    if (mean_I > 0.0) {
        stats->peak_to_avg_ratio = max_I / mean_I;
    } else {
        stats->peak_to_avg_ratio = 1.0;
    }
}

double energy_per_event(const PowerConsumerEntry *entry, double V_supply)
{
    assert(entry != NULL);
    assert(V_supply > 0.0);

    /* E_per_event = I_active * V * t_active
     * I_active in uA, V in volts, t in ms -> result in nJ
     * E_uJ = (I_active_uA * 1e-6) * V * (t_ms * 1e-3) * 1e6
     *      = I_active_uA * V * t_ms / 1000
     */
    return entry->I_active_uA * V_supply * entry->duration_per_event_ms / 1000.0;
}

/* ============================================================================
 * Budget Verification
 * ============================================================================ */

void power_budget_verify(const PowerBudget *budget, double I_measured_uA,
                         PowerBudgetVerification *verification)
{
    assert(budget != NULL);
    assert(verification != NULL);

    verification->I_from_kcl_uA = budget->I_total_avg_uA;
    verification->I_measured_uA = I_measured_uA;

    if (I_measured_uA > 0.0) {
        verification->discrepancy_pct =
            (I_measured_uA - budget->I_total_avg_uA) / I_measured_uA * 100.0;
        verification->measurement_valid = 1;
    } else {
        verification->discrepancy_pct = 0.0;
        verification->measurement_valid = 0;
    }
}

/* ============================================================================
 * Pareto Frontier Generation
 *
 * The Pareto frontier shows the trade-off between power consumption
 * and system performance. Each point on the frontier represents a
 * design that cannot be improved in one metric without worsening
 * the other.
 *
 * For a coin cell sensor node:
 *   - Performance metric: measurements per day
 *   - Power metric: average current (uA)
 *   - Constraint: minimum battery life
 *
 * By sweeping the measurement interval (duty cycle), we can generate
 * the frontier and visualize the fundamental power-performance trade-off.
 * ============================================================================ */

int generate_pareto_frontier(const PowerBudget *budget,
                             const CoinCellParams *battery,
                             const CapacityDerating *derating,
                             ParetoPoint *points, int max_points)
{
    assert(budget != NULL);
    assert(battery != NULL);
    assert(derating != NULL);
    assert(points != NULL);
    assert(max_points > 0);

    double C_usable = battery->C_nominal_mAh * derating->overall_derate;
    int num_points = 0;

    /* Sweep measurement intervals from once per second to once per day */
    double intervals_per_day[] = {
        86400.0, 43200.0, 14400.0, 7200.0, 3600.0,
        1800.0, 900.0, 300.0, 60.0, 10.0, 1.0
    };
    int num_intervals = sizeof(intervals_per_day) / sizeof(intervals_per_day[0]);

    for (int i = 0; i < num_intervals && num_points < max_points; i++) {
        /* For each measurement frequency, estimate the resulting power */
        double events_per_day = intervals_per_day[i];
        double duty_cycle = events_per_day * 0.010 / 86400.0 * 100.0;
        /* 10ms measurement time assumed */

        /* Scale the budget current by duty cycle ratio */
        double I_scaled = budget->I_total_avg_uA
                          * (duty_cycle / 10.0);  /* Relative to nominal 10% */
        if (I_scaled < 0.1) I_scaled = 0.1;

        double bat_life_days = simple_battery_life_hours(C_usable, I_scaled) / 24.0;

        points[num_points].avg_power_uW = I_scaled * budget->V_supply_V;
        points[num_points].performance_metric = events_per_day;
        points[num_points].battery_life_days = bat_life_days;
        points[num_points].feasible = (bat_life_days >= 30.0) ? 1 : 0;
        num_points++;
    }

    return num_points;
}