# Knowledge Graph - mini-power-clock-reset-bring-up

## L1: Definitions
- Voltage rail spec (power_rail_spec_t)
- LDO regulator params (ldo_regulator_t)
- DC-DC converter (dcdc_converter_t)
- Decoupling cap RLC model (decoupling_cap_t)
- PDN target impedance (pdn_target_t)
- Power tree node (power_tree_node_t)
- Power sequence step (power_sequence_step_t)
- Crystal resonator (crystal_spec_t)
- Pierce oscillator (pierce_oscillator_t)
- Clock jitter (clock_jitter_t)
- PLL config (pll_config_t)
- Phase noise (phase_noise_t)
- Reset source enum (reset_source_t)
- Supervisor IC (supervisor_ic_t)
- Watchdog config (watchdog_config_t)
- Reset timing (reset_timing_t)

## L2: Core Concepts
- LDO vs DC-DC selection, power tree, PDN FDTIM, IPC-2221 trace
- Pierce oscillator (Barkhausen), PLL synthesis, clock tree jitter
- RC POR delay, reset source decode, supervisor compatibility
- Pre-power continuity, first power-up, clock/reset/debug verification
- Eye mask test, PDN shunt-through, timing budget, microstrip Z0

## L3: Mathematical Structures
- Thermal network (Fourier), volt-second balance, inductor ripple
- RC time constant, CCM/DCM boundary, RLC impedance, LC resonance
- Leeson equation, crystal resonance, PLL transfer function
- RC charge/discharge, watchdog period, comparator hysteresis
- Voltage divider, shunt current, frequency counter, logic analyzer
- TDR reflection, crosstalk coupling, BER from Q-factor, skin effect
- Telegrapher equations, Faraday induced voltage

## L4: Fundamental Laws
- Ohm (trace Vdrop), KCL (power tree), KVL (series check)
- Thermodynamics (regulator loss), Joule heating, P=V*I, E=0.5*C*V^2
- Exponential decay, threshold crossing, Nyquist (ADC jitter)
- Parseval (PN to jitter), Allan deviation

## L5: Algorithms
- PDN impedance sweep, LDO phase margin, buck C_out selection
- Inrush limiter, anti-resonance find, power loss allocation
- Buck small-signal model, PLL loop filter, jitter decomposition
- SSCG profile, button debounce, watchdog optimal timeout
- Reset log, power-good aggregate, automated bring-up sequencer
- Boundary scan, MCU BIST, eye diagram analysis, PDN violation detect

## L6: Canonical Problems
- 3.3V/1.8V/1.2V MCU budget, STM32 rail check, nRF52 power
- STM32F4 PLL->168MHz, ESP32->240MHz, nRF52 dual clock
- STM32F4 reset tree, reset cause (STM32/nRF52)
- Nucleo bring-up, Arduino bootloader, ESP32 first flash
- USB20 eye compliance, DDR SI, SPI signal check

## L7: Applications
- Arduino Nano power, ESP32 battery life, EV/Tesla auxiliary
- GPSDO accuracy, PCIe REFCLK budget
- Auto ECU reset (ISO 26262), PLC watchdog (IEC 61131-2)
- Production test flow, FCC Part 15, CISPR 25, MIL-STD-461G

## L8: Advanced Topics
- GaN vs Si FOM, energy harvesting, buck loss decomposition
- PMBus digital power, thermal analysis, MEMS vs quartz
- SSCG EMI reduction, SIL diagnostic coverage, dual-redundant WDG
- Flying probe, X-ray BGA, statistical eye, IBIS-AMI correlation

## L9: Research Frontiers (Documented)
- AI-driven DVS, sub-threshold design, optoelectronic clock dist
- Quantum clock sync, self-healing power, terahertz characterization
