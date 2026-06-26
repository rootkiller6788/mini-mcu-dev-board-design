# Course Alignment — mini-bootloader-usb-dfu-ota-design

## Nine-School Curriculum Mapping

| School | Course | Topics Covered | Files |
|--------|--------|---------------|-------|
| MIT | 6.004 Computation Structures | Boot FSM, memory-mapped IO, state machines | boot_sequence.c, usb_dfu_core.c |
| Stanford | CS255 Applied Cryptography | SHA-256, AES-128, ECDSA P-256, HMAC, HKDF | crypto_verify.c |
| Berkeley | EE16A/B Embedded Systems | USB enumeration, memory-map, flash programming | usb_descriptors.c, flash_manager.c |
| CMU | 18-349 Embedded Real-Time | Bootloader architecture, OTA update design | All modules |
| Georgia Tech | ECE 4100 Advanced MCU | Cortex-M boot, DFU protocol, firmware update | boot_sequence.c, usb_dfu_core.c |
| Michigan | EECS 470 Computer Architecture | Memory hierarchy, VTOR relocation, cache | boot_sequence.c |
| Cambridge | CST IB Computer Design | Boot sequence, interrupt vectors, supervisor calls | boot_sequence.c |
| ETH Zurich | 227-0116 Embedded Systems | Firmware update, USB DFU, security verification | All modules |
| Tsinghua | Microcomputer Principle | Cortex-M architecture, boot flow, Flash controller | boot_sequence.c, flash_manager.c |

## USB DFU 1.1 Specification Coverage

| Section | Topic | Implementation |
|---------|-------|---------------|
| 4.1.1 | DFU Functional Descriptor | usb_descriptors.h (dfu_functional_descriptor_t) |
| 5.1 | Detach Sequence | usb_dfu_core.c (dfu_handle_detach) |
| 6.1.1 | DNLOAD Request | usb_dfu_core.c (dfu_handle_dnload) |
| 6.1.2 | GETSTATUS Response | usb_dfu_core.c (dfu_get_status) |
| 6.2 | UPLOAD Request | usb_dfu_core.c (dfu_handle_upload) |
| 7 | Manifestation Phase | usb_dfu_core.c (dfu_manifest) |
| A.1 | State Transition Table | usb_dfu_core.c (dfu_transition_table) |

## FIPS/NIST Standards Coverage

| Standard | Algorithm | Implementation |
|----------|-----------|---------------|
| FIPS 180-4 | SHA-256 | crypto_verify.c |
| FIPS 197 | AES-128 | crypto_verify.c |
| FIPS 186-5 | ECDSA P-256 | crypto_verify.c (stub) |
| NIST SP 800-38D | AES-GCM | crypto_verify.c (structure) |
| RFC 2104 | HMAC-SHA256 | crypto_verify.c |
| RFC 5869 | HKDF | crypto_verify.c |
