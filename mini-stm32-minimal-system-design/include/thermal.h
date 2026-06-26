/**
 * @file thermal.h
 * @brief Thermal management for STM32 minimal system — junction temp, heatsink, PCB copper.
 *
 * Knowledge Level: L1 (Definitions), L4 (Fundamental Laws), L5 (Methods)
 *
 * Key thermal metrics for MCU board design:
 *   - θ_JA: Junction-to-ambient thermal resistance (°C/W)
 *   - θ_JC: Junction-to-case thermal resistance
 *   - T_J: Junction temperature (must stay below T_Jmax, typically 85-125°C)
 *   - P_diss: Total power dissipation
 *
 * L4: T_J = T_A + P_diss × θ_JA  (Steady-state thermal model — analogous to Ohm's Law)
 *      Thermal "voltage" = temperature difference
 *      Thermal "current" = power flow
 *      Thermal "resistance" = θ
 *
 * Reference: JEDEC JESD51 standards, ST AN5036 (Thermal management for STM32)
 * Course mapping: MIT 6.003 (thermal-electrical analogies), Michigan EECS 411
 */

#ifndef THERMAL_H
#define THERMAL_H

#include "stm32_minimal_config.h"

/* =========================================================================
 * L1: Thermal Definitions
 * ========================================================================= */

/** Thermal resistance components */
typedef struct {
    double theta_ja;          /**< Junction-to-ambient (°C/W) */
    double theta_jc;          /**< Junction-to-case (°C/W) */
    double theta_jb;          /**< Junction-to-board (°C/W) */
    double theta_ca;          /**< Case-to-ambient (°C/W), depends on heatsink */
} ThermalResistance;

/** Thermal operating point */
typedef struct {
    double ambient_temp_c;    /**< Ambient temperature (°C) */
    double board_temp_c;      /**< PCB temperature near MCU (°C) */
    double case_temp_c;       /**< Package case temperature (°C) */
    double junction_temp_c;   /**< Silicone junction temperature (°C) */
    double power_dissipation_w; /**< Total power dissipated (W) */
    double margin_c;          /**< Temperature margin to T_Jmax (°C) */
    int    safe;              /**< 1 = within safe limits */
} ThermalPoint;

/** Heatsink specification */
typedef struct {
    double thermal_resistance_cw; /**< θ_SA: heatsink thermal resistance (°C/W) */
    double volume_cm3;            /**< Heatsink volume (cm³) */
    double surface_area_cm2;      /**< Effective surface area (cm²) */
    int    has_fan;               /**< Active cooling present */
    double airflow_cfm;           /**< Airflow if fan present (CFM = ft³/min) */
} HeatsinkSpec;

/* =========================================================================
 * L4: Fundamentals — Thermal Ohm's Law
 * ========================================================================= */

/**
 * Compute junction temperature using thermal Ohm's law.
 *
 * L4: T_J = T_A + P_diss × θ_JA
 * This is the steady-state thermal model.
 * Analogy: ΔT ↔ ΔV, P ↔ I, θ ↔ R
 *
 * @param ambient_c     Ambient temperature (°C)
 * @param power_w       Total power dissipation (W)
 * @param theta_ja      Junction-to-ambient thermal resistance (°C/W)
 * @return              Junction temperature (°C)
 */
double junction_temperature(double ambient_c, double power_w, double theta_ja);

/**
 * Compute maximum allowable power for given thermal constraints.
 *
 * L4: P_max = (T_Jmax - T_A) / θ_JA
 *
 * @param tj_max        Maximum junction temperature (°C)
 * @param ambient_c     Ambient temperature (°C)
 * @param theta_ja      Junction-to-ambient (°C/W)
 * @return              Maximum power dissipation (W)
 */
double max_power_dissipation(double tj_max, double ambient_c, double theta_ja);

/* =========================================================================
 * L5: Methods — Thermal Management Techniques
 * ========================================================================= */

