/**
 * @file battery_life.c
 * @brief Advanced battery life estimation with aging, reliability, and statistical models
 */
#include "battery_life.h"
#include "coin_cell_battery.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <math.h>
#include <float.h>

#define KELVIN_OFFSET 273.15
#define BOLTZMANN_eV 8.617333262145e-5
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* ---- Load Profile Analysis ---- */

double load_profile_avg_current(const LoadProfile *profile) {
    assert(profile != NULL); assert(profile->segments != NULL); assert(profile->num_segments > 0);
    double sum_I_t = 0.0, sum_t = 0.0;
    for (size_t i = 0; i < profile->num_segments; i++) {
        sum_I_t += profile->segments[i].I_load_uA * profile->segments[i].duration_hours;
        sum_t += profile->segments[i].duration_hours;
    }
    return (sum_t > 0.0) ? sum_I_t / sum_t : 0.0;
}

double load_profile_battery_life(const LoadProfile *profile, const CoinCellParams *battery,
                                  double initial_SoC, double V_min) {
    assert(profile != NULL); assert(battery != NULL);
    assert(initial_SoC > 0.0 && initial_SoC <= 1.0); assert(V_min > 0.0);
    double C_rem = battery->C_nominal_mAh * initial_SoC;
    double total_t = 0.0;
    double prof_dur = 0.0;
    for (size_t i = 0; i < profile->num_segments; i++) prof_dur += profile->segments[i].duration_hours;
    if (prof_dur <= 0.0) return 0.0;
    for (int cyc = 0; cyc < 100000 && C_rem > 0.0; cyc++) {
        for (size_t i = 0; i < profile->num_segments; i++) {
            const LoadSegment *seg = &profile->segments[i];
            double I_mA = seg->I_load_uA / 1000.0;
            double V_load = battery->V_nominal_V - I_mA * battery->R_internal_0_ohm;
            if (V_load < V_min) return total_t;
            double dQ = seg->I_load_uA / 1000.0 * seg->duration_hours;
            if (dQ > C_rem) { total_t += seg->duration_hours * (C_rem / dQ); C_rem = 0.0; break; }
            C_rem -= dQ; total_t += seg->duration_hours;
        }
    }
    return total_t;
}

int load_profile_dominant_segment(const LoadProfile *profile) {
    assert(profile != NULL); assert(profile->num_segments > 0);
    int dom = 0; double max_drain = 0.0;
    for (size_t i = 0; i < profile->num_segments; i++) {
        double drain = profile->segments[i].I_load_uA * profile->segments[i].duration_hours;
        if (drain > max_drain) { max_drain = drain; dom = (int)i; }
    }
    return dom;
}

/* ---- Aging Models ---- */

double calendar_aging_loss(const CalendarAgingModel *model, double time_hours, double temperature_C) {
    assert(model != NULL); assert(time_hours >= 0.0);
    if (time_hours <= 0.0) return 0.0;
    double T_K = temperature_C + KELVIN_OFFSET;
    if (T_K <= 0.0) T_K = 273.15;
    double af = exp((model->Ea_eV / BOLTZMANN_eV) * (1.0 / model->T_ref_K - 1.0 / T_K));
    double loss = model->k_cal_sqrt_hr * sqrt(time_hours) * af;
    if (loss > 1.0) loss = 1.0;
    if (loss < 0.0) loss = 0.0;
    return loss;
}

double cycle_aging_loss(const CycleAgingModel *model, uint32_t num_cycles) {
    assert(model != NULL);
    if (num_cycles == 0) return 0.0;
    double loss = 1.0 - exp(-model->k_cycle * num_cycles);
    loss += model->irreversible_pct / 100.0;
    if (loss > 1.0) loss = 1.0;
    if (loss < 0.0) loss = 0.0;
    return loss;
}

double total_aging_loss(const CalendarAgingModel *cal_model, const CycleAgingModel *cycle_model,
                        double time_hours, uint32_t num_cycles, double temperature_C) {
    (void)cycle_model;
    double cal_loss = 0.0, cycle_loss = 0.0;
    if (cal_model != NULL) cal_loss = calendar_aging_loss(cal_model, time_hours, temperature_C);
    if (cycle_model != NULL && num_cycles > 0) cycle_loss = cycle_aging_loss(cycle_model, num_cycles);
    double total = cal_loss + cycle_loss;
    if (total > 1.0) total = 1.0;
    return total;
}

/* ---- Monte Carlo Simulation (Box-Muller + qsort) ---- */

static double rand_gaussian(void) {
    double u1 = (double)rand() / RAND_MAX;
    double u2 = (double)rand() / RAND_MAX;
    if (u1 < 1e-10) u1 = 1e-10;
    return sqrt(-2.0 * log(u1)) * cos(2.0 * M_PI * u2);
}

