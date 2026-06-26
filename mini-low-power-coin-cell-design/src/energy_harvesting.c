/**
 * @file energy_harvesting.c
 * @brief Energy harvesting for coin cell applications
 */
#include "energy_harvesting.h"
#include "coin_cell_battery.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <math.h>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* ============================================================================
 * Harvester Parameter Database
 * ============================================================================ */

HarvesterParams harvester_estimate(HarvestingSource source, const char *condition, double area_cm2) {
    (void)condition;
    HarvesterParams hp; memset(&hp, 0, sizeof(hp));
    hp.source_type = source; hp.area_cm2 = area_cm2;
    if (source == HARVEST_SOLAR_INDOOR) {
        hp.P_density_uW_per_cm2 = 20.0; hp.V_oc_V = 2.5; hp.I_sc_uA = area_cm2 * 8.0;
        hp.V_mpp_V = 2.0; hp.I_mpp_uA = area_cm2 * 6.0; hp.P_mpp_uW = hp.V_mpp_V * hp.I_mpp_uA;
        hp.efficiency_pct = 10.0; hp.typical_condition = "Office lighting 500 lux";
    } else if (source == HARVEST_SOLAR_OUTDOOR) {
        hp.P_density_uW_per_cm2 = 10000.0; hp.V_oc_V = 3.5; hp.I_sc_uA = area_cm2 * 3000.0;
        hp.V_mpp_V = 2.8; hp.I_mpp_uA = area_cm2 * 2500.0; hp.P_mpp_uW = hp.V_mpp_V * hp.I_mpp_uA;
        hp.efficiency_pct = 20.0; hp.typical_condition = "Full sun 50000 lux";
    } else if (source == HARVEST_THERMAL_TEG) {
        hp.P_density_uW_per_cm2 = 50.0; hp.V_oc_V = 0.5; hp.I_sc_uA = area_cm2 * 100.0;
        hp.V_mpp_V = 0.25; hp.I_mpp_uA = area_cm2 * 80.0; hp.P_mpp_uW = hp.V_mpp_V * hp.I_mpp_uA;
        hp.efficiency_pct = 2.0; hp.typical_condition = "dT=10C, body heat";
    } else if (source == HARVEST_RF_AMBIENT) {
        hp.P_density_uW_per_cm2 = 0.1; hp.V_oc_V = 1.0; hp.I_sc_uA = area_cm2 * 0.1;
        hp.V_mpp_V = 0.5; hp.I_mpp_uA = area_cm2 * 0.08; hp.P_mpp_uW = hp.V_mpp_V * hp.I_mpp_uA;
        hp.efficiency_pct = 30.0; hp.typical_condition = "Urban WiFi ambient";
    } else if (source == HARVEST_PIEZO_VIBRATION) {
        hp.P_density_uW_per_cm2 = 200.0; hp.V_oc_V = 5.0; hp.I_sc_uA = area_cm2 * 40.0;
        hp.V_mpp_V = 2.5; hp.I_mpp_uA = area_cm2 * 30.0; hp.P_mpp_uW = hp.V_mpp_V * hp.I_mpp_uA;
        hp.efficiency_pct = 15.0; hp.typical_condition = "1g vibration at 50Hz";
    }
    return hp;
}

/* ---- Solar Power Output ---- */

double solar_power_output(const SolarIrradianceModel *model, double area_cm2) {
    assert(model != NULL); assert(area_cm2 >= 0.0);
    double P_W = model->irradiance_W_per_m2 * (area_cm2 / 10000.0) * (model->cell_efficiency_pct / 100.0);
    return P_W * 1e6; /* Convert to uW */
}

/* ---- TEG Power Output (Seebeck effect) ---- */

double teg_power_output(const TEGModel *teg) {
    assert(teg != NULL);
    double alpha_total = teg->alpha_uV_per_K * 1e-6 * teg->num_junctions;
    double V_oc = alpha_total * teg->dT_K;
    double P_max_W = (V_oc * V_oc) / (4.0 * teg->R_internal_ohm);
    return P_max_W * 1e6; /* uW */
}

/* ---- RF Harvested Power (Friis transmission equation) ---- */

double rf_harvested_power(double P_tx_dBm, double G_tx_dBi, double G_rx_dBi,
                           double frequency_MHz, double distance_m, double rectifier_efficiency) {
    double lambda = 300.0 / frequency_MHz;
    double P_tx_W = pow(10.0, (P_tx_dBm - 30.0) / 10.0);
    double G_tx_linear = pow(10.0, G_tx_dBi / 10.0);
    double G_rx_linear = pow(10.0, G_rx_dBi / 10.0);
    double path_loss = (lambda / (4.0 * M_PI * distance_m));
    double P_rx_W = P_tx_W * G_tx_linear * G_rx_linear * path_loss * path_loss;
    double P_dc_W = P_rx_W * rectifier_efficiency;
    return P_dc_W * 1e6; /* uW */
}

/* ---- MPPT Controller ---- */

