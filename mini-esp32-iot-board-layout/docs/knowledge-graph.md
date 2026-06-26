# Knowledge Graph ¡ª mini-esp32-iot-board-layout

## L1: Definitions (Complete)

| Entry | C Implementation | Header |
|-------|-----------------|--------|
| Board outline (width, height, thickness) | `board_outline_t` | `board_geometry.h` |
| Layer stackup (layer types, copper weight) | `stackup_layer_t`, `board_stackup_t` | `board_geometry.h` |
| Dielectric properties (Er, tan_delta, thermal K) | `dielectric_t` | `board_geometry.h` |
| Mounting hole specification | `mounting_hole_t` | `board_geometry.h` |
| Keepout zone (antenna, clearance) | `keepout_zone_t` | `board_geometry.h` |
| Copper weight presets | `copper_weight_t`, `CU_WEIGHT_*` | `board_geometry.h` |
| Transmission line types (microstrip, stripline, CPW) | `tl_type_t`, `tl_params_t` | `transmission_line.h` |
| Differential pair parameters | `diff_pair_params_t`, `diff_pair_result_t` | `transmission_line.h` |
| Decoupling capacitor model (C, ESL, ESR) | `decap_model_t` | `power_integrity.h` |
| PDN specification (Vdd, ripple, I_transient) | `pdn_spec_t` | `power_integrity.h` |
| Power plane properties | `plane_params_t` | `power_integrity.h` |
| Complex impedance and admittance | `complex_z_t`, `complex_y_t` | `rf_design.h` |
| S-parameters (2-port) | `s_params_2port_t`, `s_params_t` | `rf_design.h` |
| Matching network definition | `match_network_t`, `match_type_t` | `rf_design.h` |
| Thermal resistance parameters | `thermal_params_t` | `thermal_design.h` |
| Heatsink parameters | `heatsink_params_t` | `thermal_design.h` |
| Thermal via array | `thermal_via_array_t` | `thermal_design.h` |
| Coupled transmission line parameters | `coupled_line_t` | `signal_integrity.h` |
| Crosstalk results (NEXT/FEXT) | `crosstalk_result_t` | `signal_integrity.h` |
| SSN parameters and eye diagram | `ssn_params_t`, `eye_diagram_t` | `signal_integrity.h` |

## L2: Core Concepts (Complete)

| Concept | Source File | Key Functions |
|---------|------------|---------------|
| Controlled impedance PCB stackup | `board_stackup.c` | `stackup_4layer_standard()`, `stackup_6layer_advanced()` |
| Microstrip transmission line theory | `transmission_line.c` | `microstrip_z0()`, `microstrip_ereff()` |
| Stripline and CPW transmission lines | `transmission_line.c` | `stripline_z0()`, `cpw_z0()`, `cpwg_z0()` |
| Dielectric and conductor loss | `transmission_line_loss.c` | `dielectric_loss_db_per_mm()`, `conductor_loss_db_per_mm()` |
| Differential signaling | `transmission_line_diff.c` | `diff_pair_analyze()`, `diff_pair_max_mismatch()` |
| PDN target impedance methodology | `power_integrity.c` | `pdn_target_impedance()`, `decap_srf()` |
| Decoupling capacitor hierarchy | `power_integrity.c` | `decap_anti_resonance_freq()` |
| Power plane modeling | `power_integrity_plane.c` | `plane_capacitance()`, `plane_resonance_freq()` |
| Via inductance and loop inductance | `power_integrity_plane.c` | `via_inductance()`, `via_pair_inductance()` |
| Reflection coefficient and VSWR | `rf_design.c` | `reflection_coefficient()`, `vswr()` |
| Impedance matching (L, Pi, T) | `rf_matching.c` | `l_match_synthesize()`, `pi_match_synthesize()` |
| Thermal resistance network | `thermal_design.c` | `junction_temperature()`, `heatsink_thermal_resistance()` |
| Convection and radiation cooling | `thermal_design_cooling.c` | `natural_convection_power()`, `radiation_heat_power()` |
| Crosstalk (NEXT/FEXT) | `signal_integrity.c` | `near_end_crosstalk()`, `far_end_crosstalk()` |
| Ground bounce and SSN | `signal_integrity_noise.c` | `ground_bounce_voltage()`, `ssn_voltage()` |
| Eye diagram analysis | `signal_integrity_eye.c` | `eye_diagram_estimate()` |
| EMI filtering and shielding | `emc_design.c` | `emi_lc_lowpass_design()`, `shielding_effectiveness_db()` |
| PCB antenna design (IFA, meander) | `antenna_design.c` | `ifa_resonant_length_mm()`, `eirp_dbm()` |
| PCB manufacturing (DFM) | `manufacturing.c` | `solder_mask_expansion_mm()`, `panel_utilization()` |
| IPC-2221 design rules | `design_rules.c` | `ipc2221_trace_width_for_current()`, `ipc2221_clearance_mm()` |