static int double_cmp(const void *a, const void *b) {
    double da = *(const double*)a, db = *(const double*)b;
    if (da < db) return -1;
    if (da > db) return 1;
    return 0;
}

void monte_carlo_battery_life(const MonteCarloParams *params, StatisticalLife *result) {
    assert(params != NULL); assert(result != NULL); assert(params->num_simulations > 0);
    memset(result, 0, sizeof(StatisticalLife));
    result->num_simulations = params->num_simulations;
    double *lt = (double*)malloc(params->num_simulations * sizeof(double));
    assert(lt != NULL);
    double sum = 0.0, sum2 = 0.0, min_l = 1e30, max_l = 0.0;
    for (int i = 0; i < params->num_simulations; i++) {
        double Cs = params->C_nominal_mAh + params->C_stddev_mAh * rand_gaussian();
        if (Cs < params->C_nominal_mAh * 0.5) Cs = params->C_nominal_mAh * 0.5;
        double Is = params->I_mean_uA + params->I_stddev_uA * rand_gaussian();
        if (Is < 0.1) Is = 0.1;
        double Ts = params->T_mean_C + params->T_stddev_C * rand_gaussian();
        double tf = 1.0;
        if (Ts < 25.0) { tf = 0.90 + (Ts + 20.0) / 45.0 * 0.10; if (tf < 0.6) tf = 0.6; }
        double lh = Cs * tf * 1000.0 / Is;
        lt[i] = lh; sum += lh; sum2 += lh * lh;
        if (lh < min_l) min_l = lh;
        if (lh > max_l) max_l = lh;
    }
    qsort(lt, params->num_simulations, sizeof(double), double_cmp);
    int N = params->num_simulations;
    result->mean_life_hours = sum / N;
    double var = sum2 / N - result->mean_life_hours * result->mean_life_hours;
    result->stddev_life_hours = var > 0.0 ? sqrt(var) : 0.0;
    result->min_life_hours = min_l; result->max_life_hours = max_l;
    result->median_life_hours = lt[N / 2];
    result->percentile_10 = lt[N * 10 / 100];
    result->percentile_90 = lt[N * 90 / 100];
    free(lt);
}

void statistical_battery_life(double Cm, double Cs, double Im, double Is, int Ns, StatisticalLife *r) {
    MonteCarloParams p = {Cm, Cs, Im, Is, 25.0, 5.0, Ns};
    monte_carlo_battery_life(&p, r);
}

/* ---- Weibull Reliability (Lanczos Gamma + MLE fitting) ---- */

static double gamma_func(double x) {
    static const double p[] = { 0.99999999999980993, 676.5203681218851, -1259.1392167224028,
        771.32342877765313, -176.61502916214059, 12.507343278686905, -0.13857109526572012,
        9.9843695780195716e-6, 1.5056327351493116e-7 };
    if (x < 0.5) return M_PI / (sin(M_PI * x) * gamma_func(1.0 - x));
    x -= 1.0; double a = p[0];
    for (int i = 1; i < 9; i++) a += p[i] / (x + i);
    double t = x + 7.5;
    return sqrt(2.0 * M_PI) * pow(t, x + 0.5) * exp(-t) * a;
}

void weibull_reliability(const WeibullParams *params, double time_hours, ReliabilityMetrics *metrics) {
    assert(params != NULL); assert(metrics != NULL);
    double t_eta = time_hours / params->eta;
    double t_eta_b = pow(t_eta, params->beta);
    metrics->reliability = exp(-t_eta_b);
    metrics->failure_rate = time_hours > 0.0 ? (params->beta / params->eta) * pow(t_eta, params->beta - 1.0) : 0.0;
    metrics->MTTF_hours = params->eta * gamma_func(1.0 + 1.0 / params->beta);
    metrics->B10_life_hours = params->eta * pow(-log(0.90), 1.0 / params->beta);
}

