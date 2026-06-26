# Knowledge Graph — STM32 Minimal System Design

## L1: Definitions (Complete)

| Concept | C Implementation | Lean Formalization |
|---------|-----------------|-------------------|
| Voltage Domain (VDD, VDDA, VBAT, VREF, VDD_USB) | `VoltageDomain` enum | `VoltageDomain` inductive |
| Clock Source (HSI, HSE, LSI, LSE, PLL) | `ClockSource` enum | `ClockSource` inductive |
| Boot Mode (Main Flash, System Memory, SRAM) | `BootMode` enum | `BootMode` inductive |
| Reset Source (POR, BOR, External, Watchdog, SW, LP) | `ResetSource` enum | `ResetSource` inductive |
| Capacitor Type (Ceramic, Tantalum, Electrolytic, Film) | `CapacitorType` enum | — |
| STM32 Series | `STM32Series` enum | — |
| Package Type | `PackageType` enum | — |
| Power Spec | `PowerSpec` struct | `PowerSpec` structure |
| Decoupling Cap Spec | `DecouplingCapSpec` struct | — |
| Crystal Spec (Pierce) | `CrystalSpec` struct | `CrystalSpec` structure |
| PLL Config | `PLLConfig` struct | `PLLConfig` structure |
| Reset Config | `ResetConfig` struct | `ResetConfig` structure |
| PCB Trace Spec | `PCBTraceSpec` struct | — |
| Board Config (aggregate) | `BoardConfig` struct | `BoardConfig` structure |

## L2: Core Concepts (Complete)

| Concept | Implementation |
|---------|---------------|
| Power domain validation | `validate_power_spec()` |
| MCU power estimation (static + dynamic) | `estimate_mcu_power()` |
| LDO headroom/dropout | `ldo_headroom_check()` |
| LDO input bypass | `ldo_input_capacitance()` |
| Crystal load capacitor calculation | `compute_load_capacitors()` |
| External resistor (Rext) for drive limiting | `compute_rext_limit()` |
| NRST timing validation | `check_nrst_timing()` |
| Boot mode decoding | `determine_boot_mode()` |
| BOOT0 pull-down sizing | `compute_boot0_pulldown()` |
| BOR threshold check | `bor_check()` |
| Clock tree validation | `validate_clock_tree()` |
| Board config validation | `validate_board_config()` |
| Power integrity scoring | `check_power_integrity()` |
| Signal integrity check | `check_signal_integrity()` |
| FCC Part 15 compliance | `check_fcc_part15_compliance()` |

## L3: Mathematical Structures (Complete)

| Structure | Implementation |
|-----------|---------------|
| Complex impedance Z(f) = sqrt(ESR^2 + (wL - 1/wC)^2) | `capacitor_impedance()` |
| Parallel impedance: 1/Z = sum(1/Z_i) | `parallel_cap_impedance()` |
| Pierce oscillator: CL = CL1*CL2/(CL1+CL2) + C_stray | `compute_load_capacitors()` |
| PLL synthesis: VCO = F_in/M * N, SYSCLK = VCO/P | `compute_pll_frequencies()` |
| Hammerstad-Jensen microstrip model | `microstrip_impedance()` |
| Symmetric stripline formula | `stripline_impedance()` |
| Differential pair coupling | `differential_impedance()` |
| Via inductance (Johnson-Graham) | `via_inductance()` |
| Via pad capacitance | `via_pad_capacitance()` |
| NEXT crosstalk estimate | `estimate_next_crosstalk()` |
| Plane capacitance | `compute_plane_capacitance()` |
| ESR from dissipation factor | `compute_esr_from_dissipation_factor()` |
| ESL from SRF | `compute_esl_from_srf()` |
| Trace inductance | `compute_trace_inductance()` |
| Fourier harmonics of trapezoidal wave | `compute_rise_time_harmonics()` |
| FR-4 dielectric dispersion | `compute_fr4_dielectric_constant()` |

## L4: Fundamental Laws (Complete)

