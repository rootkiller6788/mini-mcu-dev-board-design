/**
 * @file voltage_regulation.c
 * @brief Implementation of voltage regulation for coin cell applications
 *
 * Implements: Regulator selection, LDO and switching regulator analysis,
 * efficiency calculation, power balance, BOD configuration,
 * voltage supervision, and energy loss analysis.
 */

#include "voltage_regulation.h"
#include "coin_cell_battery.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <math.h>

/* ============================================================================
 * Static Data - Common Regulator Parameters
 *
 * Data from manufacturer datasheets (typical values at 25C).
 *
 * LDO selection criteria for coin cell:
 *   - Ultra-low Iq (< 1uA) for always-on operation
 *   - Low dropout voltage (< 200mV) to maximize battery utilization
 *   - Stable with ceramic output capacitors (low ESR)
 *   - Small package (SOT-23, SC-70, or smaller)
 *
 * Top LDO choices for coin cell designs:
 *   TPS783:  500nA Iq, 130mV dropout @ 150mA
 *   MCP1700: 1.6uA Iq, 178mV dropout @ 250mA
 *   XC6206:  1.0uA Iq, 250mV dropout @ 150mA
 * ============================================================================ */

static const LDOParams s_ldo_tps783 = {
    .V_out_nominal_V = 3.0,
    .V_dropout_max_mV = 130.0,
    .I_q_typ_uA = 0.5,
    .I_q_max_uA = 1.0,
    .I_out_max_mA = 150.0,
    .PSRR_dB = 40.0,
    .line_regulation_mV = 2.0,
    .load_regulation_pct = 1.0,
    .efficiency_at_typical = 85.0,
    .has_enable_pin = 0,
    .I_shutdown_nA = 10.0
};

static const LDOParams s_ldo_mcp1700 = {
    .V_out_nominal_V = 3.0,
    .V_dropout_max_mV = 178.0,
    .I_q_typ_uA = 1.6,
    .I_q_max_uA = 4.0,
    .I_out_max_mA = 250.0,
    .PSRR_dB = 44.0,
    .line_regulation_mV = 1.5,
    .load_regulation_pct = 0.5,
    .efficiency_at_typical = 83.0,
    .has_enable_pin = 0,
    .I_shutdown_nA = 20.0
};

static const LDOParams s_ldo_xc6206 = {
    .V_out_nominal_V = 3.0,
    .V_dropout_max_mV = 250.0,
    .I_q_typ_uA = 1.0,
    .I_q_max_uA = 3.0,
    .I_out_max_mA = 150.0,
    .PSRR_dB = 30.0,
    .line_regulation_mV = 5.0,
    .load_regulation_pct = 1.5,
    .efficiency_at_typical = 80.0,
    .has_enable_pin = 1,
    .I_shutdown_nA = 100.0
};

/* TPS62740 - Ultra-low power buck converter for coin cell */
static const SwitchingRegulatorParams s_sw_tps62740 = {
    .V_in_min_V = 2.2,
    .V_in_max_V = 5.5,
    .V_out_V = 1.8,
    .I_out_max_mA = 300.0,
    .efficiency_peak_pct = 90.0,
    .efficiency_at_10uA = 80.0,
    .efficiency_at_1mA = 88.0,
    .I_q_uA = 0.36,
    .f_sw_kHz = 1200.0,
    .L_typ_uH = 2.2,
    .C_in_typ_uF = 4.7,
    .C_out_typ_uF = 10.0,
    .ripple_mV = 15.0,
    .has_pfm_mode = 1
};

/* TPS61099 - Ultra-low power boost converter */
static const SwitchingRegulatorParams s_sw_tps61099 = {
    .V_in_min_V = 0.7,
    .V_in_max_V = 5.5,
    .V_out_V = 3.3,
    .I_out_max_mA = 100.0,
    .efficiency_peak_pct = 93.0,
    .efficiency_at_10uA = 70.0,
    .efficiency_at_1mA = 85.0,
    .I_q_uA = 1.0,
    .f_sw_kHz = 800.0,
    .L_typ_uH = 4.7,
    .C_in_typ_uF = 10.0,
    .C_out_typ_uF = 10.0,
    .ripple_mV = 30.0,
    .has_pfm_mode = 1
};

