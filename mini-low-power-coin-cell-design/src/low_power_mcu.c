/**
 * @file low_power_mcu.c
 * @brief Implementation of low-power MCU operating modes, sleep states, and power profiling
 *
 * Implements: Power profile lookup, duty cycle analysis, power state machine,
 * clock gating calculations, wakeup scheduling, DVFS management.
 *
 * Reference data from manufacturer datasheets and measured benchmarks.
 */

#include "low_power_mcu.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <math.h>

/* ============================================================================
 * Static Data - MCU Power Profiles
 *
 * Current measurements represent typical values at 25C, 3.0V supply.
 * Actual values vary with temperature, voltage, and process variation.
 * Data sources:
 *   - STM32L4: RM0351 Rev 6, Section 6.3.5 (current consumption)
 *   - nRF52840: PS v1.1, Section 6.1.9 (power management)
 *   - MSP430FR5969: SLAU367O, Section 2.5
 * ============================================================================ */

/* STM32L4xx profile - Cortex-M4 with FPU, 80MHz capable */
static const McuPowerProfile s_profile_stm32l4 = {
    .modes = {
        /* RUN: 80MHz, all peripherals on */
        { .I_supply_uA = 10000.0, .V_core_V = 1.2, .f_cpu_MHz = 80.0,
          .wakeup_latency_us = 0.0, .entry_latency_us = 0.0 },
        /* SLEEP: CPU off, peripherals on */
        { .I_supply_uA = 3000.0, .V_core_V = 1.2, .f_cpu_MHz = 0.0,
          .wakeup_latency_us = 0.5, .entry_latency_us = 0.1 },
        /* STOP: HSI/HSE off, LSI/LSE on */
        { .I_supply_uA = 50.0, .V_core_V = 1.2, .f_cpu_MHz = 0.0,
          .wakeup_latency_us = 5.0, .entry_latency_us = 2.0 },
        /* STANDBY: Vcore off, backup domain retained */
        { .I_supply_uA = 2.0, .V_core_V = 0.0, .f_cpu_MHz = 0.0,
          .wakeup_latency_us = 50.0, .entry_latency_us = 10.0 },
        /* SHUTDOWN: All off except wakeup pins */
        { .I_supply_uA = 0.03, .V_core_V = 0.0, .f_cpu_MHz = 0.0,
          .wakeup_latency_us = 300.0, .entry_latency_us = 50.0 },
    },
    .V_supply_V = 3.0,
    .I_leakage_uA = 0.01,
    .transition_energy_uJ = 0.05,
    .clock_gating_mask = 0
};

/* nRF52840 profile - Cortex-M4 with BLE, 64MHz */
static const McuPowerProfile s_profile_nrf52840 = {
    .modes = {
        /* RUN: 64MHz, CPU active */
        { .I_supply_uA = 6500.0, .V_core_V = 1.3, .f_cpu_MHz = 64.0,
          .wakeup_latency_us = 0.0, .entry_latency_us = 0.0 },
        /* SLEEP: CPU idle */
        { .I_supply_uA = 2000.0, .V_core_V = 1.3, .f_cpu_MHz = 0.0,
          .wakeup_latency_us = 0.3, .entry_latency_us = 0.1 },
        /* STOP: System ON idle (RTC running) */
        { .I_supply_uA = 3.0, .V_core_V = 1.3, .f_cpu_MHz = 0.0,
          .wakeup_latency_us = 5.0, .entry_latency_us = 2.0 },
        /* STANDBY: System OFF, RAM retained */
        { .I_supply_uA = 1.5, .V_core_V = 0.0, .f_cpu_MHz = 0.0,
          .wakeup_latency_us = 100.0, .entry_latency_us = 20.0 },
        /* SHUTDOWN: System OFF, no RAM retention */
        { .I_supply_uA = 0.4, .V_core_V = 0.0, .f_cpu_MHz = 0.0,
          .wakeup_latency_us = 500.0, .entry_latency_us = 100.0 },
    },
    .V_supply_V = 3.0,
    .I_leakage_uA = 0.02,
    .transition_energy_uJ = 0.03,
    .clock_gating_mask = 0
};