## L3: Mathematical Structures (Complete)

| Structure | Implementation |
|-----------|---------------|
| Complex numbers (rectangular form) | `complex_z_t` with arithmetic via rf_design functions |
| Bilinear transform (Smith chart mapping) | `reflection_coefficient()`, `impedance_from_gamma()` |
| Elliptic integrals (CPW models) | `ellip_ratio()` in transmission_line.c |
| Newton-Raphson root finding | `microstrip_width_for_z0()` |
| Log-frequency sweep | `pdn_impedance_sweep()` |
| Matrix-like S-parameters | `s_params_t` |
| Thermal resistance network (series/parallel) | `parallel_thermal_resistance()`, `series_thermal_resistance()` |

## L4: Fundamental Laws (Complete)

| Law/Theorem | Verification |
|-------------|-------------|
| Telegrapher's equations | `tl_analyze()` computes Z0, propagation constant |
| Hammerstad-Jensen microstrip formula | `microstrip_z0()` with test validation |
| Cohn stripline formula | `stripline_z0()` with constraints |
| Friis transmission equation | `free_space_path_loss_db()`, `antenna_link_budget()` |
| Fourier heat conduction (thermal Ohm's law) | `junction_temperature()`, `thermal_via_count()` |
| Stefan-Boltzmann radiation law | `radiation_heat_power()` |
| Faraday's law (via inductance) | `via_inductance()` |
| Kirchhoff laws (PDN parallel/series) | `pdn_impedance_sweep()` |
| IPC-2221 current capacity formula | `ipc2221_current_capacity()` |

## L5: Algorithms/Methods (Complete)

| Algorithm | Source |
|-----------|--------|
| Newton-Raphson trace width optimization | `microstrip_width_for_z0()` |
| Conformal mapping for CPW Z0 | `ellip_ratio()`, `cpw_z0()` |
| Multi-stage PDN impedance sweep | `pdn_impedance_sweep()` |
| L-match network synthesis | `l_match_synthesize()` |
| Pi-match and T-match synthesis | `pi_match_synthesize()`, `t_match_synthesize()` |
| Stub matching (OC/SC) | `stub_oc_input_impedance()`, `stub_sc_input_impedance()` |
| Differential pair analysis | `diff_pair_analyze()` |
| Eye diagram estimation | `eye_diagram_estimate()` |
| BER from Q-factor | `bit_error_rate_q()` |
| EMI LC filter design | `emi_lc_lowpass_design()` |

## L6: Canonical Problems (Complete)

| Problem | Example/Demo |
|---------|-------------|
| 4-layer ESP32 IoT board stackup design | `example_4layer_stackup.c` |
| RF antenna impedance matching | `example_rf_matching.c` |
| PDN decoupling network design | `example_pdn_decoupling.c` |
| Thermal management for IoT board | `example_thermal_design.c` |

## L7: Applications (Partial+)

| Application | Coverage |
|-------------|----------|
| ESP32 WiFi/BLE PCB antenna design | `antenna_design.c` (IFA, meander, chip antenna) |
| FCC/CE 2.4 GHz regulatory compliance | `eirp_dbm()`, `eirp_regulatory_check()` |
| DFM for IoT PCB manufacturing | `manufacturing.c` (stencil, fiducials, panelization) |
| IPC Class 2/3 design rules | `design_rules.c` |

## L8: Advanced Topics (Partial+)

| Topic | Coverage |
|-------|----------|
| EMC shielding effectiveness | `shielding_effectiveness_db()` |
| Ferrite bead impedance modeling | `ferrite_bead_impedance()` |
| Surface roughness conductor loss correction | `conductor_loss_db_per_mm()` |
| Via impedance discontinuity (TDR) | `via_impedance_discontinuity()` |

## L9: Research Frontiers (Partial)

| Topic | Status |
|-------|--------|
| 6G/mmWave PCB materials | Documented (RO4350B constants in board_geometry.h) |
| Advanced PDN modeling | Not yet implemented (requires FEM/PEEC) |
| AI-driven PCB layout optimization | Not yet implemented |