void weibull_fit(const double *lifetimes, size_t N, WeibullParams *params) {
    assert(lifetimes != NULL); assert(params != NULL); assert(N >= 3);
    double sum_log = 0.0, sum_log2 = 0.0;
    for (size_t i = 0; i < N; i++) { double lx = log(lifetimes[i]); sum_log += lx; sum_log2 += lx * lx; }
    double mean_log = sum_log / N, var_log = sum_log2 / N - mean_log * mean_log;
    if (var_log <= 0.0) var_log = 0.01;
    double beta = 1.283 / sqrt(var_log);
    if (beta < 0.5) beta = 0.5;
    if (beta > 10.0) beta = 10.0;
    for (int iter = 0; iter < 50; iter++) {
        double sum_xb = 0.0, sum_xb_logx = 0.0;
        for (size_t i = 0; i < N; i++) { double xb = pow(lifetimes[i], beta); sum_xb += xb; sum_xb_logx += xb * log(lifetimes[i]); }
        if (sum_xb <= 0.0) break;
        double fx = 1.0 / beta + mean_log - sum_xb_logx / sum_xb;
        double sum_xb_logx2 = 0.0;
        for (size_t i = 0; i < N; i++) { double xb = pow(lifetimes[i], beta); sum_xb_logx2 += xb * log(lifetimes[i]) * log(lifetimes[i]); }
        double df = -1.0/(beta*beta) - (sum_xb_logx2*sum_xb - sum_xb_logx*sum_xb_logx)/(sum_xb*sum_xb);
        if (fabs(df) < 1e-12) break;
        double bn = beta - fx/df;
        if (bn <= 0.1) bn = 0.1;
        if (bn > 20.0) bn = 20.0;
        if (fabs(bn - beta) < 1e-6) { beta = bn; break; }
        beta = bn;
    }
    double sum_xb = 0.0;
    for (size_t i = 0; i < N; i++) sum_xb += pow(lifetimes[i], beta);
    params->beta = beta; params->eta = pow(sum_xb / N, 1.0 / beta);
}

double weibull_b10_life(const WeibullParams *params) {
    assert(params != NULL);
    return params->eta * pow(-log(0.90), 1.0 / params->beta);
}

/* ---- Arrhenius Acceleration ---- */

double arrhenius_acceleration_factor(double Ea_eV, double T_stress_K, double T_use_K) {
    assert(Ea_eV > 0.0); assert(T_stress_K > 0.0); assert(T_use_K > 0.0);
    return exp((Ea_eV / BOLTZMANN_eV) * (1.0 / T_use_K - 1.0 / T_stress_K));
}

double arrhenius_project_lifetime(double life_at_stress_hours, double Ea_eV, double T_stress_K, double T_use_K) {
    return life_at_stress_hours * arrhenius_acceleration_factor(Ea_eV, T_stress_K, T_use_K);
}

/* ---- Rainflow Cycle Counting (4-point algorithm, ASTM E1049) ---- */

void rainflow_init(RainflowResult *result, size_t max_cycles) {
    assert(result != NULL); assert(max_cycles > 0);
    memset(result, 0, sizeof(RainflowResult));
    result->cycle_depths = (double*)calloc(max_cycles, sizeof(double));
    assert(result->cycle_depths != NULL);
    result->capacity = max_cycles; result->num_cycles = 0;
}

void rainflow_process_point(RainflowResult *result, double value) {
    assert(result != NULL);
    static double prev[4] = {0.0, 0.0, 0.0, 0.0};
    static int prev_count = 0, initialized = 0;
    if (!initialized) { prev[0] = value; prev_count = 1; initialized = 1; return; }
    if (prev_count < 4) {
        if (prev_count == 1) { if (value != prev[0]) prev[prev_count++] = value; }
        else {
            double ld = prev[prev_count-1] - prev[prev_count-2];
            double td = value - prev[prev_count-1];
            if (ld * td < 0) prev[prev_count++] = value;
            else prev[prev_count-1] = value;
        }
    }
    if (prev_count >= 4) {
        double d1 = fabs(prev[1]-prev[0]), d2 = fabs(prev[2]-prev[1]), d3 = fabs(prev[3]-prev[2]);
        if (d2 <= d1 && d2 <= d3) {
            if (result->num_cycles < result->capacity)
                result->cycle_depths[result->num_cycles++] = d2;
            prev[1] = prev[3]; prev_count = 2;
        }
    }
}

void rainflow_finalize(RainflowResult *result) { (void)result; }

double rainflow_damage(const RainflowResult *result, const double *S_N_curve, size_t num_sn_points) {
    assert(result != NULL); assert(S_N_curve != NULL); assert(num_sn_points >= 2 && num_sn_points % 2 == 0);
    double damage = 0.0;
    for (size_t i = 0; i < result->num_cycles; i++) {
        double stress = result->cycle_depths[i], N_f = 1e12;
        for (size_t j = 0; j < num_sn_points - 2; j += 2) {
            double S1=S_N_curve[j], N1=S_N_curve[j+1], S2=S_N_curve[j+2], N2=S_N_curve[j+3];
            if (stress >= S2 && stress <= S1) {
                double logS=log10(stress),logS1=log10(S1),logS2=log10(S2),logN1=log10(N1),logN2=log10(N2);
                N_f = pow(10.0, logN1 + (logS-logS1)/(logS2-logS1)*(logN2-logN1));
                break;
            }
        }
        if (N_f > 0.0) damage += 1.0 / N_f;
    }
    return damage;
}

/* ---- State of Health Estimation ---- */