/* MSP430FR5969 profile - 16-bit RISC, 16MHz */
static const McuPowerProfile s_profile_msp430fr = {
    .modes = {
        /* RUN: Active mode, 16MHz */
        { .I_supply_uA = 1500.0, .V_core_V = 1.8, .f_cpu_MHz = 16.0,
          .wakeup_latency_us = 0.0, .entry_latency_us = 0.0 },
        /* SLEEP: LPM0 (CPU off, MCLK off) */
        { .I_supply_uA = 500.0, .V_core_V = 1.8, .f_cpu_MHz = 0.0,
          .wakeup_latency_us = 0.2, .entry_latency_us = 0.1 },
        /* STOP: LPM3 (ACLK only, RTC running) */
        { .I_supply_uA = 1.0, .V_core_V = 1.8, .f_cpu_MHz = 0.0,
          .wakeup_latency_us = 5.0, .entry_latency_us = 2.0 },
        /* STANDBY: LPM4 (all clocks off) */
        { .I_supply_uA = 0.3, .V_core_V = 0.0, .f_cpu_MHz = 0.0,
          .wakeup_latency_us = 100.0, .entry_latency_us = 50.0 },
        /* SHUTDOWN: LPM4.5 (SVS off) */
        { .I_supply_uA = 0.02, .V_core_V = 0.0, .f_cpu_MHz = 0.0,
          .wakeup_latency_us = 500.0, .entry_latency_us = 200.0 },
    },
    .V_supply_V = 3.0,
    .I_leakage_uA = 0.005,
    .transition_energy_uJ = 0.02,
    .clock_gating_mask = 0
};

/* STM32L0 profile - Cortex-M0+, ultra-low-power, 32MHz */
static const McuPowerProfile s_profile_stm32l0 = {
    .modes = {
        { .I_supply_uA = 4500.0, .V_core_V = 1.2, .f_cpu_MHz = 32.0,
          .wakeup_latency_us = 0.0, .entry_latency_us = 0.0 },
        { .I_supply_uA = 1500.0, .V_core_V = 1.2, .f_cpu_MHz = 0.0,
          .wakeup_latency_us = 0.3, .entry_latency_us = 0.1 },
        { .I_supply_uA = 1.5, .V_core_V = 1.2, .f_cpu_MHz = 0.0,
          .wakeup_latency_us = 3.0, .entry_latency_us = 1.5 },
        { .I_supply_uA = 0.35, .V_core_V = 0.0, .f_cpu_MHz = 0.0,
          .wakeup_latency_us = 50.0, .entry_latency_us = 10.0 },
        { .I_supply_uA = 0.03, .V_core_V = 0.0, .f_cpu_MHz = 0.0,
          .wakeup_latency_us = 150.0, .entry_latency_us = 30.0 },
    },
    .V_supply_V = 3.0,
    .I_leakage_uA = 0.005,
    .transition_energy_uJ = 0.02,
    .clock_gating_mask = 0
};

/* ============================================================================
 * MCU Family Name Lookup
 * ============================================================================ */

static const char* s_mcu_families[] = {
    "STM32L4", "nRF52840", "MSP430FR", "STM32L0"
};

static const McuPowerProfile* s_mcu_profiles[] = {
    &s_profile_stm32l4, &s_profile_nrf52840,
    &s_profile_msp430fr, &s_profile_stm32l0
};

#define NUM_MCU_FAMILIES (sizeof(s_mcu_families) / sizeof(s_mcu_families[0]))

/* ============================================================================
 * Power Mode Name Strings
 * ============================================================================ */

static const char* s_mode_names[MCU_MODE_COUNT] = {
    "RUN", "SLEEP", "STOP", "STANDBY", "SHUTDOWN"
};

static const char* s_mode_descriptions[MCU_MODE_COUNT] = {
    "Full operation, CPU executing at full speed",
    "CPU clock stopped, peripherals run, any IRQ wakes",
    "High-speed clocks off, SRAM retained, EXTI/RTC wake",
    "Core power off, backup domain only, RTC/WAKEUP pin wake",
    "Deepest sleep, all off except wakeup pins, POR/BOR reset"
};

/* ============================================================================
 * DVFS Points for STM32L4
 * ============================================================================ */

