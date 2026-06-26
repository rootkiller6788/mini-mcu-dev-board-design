# Gap Report — mini-bootloader-usb-dfu-ota-design

## Priority 1: Missing Implementations

| Gap | Current Status | Effort |
|-----|---------------|--------|
| Full ECDSA P-256 Verify | Stub returns true; needs secp256r1 arithmetic, big integer ops | High |
| AES-GCM Authenticated Encrypt/Decrypt | Structure defined; GHASH+CTR mode stubs | Medium |
| USB Hardware Abstraction Layer | Pure software simulation; needs peripheral driver | High |

## Priority 2: Partial Coverage

| Gap | Current | Missing |
|-----|---------|---------|
| XMODEM Retry with Timeout | Packet verification done | NAK/timeout retry loop |
| HTTP OTA with TLS | Simulated download | TCP/TLS stack integration |
| YMODEM Batch Transfer | Header parsing done | Full batch file transfer |

## Priority 3: Documentation Only (per SKILL.md)

| Topic | Status |
|-------|--------|
| Post-Quantum Boot (XMSS/SPHINCS+) | Documented concept |
| Blockchain Firmware Attestation | Documented concept |

## Resolved Gaps
- [x] Boot sequence const-correctness
- [x] DFU state transition table completeness
- [x] Intel HEX checksum verification
- [x] Flash wear leveling data tracking
- [x] SHA-256 empty hash test vector