/* ============================================================================
 * Regulator Type Selection
 *
 * The decision tree for selecting a regulator type in a coin cell design:
 *
 *   1. Can the MCU run directly from the battery?
 *      - Check: V_battery_min > V_mcu_min AND V_battery_max < V_mcu_max
 *      - If yes -> BYPASS (most efficient, zero BOM cost)
 *
 *   2. Is V_battery always above V_out?
 *      - If yes -> LDO (simple, low noise, low BOM)
 *      - Exception: if efficiency is critical and V_battery >> V_out,
 *        a buck converter may be justified
 *
 *   3. Is V_battery always below V_out?
 *      - If yes -> Boost (e.g., single-cell alkaline to 3.3V)
 *
 *   4. Does V_battery cross V_out during discharge?
 *      - If yes -> Buck-Boost (complex but necessary)
 *
 * The key coin cell scenario: CR2032 (3.0V nominal, 2.0V cutoff)
 * powering a 3.0V MCU. Options:
 *   a. Bypass if MCU works down to 2.0V (e.g., nRF52: 1.7-3.6V)
 *   b. LDO to 2.5V if MCU needs regulated supply
 *   c. Boost to 3.3V if peripherals require it (but efficiency hit)
 * ============================================================================ */

RegulatorType select_regulator_type(double V_battery_min, double V_battery_max,
                                     double V_out_required, double I_out_avg_uA,
                                     double I_out_peak_mA)
{
    (void)I_out_peak_mA;
    /* Bypass check: can the load tolerate the full battery voltage range? */
    /* (This function only checks voltage - caller must verify load tolerance) */

    /* LDO check: V_battery_min > V_out + V_dropout */
    double V_dropout_min = 0.15;  /* Assume 150mV dropout for good LDO */

    if (V_battery_min >= V_out_required + V_dropout_min) {
        /* LDO is feasible */
        /* For very low current, LDO is usually best despite efficiency */
        if (I_out_avg_uA < 100.0) {
            return REG_LDO;
        }
        /* For higher current and large Vin/Vout ratio, consider buck */
        if (V_battery_min > V_out_required * 1.8) {
            return REG_BUCK;
        }
        return REG_LDO;
    }

    /* Boost check: V_battery_max < V_out_required */
    if (V_battery_max < V_out_required) {
        return REG_BOOST;
    }

    /* Voltage crosses Vout -> need buck-boost */
    if (V_battery_min < V_out_required && V_battery_max > V_out_required) {
        return REG_BUCK_BOOST;
    }

    /* Default: try LDO, might work near full charge */
    return REG_LDO;
}

const LDOParams* ldo_get_params(const char *part_number)
{
    assert(part_number != NULL);

    if (strstr(part_number, "TPS783")) return &s_ldo_tps783;
    if (strstr(part_number, "MCP1700")) return &s_ldo_mcp1700;
    if (strstr(part_number, "XC6206"))  return &s_ldo_xc6206;

    return NULL;
}

const SwitchingRegulatorParams* swreg_get_params(const char *part_number)
{
    assert(part_number != NULL);

    if (strstr(part_number, "TPS62740")) return &s_sw_tps62740;
    if (strstr(part_number, "TPS61099")) return &s_sw_tps61099;

    return NULL;
}

/* ============================================================================
 * Efficiency Analysis
 *
 * Efficiency is the ratio of output power to input power.
 *
 * LDO efficiency:
 *   eta = (Vout * Iout) / (Vin * (Iout + Iq)) * 100%
 *
 * At heavy loads (Iout >> Iq), efficiency approaches Vout/Vin.
 * At light loads (Iout ~ Iq), Iq dominates and efficiency drops sharply.
 *
 * Example: CR2032 -> 2.5V LDO, I_load = 1uA, Iq = 0.5uA:
 *   eta = (2.5 * 1) / (3.0 * 1.5) * 100 = 55.6%
 *
 * Example: Same LDO, I_load = 1mA:
 *   eta = (2.5 * 1000) / (3.0 * 1000.5) * 100 = 83.3%
 *
 * This is why LOW Iq is critical for coin cell designs with
 * long sleep periods - the LDO is always consuming Iq even
 * when the load is in deep sleep.
 * ============================================================================ */