static const DVFSOperatingPoint s_dvfs_stm32l4[] = {
    { .V_core_V = 1.20, .f_cpu_MHz = 80.0, .I_run_uA = 10000.0,
      .max_f_MHz = 80.0, .performance_MIPS = 100 },
    { .V_core_V = 1.10, .f_cpu_MHz = 48.0, .I_run_uA = 6000.0,
      .max_f_MHz = 48.0, .performance_MIPS = 60 },
    { .V_core_V = 1.00, .f_cpu_MHz = 26.0, .I_run_uA = 3500.0,
      .max_f_MHz = 26.0, .performance_MIPS = 32 },
    { .V_core_V = 0.95, .f_cpu_MHz = 8.0,  .I_run_uA = 1200.0,
      .max_f_MHz = 8.0,  .performance_MIPS = 10 },
    { .V_core_V = 0.90, .f_cpu_MHz = 2.0,  .I_run_uA = 500.0,
      .max_f_MHz = 2.0,  .performance_MIPS = 2 },
};

/* ============================================================================
 * MCU Profile Lookup
 * ============================================================================ */

const McuPowerProfile* mcu_get_power_profile(const char *mcu_family, double V_supply)
{
    assert(mcu_family != NULL);
    (void)V_supply; /* For future voltage-dependent scaling */

    for (size_t i = 0; i < NUM_MCU_FAMILIES; i++) {
        if (strcmp(mcu_family, s_mcu_families[i]) == 0) {
            return s_mcu_profiles[i];
        }
    }
    return NULL;
}

const char* mcu_mode_get_name(McuPowerMode mode)
{
    if (mode >= MCU_MODE_COUNT) return "UNKNOWN";
    return s_mode_names[mode];
}

const char* mcu_mode_get_description(McuPowerMode mode)
{
    if (mode >= MCU_MODE_COUNT) return "Unknown power mode";
    return s_mode_descriptions[mode];
}

/* ============================================================================
 * Duty Cycle Analysis
 *
 * The duty cycle is the fundamental tool for analyzing periodic
 * low-power operation. It captures the pattern of active processing
 * followed by sleep, which is the dominant paradigm for coin cell devices.
 *
 * Key insight: average current is the weighted sum of phase currents,
 * weighted by their duration fraction of the total period.
 *
 * I_avg = sum( I_phase_k * t_phase_k ) / T_period
 *
 * For a CR2032-powered BLE beacon:
 *   Active (3ms @ 10mA):  3ms * 10000uA = 30 uA-ms
 *   Sleep (997ms @ 2uA): 997ms * 2uA    = 1994 uA-ms
 *   Total: 2024 uA-ms / 1000ms = 2.024 uA average
 *
 * At 2.024 uA average, a 225mAh CR2032 lasts:
 *   t = 225000 uAh / 2.024 uA = 111,166 hours ~ 12.7 years
 *
 * However, self-discharge and voltage cutoff reduce actual life.
 * ============================================================================ */

double duty_cycle_avg_current(const DutyCycle *dc, const McuPowerProfile *profile)
{
    assert(dc != NULL);
    assert(profile != NULL);
    assert(dc->phases != NULL);
    assert(dc->num_phases > 0);
    assert(dc->period_ms > 0.0);

    double sum_I_t = 0.0;

    for (size_t i = 0; i < dc->num_phases; i++) {
        const DutyCyclePhase *phase = &dc->phases[i];
        assert(phase->mode < MCU_MODE_COUNT);

        double I_base = profile->modes[phase->mode].I_supply_uA;
        double I_total = I_base + phase->I_extra_uA;

        sum_I_t += I_total * phase->duration_ms;
    }

    return sum_I_t / dc->period_ms;
}

void duty_cycle_energy(const DutyCycle *dc, const McuPowerProfile *profile,
                       OperationEnergy *energy)
{
    assert(dc != NULL);
    assert(profile != NULL);
    assert(energy != NULL);

    memset(energy, 0, sizeof(OperationEnergy));

    double V = profile->V_supply_V;

    for (size_t i = 0; i < dc->num_phases; i++) {
        const DutyCyclePhase *phase = &dc->phases[i];
        double I_total = profile->modes[phase->mode].I_supply_uA
                        + phase->I_extra_uA;

        /* Energy = V * I * t (convert ms to seconds) */
        double E_uJ = V * I_total * (phase->duration_ms / 1000.0);

        if (phase->mode == MCU_MODE_RUN || phase->mode == MCU_MODE_SLEEP) {
            energy->E_active_uJ += E_uJ;
        } else {
            energy->E_sleep_uJ += E_uJ;
        }
    }

    /* Transition energy: count mode changes */
    int transitions = 0;
    for (size_t i = 1; i < dc->num_phases; i++) {
        if (dc->phases[i].mode != dc->phases[i-1].mode) {
            transitions++;
        }
    }
    energy->E_transition_uJ = transitions * profile->transition_energy_uJ;

    /* Average current */
    energy->avg_current_uA = duty_cycle_avg_current(dc, profile);
    energy->observation_ms = dc->period_ms;

    /* Duty cycle = active time / total time */
    double active_ms = 0.0;
    for (size_t i = 0; i < dc->num_phases; i++) {
        if (dc->phases[i].mode == MCU_MODE_RUN
            || dc->phases[i].mode == MCU_MODE_SLEEP) {
            active_ms += dc->phases[i].duration_ms;
        }
    }
    energy->duty_cycle_pct = (active_ms / dc->period_ms) * 100.0;
}

