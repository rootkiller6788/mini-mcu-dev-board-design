/**
 * @file coin_cell_battery.c
 * @brief Implementation of coin cell battery models and algorithms
 *
 * Implements: Shepherd discharge model, Peukert capacity derating,
 * Arrhenius temperature effects, coulomb counting, EKF SoC estimation,
 * discharge curve interpolation, and battery statistics.
 */

#include "coin_cell_battery.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <float.h>

/* ============================================================================
 * Static Data - Battery parameter database
 * ============================================================================ */

#define KELVIN_OFFSET 273.15
#define BOLTZMANN_eV 8.617333262145e-5

/**
 * @brief Standard parameters for all supported coin cell models
 *
 * Data compiled from manufacturer datasheets:
 * - CR series: Panasonic, Energizer, Renata
 * - LR/SR series: Energizer, Varta
 * - LIR series: generic Li-ion coin cells
 *
 * Values represent typical performance at 25 deg C.
 */
static const CoinCellParams s_coin_cell_db[CELL_COUNT] = {
    /* CELL_CR2032 */  { 225.0, 3.00, 2.00, 15.0, 0.20, 0.20, 3.0, 20.0, 3.2, CHEMISTRY_LITHIUM_MNO2 },
    /* CELL_CR2025 */  { 170.0, 3.00, 2.00, 20.0, 0.20, 0.20, 2.5, 20.0, 2.5, CHEMISTRY_LITHIUM_MNO2 },
    /* CELL_CR2016 */  {  90.0, 3.00, 2.00, 30.0, 0.20, 0.20, 1.6, 20.0, 1.6, CHEMISTRY_LITHIUM_MNO2 },
    /* CELL_CR2450 */  { 620.0, 3.00, 2.00, 10.0, 0.30, 0.15, 6.8, 24.5, 5.0, CHEMISTRY_LITHIUM_MNO2 },
    /* CELL_CR2477 */  { 1000.0,3.00, 2.00,  8.0, 0.30, 0.10,10.5, 24.5, 7.7, CHEMISTRY_LITHIUM_MNO2 },
    /* CELL_CR1220 */  {  40.0, 3.00, 2.00, 40.0, 0.10, 0.30, 0.8, 12.5, 2.0, CHEMISTRY_LITHIUM_MNO2 },
    /* CELL_CR1620 */  {  75.0, 3.00, 2.00, 25.0, 0.15, 0.25, 1.3, 16.0, 2.0, CHEMISTRY_LITHIUM_MNO2 },
    /* CELL_CR1632 */  { 140.0, 3.00, 2.00, 20.0, 0.20, 0.20, 2.0, 16.0, 3.2, CHEMISTRY_LITHIUM_MNO2 },
    /* CELL_LR44 */    { 150.0, 1.50, 0.90, 5.0,  0.50, 0.50, 2.5, 11.6, 5.4, CHEMISTRY_ALKALINE },
    /* CELL_SR44 */    { 200.0, 1.55, 1.00, 3.0,  0.30, 0.10, 2.5, 11.6, 5.4, CHEMISTRY_SILVER_OXIDE },
    /* CELL_LIR2032 */ {  45.0, 3.70, 3.00, 25.0, 5.00, 2.00, 3.0, 20.0, 3.2, CHEMISTRY_LITHIUM_ION },
};

static const char *s_coin_cell_names[CELL_COUNT] = {
    "CR2032", "CR2025", "CR2016", "CR2450", "CR2477",
    "CR1220", "CR1620", "CR1632", "LR44", "SR44", "LIR2032"
};

static const char *s_chemistry_names[CHEMISTRY_COUNT] = {
    "Alkaline (MnO2/Zn)",
    "Lithium Manganese Dioxide (Li/MnO2)",
    "Silver Oxide (Ag2O/Zn)",
    "Zinc Air (Zn/O2)",
    "Lithium Carbon Monofluoride (Li/CFx)",
    "Lithium-Ion Rechargeable"
};

/* ============================================================================
 * Standard CR2032 discharge curve lookup table
 *
 * Open-circuit voltage vs State of Charge for Li/MnO2 chemistry.
 * Derived from manufacturer discharge curves at 25 deg C.
 * Key observation: Li/MnO2 has a relatively flat discharge curve,
 * staying above 2.8V for ~80% of capacity, then dropping sharply.
 * ============================================================================ */