void mppt_init(MPPTController *mppt, MPPTAlgorithm algorithm) {
    assert(mppt != NULL);
    memset(mppt, 0, sizeof(MPPTController));
    mppt->algorithm = algorithm;
    mppt->step_size_mV = 50.0;
    mppt->update_interval_ms = 100.0;
    mppt->k_voc = 0.78; /* Fractional VOC coefficient (typ 0.7-0.8 for PV) */
    mppt->converged = 0;
}

double mppt_step(MPPTController *mppt, double V_measured, double I_measured) {
    assert(mppt != NULL);
    double P_now = V_measured * I_measured;
    mppt->P_harvested_uW = P_now;
    if (mppt->algorithm == MPPT_PERTURB_OBSERVE) {
        static double V_prev = 0.0, P_prev = 0.0;
        if (V_prev > 0.0) {
            if (P_now > P_prev) {
                mppt->V_mpp_estimated += (V_measured > V_prev) ? mppt->step_size_mV / 1000.0 : -mppt->step_size_mV / 1000.0;
            } else {
                mppt->V_mpp_estimated += (V_measured > V_prev) ? -mppt->step_size_mV / 1000.0 : mppt->step_size_mV / 1000.0;
            }
        } else {
            mppt->V_mpp_estimated = V_measured;
        }
        V_prev = V_measured; P_prev = P_now;
        if (fabs((P_now - P_prev) / (P_prev + 1e-9)) < 0.01) mppt->converged = 1;
    } else if (mppt->algorithm == MPPT_FRACTIONAL_VOC) {
        mppt->V_mpp_estimated = mppt->k_voc * V_measured;
        mppt->converged = 1;
    }
    if (mppt->V_mpp_estimated < 0.1) mppt->V_mpp_estimated = 0.1;
    return mppt->V_mpp_estimated;
}

double mppt_get_power(const MPPTController *mppt) { assert(mppt != NULL); return mppt->P_harvested_uW; }

/* ---- Energy Buffer Management ---- */

void energy_buffer_init(EnergyBuffer *buffer, BufferType type, double C_F, double V_rated, double V_min) {
    assert(buffer != NULL); memset(buffer, 0, sizeof(EnergyBuffer));
    buffer->type = type; buffer->C_F = C_F; buffer->V_rated_V = V_rated; buffer->V_min_V = V_min;
    if (type == BUFFER_SUPERCAP || type == BUFFER_CERAMIC_CAP || type == BUFFER_ELEC_CAP) {
        buffer->energy_capacity_J = 0.5 * C_F * V_rated * V_rated;
        buffer->usable_energy_J = 0.5 * C_F * (V_rated*V_rated - V_min*V_min);
    }
}

double energy_buffer_stored(const EnergyBuffer *buffer, double V_current) {
    assert(buffer != NULL);
    if (buffer->type == BUFFER_SUPERCAP || buffer->type == BUFFER_CERAMIC_CAP || buffer->type == BUFFER_ELEC_CAP)
        return 0.5 * buffer->C_F * V_current * V_current;
    return buffer->energy_capacity_J * (V_current / buffer->V_rated_V);
}

double energy_buffer_charge_time(const EnergyBuffer *buffer, double P_harvest_uW) {
    assert(buffer != NULL);
    if (P_harvest_uW <= 0.0) return 1e12;
    return buffer->usable_energy_J / (P_harvest_uW * 1e-6); /* seconds */
}

double size_buffer_for_autonomy(double P_load_uW, double autonomy_hours, double V_rated, double V_min) {
    double E_needed_J = P_load_uW * 1e-6 * autonomy_hours * 3600.0;
    double dV2 = V_rated*V_rated - V_min*V_min;
    if (dV2 <= 0.0) return 1e12;
    return 2.0 * E_needed_J / dV2; /* Farads */
}

/* ---- Energy Neutrality Tracking ---- */

void energy_neutrality_init(EnergyNeutralityState *state, double buffer_max_J, double period_hours) {
    assert(state != NULL); memset(state, 0, sizeof(EnergyNeutralityState));
    state->E_buffer_max_J = buffer_max_J; state->period_hours = period_hours;
}

void energy_neutrality_update(EnergyNeutralityState *state, double E_h, double E_c, double dt_h) {
    assert(state != NULL);
    state->E_harvested_J += E_h; state->E_consumed_J += E_c;
    state->E_stored_J += (E_h - E_c);
    if (state->E_stored_J > state->E_buffer_max_J) state->E_stored_J = state->E_buffer_max_J;
    if (state->E_stored_J < 0.0) state->E_stored_J = 0.0;
    state->deficit_J += (E_c - E_h);
    if (dt_h >= state->period_hours) {
        state->is_energy_neutral = (state->E_harvested_J >= state->E_consumed_J) ? 1 : 0;
        state->E_harvested_J = 0.0; state->E_consumed_J = 0.0;
    }
}

int energy_neutrality_check(const EnergyNeutralityState *state) {
    assert(state != NULL); return state->is_energy_neutral;
}

/* ---- Energy Prediction (Exponential Smoothing) ---- */

void energy_prediction_init(EnergyPrediction *pred, double alpha) {
    assert(pred != NULL); memset(pred, 0, sizeof(EnergyPrediction));
    pred->alpha_smoothing = alpha;
}

