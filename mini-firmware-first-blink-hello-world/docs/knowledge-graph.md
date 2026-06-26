# Knowledge Graph — mini-firmware-first-blink-hello-world

## L1: Definitions (Complete)

| # | Definition | C Implementation | Lean Formalization |
|---|-----------|-----------------|-------------------|
| 1 | GPIO pin/port/mode (input/output/AF/analog) | `gpio.h`: `gpio_port_t`, `gpio_pin_t`, `gpio_mode_t`, `gpio_config_t` | `PinState` inductive type |
| 2 | GPIO output type (push-pull/open-drain) | `gpio.h`: `gpio_otype_t` | — |
| 3 | GPIO speed (slew rate) | `gpio.h`: `gpio_speed_t` | — |
| 4 | GPIO pull-up/pull-down | `gpio.h`: `gpio_pupd_t` | — |
| 5 | UART baud rate, data frame (start/data/parity/stop) | `uart.h`: `uart_config_t`, `uart_wordlen_t`, `uart_stopbits_t`, `uart_parity_t` | `UARTFrame` structure |
| 6 | Timer, prescaler, auto-reload, period | `timer.h`: `timer_regs_t`, `tick_config_t` | `TimerValue`, `TimerPrescaler` |
| 7 | PWM duty cycle | `timer.h`: `pwm_config_t` | `PWMDuty` structure |
| 8 | ADC resolution, ENOB, LSB, quantization | `adc.h`: `ADC_RESOLUTION_12BIT`, `adc_config_t` | — |
| 9 | Watchdog timer (IWDG/WWDG) | `watchdog.h`: `watchdog_type_t`, `watchdog_config_t` | `WatchdogState` inductive |
| 10 | Cortex-M vector table, NVIC, SysTick | `cortex_m.h`: exception numbers, `nvic_regs_t`, `systick_regs_t` | — |
| 11 | Flash memory layout, firmware header | `bootloader.h`: `firmware_header_t`, `flash_status_t` | — |
| 12 | CRC-32 polynomial | `bootloader.h`: `CRC32_POLYNOMIAL` | `CRC32Poly` abbrev |
| 13 | Stack bounds, memory regions | `startup.h`: `section_copy_entry_t` | `MemRegion`, `StackBounds` |
| 14 | Boot mode, reset reason | `bootloader.h`: `boot_mode_t`, `reset_reason_t` | — |
| 15 | Deadline/timeout via SysTick | `cortex_m.h`: `delay_systick_elapsed` | — |

## L2: Core Concepts (Complete)

| # | Concept | Implementation |
|---|---------|---------------|
| 1 | Memory-mapped I/O | `gpio.c`: base address arrays, `gpio_get_port_base()` |
| 2 | Register bit-field manipulation (2-bit/1-bit per pin) | `gpio.c`: `gpio_init()` MODER/OTYPER/OSPEEDR/PUPDR fields |
| 3 | Atomic BSRR write (no RMW race) | `gpio.c`: `gpio_write()` |
| 4 | Schmitt trigger hysteresis | `gpio.h`: IDR reads after Schmitt trigger |
| 5 | Ring buffer (single-producer single-consumer) | `uart.c`: `uart_ringbuf_put/get` | `RingBuffer` + invariants |
| 6 | Baud rate oversampling (×8/×16) | `uart.c`: `uart_calculate_brr()` |
| 7 | TXE vs TC distinction | `uart.c`: `uart_send_byte()` |
| 8 | UART error conditions (ORE/NF/FE/PE) | `uart.c`: `uart_get_status()` |
| 9 | Timer preload register (ARPE) | `timer.c`: `timer_set_period()` |
| 10 | PWM mode 1 (edge-aligned) vs mode 2 | `timer.c`: `pwm_init()` |
| 11 | Input capture for frequency measurement | `timer.c`: `capture_init()` |
| 12 | SAR ADC sampling time calculation | `adc.c`: `adc_init()` |
| 13 | ADC sampling time vs source impedance | `adc.h`: R_ain + C_s charge formula |
| 14 | Watchdog fail-safe (IWDG cannot be disabled) | `watchdog.c`: `iwdg_init()` |
| 15 | Window watchdog (too-fast refresh detection) | `watchdog.c`: `wwdg_init()` |
| 16 | Interrupt priority grouping (preemption vs subpriority) | `cortex_m.c`: `nvic_set_priority_grouping()` |
| 17 | Fault escalation (MemManage→BusFault→UsageFault→HardFault) | `cortex_m.c`: `enable_fault_handlers()` |
| 18 | Flash wait states vs frequency | `startup.c`: `FLASH_LATENCY()` macro |
| 19 | PLL configuration (M/N/P/Q) | `startup.c`: `system_clock_config()` |
| 20 | .data copy + .bss zero (C standard §6.7.8) | `startup.c`: `copy_data_section()`, `zero_bss_section()` |
| 21 | Weak default handlers (linker override) | `cortex_m.c`: `__attribute__((weak))` handlers |
| 22 | Bootloader image validation (magic+CRC+size+entry) | `bootloader.c`: `bootloader_validate_image()` |
| 23 | VTOR relocation for application boot | `bootloader.c`: `bootloader_jump_to_application()` |