static const double s_cr2032_SoC_lut[] = {
    0.00, 0.02, 0.05, 0.10, 0.20, 0.30, 0.40,
    0.50, 0.60, 0.70, 0.80, 0.85, 0.90, 0.95, 1.00
};

static const double s_cr2032_OCV_lut[] = {
    2.00, 2.25, 2.50, 2.65, 2.78, 2.83, 2.87,
    2.90, 2.93, 2.95, 2.97, 2.98, 2.99, 3.00, 3.00
};

static const double s_cr2032_R_lut[] = {
    60.0, 50.0, 40.0, 32.0, 25.0, 22.0, 20.0,
    18.0, 17.0, 16.0, 15.5, 15.2, 15.0, 15.0, 15.0
};

#define CR2032_LUT_SIZE (sizeof(s_cr2032_SoC_lut) / sizeof(s_cr2032_SoC_lut[0]))

/* ============================================================================
 * Lookup Functions
 * ============================================================================ */

const CoinCellParams* coin_cell_get_params(CoinCellModel model)
{
    if (model >= CELL_COUNT) {
        return &s_coin_cell_db[CELL_CR2032]; /* Default fallback */
    }
    return &s_coin_cell_db[model];
}

const char* coin_cell_get_name(CoinCellModel model)
{
    if (model >= CELL_COUNT) return "UNKNOWN";
    return s_coin_cell_names[model];
}

const char* battery_chemistry_get_name(BatteryChemistry chem)
{
    if (chem >= CHEMISTRY_COUNT) return "UNKNOWN";
    return s_chemistry_names[chem];
}

/* ============================================================================
 * Shepherd Discharge Model
 *
 * Reference: Shepherd, C.M., "Design of Primary and Secondary Cells",
 *            Journal of the Electrochemical Society, Vol. 112, No. 7, 1965
 *
 * The Shepherd model captures three distinct regions of a discharge curve:
 *   1. Exponential drop at the start (A*exp(-B*it) term) - surface charge
 *   2. Flat operating region (E0 - R*i) - nominal operation
 *   3. Rapid drop near depletion (K*(Q/(Q-it))*it term) - polarization rise
 *
 * Parameter derivation methodology:
 *   E0 ~ V_nominal + 0.1V (slightly above nominal for full-charge voltage)
 *   K  ~ R_internal * Q (polarization scales with resistance and capacity)
 *   A  ~ 0.05-0.15V (exponential amplitude = initial voltage drop)
 *   B  ~ 10-50 / Q (inverse time constant for exponential recovery)
 * ============================================================================ */

void shepherd_model_init(const CoinCellParams *params, ShepherdModel *model)
{
    assert(params != NULL);
    assert(model != NULL);

    /* E0: full-charge voltage, typically V_nominal + small offset */
    model->E0_V = params->V_nominal_V + 0.15;

    /* K: polarization constant, proportional to internal losses */
    /* K = R_internal * Q (in units V/Ah * Ah = V, correct) */
    double Q_mAh = params->C_nominal_mAh;
    double R_ohm = params->R_internal_0_ohm;
    model->K_V_per_Ah = R_ohm * (Q_mAh / 1000.0) * 0.8;

    /* Q: maximum capacity in Ah */
    model->Q_Ah = Q_mAh / 1000.0;

    /* R: internal resistance */
    model->R_ohm = R_ohm;

    /* A: exponential amplitude = initial voltage drop magnitude */
    /* Li/MnO2 has very small exponential zone (~0.02V) */
    /* Alkaline has larger exponential zone (~0.10V) */
    if (params->chemistry == CHEMISTRY_ALKALINE) {
        model->A_V = 0.12;
    } else if (params->chemistry == CHEMISTRY_SILVER_OXIDE) {
        model->A_V = 0.05;
    } else {
        model->A_V = 0.03; /* Lithium chemistries */
    }

    /* B: inverse time constant for exponential zone */
    /* Larger B = faster recovery from exponential zone */
    model->B_inv_Ah = 30.0 / model->Q_Ah;

    model->V_cutoff_V = params->V_cutoff_V;
}

