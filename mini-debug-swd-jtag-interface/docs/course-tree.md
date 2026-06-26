# Course Tree - mini-debug-swd-jtag-interface
## Prerequisites
- mini-digital-electronics: Combinational logic (parity), sequential logic (FSM), shift registers
- mini-mcu-embedded-sys: ARM Cortex-M architecture, memory-mapped I/O, flash organization
- mini-signal-system-theory: Parity/CRC error detection, timing analysis
- mini-communication-principle: Bit-level framing, half-duplex protocol, ACK handshaking
- mini-circuit-analysis: Voltage translation, pull-up/pull-down, open-drain outputs

## Co-Requisites
- mini-pcb-design-layout: Debug connector pinout, trace impedance
- mini-signal-integrity-measurement: Rise/fall time, eye diagrams, jitter

## Extensions
- mini-bootloader-usb-dfu-ota-design: Debug as bootloader entry
- mini-power-clock-reset-bring-up: Debug power domain, reset catch

## Internal Dependency Graph
```
SWD Protocol (L1-2) -> Parity/CRC (L3) -> Error Detection (L4) -> Encode/Decode (L5) -> Connect (L6)
JTAG TAP (L1-2) -> FSM Graph (L3) -> Navigation (L5) -> IDCODE Scan (L6)
Debug Port (L1-2) -> Memory Access (L5) -> Flash Programming (L6)
Transport (L1-2) -> Bit-Bang (L5) -> Auto-Baud (L5) -> Signal Quality (L8)
Security (L7-8) -> Authentication (L5) -> Lifecycle (L7)
```
