# Course Alignment ¡ª mini-esp32-iot-board-layout

## Nine-School Curriculum Mapping

| School | Course | Relevant Topics | Implementation |
|--------|--------|----------------|----------------|
| **MIT** | 6.003 Signal Processing | Fourier analysis, frequency response | `transmission_line.c` loss models |
| **MIT** | 6.630 EM Waves | Transmission lines, Smith chart | `rf_design.c`, `rf_matching.c` |
| **Stanford** | EE359 Wireless | Link budget, antenna matching | `rf_link_budget.c`, `antenna_design.c` |
| **Stanford** | EE247 Optical/High-Speed | Signal integrity, eye diagrams | `signal_integrity*.c` |
| **Berkeley** | EE117 EM Waves | Waveguides, impedance matching | `transmission_line.c` |
| **Berkeley** | EE16A/B Circuits | RLC networks, PDN modeling | `power_integrity*.c` |
| **Illinois** | ECE 310 DSP | Digital signal processing on PCB | `signal_integrity_eye.c` |
| **Illinois** | ECE 451 EM | EM modeling of vias/planes | `power_integrity_plane.c` |
| **Michigan** | EECS 411 Microwave | Microstrip, stripline, CPW design | `transmission_line.c`, `transmission_line_diff.c` |
| **Georgia Tech** | ECE 6350 EM | PCB crosstalk, EMC | `signal_integrity.c`, `emc_design.c` |
| **TU Munich** | HF Engineering | RF PCB layout, antenna design | `antenna_design.c` |
| **ETH** | 227-0455 EM | Computational EM for PCB | `emc_design.c` shielding |
| **Tsinghua** | Signal & Systems | Transmission line theory | `transmission_line_loss.c` |
| **Tsinghua** | Communication Principles | RF matching, link budget | `rf_matching.c`, `rf_link_budget.c` |

## Key Textbook References

| Textbook | Author(s) | Topics Covered |
|----------|-----------|----------------|
| High-Speed Digital Design | Johnson & Graham | Transmission lines, crosstalk, ground bounce, PDN |
| Signal and Power Integrity Simplified | Bogatin | PDN, signal integrity, decoupling |
| Microwave Engineering | Pozar | Microstrip, matching networks, S-parameters |
| RF Circuit Design | Bowick | L/Pi/T matching, Smith chart |
| Antenna Theory | Balanis | IFA, meander antenna, PCB antenna design |
| Introduction to EMC | Paul | Shielding, filtering, emissions |
| IPC-2221A | IPC | Trace width, clearance, creepage |
| IPC-2152 | IPC | Thermal via design, current capacity |