double shepherd_voltage(const ShepherdModel *model,
                        double extracted_Ah, double current_A)
{
    assert(model != NULL);

    if (extracted_Ah >= model->Q_Ah) {
        return model->V_cutoff_V;
    }
    if (extracted_Ah < 0.0) {
        extracted_Ah = 0.0;
    }

    double Q = model->Q_Ah;
    double it = extracted_Ah;
    double i  = current_A;

    /* V = E0 - K*(Q/(Q-it))*it - R*i + A*exp(-B*it) */
    double V = model->E0_V
             - model->K_V_per_Ah * (Q / (Q - it)) * it
             - model->R_ohm * i
             + model->A_V * exp(-model->B_inv_Ah * it);

    /* Clamp to physically plausible range */
    if (V > model->E0_V + 0.1) V = model->E0_V + 0.1;
    if (V < model->V_cutoff_V) V = model->V_cutoff_V;

    return V;
}

/* ============================================================================
 * Peukert's Law
 *
 * Peukert, W., "Uber die Abhangigkeit der Kapazitat von der
 * Entladestromstarke bei Bleiakkumulatoren",
 * Elektrotechnische Zeitschrift, Vol. 18, pp. 287-288, 1897.
 *
 * The Peukert equation describes how available capacity decreases
 * as discharge current increases:
 *
 *   I^k * t = constant (Peukert capacity)
 *
 * Where:
 *   I = discharge current
 *   t = discharge time
 *   k = Peukert constant (k >= 1, k=1 for ideal battery)
 *
 * Effective capacity at current I:
 *   C_eff(I) = C_nom * (I_std / I)^(k-1)
 *
 * For Li/MnO2 coin cells, k is very close to 1.0 (1.00-1.02),
 * meaning they maintain capacity well even at higher discharge rates.
 * For alkaline cells, k can be 1.1-1.2, showing significant rate sensitivity.
 * ============================================================================ */

double peukert_capacity(double C_nominal_mAh, double I_std_mA,
                        double I_actual_mA, double k)
{
    assert(C_nominal_mAh > 0.0);
    assert(I_std_mA > 0.0);
    assert(I_actual_mA > 0.0);
    assert(k >= 1.0);

    /* C_eff = C_nom * (I_std / I_actual)^(k - 1) */
    double ratio = I_std_mA / I_actual_mA;
    double exponent = k - 1.0;

    if (exponent < 1e-9) {
        return C_nominal_mAh; /* Ideal battery, no rate effect */
    }

    return C_nominal_mAh * pow(ratio, exponent);
}

double peukert_discharge_time(double C_nominal_mAh, double I_std_mA,
                               double I_actual_mA, double k)
{
    assert(C_nominal_mAh > 0.0);
    assert(I_std_mA > 0.0);
    assert(I_actual_mA > 0.0);
    assert(k >= 1.0);

    /* t = C_nom * I_std^(k-1) / I_actual^k */
    double C_eff = peukert_capacity(C_nominal_mAh, I_std_mA, I_actual_mA, k);
    return C_eff / I_actual_mA; /* hours */
}

/* ============================================================================
 * Internal Resistance Modeling
 *
 * Internal resistance of a coin cell has two primary dependencies:
 *   1. State of Charge: R increases as battery depletes
 *   2. Temperature: R increases at low temperatures (Arrhenius)
 *
 * SoC dependence model:
 *   R(SoC) = R_0 * (1 + alpha * (1 - SoC))
 *   where alpha = 0.5-2.0 (higher for alkaline, lower for lithium)
 *
 * Near depletion (SoC < 0.1), resistance rises super-linearly:
 *   R(SoC) = R_0 * (1 + alpha*(1-SoC) + beta*(1-SoC)^2)
 * ============================================================================ */

double internal_resistance_vs_soc(double R_initial, double SoC)
{
    assert(R_initial > 0.0);
    assert(SoC >= 0.0 && SoC <= 1.0);

    double alpha = 1.5;
    double depletion = 1.0 - SoC;

    /* Quadratic model for more accuracy near depletion */
    double factor = 1.0 + alpha * depletion;

    /* Add rapid rise below 10% SoC */
    if (SoC < 0.10) {
        double beta = 15.0;
        factor += beta * depletion * depletion;
    }

    return R_initial * factor;
}

