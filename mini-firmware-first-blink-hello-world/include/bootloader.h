/*
 * bootloader.h — MCU Bootloader: Firmware Update and Verification
 *
 * Knowledge Mapping (SKILL.md L1-L6):
 *   L1 Definitions: Bootloader, application image, vector table,
 *                   flash page, sector, CRC, firmware header
 *   L2 Concepts:    In-Application Programming (IAP), dual-bank flash,
 *                   fail-safe update (swap-back on failure),
 *                   memory map remap (VTOR relocation)
 *   L3 Math:        CRC-32 polynomial:
 *                     0x04C11DB7 (Ethernet) = x³² + x²⁶ + x²³ + x²² + x¹⁶ + x¹² + x¹¹
 *                     + x¹⁰ + x⁸ + x⁷ + x⁵ + x⁴ + x² + x + 1
 *                   CRC-16-CCITT: 0x1021 = x¹⁶ + x¹² + x⁵ + 1
 *                   Flash wear: erase cycles per sector (10k–100k typical)
 *   L4 Laws:        Flash endurance (reliability): N_cycles ∝ 1/V_program²
 *                   Error correction: Hamming distance for CRC detection
 *   L5 Algorithms:  Table-based CRC (256-entry lookup, O(n)),
 *                   Flash erase/write sequence (unlock→erase→program→lock),
 *                   Jump-to-application with stack pointer and reset vector load
 *   L6 Problems:    Over-the-air (OTA) firmware update,
 *                   factory default recovery (golden image),
 *                   secure boot with signature verification
 *
 * Course Mapping:
 *   MIT 6.004 — Boot sequence, exception handling
 *   Berkeley CS162 — Boot process, memory layout
 *   Valvano Ch.9 — Flash EEPROM programming
 *   ARMv7-M Architecture Manual §B1.5.5 — Vector Table Offset Register
 *
 * Reference: STM32F4xx Reference Manual RM0090 §3 — Embedded Flash Memory
 */

#ifndef BOOTLOADER_H
#define BOOTLOADER_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* ──────────────────────────────────────────────
 * L1 Definition: Flash Memory Layout
 *
 * Typical STM32F407 (1 MB Flash):
 *   0x0800 0000 – 0x0801 FFFF  Sector 0–3  (4 × 16 KB = 64 KB) → Bootloader
 *   0x0802 0000 – 0x0803 FFFF  Sector 4    (64 KB) → Application A
 *   0x0804 0000 – 0x0805 FFFF  Sector 5    (128 KB) → Application B
 *   0x0806 0000 – 0x0807 FFFF  Sector 6    (128 KB)
 *   0x0808 0000 – 0x080F FFFF  Sectors 7–11 (various)
 *
 * Minimum program size: 1 byte (on STM32F4 with x8 parallelism)
 * Minimum erase size: 1 sector (16–128 KB)
 * Page size for writing: 2–16 bytes depending on parallelism
 * ────────────────────────────────────────────── */

#define FLASH_BASE_ADDR          0x08000000U
#define FLASH_PAGE_SIZE          256U  /* Typical write alignment */
#define FLASH_SECTOR_SIZE        0x4000U  /* 16 KB — smallest sector */
#define FLASH_KEY1               0x45670123U
#define FLASH_KEY2               0xCDEF89ABU
#define FLASH_SR_BSY             (1U << 16)  /* Flash busy flag */
#define FLASH_CR_START           (1U << 16)  /* Erase start */
#define FLASH_CR_EOPIE           (1U << 24)  /* End-of-operation interrupt enable */

/* ──────────────────────────────────────────────
 * L1 Definition: Vector Table (ARM Cortex-M)
 *
 * The vector table is an array of 32-bit pointers at the start of
 * the image. The first two entries are critical:
 *   [0] = Initial Stack Pointer (SP) value — loaded into MSP on reset.
 *         Must point to the top of RAM (highest address + 1).
 *   [1] = Reset_Handler address — first instruction executed.
 *
 * For the application to run, the bootloader loads SP from the
 * application's vector table, then loads the reset handler address
 * into the PC. This is done by reading from the application's base
 * address before jumping.
 *
 * L2 Concept: VTOR (Vector Table Offset Register)
 *   On Cortex-M3/M4/M7, the vector table can be relocated to any
 *   128-byte aligned address by writing VTOR in the SCB.
 *   Bootloader may use VTOR at 0x08000000; application uses VTOR at
 *   its own base (e.g., 0x08020000).
 * ────────────────────────────────────────────── */
