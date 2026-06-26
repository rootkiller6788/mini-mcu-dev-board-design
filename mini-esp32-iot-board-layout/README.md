# mini-esp32-iot-board-layout

ESP32 IoT Board Layout Design Library - PCB stackup, RF transmission lines,
PDN design, thermal analysis, signal integrity, EMC, and DFM.

## Module Status: COMPLETE

- **L1-L6**: Complete
- **L7**: Complete (4 applications)
- **L8**: Partial+ (4/8 advanced topics)
- **L9**: Partial (documented, not implemented)

## Quick Start

```bash
make          # Build static library libesp32board.a
make test     # Build and run all tests (32 tests)
make examples # Build all 4 example programs
make all      # Build everything
```

## Nine-Layer Knowledge Coverage

| Level | Name | Status |
|-------|------|--------|
| L1 | Definitions | Complete (20 structs) |
| L2 | Core Concepts | Complete (20 concepts) |
| L3 | Math Structures | Complete (complex, elliptic, NR) |
| L4 | Fundamental Laws | Complete (9 theorems) |
| L5 | Algorithms | Complete (10 algorithms) |
| L6 | Canonical Problems | Complete (4 examples) |
| L7 | Applications | Complete (4 apps) |
| L8 | Advanced Topics | Partial+ (4 topics) |
| L9 | Research Frontiers | Partial (documented) |

## Core Definitions

- board_outline_t: Board mechanical dimensions with polygon vertices
- board_stackup_t: Multi-layer stackup (2/4/6-layer presets)
- tl_params_t / tl_result_t: Transmission line parameters and results
- decap_model_t: Real capacitor model (C + ESL + ESR)
- match_network_t: L/Pi/T matching network with component values
- thermal_params_t: Thermal resistance network parameters
- coupled_line_t: Coupled transmission lines for crosstalk analysis

## Core Theorems

| Theorem | Implementation | Reference |
|---------|---------------|-----------|
| Hammerstad-Jensen microstrip Z0 | microstrip_z0() | IEEE MTT-S 1980 |
| Cohn stripline Z0 | stripline_z0() | IRE T-MTT 1954 |
| Friis transmission equation | free_space_path_loss_db() | Friis 1946 |
| Fourier heat conduction | junction_temperature() | Fourier 1822 |
| Stefan-Boltzmann radiation | radiation_heat_power() | Stefan 1879 |
| Schellkunoff shielding | shielding_effectiveness_db() | Schellkunoff 1943 |
| IPC-2221 trace width | ipc2221_trace_width_for_current() | IPC-2221A |

## Core Algorithms

| Algorithm | Function | Complexity |
|-----------|----------|------------|
| Newton-Raphson width opt | microstrip_width_for_z0() | O(max_iter) |
| Elliptic integral ratio | ellip_ratio() | O(1) |
| Multi-stage PDN sweep | pdn_impedance_sweep() | O(n*stages) |
| L/Pi/T match synthesis | l/pi/t_match_synthesize() | O(1) |
| NEXT/FEXT crosstalk | near/far_end_crosstalk() | O(1) |
| Eye diagram estimation | eye_diagram_estimate() | O(1) |

## Examples (L6 Canonical Problems)

1. 4-layer ESP32 IoT board stackup (examples/example_4layer_stackup.c)
2. RF antenna matching and BLE link budget (examples/example_rf_matching.c)
3. PDN decoupling network design (examples/example_pdn_decoupling.c)
4. Thermal management and heatsink sizing (examples/example_thermal_design.c)

## Nine-School Curriculum Mapping

| School | Courses | Coverage |
|--------|---------|----------|
| MIT | 6.003, 6.630 | TL, Smith chart |
| Stanford | EE359, EE247 | Link budget, SI |
| Berkeley | EE117, EE16A/B | EM waves, RLC |
| Illinois | ECE 310, ECE 451 | DSP, EM modeling |
| Michigan | EECS 411 | Microstrip/stripline |
| Georgia Tech | ECE 6350 | Crosstalk, EMC |
| TU Munich | HF Engineering | RF PCB layout |
| ETH | 227-0455 | Computational EM |
| Tsinghua | Signal, Comm | TL theory, matching |

## File Count

- include/ + src/ total: 3036 lines
- 6 headers, 21 source files, 2 test files, 4 examples
- 5 knowledge documents in docs/

## Build Requirements

- GCC (C11) or compatible compiler, GNU Make, math library (-lm)

## License

MIT - mini-electronic-info project