double internal_resistance_vs_temp(double R_25C, double temperature_C,
                                    double Ea_eV)
{
    assert(R_25C > 0.0);

    double T_K = temperature_C + KELVIN_OFFSET;
    double T_ref = 298.15;  /* 25C in Kelvin */

    if (T_K <= 0.0) return R_25C * 5.0; /* Very cold - extreme resistance */

    /* Arrhenius: R(T) = R_25C * exp((Ea/kB) * (1/T - 1/T_ref)) */
    double exponent = (Ea_eV / BOLTZMANN_eV) * (1.0 / T_K - 1.0 / T_ref);
    double factor = exp(exponent);

    /* Clamp to physically plausible range */
    if (factor < 0.5) factor = 0.5;
    if (factor > 5.0) factor = 5.0;

    return R_25C * factor;
}

/* ============================================================================
 * Self-Discharge Modeling
 *
 * Self-discharge in coin cells occurs through:
 *   - Side reactions at electrode/electrolyte interface
 *   - Impurity-driven internal short circuits
 *   - Seal leakage (very slow for hermetic coin cells)
 *
 * For Li/MnO2 (CR series):
 *   - Self-discharge ~0.2% per month at 25C
 *   - Shelf life > 10 years (to 90% capacity)
 *   - Low self-discharge is a key advantage over other chemistries
 *
 * For Alkaline (LR series):
 *   - Self-discharge ~0.5% per month at 25C
 *   - Shelf life ~5 years
 *   - Higher rate due to zinc corrosion
 *
 * Temperature accelerates self-discharge via Arrhenius kinetics.
 * ============================================================================ */

double self_discharge_remaining(double initial_capacity_mAh,
                                double self_discharge_rate_pct_per_month,
                                double time_months)
{
    assert(initial_capacity_mAh > 0.0);

    if (time_months <= 0.0) return initial_capacity_mAh;

    double rate = self_discharge_rate_pct_per_month / 100.0;

    /* Compound decay: C(t) = C0 * (1 - rate)^t */
    return initial_capacity_mAh * pow(1.0 - rate, time_months);
}

double arrhenius_self_discharge_rate(const ArrheniusSelfDischarge *model,
                                     double temperature_C)
{
    assert(model != NULL);

    double T_K = temperature_C + KELVIN_OFFSET;
    if (T_K <= 0.0) T_K = 273.15;

    /* rate(T) = rate_ref * exp((Ea/kB) * (1/T_ref - 1/T)) */
    double exponent = (model->Ea_eV / BOLTZMANN_eV)
                    * (1.0 / model->T_ref_K - 1.0 / T_K);

    return model->rate_ref_per_month * exp(exponent);
}

/* ============================================================================
 * Voltage Under Load (Ohm's Law for battery terminals)
 *
 * The simplest and most fundamental battery model:
 *   V_terminal = V_oc - I_load * R_internal
 *
 * This is Ohm's law applied to the battery's internal resistance.
 * The IR drop is the voltage lost inside the battery due to its
 * internal resistance. This is NOT energy stored/recovered -
 * it is pure Joule heating (I^2*R) inside the cell.
 * ============================================================================ */

double terminal_voltage_under_load(double V_open_circuit,
                                   double I_load_mA, double R_internal_ohm)
{
    assert(R_internal_ohm >= 0.0);

    /* V_terminal = V_oc - I * R */
    /* Convert mA to A for ohm's law */
    double V_drop = (I_load_mA / 1000.0) * R_internal_ohm;
    double V_term = V_open_circuit - V_drop;

    return V_term > 0.0 ? V_term : 0.0;
}

double max_current_at_cutoff(double V_open_circuit, double V_cutoff,
                              double R_internal_ohm)
{
    assert(R_internal_ohm > 0.0);
    assert(V_open_circuit > V_cutoff);

    /* I_max = (V_oc - V_cutoff) / R */
    double I_max_A = (V_open_circuit - V_cutoff) / R_internal_ohm;
    return I_max_A * 1000.0; /* Convert to mA */
}

