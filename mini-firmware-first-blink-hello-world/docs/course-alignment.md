# Course Alignment — mini-firmware-first-blink-hello-world

Nine-school curriculum mapping for embedded firmware fundamentals.

| School | Course | Relevant Chapters | This Module's Coverage |
|--------|--------|------------------|----------------------|
| **MIT** | 6.004 Computation Structures | §15: Boot process, exceptions, linker | `startup.c`: .data copy, .bss zero, boot sequence |
| | 6.450 Digital Communications | §2: Baseband signaling, NRZ | `uart.c`: NRZ UART frame, baud rate |
| **Stanford** | EE102A Signal Processing | Embedded I/O fundamentals | `gpio.c`: Memory-mapped I/O, digital I/O |
| | EE359 Wireless | Serial link fundamentals | `uart.c`: Asynchronous serial, error handling |
| **Berkeley** | EE16A/B Circuits | GPIO as bridge to physical world | `gpio.c`: Pin modes, pull resistors, Schmitt trigger |
| | EE123 Digital Signal Processing | Quantization, ADC | `adc.c`: SAR ADC, SQNR, oversampling |
| | CS162 Operating Systems | Context switching, interrupts | `cortex_m.c`: NVIC, PendSV, stack frame |
| **Illinois** | ECE 310 DSP | Digital filter design | `adc.c`: Running average, EMA filter |
| | ECE 459 Communications | Error detection/correction | `bootloader.c`: CRC-32, Hamming distance |
| **Michigan** | EECS 351 DSP | Sampling, aliasing | `adc.h`: Nyquist, oversampling theory |
| | EECS 455 Communications | Channel coding | `bootloader.c`: CRC polynomial arithmetic |
| | EECS 411 Microwave | Transmission lines (not covered — physical layer above MCU scope) | — |
| **Georgia Tech** | ECE 4270 DSP | Multirate DSP | `adc.c`: Oversampling + decimation |
| | ECE 6601 Communications | Error control coding | `bootloader.c`: CRC integrity check |
| **TU Munich** | Signal Processing | Real-time processing | `timer.c`: Real-time constraints, PWM generation |
| | Communications | Serial protocols | `uart.c`: RS-232, UART protocol |
| **ETH** | 227-0427 Signal Processing | Adaptive filtering | `adc.c`: EMA filter (1st-order IIR) |
| | 227-0436 Communications | Modulation | `uart.c`: NRZ encoding |
| **清华** | 信号与系统 | Sampling theorem, convolution | `adc.h`: Nyquist, ADC theory |
| | 通信原理 | Serial communication | `uart.c`: UART frame format |
| | 数字信号处理 | Digital filters | `adc.c`: FIR/IIR filter implementations |

## Core Textbooks Referenced

| Textbook | Author | Year | Usage |
|----------|--------|------|-------|
| Embedded Systems: RTOS for ARM Cortex-M | Valvano | 2019 | Ch.2 Startup, Ch.4 GPIO, Ch.6 Timer, Ch.8 UART, Ch.9 Interrupts |
| Digital Communications | Proakis & Salehi | 2008 | §4.3 PAM/NRZ signaling |
| Discrete-Time Signal Processing | Oppenheim & Schafer | 2010 | §6.5 Digital filter design |
| Microelectronic Circuits | Sedra & Smith | 2020 | §10 A/D converters, §15.2 Schmitt trigger |
| Fundamentals of Power Electronics | Erickson & Maksimovic | 2001 | §18 PWM modulation schemes |
| Power Electronics: Converters, Applications | Mohan, Undeland & Robbins | 2003 | §22 Gate drive, dead-time |
| Introduction to EMC | Paul | 2006 | §3.2 Edge rate vs harmonic content |
| ARMv7-M Architecture Ref Manual | ARM | 2010 | §B1.5 Exception model, §B3 NVIC |
| Ganssle "A Guide to Debouncing" | Ganssle | 2008 | Software debounce algorithms |
| Koopman "CRC Polynomial Selection" | Koopman | 2004 | CRC-32 properties |