#define VECTOR_TABLE_SIZE        64U  /* 16 system + up to 48 IRQ entries */
#define VTOR_ALIGN               128U /* 7-bit alignment (2^7 = 128)      */

/* ──────────────────────────────────────────────
 * L1 Definition: Firmware Image Header
 *
 * Placed at the start of each application image.
 * The magic number identifies this as a valid firmware image.
 * The CRC covers the entire image (header + code + data) with
 * the crc32 field set to 0 during calculation.
 * ────────────────────────────────────────────── */
#define FIRMWARE_MAGIC           0x4D435546U  /* "F C U M" as little-endian hex */
#define FIRMWARE_VERSION_MAJOR   1
#define FIRMWARE_VERSION_MINOR   0

typedef struct __attribute__((packed)) {
    uint32_t magic;             /* Magic number (FIRMWARE_MAGIC)             */
    uint32_t image_size;        /* Total image size in bytes (including header) */
    uint32_t version_major;     /* Major version number                       */
    uint32_t version_minor;     /* Minor version number                       */
    uint32_t crc32;             /* CRC-32 of entire image (crc32 field = 0)   */
    uint32_t entry_point;       /* Offset from image base to entry function   */
    uint32_t load_address;      /* Base address where image should reside     */
    uint32_t reserved[9];       /* Padding to 64-byte header (flash alignment) */
} firmware_header_t;

/* ──────────────────────────────────────────────
 * L1 Definition: Boot Mode
 *
 * The bootloader decides which image to run based on:
 *   - External pin state (BOOT0/BOOT1 on STM32)
 *   - Flag in backup SRAM or option bytes
 *   - Validity of application images (CRC check)
 *
 * Priority: Force bootloader mode → Latest valid application → Golden image
 * ────────────────────────────────────────────── */
typedef enum {
    BOOT_MODE_BOOTLOADER = 0,  /* Stay in bootloader (firmware update) */
    BOOT_MODE_APP_A      = 1,  /* Run application in slot A            */
    BOOT_MODE_APP_B      = 2,  /* Run application in slot B            */
    BOOT_MODE_GOLDEN     = 3   /* Run factory golden image             */
} boot_mode_t;

/* Boot reason: why the MCU reset */
typedef enum {
    RESET_REASON_POWER_ON   = 0,
    RESET_REASON_SOFTWARE   = 1,
    RESET_REASON_WATCHDOG   = 2,
    RESET_REASON_EXTERNAL   = 3,
    RESET_REASON_BROWNOUT   = 4,
    RESET_REASON_UNKNOWN    = 5
} reset_reason_t;

/* ──────────────────────────────────────────────
 * Flash Operation Status
 * ────────────────────────────────────────────── */
typedef enum {
    FLASH_OK               = 0,
    FLASH_ERR_BUSY         = 1,
    FLASH_ERR_UNLOCK       = 2,
    FLASH_ERR_ERASE        = 3,
    FLASH_ERR_PROGRAM      = 4,
    FLASH_ERR_ALIGNMENT    = 5,
    FLASH_ERR_ADDRESS      = 6,
    FLASH_ERR_VERIFY       = 7
} flash_status_t;

/* ──────────────────────────────────────────────
 * L5 Algorithm: CRC-32 (Ethernet/MPEG-2 polynomial)
 *
 * CRC (Cyclic Redundancy Check) treats data as a polynomial over
 * GF(2), divides by the generator polynomial, and appends the
 * remainder. The receiver repeats the division — a zero remainder
 * indicates error-free transmission (with high probability).
 *
 * Ethernet CRC-32 (IEEE 802.3):
 *   G(x) = x³² + x²⁶ + x²³ + x²² + x¹⁶ + x¹² + x¹¹
 *        + x¹⁰ + x⁸ + x⁷ + x⁵ + x⁴ + x² + x + 1
 *
 * Lookup-table method: Precompute a 256-entry table of CRC values
 * for all possible bytes. Processing each byte becomes:
 *   crc = (crc >> 8) ^ table[(crc ^ byte) & 0xFF]
 *
 * Reference: Koopman & Chakravarty, "Cyclic Redundancy Code (CRC)
 *            Polynomial Selection For Embedded Networks" (2004)
 * ────────────────────────────────────────────── */