/* ============================================================================
 * Coulomb Counting SoC Estimation
 *
 * Coulomb counting is the simplest SoC estimation method:
 *   SoC(t) = SoC(0) - (1/Q_nominal) * integral(I dt)
 *
 * Advantages:
 *   - Simple, low computational cost
 *   - Works with any battery chemistry
 *   - Good accuracy if initial SoC is known and current is measured accurately
 *
 * Disadvantages:
 *   - Accumulates integration error over time
 *   - Requires accurate current sensing
 *   - Doesn't account for self-discharge (unless modeled separately)
 *   - Needs periodic recalibration (full charge/discharge)
 *
 * For coin cell applications, coulomb counting is often adequate because:
 *   - Current levels are low and stable
 *   - Complete discharge is acceptable for primary cells
 *   - Rechargeable coin cells have small capacity, frequent full cycles
 * ============================================================================ */

void coulomb_counter_init(CoulombCounter *cc, double initial_capacity_mAh)
{
    assert(cc != NULL);
    assert(initial_capacity_mAh > 0.0);

    cc->initial_capacity_mAh = initial_capacity_mAh;
    cc->accumulated_charge_mAh = 0.0;
    cc->last_current_mA = 0.0;
    cc->last_timestamp_s = 0.0;
    cc->SoC_estimated = 1.0;
    cc->SoC_error_bound = 0.02; /* Initial uncertainty ~2% */
}

void coulomb_counter_update(CoulombCounter *cc, double current_mA, double dt_s)
{
    assert(cc != NULL);
    assert(dt_s >= 0.0);

    if (dt_s <= 0.0) return;

    /* Q_accumulated += I * dt (convert seconds to hours) */
    double dQ_mAh = current_mA * (dt_s / 3600.0);
    cc->accumulated_charge_mAh += dQ_mAh;
    cc->last_current_mA = current_mA;
    cc->last_timestamp_s += dt_s;

    /* SoC = 1 - Q_accumulated / Q_initial */
    cc->SoC_estimated = 1.0 - cc->accumulated_charge_mAh
                        / cc->initial_capacity_mAh;

    /* Clamp to [0, 1] */
    if (cc->SoC_estimated > 1.0) cc->SoC_estimated = 1.0;
    if (cc->SoC_estimated < 0.0) cc->SoC_estimated = 0.0;

    /* Error bound grows with accumulated charge (integration drift) */
    cc->SoC_error_bound = 0.02 + 0.001 * (cc->accumulated_charge_mAh
                         / cc->initial_capacity_mAh);
}

void coulomb_counter_reset(CoulombCounter *cc, double new_capacity_mAh)
{
    assert(cc != NULL);
    coulomb_counter_init(cc, new_capacity_mAh);
}

double coulomb_counter_get_soc(const CoulombCounter *cc)
{
    assert(cc != NULL);
    return cc->SoC_estimated;
}

/* ============================================================================
 * Extended Kalman Filter for SoC Estimation
 *
 * The EKF combines coulomb counting (process model) with voltage
 * measurement (measurement model) to produce an optimal SoC estimate.
 *
 * State: x = [SoC, R]^T
 *
 * Process model (time update):
 *   SoC_{k+1|k} = SoC_k - (dt/Q) * i_k
 *   R_{k+1|k} = R_k  (assumed slowly varying)
 *
 * Measurement model:
 *   V_k = OCV(SoC_k) - i_k * R_k
 *
 * The EKF linearizes the measurement model using the OCV derivative.
 * This handles the nonlinear OCV-SoC relationship.
 *
 * Reference: Plett, "Extended Kalman filtering for battery management
 *            systems of LiPB-based HEV battery packs", J. Power Sources, 2004
 * ============================================================================ */

void ekf_soc_init(EKF_SoCEstimator *ekf, double Q_Ah, double R_initial,
                  double dt_s)
{
    assert(ekf != NULL);
    assert(Q_Ah > 0.0);
    assert(R_initial > 0.0);

    ekf->x_SoC = 1.0;      /* Assume fully charged initially */
    ekf->x_R_ohm = R_initial;

    /* Initial covariance: moderate uncertainty */
    ekf->P_00 = 0.01;      /* SoC variance = (10%)^2 */
    ekf->P_01 = 0.0;
    ekf->P_10 = 0.0;
    ekf->P_11 = 0.01;      /* R variance = (10% of R_initial)^2 */

    /* Process noise */
    ekf->Q_soc = 1e-6;     /* Small SoC process noise */
    ekf->Q_R = 1e-4;       /* Small R process noise */

    /* Measurement noise */
    ekf->R_meas = 0.01;    /* Voltage measurement noise (0.1V)^2 */

    ekf->Q_nominal_Ah = Q_Ah;
    ekf->dt_s = dt_s;
}