## L3: Mathematical Structures (Complete)

| # | Structure | C Implementation | Lean Formalization |
|---|----------|-----------------|-------------------|
| 1 | Bitwise masks and shifts for register fields | `gpio.c`: MODER 2-bit fields | `gpio_set_bit` / `gpio_clear_bit` |
| 2 | Fractional baud rate divider (fixed-point) | `uart.c`: `uart_calculate_brr()` — USARTDIV × 1000 | — |
| 3 | Modular arithmetic for ring buffer indices | `uart.c`: `(head+1) & MASK` | `RingBuffer.count` |
| 4 | Timer period formula: f = f_clk / ((PSC+1)(ARR+1)) | `timer.c`: `timer_init()`, `pwm_init()` | `TimerPrescaler.frequency` |
| 5 | Center-aligned PWM: T = 2(ARR+1)(PSC+1)/f_clk | `timer.c`: `pwm_init()` center modes | — |
| 6 | Dead-time encoding (4 ranges, logarithmic scale) | `timer.c`: `dead_time_calculate()` | — |
| 7 | ADC quantization error: σ_q² = LSB²/12 | `adc.h`: formula in documentation | — |
| 8 | ADC SQNR = 6.02N + 1.76 dB | `adc.h`: formula in documentation | — |
| 9 | Oversampling SNR improvement: ΔSNR = 10 log₁₀(N) | `adc.c`: `adc_oversample_and_decimate()` | — |
| 10 | CRC-32 polynomial arithmetic in GF(2) | `bootloader.c`: `crc32_init_table()` | `crc32_table_entry` recursion |
| 11 | Watchdog timeout: T = prescaler × (RLR+1) / f_LSI | `watchdog.c`: `iwdg_calculate_timeout()` | `watchdog_eventually_expires` |
| 12 | WWDG timeout: T = 4096 × 2^WDGTB × (T[6:0]−0x3F) / f_PCLK | `watchdog.c`: `wwdg_calculate_timeout()` | — |
| 13 | PLL: f_VCO = f_in/PLLM × PLLN, f_PLL = f_VCO/PLLP | `startup.c`: `system_clock_config()` | — |
| 14 | EMA filter: y[n] = αx[n] + (1−α)y[n−1] | `adc.c`: `adc_ema_filter_update()` | — |
| 15 | Running average FIR: y[k] = (1/N)Σx[k−i] | `adc.c`: `adc_avg_filter_update()` | — |

## L4: Fundamental Laws (Complete)

| # | Law/Theorem | C Verification | Lean Proof |
|---|------------|---------------|-----------|
| 1 | CRC-32 detects all single-bit errors (Hamming weight 1) | `test_bootloader.c`: test vectors | `crc32_detects_single_bit_error` |
| 2 | CRC-32 of all-zero data with standard init equals 0 after XOROUT | `test_bootloader.c`: `test_crc32_empty()` | — |
| 3 | Ring buffer single-slot-waste invariant | `test_uart.c`: overflow at 255, not 256 | `ringbuffer_count_bounded` |
| 4 | Ring buffer put-then-get restores empty (FIFO property) | `test_uart.c`: `test_ring_buffer()` | `ringbuffer_put_get_empty` |
| 5 | GPIO set-bit + clear-bit idempotent | `test_gpio.c`: BSRR bit positions | `gpio_set_clear_idempotent` |
| 6 | GPIO toggle twice restores original (XOR involution) | — | `gpio_toggle_twice` |
| 7 | Watchdog liveness: infinitely refreshed → never expires | — | `watchdog_refresh_preserves_running` |
| 8 | Watchdog deadness: never refreshed → eventually expires | `test_core.c`: `test_iwdg_timeout()` | `watchdog_liveness` |
| 9 | PWM average voltage = duty × V_dd | `test_core.c`: duty=50% → V_avg=1.65V | `pwm_average_bounded` |
| 10 | Timer frequency exactness for divisor pairs | `test_core.c`: PSC/ARR math | `timer_exact_frequency` |
| 11 | UART encode-decode roundtrip (8N1 format) | — | `uart_encode_decode_roundtrip` |
| 12 | Flash and SRAM address spaces do not overlap | — | `flash_sram_no_overlap` |