/**
 * Compute required PCB copper area for heat spreading.
 *
 * L5: For a given package, θ_JA decreases as copper area increases
 * (better heat spreading into the PCB). Empirical model:
 *   θ_JA(area) ≈ θ_JA_min + k / sqrt(area)
 *
 * Uses JEDEC 2s2p (2 signal + 2 plane layers) board model from datasheet.
 * Typical LQFP packages on 2-layer board:
 *   θ_JA ≈ 50-60 °C/W (minimal copper)
 *   θ_JA ≈ 35-45 °C/W (with thermal vias and ground plane)
 *
 * Reference: ST AN5036, JEDEC JESD51-7
 *
 * @param pkg_theta_ja_bare     θ_JA with minimal copper, from datasheet (°C/W)
 * @param target_theta_ja       Desired θ_JA after adding copper (°C/W)
 * @param num_layers            Number of PCB layers
 * @return                      Required copper area (mm²)
 */
double required_copper_area(double pkg_theta_ja_bare, double target_theta_ja,
                            int num_layers);

/**
 * Compute effective θ_JA with heatsink.
 *
 * L4: θ_JA_total = θ_JC + θ_CA_effective
 * θ_CA_effective = θ_CS (thermal interface) + θ_SA (heatsink)
 * without heatsink: θ_CA_effective = θ_CA (∞ for no airflow)
 *
 * @param thermal        Thermal resistance of the package
 * @param heatsink       Heatsink specification (NULL for no heatsink)
 * @param tim_resistance Thermal interface material resistance (°C/W)
 *                       (thermal paste: ~0.1-1.0 °C/W depending on pressure)
 * @return               Effective θ_JA (°C/W)
 */
double effective_theta_ja(const ThermalResistance *thermal,
                          const HeatsinkSpec *heatsink,
                          double tim_resistance);

/**
 * Estimate board temperature rise from copper area and power.
 *
 * L5: ΔT_board ≈ P_diss × θ_BA, where θ_BA decreases with copper area.
 * Empirical formula based on board thermal spreading.
 *
 * @param power_w            Power dissipated into board (W)
 * @param copper_area_mm2    Copper area connected to thermal pad (mm²)
 * @param num_layers         Number of copper layers
 * @return                   Board temperature rise above ambient (°C)
 */
double board_temp_rise(double power_w, double copper_area_mm2, int num_layers);

/**
 * Compute thermal via effectiveness.
 *
 * L5: Thermal vias under exposed pad conduct heat to internal ground planes.
 * Each thermal via: θ_via = L / (k_copper × A_via)
 * where L = board thickness, k_copper = 385 W/(m·K), A_via = π × d × t_wall
 *
 * For N thermal vias in parallel: θ_vias = θ_via / N
 *
 * @param num_vias            Number of thermal vias
 * @param via_diameter_mm     Via drill diameter (mm)
 * @param via_wall_thickness_um Via wall plating thickness (µm)
 * @param board_thickness_mm  PCB thickness (mm)
 * @return                    Total via thermal resistance (°C/W)
 */
double thermal_via_resistance(int num_vias, double via_diameter_mm,
                              double via_wall_thickness_um,
                              double board_thickness_mm);

/**
 * Full thermal analysis of a complete board setup.
 *
 * L5: Computes junction temperature considering:
 *   - Ambient temperature
 *   - Power dissipation
 *   - Package θ_JA with PCB copper area
 *   - Optional heatsink and airflow
 *   - Safety margin to T_Jmax
 *
 * @param power_w            Power dissipation (W)
 * @param ambient_c          Ambient temperature (°C)
 * @param tj_max             Maximum junction temperature (°C)
 * @param pkg_theta_jc       Package θ_JC from datasheet (°C/W)
 * @param pkg_theta_ja_min   Package θ_JA with minimum PCB (°C/W)
 * @param copper_area_mm2    PCB copper area for heat spreading (mm²)
 * @param num_layers         PCB layer count
 * @param heatsink           Heatsink spec (NULL if none)
 * @param result             Output thermal operating point
 */
void full_thermal_analysis(double power_w, double ambient_c, double tj_max,
                           double pkg_theta_jc, double pkg_theta_ja_min,
                           double copper_area_mm2, int num_layers,
                           const HeatsinkSpec *heatsink, ThermalPoint *result);

#endif /* THERMAL_H */

/* Additional thermal functions */
double compute_theta_ja_from_layout(double theta_jc, double copper_area_mm2,
                                     int num_layers, int num_thermal_vias);
double compute_ambient_derating(double tj_max, double theta_ja,
                                 double power_w, double safety_margin_c);
int check_thermal_throttling_required(double junction_temp_c,
                                       double throttle_threshold_c);