void ekf_soc_predict(EKF_SoCEstimator *ekf, double current_A)
{
    assert(ekf != NULL);

    double dt = ekf->dt_s;
    double Q  = ekf->Q_nominal_Ah;

    /* State prediction */
    /* SoC_{k+1} = SoC_k - (dt/Q) * i_k (convert dt from seconds to hours) */
    ekf->x_SoC = ekf->x_SoC - (dt / 3600.0) * current_A / Q;

    /* Clamp SoC */
    if (ekf->x_SoC > 1.0) ekf->x_SoC = 1.0;
    if (ekf->x_SoC < 0.0) ekf->x_SoC = 0.0;

    /* R stays the same (random walk with small noise) */

    /* Covariance prediction: P = F*P*F' + Q */
    /* F = [[1, 0], [0, 1]] (identity, linear process model) */
    /* So P prediction simplifies to P = P + Q */
    ekf->P_00 += ekf->Q_soc;
    ekf->P_11 += ekf->Q_R;
    /* Off-diagonals unchanged */
}

void ekf_soc_update(EKF_SoCEstimator *ekf, double V_measured,
                    double (*OCV_function)(double SoC), double current_A)
{
    assert(ekf != NULL);
    assert(OCV_function != NULL);

    double SoC = ekf->x_SoC;
    double R   = ekf->x_R_ohm;

    /* Predicted measurement: V_pred = OCV(SoC) - I * R */
    double OCV = OCV_function(SoC);
    double V_pred = OCV - current_A * R;

    /* Innovation: y = z - h(x) */
    double innovation = V_measured - V_pred;

    /* Measurement Jacobian H = [dOCV/dSoC, -I] */
    /* Approximate dOCV/dSoC via central difference */
    double delta = 0.001;
    double SoC_plus  = SoC + delta;
    double SoC_minus = SoC - delta;
    if (SoC_plus  > 1.0) SoC_plus  = 1.0;
    if (SoC_minus < 0.0) SoC_minus = 0.0;
    double dOCV_dSoC = (OCV_function(SoC_plus) - OCV_function(SoC_minus))
                       / (SoC_plus - SoC_minus);

    double H_SoC = dOCV_dSoC;
    double H_R   = -current_A;

    /* Innovation covariance: S = H*P*H' + R_meas */
    double S = H_SoC * H_SoC * ekf->P_00
             + 2.0 * H_SoC * H_R * ekf->P_01
             + H_R * H_R * ekf->P_11
             + ekf->R_meas;

    if (S <= 0.0) return; /* Degenerate case */

    /* Kalman gain: K = P*H' / S */
    double K_SoC = (ekf->P_00 * H_SoC + ekf->P_01 * H_R) / S;
    double K_R   = (ekf->P_10 * H_SoC + ekf->P_11 * H_R) / S;

    /* State update: x = x + K * innovation */
    ekf->x_SoC += K_SoC * innovation;
    ekf->x_R_ohm += K_R * innovation;

    /* Clamp states */
    if (ekf->x_SoC > 1.0) ekf->x_SoC = 1.0;
    if (ekf->x_SoC < 0.0) ekf->x_SoC = 0.0;
    if (ekf->x_R_ohm < 0.5 * ekf->x_R_ohm && K_R < 0)
        ekf->x_R_ohm *= 0.5; /* Don't let R go too low */

    /* Covariance update: P = (I - K*H) * P */
    double P00_old = ekf->P_00;
    double P01_old = ekf->P_01;
    double P10_old = ekf->P_10;
    double P11_old = ekf->P_11;

    ekf->P_00 = (1.0 - K_SoC * H_SoC) * P00_old - K_SoC * H_R * P10_old;
    ekf->P_01 = (1.0 - K_SoC * H_SoC) * P01_old - K_SoC * H_R * P11_old;
    ekf->P_10 = -K_R * H_SoC * P00_old + (1.0 - K_R * H_R) * P10_old;
    ekf->P_11 = -K_R * H_SoC * P01_old + (1.0 - K_R * H_R) * P11_old;
}

double ekf_soc_get_soc(const EKF_SoCEstimator *ekf)
{
    assert(ekf != NULL);
    return ekf->x_SoC;
}

