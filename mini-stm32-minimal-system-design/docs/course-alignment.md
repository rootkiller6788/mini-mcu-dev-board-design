# Course Alignment — STM32 Minimal System Design

## Nine-School Curriculum Mapping

| School | Course | Module Mapping |
|--------|--------|---------------|
| **MIT** | 6.003 Signal Processing | RC circuits → reset timing; Fourier → EMI harmonics |
| **Stanford** | EE102A Signal Processing | Transmission lines → PCB impedance; PLL → clock synthesis |
| **Berkeley** | EE16A/B Circuits | Ohm's Law → power distribution; RC circuits → reset timing |
| **Berkeley** | EE117 EM | Loop radiation → EMI analysis; shielding → EMC design |
| **Illinois** | ECE 310 DSP | Digital waveforms → corner frequency; harmonics analysis |
| **Michigan** | EECS 411 Microwave | Microstrip/stripline → PCB impedance control |
| **Georgia Tech** | ECE 6350 EM | Common-mode radiation → cable EMI; shielding effectiveness |
| **TU Munich** | High-Frequency Engineering | Via modeling → parasitic extraction; PDN design |
| **ETH** | 227-0427 Signal Processing | System modeling → clock tree; PLL analysis |
| **Tsinghua** | Digital Signal Processing | Digital edge analysis → EMI corner frequency |

## Reference Textbooks

| Topic | Textbook | Author(s) |
|-------|----------|-----------|
| Signal Processing | Signals and Systems | Oppenheim & Willsky (1997) |
| EM Compatibility | Introduction to EMC | Paul (2006) |
| EM Compatibility Eng. | Electromagnetic Compatibility Engineering | Ott (2009) |
| High-Speed Digital | High-Speed Digital Design | Johnson & Graham (1993) |
| Power Integrity | Principles of Power Integrity for PDN Design | Smith & Bogatin (2017) |
| Thermal Design | Principles of Heat Transfer | Kreith & Bohn |
| Embedded Systems | Embedded Systems: RTOS for ARM Cortex-M | Valvano (2019) |
| STM32 Hardware | AN4488, AN2586, AN2867, AN5036 | STMicroelectronics |

## ST Application Notes Used

| AN | Title | Module Section |
|----|-------|---------------|
| AN4488 | Getting started with STM32F4xxxx hardware development | Power, Layout |
| AN2586 | Getting started with STM32F10xxx hardware development | Power, Reset |
| AN2867 | Oscillator design guide for STM8 and STM32 | Clock system |
| AN1709 | PLL programming guide | Clock/PLL |
| AN5036 | Thermal management guidelines for STM32 | Thermal |