## L5: Algorithms/Methods (Complete)

| # | Algorithm | Implementation |
|---|----------|---------------|
| 1 | Software debounce (integrate-and-dump) | `gpio.c`: `gpio_debounce_update()` |
| 2 | Table-driven CRC-32 (256-entry LUT) | `bootloader.c`: `crc32_calculate()` |
| 3 | Ring buffer (lock-free SPSC) | `uart.c`: `uart_ringbuf_put/get` |
| 4 | Integer to ASCII (itoa with any base) | `uart.c`: `u32_to_str()`, `i32_to_str()` |
| 5 | Variadic printf over UART | `uart.c`: `uart_printf()` |
| 6 | PWM duty-to-CCR mapping | `timer.c`: `pwm_set_duty()` |
| 7 | Dead-time DTG encoding | `timer.c`: `dead_time_calculate()` |
| 8 | Running average filter (O(1) per sample) | `adc.c`: `adc_avg_filter_update()` |
| 9 | Exponential moving average (IIR 1st-order) | `adc.c`: `adc_ema_filter_update()` |
| 10 | Oversampling + decimation (extra ADC bits) | `adc.c`: `adc_oversample_and_decimate()` |
| 11 | Fault register decoding (CFSR/HFSR) | `cortex_m.c`: `scb_get_fault_info()` |
| 12 | SysTick non-blocking delay | `cortex_m.c`: `delay_systick_elapsed()` |
| 13 | Flash unlock key sequence | `bootloader.c`: `flash_unlock()` |
| 14 | Firmware CRC integrity check | `bootloader.c`: `crc32_verify()` |
| 15 | PLL clock tree configuration | `startup.c`: `system_clock_config()` |

## L6: Canonical Problems (Complete)

| # | Problem | Example/Solution |
|---|---------|-----------------|
| 1 | LED Blink + "Hello World" | `examples/blink_hello.c` — full firmware with GPIO+UART+SysTick |
| 2 | UART Echo Server | `examples/uart_echo.c` — RX ring buffer, echo, error handling |
| 3 | PWM LED Brightness Control | `examples/pwm_led.c` — breathing effect, discrete levels, center-aligned |
| 4 | Hardware fault diagnosis | `cortex_m.c`: `fault_handler_dump()` — HardFault crash dump |
| 5 | Firmware secure update (bootloader) | `bootloader.c`: `bootloader_enter()` — validate+jump flow |
| 6 | ADC sensor reading with filtering | `adc.c`: average+EMA+oversampling pipelines |
| 7 | Watchdog recovery | `watchdog.c`: multi-stage `watchdog_self_test()` |

## L7: Applications (Partial+ — 3 applications)

| # | Application | Implementation |
|---|------------|---------------|
| 1 | Modbus RTU framing (UART idle-line detection) | `uart.h`: `UART_SR_IDLE` used for frame boundary |
| 2 | Motor H-bridge dead-time insertion | `timer.c`: `dead_time_calculate()` for gate drive |
| 3 | Bootloader OTA (Over-The-Air) update framework | `bootloader.c`: full image validate → erase → program → verify → boot |

## L8: Advanced Topics (Partial+ — 5 topics)

| # | Topic | Implementation |
|---|-------|---------------|
| 1 | Center-aligned PWM for reduced EMI | `timer.c`: `TIMER_MODE_CENTER1/2/3` modes |
| 2 | Flash ECC (Error Correction Code) | `bootloader.c`: `flash_verify_buffer()` post-program verification |
| 3 | Fault escalation chain (MemManage→Bus→Usage→Hard) | `cortex_m.c`: `enable_fault_handlers()` + `scb_get_fault_info()` |
| 4 | Priority ceiling protocol for NVIC | `cortex_m.h`: `nvic_priority_group_t` + `nvic_set_priority_grouping()` |
| 5 | Formal verification of firmware invariants | `src/firmware_verify.lean` — 10 theorems with Lean 4 proofs |

## L9: Research Frontiers (Partial — documented)

| # | Topic | Status |
|---|-------|--------|
| 1 | Formally verified bootloader (seL4-level) | Documented; `firmware_verify.lean` provides foundational proofs |
| 2 | Secure boot with cryptographic signature verification | Documented in `bootloader.h`: CRC→hash extension path |
| 3 | Rust-based bare-metal firmware for memory safety | Documented; C code provides reference implementation |