double duty_cycle_percentage(const DutyCycle *dc)
{
    assert(dc != NULL);

    double active_ms = 0.0;
    for (size_t i = 0; i < dc->num_phases; i++) {
        if (dc->phases[i].mode == MCU_MODE_RUN
            || dc->phases[i].mode == MCU_MODE_SLEEP) {
            active_ms += dc->phases[i].duration_ms;
        }
    }

    return (active_ms / dc->period_ms) * 100.0;
}

double duty_cycle_optimal_sleep_ms(const DutyCycle *dc,
                                    const McuPowerProfile *profile,
                                    McuPowerMode sleep_mode)
{
    assert(dc != NULL);
    assert(profile != NULL);
    assert(sleep_mode >= MCU_MODE_STOP);

    /* Calculate fixed active energy (independent of sleep duration) */
    double E_active_uJ = 0.0;
    double T_active_ms = 0.0;
    double V = profile->V_supply_V;

    for (size_t i = 0; i < dc->num_phases; i++) {
        if (dc->phases[i].mode == MCU_MODE_RUN
            || dc->phases[i].mode == MCU_MODE_SLEEP) {
            double I = profile->modes[dc->phases[i].mode].I_supply_uA
                      + dc->phases[i].I_extra_uA;
            E_active_uJ += V * I * (dc->phases[i].duration_ms / 1000.0);
            T_active_ms += dc->phases[i].duration_ms;
        }
    }

    /* Sleep parameters */
    double I_sleep_uA = profile->modes[sleep_mode].I_supply_uA;
    double E_transition_uJ = profile->transition_energy_uJ;

    /* Minimize average power by choosing optimal sleep duration.
     * P_avg(T_sleep) = (E_active + E_transition + V*I_sleep*T_sleep)
     *                  / (T_active + T_sleep)
     *
     * dP/dT_sleep = 0  =>  (V*I_sleep)*(T_active+T_sleep) = E_active+E_transition+V*I_sleep*T_sleep
     *                  =>  V*I_sleep*T_active = E_active + E_transition
     *
     * This doesn't depend on T_sleep! The derivative is always positive or
     * negative, meaning the optimal is at a boundary.
     *
     * The real optimization: sleep as long as possible while meeting
     * application timing requirements. The break-even point is:
     *   T_breakeven = E_transition / (V * (I_active_avg - I_sleep))
     */

    /* Compute active average current (during active phases) */
    double I_active_avg_uA = 0.0;
    if (T_active_ms > 0.0) {
        I_active_avg_uA = (E_active_uJ / V) / (T_active_ms / 1000.0);
    }

    double P_active_uW = V * I_active_avg_uA;
    double P_sleep_uW = V * I_sleep_uA;

    if (P_active_uW <= P_sleep_uW) {
        /* Sleep doesn't save power - stay active */
        return 0.0;
    }

    /* Break-even: transition cost / power savings */
    double T_breakeven_s = E_transition_uJ / (P_active_uW - P_sleep_uW);
    double T_breakeven_ms = T_breakeven_s * 1000.0;

    /* Practical minimum: at least the break-even time */
    /* Optimal: as long as the application allows */
    return T_breakeven_ms;
}

/* ============================================================================
 * Power State Machine
 *
 * The power state machine models allowed transitions between MCU
 * power modes and tracks energy consumption over time.
 *
 * Not all transitions are physically possible. For example:
 *   SHUTDOWN -> RUN requires a full reset (POR)
 *   RUN -> SHUTDOWN is always allowed (software-triggered)
 *
 * The state machine enforces these constraints and accounts for
 * the energy cost of each transition.
 * ============================================================================ */