double ldo_efficiency(const LDOParams *ldo, double V_in, double I_out)
{
    assert(ldo != NULL);
    assert(V_in > 0.0);
    assert(I_out >= 0.0);

    if (V_in <= ldo->V_out_nominal_V + ldo->V_dropout_max_mV / 1000.0) {
        return 0.0; /* In dropout - no regulation */
    }

    /* Pin = Vin * (Iout + Iq) */
    double I_in_mA = I_out + (ldo->I_q_typ_uA / 1000.0);
    double P_in_mW = V_in * I_in_mA;
    double P_out_mW = ldo->V_out_nominal_V * I_out;

    if (P_in_mW <= 0.0) return 0.0;

    return (P_out_mW / P_in_mW) * 100.0;
}

double switching_regulator_efficiency(const SwitchingRegulatorParams *sw,
                                       double V_in, double I_out)
{
    assert(sw != NULL);
    assert(V_in >= sw->V_in_min_V && V_in <= sw->V_in_max_V);
    assert(I_out >= 0.0);

    if (I_out <= 0.0) return 0.0;

    /* Piecewise efficiency model derived from datasheet curves.
     *
     * Switching regulator losses consist of:
     *   1. Fixed losses (control circuit, gate drive): P_fixed
     *   2. Conduction losses (I^2*R): proportional to I_out^2
     *   3. Switching losses: proportional to f_sw
     *
     * Ploss = P_fixed + R_eff * I_out^2
     * P_out = V_out * I_out
     * eta = P_out / (P_out + Ploss)
     *
     * From peak efficiency point, we can derive R_eff:
     *   At peak eta, d(eta)/dI = 0  =>  R_eff * I^2 = P_fixed
     *   => P_fixed = R_eff * I_peak^2
     *   => eta_peak = V_out*I_peak / (V_out*I_peak + 2*R_eff*I_peak^2)
     */

    double V_out = sw->V_out_V;
    double eta_peak = sw->efficiency_peak_pct / 100.0;

    /* Estimate I at peak efficiency (typically 30-50% of max) */
    double I_peak = sw->I_out_max_mA * 0.4;

    /* Derive R_eff from peak efficiency */
    /* eta = V*I / (V*I + 2*R*I^2) at peak
     * => 2*R*I = V*(1/eta - 1)
     * => R = V*(1/eta - 1) / (2*I)
     */
    double R_eff = V_out * (1.0 / eta_peak - 1.0) / (2.0 * I_peak);
    if (R_eff < 0.0) R_eff = 0.001; /* Fallback */

    double P_fixed = R_eff * I_peak * I_peak;

    /* Compute efficiency at given operating point */
    double I_out_mA = I_out;
    double P_out_mW = V_out * I_out_mA;
    double P_loss_mW = P_fixed + R_eff * I_out_mA * I_out_mA;

    /* Add quiescent power */
    P_loss_mW += sw->I_q_uA / 1000.0 * V_in;

    if (P_out_mW + P_loss_mW <= 0.0) return 0.0;

    double eta = P_out_mW / (P_out_mW + P_loss_mW) * 100.0;

    /* Clamp to realistic range */
    if (eta > 98.0) eta = 98.0;
    if (eta < 0.0) eta = 0.0;

    return eta;
}

