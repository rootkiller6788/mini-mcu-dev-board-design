# Course Tree — mini-firmware-first-blink-hello-world

## Prerequisites (what this module depends on)

```
Digital Logic (MIT 6.004)
  └─ Binary arithmetic, bitwise operations
       └─ Register manipulation ← THIS MODULE

C Programming (MIT 6.S096)
  └─ Pointers, structs, volatile, memory layout
       └─ Memory-mapped I/O ← THIS MODULE

Computer Architecture (MIT 6.004)
  └─ CPU registers, stack, instruction set
       └─ Cortex-M core, NVIC, SysTick ← THIS MODULE

Electrical Circuits (Berkeley EE16A)
  └─ Ohm's law, Kirchhoff's laws, RC circuits
       └─ LED current-limiting resistors, GPIO output drivers ← THIS MODULE

Signals & Systems (MIT 6.003)
  └─ Sampling theorem, frequency domain
       └─ ADC aliasing, PWM frequency analysis ← THIS MODULE
```

## What depends on this module (downstream)

```
THIS MODULE: Firmware First Blink / Hello World
  │
  ├─ RTOS (FreeRTOS, Zephyr)
  │    └─ Task scheduling, context switching on top of SysTick + PendSV
  │
  ├─ Communication Stacks (TCP/IP, BLE, LoRaWAN)
  │    └─ UART → AT-command modems, PPP, SLIP
  │
  ├─ Motor Control
  │    └─ Timer PWM → H-bridge → FOC (Field-Oriented Control)
  │
  ├─ Sensor Fusion
  │    └─ ADC → I²C/SPI sensors → Kalman filter
  │
  ├─ Firmware Update / OTA
  │    └─ Bootloader CRC → cryptographic signature → dual-bank swap
  │
  └─ Safety-Critical Systems (ISO 26262)
       └─ Watchdog → multi-stage recovery → ASIL decomposition
```

## Module internal dependency graph

```
startup.c ──────────────────────────────────────────┐
  system_init() → clock + FPU + SysTick               │
  copy_data_section() → .data init                    │
  zero_bss_section() → .bss init                      │
                                                      │
gpio.c ──────────────────────────────────────────────┤
  gpio_init() → pin configuration                     │
  gpio_write() → atomic output                        │
  gpio_debounce_update() → switch debounce            │
                                                      │
uart.c ───────────────────────────────────┐          │
  uart_init() → baud rate + format        │          │
  uart_send_byte() → TX polling           │   Depends on
  uart_receive_byte() → RX with timeout   │   systick_get_count()
  uart_printf() → formatted output        │          │
  uart_ringbuf_put/get → circular buffer  │          │
                                          │          │
timer.c ──────────────────────────────────┤          │
  timer_init() → time base                │          │
  pwm_init() → PWM output                 │          │
  capture_init() → input capture          │          │
  dead_time_calculate() → H-bridge safety │          │
                                          │          │
adc.c ────────────────────────────────────┤          │
  adc_init() → SAR configuration         │          │
  adc_read() → single conversion          │          │
  adc_avg_filter_update() → MA filter     │          │
  adc_ema_filter_update() → IIR filter    │          │
                                          │          │
watchdog.c ───────────────────────────────┤          │
  iwdg_init() → independent watchdog      │          │
  wwdg_init() → window watchdog           │          │
  watchdog_self_test() → safety test      │          │
                                          │          │
cortex_m.c ───────────────────────────────┘          │
  nvic_enable/disable → interrupt control            │
  systick_init() → 1 ms tick                         │
  scb_get_fault_info() → crash dump                  │
  fault_handler_dump() → UART output                 │
                                          │          │
bootloader.c ───────────────────────────────────────┘
  crc32_calculate() → integrity check
  flash_erase/program → firmware update
  bootloader_jump_to_application() → boot
```

## Knowledge dependency graph

```
Binary/Hex Arithmetic
  └─→ Bitwise register manipulation (gpio.c)
       └─→ GPIO pin control
            └─→ LED blink (blink_hello.c)
                 └─→ Timer-based delays
                      └─→ PWM (timer.c)
                           └─→ Breathing LED (pwm_led.c)

Serial Communication Theory
  └─→ UART frame format (uart.c)
       └─→ Baud rate generation
            └─→ Ring buffer (lock-free SPSC)
                 └─→ Echo server (uart_echo.c)
                      └─→ Error handling (ORE/FE/PE)

CRC Theory (GF(2))
  └─→ CRC-32 table generation (bootloader.c)
       └─→ Image integrity verification
            └─→ Flash programming
                 └─→ Bootloader (boot from valid image)

Sampling Theory (Nyquist)
  └─→ ADC configuration (adc.c)
       └─→ Oversampling + decimation
            └─→ Digital filtering (MA, EMA)
                 └─→ Sensor signal conditioning

Exception Model (ARMv7-M)
  └─→ NVIC priority management (cortex_m.c)
       └─→ Fault diagnosis
            └─→ Crash dump over UART

Liveness Properties
  └─→ Watchdog timeout (watchdog.c)
       └─→ Window watchdog timing
            └─→ Multi-stage recovery
```