void estimate_state_of_health(double Cc, double Ci, double Rc, double Ri, StateOfHealth *soh) {
    assert(soh != NULL);
    memset(soh, 0, sizeof(StateOfHealth));
    if (Ci > 0.0) { soh->SoH_capacity_pct = (Cc/Ci)*100.0; if (soh->SoH_capacity_pct > 100.0) soh->SoH_capacity_pct = 100.0;
    if (soh->SoH_capacity_pct < 0.0) soh->SoH_capacity_pct = 0.0; }
    if (Rc > 0.0 && Ri > 0.0) { soh->SoH_resistance_pct = (Ri/Rc)*100.0; if (soh->SoH_resistance_pct > 100.0) soh->SoH_resistance_pct = 100.0;
    if (soh->SoH_resistance_pct < 0.0) soh->SoH_resistance_pct = 0.0; }
    soh->SoH_combined_pct = 0.7*soh->SoH_capacity_pct + 0.3*soh->SoH_resistance_pct;
    soh->confidence = 1.0 - fabs(soh->SoH_capacity_pct - soh->SoH_resistance_pct)/100.0;
    if (soh->confidence < 0.0) soh->confidence = 0.0;
}

/* ---- Remaining Useful Life Prediction (linear regression on SoH trend) ---- */

double predict_remaining_life(const StateOfHealth *soh_history, const double *times_hours, size_t N, double soh_threshold_pct) {
    assert(soh_history != NULL); assert(times_hours != NULL); assert(N >= 2);
    assert(soh_threshold_pct > 0.0 && soh_threshold_pct < 100.0);
    double st=0.0, sS=0.0, st2=0.0, stS=0.0;
    for (size_t i=0; i<N; i++) { double t=times_hours[i], s=soh_history[i].SoH_combined_pct; st+=t; sS+=s; st2+=t*t; stS+=t*s; }
    double denom = N*st2 - st*st;
    if (fabs(denom) < 1e-12) return 0.0;
    double a = (N*stS - st*sS)/denom;
    double b = (sS - a*st)/N;
    if (fabs(a) < 1e-12) return 1e12;
    double t_thresh = (soh_threshold_pct - b)/a;
    double RUL = t_thresh - times_hours[N-1];
    return RUL > 0.0 ? RUL : 0.0;
}

/* ---- Comprehensive Battery Life Simulation (all degradation mechanisms) ---- */

int comprehensive_battery_life(const LoadProfile *profile, double I_constant_uA,
                                const CoinCellParams *battery, double temperature_C,
                                const CalendarAgingModel *cal_model, const CycleAgingModel *cycle_model,
                                double V_min, double *lifetime_hours) {
    (void)cycle_model;
    assert(battery != NULL); assert(lifetime_hours != NULL); assert(V_min > 0.0);
    double C_rem = battery->C_nominal_mAh, time_h = 0.0, dt_h = 1.0, max_t = 876600.0;
    ArrheniusSelfDischarge arr = {0.65, battery->self_discharge_pct_per_month, 298.15};
    double sd_rate = arrhenius_self_discharge_rate(&arr, temperature_C);
    while (C_rem > 0.0 && time_h < max_t) {
        double I_uA = I_constant_uA;
        if (profile != NULL) {
            double pt = fmod(time_h, profile->total_period_hours), ct = 0.0;
            I_uA = profile->segments[profile->num_segments-1].I_load_uA;
            for (size_t i=0; i<profile->num_segments; i++) { ct += profile->segments[i].duration_hours; if (pt < ct) { I_uA = profile->segments[i].I_load_uA; break; } }
        }
        double SoC = C_rem / battery->C_nominal_mAh, V_oc = battery->V_nominal_V;
        if (SoC < 0.2) V_oc = battery->V_cutoff_V + (battery->V_nominal_V - battery->V_cutoff_V)*SoC/0.2;
        if (terminal_voltage_under_load(V_oc, I_uA/1000.0, battery->R_internal_0_ohm) < V_min) { *lifetime_hours = time_h; return 0; }
        double dQ_load = I_uA/1000.0*dt_h;
        double dQ_sd = C_rem*(sd_rate/100.0)*(dt_h/(30.4375*24.0));
        double dQ_aging = 0.0;
        if (cal_model != NULL) { double lb=calendar_aging_loss(cal_model,time_h,temperature_C); double la=calendar_aging_loss(cal_model,time_h+dt_h,temperature_C); if (la>lb) dQ_aging=(la-lb)*battery->C_nominal_mAh; }
        double dQ_tot = dQ_load+dQ_sd+dQ_aging;
        if (dQ_tot > C_rem) { time_h += dt_h*(C_rem/dQ_tot); C_rem=0.0; break; }
        C_rem -= dQ_tot; time_h += dt_h;
    }
    *lifetime_hours = time_h;
    return 0;
}