#define CRC32_POLYNOMIAL    0xEDB88320U  /* Reversed 0x04C11DB7 for LSB-first */
#define CRC32_INITIAL       0xFFFFFFFFU
#define CRC32_XOROUT        0xFFFFFFFFU

/*
 * crc32_init_table — precompute the 256-entry CRC-32 lookup table
 *
 * Must be called once before crc32_calculate().
 * Each table entry tab[i] = CRC-32 of byte value i.
 *
 * @param table: pointer to 256-element uint32_t array
 *
 * Complexity: O(256) — done once at boot
 */
void crc32_init_table(uint32_t table[256]);

/*
 * crc32_calculate — compute CRC-32 over a buffer
 *
 * Processes each byte through the lookup table.
 * Caller initialises crc to CRC32_INITIAL, then
 * xor's the result with CRC32_XOROUT after processing.
 *
 * @param table: precomputed CRC table
 * @param data:  data buffer
 * @param len:   buffer length in bytes
 * @param crc:   running CRC value (start with CRC32_INITIAL)
 * @return updated CRC value
 *
 * Complexity: O(len)
 */
uint32_t crc32_calculate(const uint32_t table[256], const uint8_t *data,
                         size_t len, uint32_t crc);

/*
 * crc32_verify — compute CRC over image and compare to stored value
 *
 * The image's crc32 header field is temporarily set to 0 during
 * computation, then restored.
 *
 * @param table:    precomputed CRC table
 * @param header:   firmware image header
 * @param image:    pointer to start of image
 * @return true if CRC matches
 *
 * Complexity: O(image_size)
 */
bool crc32_verify(const uint32_t table[256], const firmware_header_t *header,
                  const uint8_t *image);

/* ──────────────────────────────────────────────
 * Flash Programming API
 * ────────────────────────────────────────────── */

/*
 * flash_unlock — unlock the flash control register
 *
 * Writes KEY1 then KEY2 to FLASH_KEYR. If already unlocked,
 * the sequence has no effect. After unlock, FLASH_CR can be
 * written to initiate erase/program operations.
 *
 * @return FLASH_OK on success
 *
 * Reference: STM32F4 §3.5.1 — Unlocking the Flash control register
 */
flash_status_t flash_unlock(void);

/*
 * flash_lock — lock the flash control register
 *
 * Sets the LOCK bit in FLASH_CR. Prevents accidental erase/program.
 * Always lock when not actively programming flash.
 *
 * @return FLASH_OK
 */
flash_status_t flash_lock(void);

/*
 * flash_erase_sector — erase a flash sector
 *
 * Flash must be erased (all bits set to 1) before programming
 * (which can only change bits from 1 to 0). Erase works at
 * sector granularity.
 *
 * Erase sequence:
 *   1. Check BSY flag (wait until 0)
 *   2. Set SER bit in CR, write sector number to SNB field
 *   3. Set STRT bit → hardware performs erase (~1 second for 128 KB)
 *   4. Wait for BSY=0 → check status for errors
 *
 * @param sector_address: any address within the target sector
 * @return FLASH_OK or error code
 *
 * Complexity: O(sector_size / erase_speed), blocking
 * Reference: STM32F4 §3.5.2 — Erase
 */
flash_status_t flash_erase_sector(uint32_t sector_address);

/*
 * flash_program_word — program a 32-bit word into flash
 *
 * Flash programming changes bits only from 1→0. To change 0→1,
 * the sector must be erased first.
 *
 * Program sequence:
 *   1. Check BSY
 *   2. Set PG bit in CR
 *   3. Write word to target address (size depends on parallelism)
 *   4. Wait for BSY=0 → check status
 *
 * @param address: destination address (must be within flash)
 * @param data:    32-bit word to program
 * @return FLASH_OK or error code
 *
 * Complexity: O(1) per word
 * Reference: STM32F4 §3.5.3 — Programming
 */
flash_status_t flash_program_word(uint32_t address, uint32_t data);

/*
 * flash_program_buffer — program a buffer into flash
 *
 * Programs words sequentially. For STM32F4 ×8 parallelism,
 * programs 8 bytes at a time for faster throughput.
 *
 * @param dest:   destination flash address
 * @param src:    source buffer in RAM
 * @param len:    number of bytes (will be rounded up to word)
 * @return FLASH_OK or error code
 *
 * Complexity: O(len / bytes_per_word)
 */
