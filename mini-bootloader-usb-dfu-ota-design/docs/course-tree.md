# Course Prerequisite Tree — mini-bootloader-usb-dfu-ota-design

## Upstream Dependencies

```
mini-bootloader-usb-dfu-ota-design
├── mini-mcu-embedded-sys (section 4)
│   ├── mini-arm-cortex-m — Vector table, VTOR register, NVIC
│   ├── mini-bare-metal-program — Startup code, linker scripts
│   ├── mini-bootloader-ota — General bootloader/OTA concepts
│   └── mini-peripheral-interface — USB peripheral drivers
├── mini-digital-electronics (section 3)
│   └── Finite state machines, logic design
├── mini-communication-principle (section 5)
│   └── USB protocol layers, OTA channel models
└── mini-signal-system-theory (section 0)
    └── CRC polynomials, checksums, error detection
```

## Internal Module Dependencies

```
boot_sequence.c (entry point, boot flow orchestration)
├── usb_dfu_core.c (DFU protocol state machine)
│   └── usb_descriptors.c (USB descriptor construction)
├── firmware_image.c (HEX/SREC parsing, version comparison)
│   └── crypto_verify.c (CRC-32 computation)
├── crypto_verify.c (SHA-256, AES-128, HMAC, HKDF, CRC)
├── flash_manager.c (flash operations, partitions, wear leveling)
│   └── crypto_verify.c (CRC-32 for image verification)
└── ota_transport.c (XMODEM/YMODEM/HTTP protocols)
    └── firmware_image.c (CRC-16-CCITT)
```

## L9 Research Frontiers (Prerequisites for Future Work)

```
L9 Advanced Research
├── Post-Quantum Boot
│   ├── Hash-based signatures (XMSS, SPHINCS+)
│   ├── Lattice-based signatures (Dilithium, Falcon)
│   └── Formal verification of PQ signature schemes
├── Blockchain Firmware Attestation
│   ├── Distributed ledger for firmware hashes
│   ├── Smart contract-based update authorization
│   └── Zero-knowledge proofs for firmware integrity
├── AI-Driven OTA Scheduling
│   └── Reinforcement learning for update timing
└── Quantum-Secure Firmware Delivery
    └── Quantum key distribution for OTA encryption
```