void psm_init(PowerStateMachine *psm, McuPowerMode initial_mode)
{
    assert(psm != NULL);
    assert(initial_mode < MCU_MODE_COUNT);

    memset(psm, 0, sizeof(PowerStateMachine));
    psm->current_mode = initial_mode;
    psm->transitions = NULL;
    psm->num_transitions = 0;
}

void psm_add_transition(PowerStateMachine *psm, McuPowerMode from, McuPowerMode to,
                        double energy_cost_uJ, double latency_us)
{
    assert(psm != NULL);
    assert(from < MCU_MODE_COUNT);
    assert(to < MCU_MODE_COUNT);

    /* Resize transition array */
    size_t new_size = (psm->num_transitions + 1) * sizeof(PowerStateTransition);
    psm->transitions = (PowerStateTransition*)realloc(psm->transitions, new_size);
    assert(psm->transitions != NULL);

    PowerStateTransition *t = &psm->transitions[psm->num_transitions];
    t->from_mode = from;
    t->to_mode = to;
    t->energy_cost_uJ = energy_cost_uJ;
    t->latency_us = latency_us;

    psm->num_transitions++;
}

int psm_transition(PowerStateMachine *psm, McuPowerMode target_mode)
{
    assert(psm != NULL);
    assert(target_mode < MCU_MODE_COUNT);

    if (psm->current_mode == target_mode) {
        return 1; /* Already in target mode */
    }

    /* Find valid transition */
    for (size_t i = 0; i < psm->num_transitions; i++) {
        const PowerStateTransition *t = &psm->transitions[i];
        if (t->from_mode == psm->current_mode && t->to_mode == target_mode) {
            /* Valid transition found */
            psm->energy_used_uJ += t->energy_cost_uJ;
            psm->uptime_ms += t->latency_us / 1000.0;
            psm->current_mode = target_mode;
            psm->transition_count++;
            return 1;
        }
    }

    return 0; /* Invalid transition */
}

double psm_get_energy(const PowerStateMachine *psm)
{
    assert(psm != NULL);
    return psm->energy_used_uJ;
}

double psm_get_uptime_ms(const PowerStateMachine *psm)
{
    assert(psm != NULL);
    return psm->uptime_ms;
}

/* ============================================================================
 * Clock Gating Analysis
 *
 * Clock gating is the most effective power reduction technique at
 * the digital logic level. By stopping the clock to unused peripherals,
 * dynamic power (CV^2*f) is eliminated for those circuits.
 *
 * Approximate per-clock power costs (uW at 3V, STM32L4):
 *   AHB:  800 uW (CPU + bus matrix + flash)
 *   APB1: 300 uW (low-speed peripherals)
 *   APB2: 400 uW (high-speed peripherals)
 *   ADC:  200 uW (analog + digital)
 *   Timer: 100 uW (16-bit counter + prescaler)
 *   SPI:   50 uW
 *   USART: 80 uW
 *   I2C:   40 uW
 *   GPIO:  20 uW
 *
 * Total savings from aggressive clock gating can exceed 50% of
 * dynamic power in RUN mode.
 * ============================================================================ */

double clock_gating_savings_uW(const McuPowerProfile *profile, uint32_t gated_clocks)
{
    assert(profile != NULL);

    /* Per-clock power consumption estimates (uW at 3V) */
    static const struct {
        ClockDomain clock;
        double power_uW;
    } clock_power_table[] = {
        { CLOCK_AHB,   800.0 },
        { CLOCK_APB1,  300.0 },
        { CLOCK_APB2,  400.0 },
        { CLOCK_ADC,   200.0 },
        { CLOCK_TIMER, 100.0 },
        { CLOCK_USART,  80.0 },
        { CLOCK_SPI,    50.0 },
        { CLOCK_I2C,    40.0 },
        { CLOCK_GPIO,   20.0 },
        { CLOCK_DMA,   150.0 },
        { CLOCK_HSE,   200.0 },
    };

    double savings = 0.0;
    size_t num_entries = sizeof(clock_power_table) / sizeof(clock_power_table[0]);

    for (size_t i = 0; i < num_entries; i++) {
        if (gated_clocks & clock_power_table[i].clock) {
            savings += clock_power_table[i].power_uW;
        }
    }

    /* Scale by V_supply/V_ref (power scales approximately with V^2) */
    double V_ratio = profile->V_supply_V / 3.0;
    savings *= V_ratio * V_ratio;

    return savings;
}