void regulator_analyze_operating_point(const PowerSupplyDesign *design,
                                        double V_battery, double I_load,
                                        RegulatorOperatingPoint *op)
{
    assert(design != NULL);
    assert(op != NULL);

    memset(op, 0, sizeof(RegulatorOperatingPoint));
    op->V_in_V = V_battery;
    op->I_out_mA = I_load;

    switch (design->type) {
    case REG_LDO: {
        op->V_out_V = design->params.ldo.V_out_nominal_V;
        op->in_regulation = (V_battery >= op->V_out_V
                             + design->params.ldo.V_dropout_max_mV / 1000.0) ? 1 : 0;
        op->efficiency_pct = ldo_efficiency(&design->params.ldo, V_battery, I_load);
        op->power_loss_mW = V_battery * (I_load + design->params.ldo.I_q_typ_uA/1000.0)
                           - op->V_out_V * I_load;
        break;
    }
    case REG_BOOST:
    case REG_BUCK:
    case REG_BUCK_BOOST:
        op->V_out_V = design->params.sw.V_out_V;
        op->in_regulation = (V_battery >= design->params.sw.V_in_min_V
                             && V_battery <= design->params.sw.V_in_max_V) ? 1 : 0;
        op->efficiency_pct = switching_regulator_efficiency(
            &design->params.sw, V_battery, I_load);
        op->power_loss_mW = (op->V_out_V * I_load)
                           * (100.0 / op->efficiency_pct - 1.0);
        break;
    case REG_BYPASS:
        op->V_out_V = V_battery;
        op->efficiency_pct = 100.0;
        op->in_regulation = 1;
        op->power_loss_mW = 0.0;
        break;
    default:
        break;
    }

    op->in_current_limit = (I_load > design->I_out_peak_mA) ? 1 : 0;
}

void regulator_power_balance(const PowerSupplyDesign *design,
                              double V_in, double I_out,
                              RegulatorPowerBalance *balance)
{
    assert(design != NULL);
    assert(balance != NULL);

    memset(balance, 0, sizeof(RegulatorPowerBalance));

    switch (design->type) {
    case REG_LDO: {
        double I_total = I_out + design->params.ldo.I_q_typ_uA / 1000.0;
        balance->P_in_mW = V_in * I_total;
        balance->P_out_mW = design->params.ldo.V_out_nominal_V * I_out;
        balance->P_quiescent_mW = V_in * design->params.ldo.I_q_typ_uA / 1000.0;
        balance->P_conduction_mW = (V_in - design->params.ldo.V_out_nominal_V) * I_out;
        balance->P_total_loss_mW = balance->P_in_mW - balance->P_out_mW;
        balance->efficiency_pct = balance->P_in_mW > 0.0
            ? (balance->P_out_mW / balance->P_in_mW) * 100.0 : 0.0;
        break;
    }
    case REG_BOOST:
    case REG_BUCK:
    case REG_BUCK_BOOST:
        balance->P_out_mW = design->params.sw.V_out_V * I_out;
        balance->efficiency_pct = switching_regulator_efficiency(
            &design->params.sw, V_in, I_out);
        if (balance->efficiency_pct > 0.0) {
            balance->P_in_mW = balance->P_out_mW
                             / (balance->efficiency_pct / 100.0);
        }
        balance->P_total_loss_mW = balance->P_in_mW - balance->P_out_mW;
        balance->P_switching_mW = balance->P_total_loss_mW * 0.6;
        balance->P_conduction_mW = balance->P_total_loss_mW * 0.3;
        balance->P_quiescent_mW = balance->P_total_loss_mW * 0.1;
        break;
    case REG_BYPASS:
        balance->P_in_mW = V_in * I_out;
        balance->P_out_mW = V_in * I_out;
        balance->efficiency_pct = 100.0;
        break;
    default:
        break;
    }
}

/* ============================================================================
 * Battery Voltage Range Analysis
 * ============================================================================ */

int can_use_direct_battery(double V_battery_min, double V_battery_max,
                            double V_load_min, double V_load_max)
{
    /* Direct connection is feasible if battery voltage range is a subset
     * of the load's acceptable voltage range */
    return (V_battery_min >= V_load_min && V_battery_max <= V_load_max) ? 1 : 0;
}

double usable_capacity_fraction(double battery_min_V, double battery_max_V,
                                 double regulator_min_input_V,
                                 const double *discharge_V, const double *discharge_SoC,
                                 size_t N)
{
    (void)battery_min_V;
    (void)battery_max_V;
    assert(discharge_V != NULL);
    assert(discharge_SoC != NULL);
    assert(N >= 2);

    /* Find the SoC at which voltage drops below regulator minimum */
    double SoC_at_cutoff = 0.0;

    for (size_t i = 0; i < N; i++) {
        if (discharge_V[i] >= regulator_min_input_V) {
            SoC_at_cutoff = discharge_SoC[i];
        } else {
            break; /* First point below threshold */
        }
    }

    /* Usable fraction = SoC range above cutoff / full SoC range */
    double usable = SoC_at_cutoff - discharge_SoC[N-1];
    double full_range = discharge_SoC[0] - discharge_SoC[N-1];

    if (full_range <= 0.0) return 1.0;
    return usable / full_range;
}