void energy_prediction_update(EnergyPrediction *pred, double energy_today_J) {
    assert(pred != NULL);
    if (pred->num_samples == 0) { pred->predicted_energy_J_per_day = energy_today_J; }
    else { pred->predicted_energy_J_per_day = pred->alpha_smoothing * energy_today_J + (1.0 - pred->alpha_smoothing) * pred->predicted_energy_J_per_day; }
    pred->num_samples++;
    pred->variance_J2 = pred->alpha_smoothing * (energy_today_J - pred->predicted_energy_J_per_day) * (energy_today_J - pred->predicted_energy_J_per_day) + (1.0 - pred->alpha_smoothing) * pred->variance_J2;
    pred->confidence_interval_J = 1.96 * sqrt(pred->variance_J2);
}

double energy_prediction_get(const EnergyPrediction *pred) { assert(pred != NULL); return pred->predicted_energy_J_per_day; }

/* ---- Adaptive Duty Cycling (PI controller) ---- */

void adaptive_duty_init(AdaptiveDutyController *adc, double base_duty, double min_duty, double max_duty) {
    assert(adc != NULL); memset(adc, 0, sizeof(AdaptiveDutyController));
    adc->base_duty_cycle_pct = base_duty; adc->min_duty_cycle_pct = min_duty; adc->max_duty_cycle_pct = max_duty;
    adc->current_duty_cycle = base_duty; adc->gain_P = 0.5; adc->gain_I = 0.1;
}

double adaptive_duty_update(AdaptiveDutyController *adc, double E_buf, double E_target, double dt_h) {
    assert(adc != NULL);
    double error = E_buf - E_target;
    adc->integral_error += error * dt_h;
    double correction = adc->gain_P * error + adc->gain_I * adc->integral_error;
    adc->current_duty_cycle = adc->base_duty_cycle_pct - correction;
    if (adc->current_duty_cycle > adc->max_duty_cycle_pct) adc->current_duty_cycle = adc->max_duty_cycle_pct;
    if (adc->current_duty_cycle < adc->min_duty_cycle_pct) adc->current_duty_cycle = adc->min_duty_cycle_pct;
    adc->E_buffer_J = E_buf; adc->E_buffer_target_J = E_target;
    return adc->current_duty_cycle;
}

/* ---- System-Level Design ---- */

void harvesting_system_design(HarvestingSystem *sys, HarvestingSource source, const char *condition,
                               double area_cm2, double P_load_uW, double autonomy_hours) {
    assert(sys != NULL); memset(sys, 0, sizeof(HarvestingSystem));
    sys->source = source;
    sys->harvester = harvester_estimate(source, condition, area_cm2);
    mppt_init(&sys->mppt, MPPT_FRACTIONAL_VOC);
    double C_buf = size_buffer_for_autonomy(P_load_uW, autonomy_hours, 3.0, 1.5);
    energy_buffer_init(&sys->buffer, BUFFER_SUPERCAP, C_buf, 3.0, 1.5);
    energy_prediction_init(&sys->prediction, 0.3);
    energy_neutrality_init(&sys->neutrality, sys->buffer.usable_energy_J, 24.0);
    sys->system_load_uW = P_load_uW;
    sys->autonomy_hours = autonomy_hours;
    sys->is_perpetual = (sys->harvester.P_mpp_uW >= P_load_uW) ? 1 : 0;
}

void harvesting_system_simulate_day(HarvestingSystem *sys, const double *hourly_harvest_uW, const double *hourly_load_uW) {
    assert(sys != NULL && hourly_harvest_uW != NULL && hourly_load_uW != NULL);
    for (int h = 0; h < 24; h++) {
        double Eh = hourly_harvest_uW[h] * 3600.0 * 1e-6; /* uW * s -> J */
        double Ec = hourly_load_uW[h] * 3600.0 * 1e-6;
        energy_neutrality_update(&sys->neutrality, Eh, Ec, 1.0);
    }
    double total_harvest = 0.0;
    for (int h = 0; h < 24; h++) total_harvest += hourly_harvest_uW[h] * 3600.0 * 1e-6;
    energy_prediction_update(&sys->prediction, total_harvest);
    sys->is_perpetual = harvesting_system_is_perpetual(sys);
}

int harvesting_system_is_perpetual(const HarvestingSystem *sys) {
    assert(sys != NULL);
    if (sys->harvester.P_mpp_uW >= sys->system_load_uW) return 1;
    if (sys->neutrality.is_energy_neutral) return 1;
    return 0;
}

void compare_harvesters(const HarvesterParams *options, int num_options, double P_load_uW, double *scores) {
    assert(options != NULL && scores != NULL);
    for (int i = 0; i < num_options; i++) {
        double power_margin = options[i].P_mpp_uW - P_load_uW;
        double size_score = options[i].area_cm2 > 0.0 ? 10.0 / options[i].area_cm2 : 10.0;
        double eff_score = options[i].efficiency_pct;
        scores[i] = power_margin * 0.5 + size_score * 0.3 + eff_score * 0.2;
    }
}