double peripheral_current_estimate(const McuPowerProfile *profile,
                                   uint32_t active_clocks)
{
    assert(profile != NULL);

    double power_uW = 0.0;

    /* Sum power for all active clocks (inverse of gating - compute all then subtract) */
    uint32_t all_clocks = CLOCK_AHB | CLOCK_APB1 | CLOCK_APB2 | CLOCK_ADC
                        | CLOCK_TIMER | CLOCK_USART | CLOCK_SPI | CLOCK_I2C
                        | CLOCK_GPIO | CLOCK_DMA | CLOCK_HSE;

    double total_possible = clock_gating_savings_uW(profile, all_clocks);
    double gated_savings = clock_gating_savings_uW(profile, ~active_clocks & all_clocks);
    power_uW = total_possible - gated_savings;

    /* Convert to current: I = P / V */
    return power_uW / profile->V_supply_V;
}

/* ============================================================================
 * Wakeup Scheduler
 *
 * The wakeup scheduler manages periodic MCU wakeups using the RTC.
 * Key responsibilities:
 *   1. Schedule next wakeup at precise intervals
 *   2. Compensate for crystal frequency drift
 *   3. Adapt to temperature changes (crystal tempco)
 *   4. Track missed wakeups for reliability monitoring
 *
 * Crystal drift compensation:
 *   - Tuning fork crystals (32.768kHz): typ +/-20ppm at 25C
 *   - Temperature coefficient: -0.034 ppm/C^2 (parabolic)
 *   - Aging: +/-3ppm first year, +/-1ppm/year thereafter
 *
 * Without compensation, 20ppm drift causes:
 *   - 1.7 seconds/day error
 *   - 10.4 minutes/year error
 *   - For a 10-second wakeup interval, drift of 200us per wakeup
 * ============================================================================ */

void wakeup_scheduler_init(WakeupScheduler *sched, double interval_ms,
                           double crystal_ppm)
{
    assert(sched != NULL);
    assert(interval_ms > 0.0);

    memset(sched, 0, sizeof(WakeupScheduler));
    sched->wakeup_interval_ms = interval_ms;
    sched->next_wakeup_ms = interval_ms;
    sched->drift_compensation = crystal_ppm;
    sched->crystal_aging_enabled = 0;
    sched->missed_wakeups = 0;

    /* Initialize calibration data */
    sched->rtc_cal.nominal_freq_Hz = 32768.0;
    sched->rtc_cal.calibrated_freq_Hz = 32768.0;
    sched->rtc_cal.drift_ppm = crystal_ppm;
    sched->rtc_cal.temperature_C = 25.0;
    sched->rtc_cal.voltage_V = 3.0;
}

double wakeup_schedule_next(WakeupScheduler *sched, double current_time_ms)
{
    assert(sched != NULL);

    /* Apply drift compensation: adjust interval by ppm error */
    double corrected_interval = sched->wakeup_interval_ms
        * (1.0 + sched->drift_compensation / 1e6);

    sched->next_wakeup_ms = current_time_ms + corrected_interval;
    return sched->next_wakeup_ms;
}

void wakeup_report_fired(WakeupScheduler *sched, double actual_time_ms,
                         double expected_time_ms)
{
    assert(sched != NULL);

    double error_ms = actual_time_ms - expected_time_ms;
    double error_ppm = (error_ms / sched->wakeup_interval_ms) * 1e6;

    /* Exponential moving average of drift */
    double alpha = 0.1; /* Smoothing factor */
    sched->drift_compensation = (1.0 - alpha) * sched->drift_compensation
                                + alpha * error_ppm;

    /* Track large deviations as missed wakeups */
    if (fabs(error_ms) > sched->wakeup_interval_ms * 0.5) {
        sched->missed_wakeups++;
    }
}

void wakeup_temp_compensate(WakeupScheduler *sched, double temperature_C)
{
    assert(sched != NULL);

    /* Parabolic temperature coefficient for tuning fork crystal:
     *   df/f = k * (T - T_turnover)^2
     *   k = -0.034 ppm/C^2 (typical)
     *   T_turnover = 25C (turnover temperature)
     */
    double dT = temperature_C - 25.0;
    double temp_drift_ppm = -0.034 * dT * dT;

    sched->drift_compensation += temp_drift_ppm;
    sched->rtc_cal.temperature_C = temperature_C;
    sched->rtc_cal.drift_ppm = sched->drift_compensation;
}