/* ============================================================================
 * LDO Component Selection
 * ============================================================================ */

double ldo_output_capacitor(const LDOParams *ldo, double I_out_max,
                             double required_transient_mV)
{
    assert(ldo != NULL);
    assert(I_out_max >= 0.0);
    assert(required_transient_mV > 0.0);

    /* C_out >= I_step * dt / dV
     *
     * dt is the LDO's response time (typically tens of microseconds).
     * For a conservative design, assume dt = 50us.
     *
     * I_step = I_out_max (worst-case load step from 0 to full load)
     */
    double dt_s = 50e-6;  /* 50us response time */
    double dV_V = required_transient_mV / 1000.0;

    double C_min_F = (I_out_max / 1000.0) * dt_s / dV_V;

    /* Convert to uF */
    double C_min_uF = C_min_F * 1e6;

    /* Apply safety factor of 2x */
    C_min_uF *= 2.0;

    /* Ensure minimum capacitance for LDO stability */
    double C_min_stable_uF = 1.0;  /* Most modern LDOs stable with 1uF ceramic */

    return (C_min_uF > C_min_stable_uF) ? C_min_uF : C_min_stable_uF;
}

double ldo_junction_temperature(const LDOParams *ldo, double V_in,
                                 double I_out, double T_ambient_C,
                                 double R_theta_JA)
{
    assert(ldo != NULL);
    assert(R_theta_JA > 0.0);

    /* Power dissipation = (Vin - Vout) * Iout + Vin * Iq */
    double P_diss_W = (V_in - ldo->V_out_nominal_V) * (I_out / 1000.0)
                     + V_in * (ldo->I_q_typ_uA / 1e6);

    /* Junction temperature: Tj = Ta + Pd * Rth_JA */
    double T_junction = T_ambient_C + P_diss_W * R_theta_JA;

    /* Typical max junction temperature is 125C */
    if (T_junction > 125.0) return -1.0;

    return T_junction;
}

/* ============================================================================
 * Brown-Out Detection and Voltage Supervision
 *
 * Brown-out detection (BOD) is critical for coin cell designs because:
 *   - Battery voltage decays slowly and predictably
 *   - Below minimum voltage, MCU behavior is undefined
 *   - Flash writes below minimum voltage cause corruption
 *   - BOD prevents these failure modes by holding the MCU in reset
 *
 * Typical BOD thresholds for 3.0V systems:
 *   - Warning:  2.5V (low battery, time to notify user)
 *   - Critical: 2.2V (prepare for safe shutdown, save state)
 *   - Shutdown: 2.0V (hard reset, prevent corruption)
 *
 * Hysteresis is essential: without it, recovering voltage after
 * a load reduction could cause rapid BOD toggling (chatter).
 * ============================================================================ */

void bod_configure(double V_system_min, double V_hysteresis, BODConfig *config)
{
    assert(config != NULL);
    assert(V_system_min > 0.0);
    assert(V_hysteresis >= 0.0);

    memset(config, 0, sizeof(BODConfig));
    config->V_threshold_V = V_system_min;
    config->V_hysteresis_mV = V_hysteresis * 1000.0;
    config->response_time_us = 10.0;  /* Fast response for data integrity */
    config->enabled_in_sleep = 1;     /* Always protect, even in sleep */
    config->generate_interrupt = 1;   /* Warn before reset */
}

void voltage_supervisor_init(VoltageSupervisor *sup, double V_nominal,
                              double V_min_system)
{
    assert(sup != NULL);
    assert(V_nominal > V_min_system);

    memset(sup, 0, sizeof(VoltageSupervisor));

    /* Set thresholds as percentages of nominal voltage */
    sup->V_warning_V  = V_min_system + (V_nominal - V_min_system) * 0.30;
    sup->V_critical_V = V_min_system + (V_nominal - V_min_system) * 0.10;
    sup->V_shutdown_V = V_min_system;

    /* Hysteresis: 2% of threshold to prevent chatter */
    sup->V_hysteresis_mV = sup->V_warning_V * 0.02 * 1000.0;

    sup->warning_active = 0;
    sup->critical_active = 0;
    sup->shutdown_active = 0;
}

