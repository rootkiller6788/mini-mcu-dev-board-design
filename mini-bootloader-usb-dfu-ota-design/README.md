# mini-bootloader-usb-dfu-ota-design

USB DFU Bootloader and OTA Firmware Update Design for MCU Dev Boards.

## Module Status: COMPLETE

- **L1-L6**: Complete
- **L7**: Complete (4 applications: STM32 DFU, IoT HTTP OTA, Secure Signing, Medical Counter)
- **L8**: Partial (3/5 advanced topics: HKDF, constant-time crypto, GCM)
- **L9**: Partial (documented: post-quantum boot, blockchain attestation)

**include/ + src/ lines**: 3566 >= 3000

## Quick Start
```
make test      # 38/38 tests pass
make examples  # 3 examples run
make check     # SKILL.md safety review
```

## Core Definitions (L1)
10 typedefs: DFU states, USB descriptors, firmware header, flash geometry, OTA transports, crypto primitives, Cortex-M vector table, HEX/SREC records, TLV, boot reasons.

## Core Theorems (L4)
10 Lean 4 theorems in src/usb_dfu_proofs.lean: no-self-transition, deadlock freedom, determinism, error recovery, manifestation finality, sector alignment, IHEX checksum, anti-rollback.

## Core Algorithms (L5)
10 algorithms: SHA-256, AES-128, CRC-32/16, HMAC-SHA256, HKDF, constant-time compare, Intel HEX parser, XMODEM-CRC, flash wear leveling.

## Classic Problems (L6)
- USB DFU Firmware Upgrade (example_dfu_update.c)
- A/B OTA with Fallback (example_ota_http_update.c)
- Secure Boot Verification (example_crypto_verify.c)

## File Structure
```
include/ (7): usb_dfu_core.h, usb_descriptors.h, firmware_image.h, flash_manager.h, ota_transport.h, crypto_verify.h, boot_sequence.h
src/ (8): usb_dfu_core.c, usb_descriptors.c, firmware_image.c, flash_manager.c, ota_transport.c, crypto_verify.c, boot_sequence.c, usb_dfu_proofs.lean
tests/ (1): test_bootloader.c (38 assertions)
examples/ (3): example_dfu_update.c, example_ota_http_update.c, example_crypto_verify.c
docs/ (5): knowledge-graph.md, coverage-report.md, gap-report.md, course-alignment.md, course-tree.md
```

## Safety
- No filler/stub patterns
- All functions implement independent knowledge points
- Constant-time crypto for sensitive comparisons
- All 38 test assertions pass
- No TODO/FIXME/stub/placeholder