double ekf_soc_get_uncertainty(const EKF_SoCEstimator *ekf)
{
    assert(ekf != NULL);
    /* Return standard deviation = sqrt(variance) */
    if (ekf->P_00 < 0.0) return 1.0;
    return sqrt(ekf->P_00);
}

/* ============================================================================
 * Discharge LUT Interpolation
 *
 * Battery OCV-SoC relationship is stored as lookup tables from
 * manufacturer data. Linear interpolation provides smooth estimates
 * between measured points.
 *
 * For CR2032, the OCV-SoC curve is relatively flat (2.8-3.0V for
 * 20%-100% SoC), making voltage-based SoC estimation challenging
 * but very predictable for run-time estimation.
 * ============================================================================ */

InterpResult discharge_lut_interpolate(const DischargeLUT *lut, double SoC_query)
{
    InterpResult result = {0.0, 0};

    assert(lut != NULL);
    assert(lut->num_points >= 2);
    assert(lut->SoC_points != NULL);
    assert(lut->V_OCV_points != NULL);

    /* Clamp query to valid range */
    if (SoC_query <= lut->SoC_points[0]) {
        result.value = lut->V_OCV_points[0];
        result.valid = 1;
        return result;
    }
    if (SoC_query >= lut->SoC_points[lut->num_points - 1]) {
        result.value = lut->V_OCV_points[lut->num_points - 1];
        result.valid = 1;
        return result;
    }

    /* Binary search for interval */
    size_t lo = 0, hi = lut->num_points - 1;
    while (hi - lo > 1) {
        size_t mid = (lo + hi) / 2;
        if (lut->SoC_points[mid] <= SoC_query) {
            lo = mid;
        } else {
            hi = mid;
        }
    }

    /* Linear interpolation between points lo and hi */
    double x0 = lut->SoC_points[lo];
    double x1 = lut->SoC_points[hi];
    double y0 = lut->V_OCV_points[lo];
    double y1 = lut->V_OCV_points[hi];

    double t = (SoC_query - x0) / (x1 - x0);
    result.value = y0 + t * (y1 - y0);
    result.valid = 1;

    return result;
}

double discharge_lut_energy(const DischargeLUT *lut,
                            double SoC_start, double SoC_end,
                            double capacity_Ah)
{
    assert(lut != NULL);
    assert(capacity_Ah > 0.0);

    /* Ensure SoC_start > SoC_end (discharging) */
    if (SoC_start < SoC_end) {
        double tmp = SoC_start;
        SoC_start = SoC_end;
        SoC_end = tmp;
    }

    /* Trapezoidal integration of V(SoC) * capacity * dSoC */
    /* E = Q * integral_{SoC_end}^{SoC_start} V(s) ds */

    int num_steps = 100;
    double dSoC = (SoC_start - SoC_end) / num_steps;
    double energy_Wh = 0.0;

    for (int i = 0; i < num_steps; i++) {
        double s0 = SoC_start - i * dSoC;
        double s1 = s0 - dSoC;

        InterpResult v0 = discharge_lut_interpolate(lut, s0);
        InterpResult v1 = discharge_lut_interpolate(lut, s1);

        if (!v0.valid || !v1.valid) continue;

        /* Trapezoid area: (V0 + V1)/2 * dSoC */
        energy_Wh += (v0.value + v1.value) / 2.0 * dSoC * capacity_Ah;
    }

    return energy_Wh;
}

/* ============================================================================
 * Battery Statistics
 * ============================================================================ */