int voltage_supervisor_check(VoltageSupervisor *sup, double V_measured)
{
    assert(sup != NULL);

    double hyster = sup->V_hysteresis_mV / 1000.0;

    /* Shutdown check (highest priority) */
    if (V_measured <= sup->V_shutdown_V) {
        sup->shutdown_active = 1;
        sup->critical_active = 1;
        sup->warning_active = 1;
        return 3;
    }
    if (sup->shutdown_active && V_measured > sup->V_shutdown_V + hyster) {
        sup->shutdown_active = 0;
    }

    /* Critical check */
    if (V_measured <= sup->V_critical_V) {
        sup->critical_active = 1;
        sup->warning_active = 1;
        return 2;
    }
    if (sup->critical_active && V_measured > sup->V_critical_V + hyster) {
        sup->critical_active = 0;
    }

    /* Warning check */
    if (V_measured <= sup->V_warning_V) {
        sup->warning_active = 1;
        return 1;
    }
    if (sup->warning_active && V_measured > sup->V_warning_V + hyster) {
        sup->warning_active = 0;
    }

    /* Normal operation */
    return 0;
}

const char* voltage_supervisor_action(const VoltageSupervisor *sup)
{
    assert(sup != NULL);

    if (sup->shutdown_active) {
        return "EMERGENCY: Save critical data and shutdown immediately. "
               "Flash writes disabled. Switch to lowest power mode.";
    }
    if (sup->critical_active) {
        return "CRITICAL: Battery nearly depleted. "
               "Reduce reporting frequency. Prepare for shutdown.";
    }
    if (sup->warning_active) {
        return "WARNING: Battery voltage low. "
               "Consider replacing battery soon. Monitor closely.";
    }
    return "OK: Battery voltage normal. Normal operation.";
}

/* ============================================================================
 * Energy Loss Analysis
 * ============================================================================ */

double regulator_total_energy_loss(const PowerSupplyDesign *design,
                                    double C_battery_mAh, double I_load_avg_uA)
{
    assert(design != NULL);
    assert(C_battery_mAh > 0.0);
    assert(I_load_avg_uA > 0.0);

    /* Approximate by integrating losses over the discharge.
     * Simplified: use midpoint voltage as representative.
     */
    double V_mid;
    switch (design->type) {
    case REG_LDO:
        V_mid = 2.8;  /* Midpoint of CR2032 discharge */
        break;
    case REG_BOOST:
        V_mid = 2.5;
        break;
    default:
        V_mid = 3.0;
    }

    RegulatorOperatingPoint op;
    regulator_analyze_operating_point(design, V_mid,
                                       I_load_avg_uA / 1000.0, &op);

    /* Total energy = loss power * lifetime */
    double lifetime_hours = C_battery_mAh * 1000.0 / I_load_avg_uA;
    return op.power_loss_mW * lifetime_hours / 1000.0;  /* Joules */
}

double regulator_compare(const PowerSupplyDesign *opt_a,
                          const PowerSupplyDesign *opt_b,
                          double C_battery_mAh, double I_load_avg_uA)
{
    assert(opt_a != NULL);
    assert(opt_b != NULL);

    double loss_a = regulator_total_energy_loss(opt_a, C_battery_mAh, I_load_avg_uA);
    double loss_b = regulator_total_energy_loss(opt_b, C_battery_mAh, I_load_avg_uA);

    /* Lower loss = longer life. Return life difference in hours.
     * E_available = C * V * 3.6 (Joules from mAh at nominal V)
     */
    double E_available_J = C_battery_mAh * 3.0 * 3.6;
    double E_usable_a = E_available_J - loss_a;
    double E_usable_b = E_available_J - loss_b;

    double I_avg_A = I_load_avg_uA / 1e6;
    double life_a = (E_usable_a / 3.0) / I_avg_A / 3600.0;
    double life_b = (E_usable_b / 3.0) / I_avg_A / 3600.0;

    return life_a - life_b;
}