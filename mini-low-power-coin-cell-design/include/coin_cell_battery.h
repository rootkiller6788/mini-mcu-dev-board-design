/**
 * @file coin_cell_battery.h
 * @brief Coin cell battery models, discharge characteristics, and electrochemistry
 *
 * Knowledge Coverage: L1 Definitions, L2 Core Concepts, L3 Math Structures, L4 Laws
 *
 * References:
 * - Linden & Reddy, "Handbook of Batteries", 4th Ed, McGraw-Hill, 2011
 * - Peukert, W., "Uber die Abhangigkeit der Kapazitat von der Entladestromstarke", 1897
 * - Shepherd, C.M., "Design of Primary and Secondary Cells", J. Electrochem Soc, 1965
 *
 * Course Alignment:
 * - MIT 6.630 EM Waves -> Electrochemical potential, ionic transport
 * - Berkeley EE105 Analog -> Battery internal resistance modeling
 * - ETH 227-0455 EM -> Energy storage principles
 */

#ifndef COIN_CELL_BATTERY_H
#define COIN_CELL_BATTERY_H

#include <stddef.h>
#include <stdint.h>
#include <math.h>

/* ============================================================================
 * L1 Definitions - Battery types, chemistry, specifications
 * ============================================================================ */

/** Coin cell chemistry types */
typedef enum {
    CHEMISTRY_ALKALINE     = 0,  /* MnO2/Zn, 1.5V nominal (LR series) */
    CHEMISTRY_LITHIUM_MNO2 = 1,  /* Li/MnO2, 3.0V nominal (CR series) */
    CHEMISTRY_SILVER_OXIDE = 2,  /* Ag2O/Zn, 1.55V nominal (SR series) */
    CHEMISTRY_ZINC_AIR     = 3,  /* Zn/O2, 1.4V nominal (PR series) */
    CHEMISTRY_LITHIUM_CFX  = 4,  /* Li/CFx, 3.0V nominal (BR series) */
    CHEMISTRY_LITHIUM_ION  = 5,  /* Li-ion coin (LIR series), 3.7V nominal */
    CHEMISTRY_COUNT        = 6
} BatteryChemistry;

/** Standard coin cell model designations */
typedef enum {
    CELL_CR2032 = 0,  /* 20mm dia, 3.2mm thick, ~225 mAh, Li/MnO2 */
    CELL_CR2025 = 1,  /* 20mm dia, 2.5mm thick, ~170 mAh, Li/MnO2 */
    CELL_CR2016 = 2,  /* 20mm dia, 1.6mm thick, ~90 mAh, Li/MnO2 */
    CELL_CR2450 = 3,  /* 24mm dia, 5.0mm thick, ~620 mAh, Li/MnO2 */
    CELL_CR2477 = 4,  /* 24mm dia, 7.7mm thick, ~1000 mAh, Li/MnO2 */
    CELL_CR1220 = 5,  /* 12mm dia, 2.0mm thick, ~40 mAh, Li/MnO2 */
    CELL_CR1620 = 6,  /* 16mm dia, 2.0mm thick, ~75 mAh, Li/MnO2 */
    CELL_CR1632 = 7,  /* 16mm dia, 3.2mm thick, ~140 mAh, Li/MnO2 */
    CELL_LR44  = 8,  /* 11.6mm dia, 5.4mm thick, ~150 mAh, Alkaline */
    CELL_SR44  = 9,  /* 11.6mm dia, 5.4mm thick, ~200 mAh, Silver Oxide */
    CELL_LIR2032 = 10, /* 20mm dia, 3.2mm thick, ~45 mAh, Li-ion rechargeable */
    CELL_COUNT = 11
} CoinCellModel;

/**
 * @brief Nominal parameters for a coin cell model
 */
typedef struct {
    double    C_nominal_mAh;
    double    V_nominal_V;
    double    V_cutoff_V;
    double    R_internal_0_ohm;
    double    I_std_mA;
    double    self_discharge_pct_per_month;
    double    mass_g;
    double    diameter_mm;
    double    height_mm;
    BatteryChemistry chemistry;
} CoinCellParams;

/**
 * @brief Runtime battery state
 */
typedef struct {
    double    SoC;
    double    V_terminal_V;
    double    R_internal_ohm;
    uint32_t  cycle_count;
    double    cumulative_discharge_mAh;
    double    temperature_C;
} BatteryState;

/* ============================================================================
 * L2 Core Concepts - Discharge curves, capacity derating, temperature effects
 * ============================================================================ */

/**
 * @brief Peukert''s Law parameters
 *
 * Peukert equation: Cp = I^k * t
 * Effective capacity at current I: C_eff = C_nom * (I_std / I)^(k-1)
 * k ~ 1.0 for ideal battery, 1.1-1.3 for lead-acid, 1.0-1.05 for lithium
 */
typedef struct {
    double k_peukert;
    double C_peukert_mAh;
    double I_reference_mA;
} PeukertModel;