| Law | Formula | C Verification | Lean Statement |
|-----|---------|---------------|----------------|
| Ohm's Law (trace IR drop) | V = I * R | `trace_dc_resistance()`, `estimate_vdd_rail_voltage()` | — |
| Capacitor energy storage | C = I * dt / dV | `size_bulk_capacitance()` | — |
| RC charging | V(t) = VDD*(1-e^(-t/RC)) | `nrst_rc_delay()`, `nrst_voltage_at_time()` | `rc_voltage`, `rc_time_to_threshold` |
| Barkhausen criterion | gm >= gm_crit | `compute_gain_margin()` | `gm_critical` |
| Crystal drive level | DL = ESR * I_RMS^2 | `compute_drive_level()` | — |
| Thermal Ohm's Law | T_J = T_A + P * theta_JA | `junction_temperature()` | `junction_temperature` |
| IPC-2221 | I = k * deltaT^0.44 * A^0.725 | `ipc2221_trace_width()` | `ipc2221_current` |
| Resonance | f0 = 1/(2*pi*sqrt(LC)) | `cap_self_resonant_freq()` | `self_resonant_freq` |
| Maxwell (loop radiation) | E = 120*pi^2*I*A/(r*lambda^2) | `compute_loop_antenna_radiation()` | — |
| CM radiation | E = 1.26e-6 * I_cm * L * f / r | `compute_common_mode_radiation()` | — |
| Fourier (corner frequency) | fc = 1/(pi*tr) | `compute_corner_frequency()` | — |
| Shielding effectiveness | SE = R + A + B | `compute_shielding_effectiveness()` | — |

## L5: Algorithms/Methods (Complete)

| Method | Implementation |
|--------|---------------|
| PLL parameter search | `find_pll_config()` |
| Microstrip width iterative solve | `microstrip_width_for_impedance()` |
| Target impedance method | `compute_target_impedance()` |
| Capacitor count optimization | `min_caps_for_impedance()` |
| PDN frequency sweep | `pdn_impedance_sweep()` |
| Anti-resonance peak detection | `find_anti_resonance_peaks()` |
| Mounting inductance estimation | `estimate_mounting_inductance()` |
| DC bias derating model | `dc_bias_derating()` |
| Capacitor bank optimization | `compute_capacitor_bank_optimization()` |
| BOR threshold selection | `recommend_bor_threshold()` |
| LDO copper area calculation | `ldo_required_copper_area()` |
| PCB thermal via design | `thermal_via_resistance()` |
| Full thermal analysis | `full_thermal_analysis()` |
| Layout validation (DRC) | `validate_stm32_layout()` |
| BOM generation | `compute_board_bom()` |
| PCB area estimation | `compute_pcb_area_estimate()` |
| Design margin analysis | `compute_design_margin()` |
| Design report generation | `generate_design_report()` |
| Crosstalk spacing design | `minimum_crosstalk_spacing()` |
| Decoupling radius | `compute_decoupling_radius()` |
| Ground plane impedance | `compute_ground_plane_impedance()` |
| Quick bringup checklist | `quick_checklist()` |
| PLL prescaler chain | `compute_pll_divider_prescaler_chain()` |
| Ferrite bead selection | `compute_ferrite_bead_impedance()` |
| EMC harmonic analysis | `compute_max_clock_harmonic_for_emc()` |

## L6: Canonical Problems (Complete)

| Problem | Example |
|---------|---------|
| STM32F103 Blue Pill design | `examples/bluepill_design.c` |
| PDN impedance analysis | `examples/pdn_analysis.c` |
| Board validation workflow | `examples/board_validation.c` |

## L7: Applications (Complete — 5 real chips)

| Application | Implementation |
|-------------|---------------|
| STM32F103C8T6 Blue Pill | `stm32f103c8_bluepill_config()` |
| STM32F407VET6 Black | `stm32f407vet6_black_config()` |
| STM32H743VIT6 | `stm32h743vit6_config()` |
| STM32G070RB (IoT/Consumer) | `stm32g070rb_config()` |
| STM32L452RE (Ultra-low-power) | `stm32l452re_config()` |

## L8: Advanced Topics (Partial — 3 topics)

| Topic | Implementation |
|-------|---------------|
| Multi-layer PCB thermal management | `required_copper_area()`, `thermal_via_resistance()` |
| PDN anti-resonance analysis | `pdn_impedance_sweep()`, `find_anti_resonance_peaks()` |
| EMI/EMC shielding design | `compute_shielding_effectiveness()`, `check_fcc_part15_compliance()` |

## L9: Research Frontiers (Partial — documented only)

- 6-layer+ PCB designs with buried capacitance
- Ultra-low-power energy harvesting power management
- AI-assisted PCB layout optimization
- Advanced PDN with embedded planar capacitance
