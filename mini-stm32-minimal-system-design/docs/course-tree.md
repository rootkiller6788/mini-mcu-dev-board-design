# Course Tree — STM32 Minimal System Design

## Prerequisites

```
Basic Circuit Theory
├── Ohm's Law
├── Kirchhoff's Laws
├── RC/RL circuits (first-order systems)
└── Complex impedance

Electromagnetics Basics
├── Maxwell's Equations (conceptual)
├── Transmission line theory
├── Skin effect
└── Radiation from currents

Digital Logic
├── CMOS input/output levels (VIH, VIL, VOH, VOL)
├── Timing (setup/hold, rise/fall time)
└── Power consumption (static vs dynamic)

Microcontroller Architecture
├── ARM Cortex-M core
├── Memory map (Flash, SRAM, Peripherals)
├── Clock tree
└── Reset architecture
```

## This Module

```
STM32 Minimal System Design
├── Power System
│   ├── Voltage domains (VDD, VDDA, VBAT)
│   ├── LDO selection and stability
│   ├── Bulk capacitance sizing
│   └── Power integrity (PSRR, ripple)
├── Clock System
│   ├── Crystal oscillator design (Pierce)
│   ├── Barkhausen criterion
│   ├── PLL frequency synthesis
│   └── Clock tree constraints
├── Decoupling Network
│   ├── PDN target impedance
│   ├── Capacitor impedance model (RLC)
│   ├── Anti-resonance analysis
│   └── DC bias derating
├── PCB Layout
│   ├── Trace width (IPC-2221)
│   ├── Impedance control (microstrip/stripline)
│   ├── Via modeling
│   └── Crosstalk estimation
├── Reset and Boot
│   ├── RC timing for NRST
│   ├── BOR threshold selection
│   └── Boot mode configuration
├── Thermal Management
│   ├── Thermal Ohm's Law
│   ├── PCB copper heat spreading
│   └── Thermal via design
└── EMI/EMC
    ├── Loop antenna radiation
    ├── Common-mode radiation
    ├── Shielding effectiveness
    └── FCC Part 15 compliance
```

## Downstream Modules

```
This module → 
  ├── mini-mcu-embedded-sys (firmware running on this hardware)
  ├── mini-circuit-analysis (power supply circuit analysis)
  ├── mini-emc-signal-integrity (advanced EMC)
  └── mini-electronic-mfg-test (DFM/DFT)
```
