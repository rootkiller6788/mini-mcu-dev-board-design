/**
 * @file pcb_layout.h
 * @brief PCB layout rules for STM32 minimal system.
 * Knowledge Level: L1, L2, L4
 * Reference: ST AN4488, IPC-2221, IPC-2141
 * Course mapping: Berkeley EE117, TU Munich HF Engineering
 */
#ifndef PCB_LAYOUT_H
#define PCB_LAYOUT_H
#include "stm32_minimal_config.h"

typedef struct {
    int num_layers;
    double total_thickness_mm, prepreg_thickness_mm, core_thickness_mm, copper_thickness_mm;
    int has_ground_plane, has_power_plane;
} PCBStackup;

typedef struct {
    double trace_width_mm, trace_thickness_mm, substrate_height_mm;
    double dielectric_constant, impedance_ohm, propagation_delay_ps_per_mm;
} MicrostripModel;

typedef struct {
    double trace_width_mm, trace_thickness_mm, plane_spacing_mm;
    double dielectric_constant, impedance_ohm, propagation_delay_ps_per_mm;
} StriplineModel;

typedef struct {
    double drill_diameter_mm, pad_diameter_mm, antipad_diameter_mm;
    double barrel_inductance_nh, pad_capacitance_pf;
} ViaModel;

typedef struct {
    int passes, total_checks, violations;
    double margin_worst_case;
    char failing_items[16][128];
} LayoutValidation;

/**
 * IPC-2221 trace width for given current.
 * L4: I = k*deltaT^0.44 * A^0.725, A=W*t (mil^2)
 */
double ipc2221_trace_width(double current_a, double temp_rise_c,
                           double copper_weight_oz, int inner_layer);
double ipc2221_current_capacity(double trace_width_mm, double temp_rise_c,
                                double copper_weight_oz, int inner_layer);
/**
 * DC resistance. L4: R = rho*L/(W*t), rho_Cu=1.72e-8 ohm*m, alpha=0.00393
 */
double trace_dc_resistance(double length_mm, double width_mm,
                           double copper_weight_oz, double temperature_c);
/**
 * Microstrip impedance (Hammerstad-Jensen). L3: Z0=87/sqrt(er+1.41)*ln(5.98h/(0.8W+t))
 * Reference: IPC-2141
 */
double microstrip_impedance(double er, double trace_width_mm,
                            double thickness_mm, double height_mm);
double microstrip_width_for_impedance(double er, double target_z_ohm,
                                      double thickness_mm, double height_mm);
double differential_impedance(double z0_odd, double spacing_mm, double height_mm);
double stripline_impedance(double er, double trace_width_mm,
                           double thickness_mm, double spacing_mm);
double estimate_next_crosstalk(double spacing_mm, double height_mm,
                               double parallel_length_mm);
double minimum_crosstalk_spacing(double height_mm, double target_next_db);
double via_inductance(double via_height_mm, double drill_diameter_mm);
double via_pad_capacitance(double pad_diameter_mm, double antipad_diameter_mm,
                           double plane_thickness_mm, double er);
void validate_stm32_layout(double crystal_dist_mm, double max_decoup_dist_mm,
                           double swd_trace_length_mm, int gnd_plane_continuous,
                           double power_trace_width_mm, double power_trace_current_a,
                           double copper_weight_oz, LayoutValidation *result);

#endif /* PCB_LAYOUT_H */

/* Additional PCB functions */
double compute_trace_inductance(double length_mm, double width_mm, double thickness_mm);
double compute_propagation_delay(double length_mm, double epsilon_r_eff);
double compute_critical_length(double rise_time_ps, double epsilon_r_eff);
double compute_return_path_inductance(double signal_height_mm,
                                       double gap_length_mm, double gap_width_mm);
double compute_decoupling_radius(double rise_time_ps, double epsilon_r_eff);
double compute_ground_plane_impedance(double plane_length_mm,
                                       double plane_width_mm,
                                       double copper_weight_oz, double freq_hz);
double compute_fr4_dielectric_constant(double freq_hz,
                                        double er_at_1mhz,
                                        double dispersion_coefficient);