/* ============================================================================
 * DVFS Management
 *
 * Dynamic Voltage and Frequency Scaling allows the MCU to operate
 * at lower voltage/frequency points when full performance is not needed.
 * Power scales approximately as P = C * V^2 * f, so reducing both
 * voltage and frequency yields cubic power savings.
 *
 * Example (STM32L4):
 *   Range 1: 1.2V @ 80MHz -> ~10mA (baseline)
 *   Range 2: 1.0V @ 26MHz -> ~3.5mA (3x power reduction, 3x frequency reduction)
 *
 * The key insight: if the CPU is active for a fixed workload (not a fixed time),
 * the energy per operation may be LOWER at a lower frequency due to reduced
 * leakage and voltage headroom.
 * ============================================================================ */

const DVFSOperatingPoint* dvfs_get_points(const char *mcu_family, int *num_points)
{
    assert(mcu_family != NULL);
    assert(num_points != NULL);

    if (strcmp(mcu_family, "STM32L4") == 0) {
        *num_points = sizeof(s_dvfs_stm32l4) / sizeof(s_dvfs_stm32l4[0]);
        return s_dvfs_stm32l4;
    }

    *num_points = 0;
    return NULL;
}

int dvfs_select_point(const DVFSOperatingPoint *points, int num_points,
                      double required_mips)
{
    assert(points != NULL);
    assert(num_points > 0);

    int best_idx = -1;
    double best_current = 1e9; /* Large number */

    for (int i = 0; i < num_points; i++) {
        if (points[i].performance_MIPS >= required_mips) {
            if (points[i].I_run_uA < best_current) {
                best_current = points[i].I_run_uA;
                best_idx = i;
            }
        }
    }

    return best_idx;
}

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

double current_to_power_uW(double I_uA, double V_volts)
{
    return I_uA * V_volts;
}

double energy_per_instruction(const McuPowerProfile *profile,
                              const char *mcu_family)
{
    assert(profile != NULL);
    assert(mcu_family != NULL);

    double I_run_uA = profile->modes[MCU_MODE_RUN].I_supply_uA;
    double f_MHz = profile->modes[MCU_MODE_RUN].f_cpu_MHz;
    double V = profile->V_supply_V;

    if (f_MHz <= 0.0) return 0.0;

    /* Instructions per second = f_MHz * 1e6 (approximate, varies by ISA) */
    /* Cortex-M: ~1 DMIPS/MHz, MSP430: ~0.3 MIPS/MHz */
    double ips;
    if (strstr(mcu_family, "MSP430")) {
        ips = f_MHz * 1e6 * 0.3; /* 16-bit RISC, lower IPC */
    } else if (strstr(mcu_family, "nRF") || strstr(mcu_family, "STM32")) {
        ips = f_MHz * 1e6 * 0.95; /* Cortex-M, near 1 IPC */
    } else {
        ips = f_MHz * 1e6 * 0.8; /* Default assumption */
    }

    /* Power = V * I, Energy per instruction = Power / ips */
    double power_uW = V * I_run_uA;
    double E_per_inst_uJ = power_uW / (ips / 1e6); /* uJ per instruction */

    return E_per_inst_uJ * 1000.0; /* Convert to nJ */
}

double sleep_breakeven_us(const McuPowerProfile *profile,
                          McuPowerMode sleep_mode, McuPowerMode run_mode)
{
    assert(profile != NULL);
    assert(sleep_mode < MCU_MODE_COUNT);
    assert(run_mode < MCU_MODE_COUNT);

    double P_run_uW = profile->V_supply_V * profile->modes[run_mode].I_supply_uA;
    double P_sleep_uW = profile->V_supply_V * profile->modes[sleep_mode].I_supply_uA;

    if (P_run_uW <= P_sleep_uW) {
        return 1e9; /* Sleep doesn't save power - never beneficial */
    }

    /* T_breakeven = E_transition / (P_run - P_sleep) */
    double E_transition_uJ = profile->transition_energy_uJ;
    double power_saving_uW = P_run_uW - P_sleep_uW;

    return E_transition_uJ / power_saving_uW * 1e6; /* Convert to us */
}