/**
 * @brief Shepherd battery discharge model
 *
 * V(t) = E0 - K * (Q/(Q-it)) * it - R*i + A*exp(-B*it)
 *
 * Reference: Shepherd, JECS, 1965
 * Extended: Tremblay & Dessaint, EVS24, 2009
 */
typedef struct {
    double E0_V;         /* Constant voltage (V) */
    double K_V_per_Ah;   /* Polarization constant (V/Ah) */
    double Q_Ah;         /* Maximum capacity (Ah) */
    double R_ohm;        /* Internal resistance (Ohm) */
    double A_V;          /* Exponential amplitude (V) */
    double B_inv_Ah;     /* Exponential time constant inverse (1/Ah) */
    double V_cutoff_V;   /* Cutoff voltage (V) */
} ShepherdModel;

/** Discharge curve interpolated lookup table */
typedef struct {
    const double *SoC_points;
    const double *V_OCV_points;
    const double *R_int_points;
    size_t        num_points;
} DischargeLUT;

/** Linear interpolation result */
typedef struct {
    double value;
    int    valid;
} InterpResult;

/** Statistical summary of battery performance */
typedef struct {
    double mean_lifetime_hours;
    double stddev_lifetime_hours;
    double min_voltage_V;
    double max_voltage_V;
    double capacity_used_mAh;
    double energy_delivered_mWh;
} BatteryStats;

/** Arrhenius model for temperature-dependent self-discharge */
typedef struct {
    double Ea_eV;
    double rate_ref_per_month;
    double T_ref_K;
} ArrheniusSelfDischarge;

/** Coulomb counting state machine */
typedef struct {
    double initial_capacity_mAh;
    double accumulated_charge_mAh;
    double last_current_mA;
    double last_timestamp_s;
    double SoC_estimated;
    double SoC_error_bound;
} CoulombCounter;

/**
 * @brief Extended Kalman Filter state for SoC estimation
 */
typedef struct {
    double x_SoC;
    double x_R_ohm;
    double P_00, P_01;
    double P_10, P_11;
    double Q_soc;
    double Q_R;
    double R_meas;
    double Q_nominal_Ah;
    double dt_s;
} EKF_SoCEstimator;

/* ============================================================================
 * API Functions
 * ============================================================================ */

const CoinCellParams* coin_cell_get_params(CoinCellModel model);
const char* coin_cell_get_name(CoinCellModel model);
const char* battery_chemistry_get_name(BatteryChemistry chem);

void shepherd_model_init(const CoinCellParams *params, ShepherdModel *model);
double shepherd_voltage(const ShepherdModel *model, double extracted_Ah, double current_A);
double peukert_capacity(double C_nominal_mAh, double I_std_mA,
                        double I_actual_mA, double k);
double peukert_discharge_time(double C_nominal_mAh, double I_std_mA,
                               double I_actual_mA, double k);

double internal_resistance_vs_soc(double R_initial, double SoC);
double internal_resistance_vs_temp(double R_25C, double temperature_C, double Ea_eV);

double self_discharge_remaining(double initial_capacity_mAh,
                                double self_discharge_rate_pct_per_month,
                                double time_months);
double arrhenius_self_discharge_rate(const ArrheniusSelfDischarge *model,
                                     double temperature_C);

double terminal_voltage_under_load(double V_open_circuit,
                                   double I_load_mA, double R_internal_ohm);
double max_current_at_cutoff(double V_open_circuit, double V_cutoff,
                              double R_internal_ohm);

void coulomb_counter_init(CoulombCounter *cc, double initial_capacity_mAh);
void coulomb_counter_update(CoulombCounter *cc, double current_mA, double dt_s);
void coulomb_counter_reset(CoulombCounter *cc, double new_capacity_mAh);
double coulomb_counter_get_soc(const CoulombCounter *cc);

void ekf_soc_init(EKF_SoCEstimator *ekf, double Q_Ah, double R_initial, double dt_s);
void ekf_soc_predict(EKF_SoCEstimator *ekf, double current_A);
void ekf_soc_update(EKF_SoCEstimator *ekf, double V_measured,
                    double (*OCV_function)(double SoC), double current_A);
double ekf_soc_get_soc(const EKF_SoCEstimator *ekf);
double ekf_soc_get_uncertainty(const EKF_SoCEstimator *ekf);

InterpResult discharge_lut_interpolate(const DischargeLUT *lut, double SoC_query);
double discharge_lut_energy(const DischargeLUT *lut,
                            double SoC_start, double SoC_end, double capacity_Ah);

void battery_compute_stats(const double *voltages, const double *currents_mA,
                           const double *times_s, size_t N, BatteryStats *stats);
double estimate_shelf_life(const CoinCellParams *params,
                           double temperature_C, double min_usable_capacity_pct);
int battery_can_deliver_pulse(const BatteryState *state,
                              double pulse_current_mA, double pulse_duration_ms,
                              double V_min_allowable);

#endif /* COIN_CELL_BATTERY_H */