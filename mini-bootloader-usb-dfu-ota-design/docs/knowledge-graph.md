# Knowledge Graph — mini-bootloader-usb-dfu-ota-design

## L1: Definitions (Complete) — 10 items
USB DFU State Machine (11 states), USB Descriptors (5 types), Firmware Image Header (MCUboot), Flash Geometry (sector/page/block), OTA Transport Types (9), Cryptographic Primitives (SHA-256/AES/ECDSA/HMAC), Cortex-M Vector Table, Boot Reasons (9 types), Intel HEX/SREC Records, TLV Types (MCUboot).

## L2: Core Concepts (Complete) — 7 items
DFU Detach-Attach Re-enumeration, USB Descriptor Hierarchy Parsing, Firmware Image Validation, Flash Erase-Before-Write Constraint, XMODEM Stop-and-Wait ARQ, SHA-256 Merkle-Damgard Construction, Boot Flow Stage Transitions (ROM→Stage1→Stage2→App).

## L3: Mathematical Structures (Complete) — 5 items
CRC-32 Polynomial Arithmetic (IEEE 802.3, poly 0xEDB88320), CRC-16-CCITT (poly 0x1021), AES GF(2^8) Field Multiplication, Intel HEX Two's Complement Checksum (mod 256), SREC One's Complement Checksum (mod 256).

## L4: Fundamental Laws (Complete) — 10 Lean 4 Theorems
See `src/usb_dfu_proofs.lean`: No-Self-Transition, Deadlock Freedom, Transition Determinism, Error Recovery Uniqueness, Manifestation Finality, Download Progression, Boot Magic Distinctness, Sector Alignment Property, CRC-32 Property, IHEX Checksum Invariance.

## L5: Algorithms (Complete) — 10 items
SHA-256 (FIPS 180-4, 64 rounds), AES-128 Encrypt/Decrypt (FIPS 197, 10 rounds), CRC-32 Table-Driven, CRC-16-CCITT Bitwise, HMAC-SHA256 (RFC 2104), HKDF Extract-Expand (RFC 5869), Constant-Time Memory Compare, Intel HEX Line Parser, XMODEM Packet CRC/Checksum, Flash Wear Leveling (Count-Min).

## L6: Canonical Problems (Complete) — 5 items
USB DFU Firmware Upgrade (example_dfu_update.c), A/B OTA with Fallback (example_ota_http_update.c), Secure Boot Signature Verification (example_crypto_verify.c), Power-Loss-Safe Flash Swap (flash_manager.c swap FSM), Bootloader Entry Detection (boot_sequence.c GPIO/magic/BOOT0).

## L7: Applications (Partial+) — 4 items
STM32 USB DFU Bootloader (VID 0x0483, PID 0xDF11), IoT HTTP OTA Update (WiFi-based firmware download), Secure Firmware Signing (ECDSA P-256 verification flow), Medical Device Security Counter (anti-rollback monotonic counter).

## L8: Advanced Topics (Partial+) — 3 items
HKDF Key Derivation (RFC 5869, implemented), Constant-Time Cryptography (ct_memcmp for side-channel resistance), AES-GCM Authenticated Encryption (structure defined, GHASH/CTR stubs).

## L9: Research Frontiers (Partial) — 2 items
Post-Quantum Secure Boot (XMSS/SPHINCS+ hash-based signatures — documented), Blockchain Firmware Attestation (distributed integrity verification — documented).