flash_status_t flash_program_buffer(uint32_t dest, const uint8_t *src, size_t len);

/*
 * flash_verify_buffer — verify flash contents match expected data
 *
 * Performs word-by-word comparison. Used after programming to
 * confirm data integrity before CRC check.
 *
 * @param addr: flash address
 * @param data: expected data buffer
 * @param len:  length in bytes
 * @return true if all bytes match
 *
 * Complexity: O(len)
 */
bool flash_verify_buffer(uint32_t addr, const uint8_t *data, size_t len);

/* ──────────────────────────────────────────────
 * Bootloader Logic API
 * ────────────────────────────────────────────── */

/*
 * bootloader_get_reset_reason — determine why the MCU reset
 *
 * Reads RCC CSR register to check reset flags:
 *   LPWRRSTF (bit 31), WWDGRSTF (bit 30), IWDGRSTF (bit 29),
 *   SFTRSTF (bit 28), PORRSTF (bit 27), PINRSTF (bit 26),
 *   BORRSTF (bit 25).
 *
 * Clears the flags after reading for next reset detection.
 *
 * @return reset reason enumeration
 *
 * Reference: STM32F4 §6.3.23 — RCC clock control & status register
 */
reset_reason_t bootloader_get_reset_reason(void);

/*
 * bootloader_validate_image — check if a firmware image is valid
 *
 * Verifies:
 *   1. Magic number matches FIRMWARE_MAGIC
 *   2. Image size is within flash bounds
 *   3. CRC-32 of the entire image matches
 *   4. Entry point is within the image
 *
 * @param image_base: base address of the image
 * @param crc_table:  precomputed CRC-32 table
 * @return true if the image is valid and runnable
 *
 * Complexity: O(image_size) due to CRC computation
 */
bool bootloader_validate_image(uint32_t image_base, const uint32_t crc_table[256]);

/*
 * bootloader_decide_mode — determine which image to boot
 *
 * Decision logic (priority order):
 *   1. If BOOT pin is asserted → stay in bootloader for update
 *   2. If application A is valid & newer → boot A
 *   3. If application B is valid & newer → boot B
 *   4. If golden image is valid → boot golden
 *   5. Fall through → stay in bootloader (error)
 *
 * @param crc_table: precomputed CRC-32 table
 * @return selected boot mode
 */
boot_mode_t bootloader_decide_mode(const uint32_t crc_table[256]);

/*
 * bootloader_jump_to_application — transfer control to application
 *
 * CRITICAL steps (must be done in exact order):
 *   1. Disable all interrupts (__disable_irq())
 *   2. Set VTOR to the application's vector table base
 *   3. Load MSP from application vector table[0]
 *   4. Load PC from application vector table[1] → jump
 *
 * The jump is typically implemented with inline assembly:
 *   __set_MSP(*(uint32_t *)app_addr);
 *   ((void(*)(void))(*(uint32_t *)(app_addr + 4)))();
 *
 * @param app_base: base address of application image
 *
 * Complexity: O(1)
 * Caution: This function does NOT return.
 *
 * Reference: ARMv7-M Architecture Manual §B1.5.5 — VTOR
 */
void bootloader_jump_to_application(uint32_t app_base) __attribute__((noreturn));

/*
 * bootloader_copy_image — copy firmware from RAM buffer to flash
 *
 * Used during firmware update:
 *   1. Erase target sector(s)
 *   2. Program image from RAM buffer
 *   3. Verify programmed data
 *
 * @param dest_addr: destination in flash
 * @param src:       source buffer in RAM
 * @param size:      image size in bytes
 * @return FLASH_OK on success
 *
 * Complexity: O(size) — erase + program + verify
 */
flash_status_t bootloader_copy_image(uint32_t dest_addr, const uint8_t *src,
                                     size_t size);

/*
 * bootloader_enter — main bootloader entry point
 *
 * Runs the bootloader loop: check for update requests over UART/USB,
 * receive new firmware, validate, program, and boot.
 *
 * @param update_port: UART index for firmware reception
 * @return never returns (jumps to app or loops in bootloader)
 */
void bootloader_enter(uint8_t update_port) __attribute__((noreturn));

#endif /* BOOTLOADER_H */
