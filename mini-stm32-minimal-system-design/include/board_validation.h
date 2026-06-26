/**
 * @file board_validation.h
 * @brief Complete board validation and design rule checking.
 * Knowledge Level: L2, L5
 */
#ifndef BOARD_VALIDATION_H
#define BOARD_VALIDATION_H

#include "stm32_minimal_config.h"
#include "reset_boot.h"

typedef struct {
    int power_ok, clock_ok, reset_ok, layout_ok, thermal_ok, emc_ok;
    int overall_pass, total_checks, passed_checks;
    char warnings[32][256]; int warning_count;
    char errors[16][256]; int error_count;
} DesignValidationReport;

typedef struct {
    double vdd_margin_percent, clock_margin_percent, thermal_margin_percent;
    double overall_margin_percent;
} DesignMargin;

void validate_complete_design(const BoardConfig *cfg,
                               const ResetCircuit *reset,
                               double crystal_dist_mm,
                               double max_decoup_dist_mm,
                               double swd_trace_length_mm,
                               double copper_area_mm2,
                               int num_layers,
                               DesignValidationReport *report);
void compute_design_margin(const BoardConfig *cfg, double ambient_temp_c,
                            double actual_vdd, double actual_sysclk,
                            DesignMargin *margin);
int check_power_integrity(double vdd_actual, double vdd_nominal,
                           double vdd_ripple_mv, double ripple_max_mv,
                           double bulk_cap_uf, double min_bulk_cap_uf);
int check_signal_integrity(double crystal_dist_mm, double swd_trace_length_mm,
                            double swd_clock_freq_hz, double epsilon_r_eff);
void generate_design_report(const BoardConfig *cfg,
                             const DesignValidationReport *validation,
                             const DesignMargin *margin,
                             char *report_buffer, int buffer_size);

#endif /* BOARD_VALIDATION_H */