void battery_compute_stats(const double *voltages, const double *currents_mA,
                           const double *times_s, size_t N, BatteryStats *stats)
{
    assert(voltages != NULL);
    assert(currents_mA != NULL);
    assert(times_s != NULL);
    assert(stats != NULL);
    assert(N >= 1);

    /* Initialize */
    memset(stats, 0, sizeof(BatteryStats));
    stats->min_voltage_V = DBL_MAX;
    stats->max_voltage_V = -DBL_MAX;

    double sum_V = 0.0, sum_V2 = 0.0;

    for (size_t i = 0; i < N; i++) {
        double V = voltages[i];
        double I = currents_mA[i];

        /* Min/max voltage */
        if (V < stats->min_voltage_V) stats->min_voltage_V = V;
        if (V > stats->max_voltage_V) stats->max_voltage_V = V;

        sum_V += V;
        sum_V2 += V * V;

        /* Capacity integration (mAh): sum I * dt/3600 */
        if (i > 0) {
            double dt_s = times_s[i] - times_s[i-1];
            if (dt_s > 0) {
                double avg_I = (I + currents_mA[i-1]) / 2.0;
                stats->capacity_used_mAh += avg_I * (dt_s / 3600.0);
                double avg_V = (V + voltages[i-1]) / 2.0;
                stats->energy_delivered_mWh += avg_V * avg_I * (dt_s / 3600.0);
            }
        }
    }

    /* Mean and stddev */
    double mean_V = sum_V / N;
    double variance = sum_V2 / N - mean_V * mean_V;
    if (variance < 0.0) variance = 0.0;

    /* Lifetime estimate */
    if (N >= 2) {
        stats->mean_lifetime_hours = times_s[N-1] - times_s[0];
    }
    stats->stddev_lifetime_hours = sqrt(variance);
}

/* ============================================================================
 * Shelf Life Estimation
 * ============================================================================ */

double estimate_shelf_life(const CoinCellParams *params,
                           double temperature_C, double min_usable_capacity_pct)
{
    assert(params != NULL);
    assert(min_usable_capacity_pct > 0.0 && min_usable_capacity_pct <= 100.0);

    /* Solve: C0 * (1 - rate/100)^t = C0 * min_pct/100 */
    /* => t = log(min_pct/100) / log(1 - rate/100) */

    double rate = params->self_discharge_pct_per_month;

    /* Temperature adjustment using Arrhenius */
    ArrheniusSelfDischarge arr = {0.65, rate, 298.15};
    double rate_adj = arrhenius_self_discharge_rate(&arr, temperature_C);

    double target_frac = min_usable_capacity_pct / 100.0;
    double decay_factor = 1.0 - rate_adj / 100.0;

    if (decay_factor <= 0.0 || decay_factor >= 1.0) {
        return 0.0; /* Instant decay or no decay */
    }

    return log(target_frac) / log(decay_factor);
}

/* ============================================================================
 * Pulse Capability Check
 *
 * Coin cells have limited pulse current capability due to:
 *   1. IR drop across internal resistance
 *   2. Electrode polarization under high current
 *   3. Ion diffusion limitations in the electrolyte
 *
 * A CR2032 can typically deliver 10-20mA pulses for short durations
 * (<100ms) without excessive voltage sag. Beyond ~30mA, voltage
 * drops below 2.0V almost instantly.
 *
 * This function models both the instantaneous IR drop and the
 * time-dependent polarization effect.
 * ============================================================================ */

int battery_can_deliver_pulse(const BatteryState *state,
                              double pulse_current_mA, double pulse_duration_ms,
                              double V_min_allowable)
{
    assert(state != NULL);

    /* Instantaneous IR drop */
    double V_under_load = terminal_voltage_under_load(
        state->V_terminal_V, pulse_current_mA, state->R_internal_ohm);

    /* Add polarization sag for longer pulses */
    /* Polarization adds ~10-20% extra sag for pulses > 10ms */
    double polarization_factor = 1.0;
    if (pulse_duration_ms > 10.0) {
        /* Time-dependent polarization grows with sqrt(time) */
        polarization_factor = 1.0 + 0.05 * sqrt(pulse_duration_ms / 10.0);
    }

    double V_sag = state->V_terminal_V - V_under_load;
    V_under_load -= V_sag * (polarization_factor - 1.0);

    return (V_under_load >= V_min_allowable) ? 1 : 0;
}

/* ============================================================================
 * CR2032 OCV function for EKF use
 * ============================================================================ */

/**
 * @brief OCV as function of SoC for CR2032 (linear interpolation in LUT)
 *
 * This function is designed to be passed as a callback to ekf_soc_update().
 */
double cr2032_ocv_function(double SoC)
{
    DischargeLUT lut = {s_cr2032_SoC_lut, s_cr2032_OCV_lut,
                        s_cr2032_R_lut, CR2032_LUT_SIZE};
    InterpResult r = discharge_lut_interpolate(&lut, SoC);
    return r.valid ? r.value : 3.0;
}