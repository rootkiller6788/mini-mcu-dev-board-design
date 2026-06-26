# Course Tree ¡ª mini-esp32-iot-board-layout

## Prerequisite Dependency Graph

```
©°©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©´
©¦              mini-esp32-iot-board-layout                ©¦
©¦         ESP32 IoT Board Layout Design Library           ©¦
©¸©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¼
                          ©¦
          ©°©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©à©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©´
          ©¦               ©¦               ©¦
    ©°©¤©¤©¤©¤©¤¨‹©¤©¤©¤©¤©¤©´   ©°©¤©¤©¤©¤¨‹©¤©¤©¤©¤©¤©´   ©°©¤©¤©¤©¤©¤¨‹©¤©¤©¤©¤©¤©¤©´
    ©¦ EM Theory ©¦   ©¦ Circuit  ©¦   ©¦ Thermal    ©¦
    ©¦ (L3-L4)   ©¦   ©¦ Theory   ©¦   ©¦ Physics    ©¦
    ©¦           ©¦   ©¦ (L3-L4)  ©¦   ©¦ (L3-L4)    ©¦
    ©¸©¤©¤©¤©¤©¤©Ð©¤©¤©¤©¤©¤©¼   ©¸©¤©¤©¤©¤©Ð©¤©¤©¤©¤©¤©¼   ©¸©¤©¤©¤©¤©¤©Ð©¤©¤©¤©¤©¤©¤©¼
          ©¦               ©¦               ©¦
    ©°©¤©¤©¤©¤©¤¨‹©¤©¤©¤©¤©¤©´   ©°©¤©¤©¤©¤¨‹©¤©¤©¤©¤©¤©´   ©°©¤©¤©¤©¤©¤¨‹©¤©¤©¤©¤©¤©¤©´
    ©¦ Maxwell   ©¦   ©¦ Kirchhoff©¦   ©¦ Fourier    ©¦
    ©¦ Equations ©¦   ©¦ Laws     ©¦   ©¦ Heat Eq.   ©¦
    ©¦ Telegrapher©¦   ©¦ Ohm's Law©¦   ©¦ Stefan-    ©¦
    ©¦ Equations ©¦   ©¦ RLC Nets ©¦   ©¦ Boltzmann  ©¦
    ©¸©¤©¤©¤©¤©¤©Ð©¤©¤©¤©¤©¤©¼   ©¸©¤©¤©¤©¤©Ð©¤©¤©¤©¤©¤©¼   ©¸©¤©¤©¤©¤©¤©Ð©¤©¤©¤©¤©¤©¤©¼
          ©¦               ©¦               ©¦
    ©°©¤©¤©¤©¤©¤¨‹©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤¨‹©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤¨‹©¤©¤©¤©¤©¤©¤©´
    ©¦         Core PCB Design Knowledge          ©¦
    ©¦  - Transmission line impedance (microstrip,©¦
    ©¦    stripline, CPW, differential pairs)      ©¦
    ©¦  - PDN and decoupling design                ©¦
    ©¦  - RF matching networks (L, Pi, T, stubs)   ©¦
    ©¦  - Thermal management (heatsink, vias)      ©¦
    ©¦  - Signal integrity (crosstalk, SSN, eye)   ©¦
    ©¦  - EMC/EMI (filtering, shielding)           ©¦
    ©¦  - IPC design rules (trace, clearance)      ©¦
    ©¦  - PCB antenna design (IFA, meander)        ©¦
    ©¦  - DFM (solder mask, stencil, panels)       ©¦
    ©¸©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©Ð©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¼
                          ©¦
    ©°©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤¨‹©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©´
    ©¦         Application Layer (L6-L7)           ©¦
    ©¦  - ESP32 4-layer IoT board stackup          ©¦
    ©¦  - Antenna matching & BLE link budget       ©¦
    ©¦  - PDN decoupling strategy design           ©¦
    ©¦  - Thermal analysis with heatsink sizing    ©¦
    ©¸©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¼
```

## Module Internal Dependencies

| Module | Depends On | Provides |
|--------|-----------|----------|
| `board_geometry.h` | (none) | Material constants, unit conversions |
| `transmission_line.h` | `board_geometry.h` | Z0, loss, wavelength, tpd |
| `power_integrity.h` | `board_geometry.h` | PDN, decap, plane, via inductance |
| `rf_design.h` | `board_geometry.h` | Reflection, VSWR, matching, link budget |
| `thermal_design.h` | `board_geometry.h` | Junction temp, heatsink, convection |
| `signal_integrity.h` | `board_geometry.h` | Crosstalk, SSN, eye, via discontinuity |

## External Prerequisites

| Prerequisite | Knowledge Level | Covered By |
|-------------|----------------|------------|
| Complex numbers | Undergraduate | Standard math curriculum |
| Circuit analysis (RLC) | Undergraduate | Basic EE |
| Electromagnetic waves | Undergraduate | Physics/EE |
| Thermal physics | Undergraduate | Physics |
| C programming | Undergraduate | CS/EE |
