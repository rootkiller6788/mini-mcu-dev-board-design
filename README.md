# Mini MCU Dev Board Design

A collection of **from-scratch, zero-dependency C implementations** for MCU development board design — covering minimal system bring-up, bootloader design, debug interfaces, board layout, low-power design, sensor shields, and firmware foundations. Each module bridges EE/embedded-systems theory and hands-on board-level practice with runnable C code.

## Sub-Modules

| Sub-Module | Topics | Key Courses |
|-----------|--------|-------------|
| [mini-stm32-minimal-system-design](mini-stm32-minimal-system-design/) | STM32 power/clock/reset system, decoupling PDN, PCB layout, EMI/EMC, thermal management, board validation | MIT 6.002, MIT 6.004 |
| [mini-bootloader-usb-dfu-ota-design](mini-bootloader-usb-dfu-ota-design/) | USB DFU core, firmware image management, flash manager, OTA transport, crypto verification, boot sequence | Berkeley EECS 149, MIT 6.S082 |
| [mini-debug-swd-jtag-interface](mini-debug-swd-jtag-interface/) | SWD protocol stack, JTAG TAP controller, debug port access, debug transport abstraction | MIT 6.004, ARM DDI 0314 |
| [mini-esp32-iot-board-layout](mini-esp32-iot-board-layout/) | Board geometry & layer stackup, PDN power integrity, RF matching & Smith chart, signal integrity, thermal design, transmission line | Stanford EE214, MIT 6.002 |
| [mini-firmware-first-blink-hello-world](mini-firmware-first-blink-hello-world/) | Cortex-M bare-metal startup, GPIO/UART/ADC/timer drivers, watchdog, bootloader interface, arch compatibility | UT Austin EE319K, MIT 6.S082 |
| [mini-low-power-coin-cell-design](mini-low-power-coin-cell-design/) | Coin cell battery models, LDO/boost/buck-boost regulation, power budget analysis, low-power MCU sleep states, energy harvesting, battery life estimation | Berkeley EE290C, MIT 6.002 |
| [mini-peripheral-sensor-shield-design](mini-peripheral-sensor-shield-design/) | Sensor types & taxonomy, shield form factors & bus protocols, signal conditioning, sensor calibration, digital filtering, sensor fusion (Kalman/AHRS) | Stanford ME220, MIT 6.002 |
| [mini-power-clock-reset-bring-up](mini-power-clock-reset-bring-up/) | Power supply design, crystal oscillator design, reset circuit & supervisory, board bring-up procedures, signal integrity validation | MIT 6.002, MIT 6.004 |

## Design Philosophy

- **Zero external dependencies** — pure C (C99/C11), only `libc` and `libm`
- **Self-contained modules** — each directory has its own `include/`, `src/`, `tests/`, and `examples/`
- **Theory-to-board mapping** — every module translates EE/embedded textbooks and datasheet specs into runnable C implementations
- **Practical demos** — PDN calculators, flash layout tools, sensor fusion pipelines, power budget spreadsheets, and board validation checkers

## Building

Each module is standalone. Navigate to a module directory and run:

```bash
cd mini-stm32-minimal-system-design
make all    # build everything
make test   # run tests
```

Requires **GCC** and **GNU Make**.

## Project Structure

```
mini-mcu-dev-board-design/
├── mini-stm32-minimal-system-design/     # STM32 minimal system: power, clock, reset, PDN, PCB layout, EMI/EMC, thermal, validation
├── mini-bootloader-usb-dfu-ota-design/   # USB DFU bootloader, firmware images, flash manager, OTA transport, crypto verify
├── mini-debug-swd-jtag-interface/        # SWD protocol, JTAG TAP controller, debug port abstraction
├── mini-esp32-iot-board-layout/          # ESP32 IoT: board geometry, PDN, RF matching, signal integrity, thermal, transmission line
├── mini-firmware-first-blink-hello-world/ # Cortex-M bare-metal: GPIO, UART, ADC, timer, watchdog, startup, bootloader
├── mini-low-power-coin-cell-design/      # Coin cell models, voltage regulation, power budget, sleep states, energy harvesting
├── mini-peripheral-sensor-shield-design/ # Sensor shields: types, interfaces, signal conditioning, calibration, filtering, fusion
└── mini-power-clock-reset-bring-up/      # Board bring-up: power supply, crystal oscillator, reset circuit, validation
```

## License

MIT
