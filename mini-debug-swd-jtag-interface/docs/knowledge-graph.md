# Knowledge Graph - mini-debug-swd-jtag-interface
## L1: Definitions (26 types)
SWD: swd_line_state_t, swd_ack_t, swd_direction_t, swd_port_select_t, dp_register_addr_t, ap_register_addr_t, swd_timing_params_t, swd_transaction_t, swd_error_t, swd_version_t
JTAG: jtag_signal_t, jtag_signal_levels_t, tap_state_t, jtag_instruction_t, arm_jtag_instruction_t, jtag_dr_type_t, jtag_idcode_t, jtag_device_t, jtag_scan_chain_t, jtag_timing_t
Debug: port_type_t, ap_type_t, dap_status_t, core_register_t, breakpoint_t, watchpoint_t, mem_access_txn_t
Transport: gpio_pin_t, swd_pin_map_t, jtag_pin_map_t, target_voltage_mv_t, level_shifter_config_t, transport_type_t, transport_ops_t, debug_transport_t, signal_quality_t
Security: debug_security_state_t, debug_security_context_t, device_lifecycle_t, device_identity_t, firmware_image_header_t

## L2: Core Concepts (18 concepts)
SWD bidirectional protocol, turnaround timing, overrun detection, WAIT retry, JTAG TAP FSM operation, boundary scan, IR/DR scan paths, BYPASS/IDCODE, DAP pipelined read, debug power domains, SWD v2 multi-drop, JTAG-to-SWD switching, SWD line reset, connection sequence, stable TAP states, core halt/resume/step, DCRSR/DCRDR protocol, FPB/DWT breakpoints

## L3: Math Structures (12 structures)
Odd parity (4-bit XOR), even parity (32-bit XOR folding), CRC-5-USB (LFSR), TAP FSM graph (BFS), timing (T=1/f), JTAG min TCK period (propagation), address alignment (bitwise), signal quality (BER estimate), JTAG even parity, hash function (simplified SHA-like), binary search (auto-baud)

## L4: Fundamental Laws (12 theorems)
TLR reachability in 5 TMS=1 clocks, TAP determinism, RTI idempotence, TLR fixpoint, SWD parity oddness, SWD request well-formedness, IDCODE LSB validity, IDCODE field widths, stable state preservation, SWD ACK encoding, JTAG min TCK period, SWD max frequency from turnaround

## L5: Algorithms (33 algorithms)
SWD: request encode/decode, line reset, connection sequence, DP/AP read/write, bank select, CRC-5, burst transfer, power-up, abort, flash programming
JTAG: FSM navigation (BFS), IR/DR scan sequence, IDCODE scan, chain detection, FSM validation, boundary scan test, timing compute
Transport: init/deinit, clock frequency set, clock pulse, SWD line reset, SWD connect, SWD transaction, JTAG TAP reset, JTAG shift, JTAG navigate, auto-baud, signal quality, calibration, multi-drop select/scan
Debug: power-up, status, clear errors, CSW setup, mem read/write/burst, core halt/resume/step, core reg read/write, breakpoint set/clear, watchpoint set/clear, flash unlock/program, alignment check

## L6: Canonical Problems (4 problems)
ex1: SWD connect + DPIDR decode, ex2: JTAG TAP FSM demonstration, ex3: Flash programming workflow, ex4: Debug halt and register access

## L7: Applications (4 apps)
Production flash programming, IoT device provisioning, multi-target debug bus (SWD v2), secure debug unlock

## L8: Advanced Topics (6 topics)
SWD v2 multi-drop (16 targets), TrustZone debug authentication, device lifecycle management, signal integrity monitoring, CRC-5-USB for trace, auto-baud frequency detection

## L9: Research Frontiers (3 topics)
Post-quantum secure debug, wireless debug interface, AI-assisted debug analysis
