/*
 * bootloader.c — Bootloader Implementation
 *
 * Knowledge points implemented (independent):
 *   1. crc32_init_table — 256-entry CRC-32 lookup table precomputation
 *   2. crc32_calculate — software CRC-32 over arbitrary buffer
 *   3. crc32_verify — firmware image integrity verification
 *   4. flash_unlock/lock — flash control register key sequence
 *   5. flash_erase_sector — sector erase with BSY polling
 *   6. flash_program_word — word-level flash programming
 *   7. flash_program_buffer — multi-word flash programming
 *   8. flash_verify_buffer — post-program verification
 *   9. bootloader_validate_image — multi-factor image validation
 *  10. bootloader_jump_to_application — vector table based jump
 */

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "bootloader.h"
#include "cortex_m.h"
#include "arch_compat.h"

/* ──────────────────────────────────────────────
 * Flash Register Map (STM32F4)
 *
 * FLASH base: 0x40023C00
 * Key Register (FLASH_KEYR): write unlock keys
 * Control Register (FLASH_CR): program/erase control
 * Status Register (FLASH_SR): busy, errors, EOP
 * ────────────────────────────────────────────── */

typedef struct {
    volatile uint32_t ACR;       /* 0x00: Access Control */
    volatile uint32_t KEYR;      /* 0x04: Key Register */
    volatile uint32_t OPTKEYR;   /* 0x08: Option Byte Key Register */
    volatile uint32_t SR;        /* 0x0C: Status Register */
    volatile uint32_t CR;        /* 0x10: Control Register */
    volatile uint32_t OPTCR;     /* 0x14: Option Control Register */
} flash_regs_t;

#define FLASH_REGS  ((flash_regs_t *)0x40023C00U)

/* FLASH_CR bits */
#define FLASH_CR_PG      (1U << 0)    /* Program */
#define FLASH_CR_SER     (1U << 1)    /* Sector Erase */
#define FLASH_CR_MER     (1U << 2)    /* Mass Erase (all sectors) */
#define FLASH_CR_SNB_POS 3U
#define FLASH_CR_SNB_MASK (0xFU << 3U) /* Sector Number */
#define FLASH_CR_STRT    (1U << 16)   /* Start Erase */
#define FLASH_CR_LOCK    (1U << 31)   /* Lock */

/* FLASH_SR bits */
#define FLASH_SR_EOP     (1U << 0)    /* End of Operation */
#define FLASH_SR_OPERR   (1U << 1)    /* Operation Error */
#define FLASH_SR_WRPERR  (1U << 4)    /* Write Protection Error */
#define FLASH_SR_PGAERR  (1U << 5)    /* Programming Alignment Error */
#define FLASH_SR_PGPERR  (1U << 6)    /* Programming Parallelism Error */
#define FLASH_SR_PGSERR  (1U << 7)    /* Programming Sequence Error */
#define FLASH_SR_BSY     (1U << 16)   /* Busy */

/* Option bytes base */
#define OPT_BYTES_BASE  0x1FFFC000U

/* ──────────────────────────────────────────────
 * crc32_init_table — precompute CRC-32 lookup table
 *
 * Knowledge: CRC polynomial in GF(2).
 *
 * CRC-32 (IEEE 802.3 / MPEG-2):
 *   G(x) = x³² + x²⁶ + x²³ + x²² + x¹⁶ + x¹² + x¹¹
 *        + x¹⁰ + x⁸ + x⁷ + x⁵ + x⁴ + x² + x + 1
 *
 * Reversed polynomial (for LSB-first processing): 0xEDB88320
 *
 * The table-based CRC algorithm treats data as a polynomial
 * and uses precomputed remainders for each possible byte value.
 * Without the table, CRC would be O(n) per bit shift; with the
 * table, it's O(n) per byte (8× faster).
 *
 * CRC detection properties for CRC-32:
 *   - All single-bit errors
 *   - All double-bit errors (for messages < 2³² − 1 bits)
 *   - All odd number of bit errors
 *   - All burst errors ≤ 32 bits
 *   - 99.99999998% of burst errors > 32 bits
 *
 * Reference: Koopman, "32-Bit Cyclic Redundancy Codes for Internet
 *            Applications" (2002), DSN Conference.
 * ────────────────────────────────────────────── */

void crc32_init_table(uint32_t table[256])
{
    uint32_t i, j;
    uint32_t crc;

    if (table == NULL) {
        return;
    }

    for (i = 0; i < 256U; i++) {
        crc = i;
        for (j = 0; j < 8U; j++) {
            if (crc & 1U) {
                crc = (crc >> 1U) ^ CRC32_POLYNOMIAL;
            } else {
                crc = (crc >> 1U);
            }
        }
        table[i] = crc;
    }
}

/* ──────────────────────────────────────────────
 * crc32_calculate — compute CRC-32 over arbitrary data
 *
 * Knowledge: Table-driven CRC.
 *
 * Algorithm:
 *   1. Initialize crc = CRC32_INITIAL (0xFFFFFFFF)
 *   2. For each byte: crc = (crc >> 8) ^ table[(crc ^ byte) & 0xFF]
 *   3. Finalize: crc ^= CRC32_XOROUT (0xFFFFFFFF)
 *
 * The XOROUT inverts all bits, which means the CRC of a message
 * followed by its CRC (in LSB order) equals a constant "magic number"
 * (0x38FB2284 for CRC-32/Ethernet), enabling the receiver to verify
 * by computing CRC over (data + received_crc) and checking for the
 * magic constant.
 * ────────────────────────────────────────────── */

uint32_t crc32_calculate(const uint32_t table[256], const uint8_t *data,
                         size_t len, uint32_t crc)
{
    size_t i;

    if (data == NULL || table == NULL) {
        return crc;
    }

    for (i = 0; i < len; i++) {
        crc = (crc >> 8U) ^ table[(crc ^ (uint32_t)data[i]) & 0xFFU];
    }

    return crc;
}

/* ──────────────────────────────────────────────
 * crc32_verify — verify firmware image CRC
 *
 * Knowledge: CRC-32 integrity check for firmware images.
 *
 * Procedure:
 *   1. Store the original crc32 header field.
 *   2. Zero the crc32 field in the header.
 *   3. Compute CRC over the entire image (crc32 field = 0).
 *   4. Restore the original crc32 field.
 *   5. Compare computed CRC against stored CRC.
 *
 * Why zero the field? The crc32 stored in the header was computed
 * with that field set to zero. Otherwise, the CRC would be
 * self-referential (the CRC value affects its own computation).
 * ────────────────────────────────────────────── */

bool crc32_verify(const uint32_t table[256], const firmware_header_t *header,
                  const uint8_t *image)
{
    uint32_t stored_crc;
    uint32_t computed_crc;
    firmware_header_t *mutable_header;

    if (table == NULL || header == NULL || image == NULL) {
        return false;
    }

    /* Check magic number first (fast rejection) */
    if (header->magic != FIRMWARE_MAGIC) {
        return false;
    }

    /* Check that image_size is reasonable */
    if (header->image_size < sizeof(firmware_header_t)) {
        return false;
    }

    /* Save and zero the crc32 field */
    stored_crc = header->crc32;

    /* Use a writable copy of the header */
    mutable_header = (firmware_header_t *)image;
    mutable_header->crc32 = 0U;

    /* Compute CRC over entire image */
    computed_crc = CRC32_INITIAL;
    computed_crc = crc32_calculate(table, image, header->image_size, computed_crc);
    computed_crc ^= CRC32_XOROUT;

    /* Restore header */
    mutable_header->crc32 = stored_crc;

    return (computed_crc == stored_crc);
}

/* ──────────────────────────────────────────────
 * flash_unlock — unlock flash control register
 *
 * Knowledge: Flash protection key mechanism.
 *
 * After reset, the FLASH_CR is locked (LOCK=1). Two consecutive
 * writes of KEY1=0x45670123 and KEY2=0xCDEF89AB to FLASH_KEYR
 * unlock the register. Any other sequence or a system reset
 * re-locks the flash.
 *
 * This mechanism prevents accidental flash corruption from a
 * runaway pointer writing random values to FLASH_CR.
 * ────────────────────────────────────────────── */

flash_status_t flash_unlock(void)
{
    flash_regs_t *flash = FLASH_REGS;

    /* Check if already unlocked */
    if ((flash->CR & FLASH_CR_LOCK) == 0U) {
        return FLASH_OK;
    }

    /* Write key sequence */
    flash->KEYR = FLASH_KEY1;
    flash->KEYR = FLASH_KEY2;

    /* Verify unlock */
    if (flash->CR & FLASH_CR_LOCK) {
        return FLASH_ERR_UNLOCK;
    }

    return FLASH_OK;
}

/* ──────────────────────────────────────────────
 * flash_lock — lock flash control register
 * ────────────────────────────────────────────── */

flash_status_t flash_lock(void)
{
    flash_regs_t *flash = FLASH_REGS;
    flash->CR |= FLASH_CR_LOCK;
    return FLASH_OK;
}

/* ──────────────────────────────────────────────
 * flash_wait_busy — wait for flash operation to complete
 *
 * Polls the BSY flag. Returns error if operation failed.
 * ────────────────────────────────────────────── */

static flash_status_t flash_wait_busy(void)
{
    flash_regs_t *flash = FLASH_REGS;

    /* Wait for BSY to clear */
    while (flash->SR & FLASH_SR_BSY) {
        /* Spin-wait. On real hardware, could add a timeout here. */
    }

    /* Check for errors */
    if (flash->SR & (FLASH_SR_OPERR | FLASH_SR_WRPERR | FLASH_SR_PGAERR |
                     FLASH_SR_PGPERR | FLASH_SR_PGSERR)) {
        return FLASH_ERR_PROGRAM;
    }

    /* Check EOP */
    if (flash->SR & FLASH_SR_EOP) {
        flash->SR = FLASH_SR_EOP;  /* Clear EOP by writing 1 */
    }

    return FLASH_OK;
}

/* ──────────────────────────────────────────────
 * flash_erase_sector — erase a flash sector
 *
 * Knowledge: Flash sector erase operation.
 *
 * Flash memory must be erased before programming. Erase sets all
 * bits in the sector to 1. Programming can only change individual
 * bits from 1 → 0 (via hot-carrier injection or Fowler-Nordheim
 * tunneling, depending on flash technology).
 *
 * Sector mapping (STM32F407, 1 MB flash):
 *   Sector 0: 0x08000000 – 0x08003FFF (16 KB)
 *   Sector 1: 0x08004000 – 0x08007FFF (16 KB)
 *   Sector 2: 0x08008000 – 0x0800BFFF (16 KB)
 *   Sector 3: 0x0800C000 – 0x0800FFFF (16 KB)
 *   Sector 4: 0x08010000 – 0x0801FFFF (64 KB)
 *   Sector 5: 0x08020000 – 0x0803FFFF (128 KB)
 *   Sector 6: 0x08040000 – 0x0805FFFF (128 KB)
 *   Sectors 7–11: larger sectors...
 *
 * Erase time: ~1 second for a 128 KB sector. Blocking.
 * ────────────────────────────────────────────── */

flash_status_t flash_erase_sector(uint32_t sector_address)
{
    flash_regs_t *flash = FLASH_REGS;
    flash_status_t status;
    uint32_t sector_number;

    /* Validate address */
    if (sector_address < FLASH_BASE_ADDR) {
        return FLASH_ERR_ADDRESS;
    }

    /* Unlock if necessary */
    status = flash_unlock();
    if (status != FLASH_OK) {
        return status;
    }

    /* Wait until not busy */
    status = flash_wait_busy();
    if (status != FLASH_OK) {
        return status;
    }

    /* Determine sector number from address.
     * Simplified: sector = (addr - base) / sector_size for small sectors. */
    {
        uint32_t offset = sector_address - FLASH_BASE_ADDR;

        if (offset < 0x10000U) {
            /* Sectors 0–3: 16 KB each */
            sector_number = offset / 0x4000U;
        } else if (offset < 0x20000U) {
            sector_number = 4U;  /* 64 KB */
        } else if (offset < 0x40000U) {
            sector_number = 5U;  /* 128 KB */
        } else if (offset < 0x60000U) {
            sector_number = 6U;  /* 128 KB */
        } else if (offset < 0x80000U) {
            sector_number = 7U;  /* 128 KB */
        } else if (offset < 0xA0000U) {
            sector_number = 8U;  /* 128 KB */
        } else if (offset < 0xC0000U) {
            sector_number = 9U;  /* 128 KB */
        } else if (offset < 0xE0000U) {
            sector_number = 10U; /* 128 KB */
        } else {
            sector_number = 11U; /* 128 KB */
        }
    }

    /* Configure sector erase */
    flash->CR &= ~FLASH_CR_SNB_MASK;
    flash->CR |= (sector_number << FLASH_CR_SNB_POS) & FLASH_CR_SNB_MASK;
    flash->CR |= FLASH_CR_SER;  /* Sector Erase enable */

    /* Start erase */
    flash->CR |= FLASH_CR_STRT;

    /* Wait for completion */
    status = flash_wait_busy();

    /* Clear SER bit */
    flash->CR &= ~FLASH_CR_SER;

    return status;
}

/* ──────────────────────────────────────────────
 * flash_program_word — program a single 32-bit word
 *
 * Knowledge: Flash word programming.
 *
 * Flash programming works at word (32-bit) granularity on STM32F4.
 * The PG bit enables programming mode, and each write to the target
 * address initiates a programming operation.
 *
 * Parallelism (PSIZE in FLASH_CR):
 *   ×8  — programs 64 bits at a time (2 words) → fastest
 *   ×16 — programs 128 bits at a time (4 words)
 *   ×32 — programs 256 bits at a time (8 words)
 *   ×64 — programs 512 bits at a time (16 words, F4 series)
 *
 * This implementation uses ×8 (PSIZE = 00, default after reset).
 * ────────────────────────────────────────────── */

flash_status_t flash_program_word(uint32_t address, uint32_t data)
{
    flash_regs_t *flash = FLASH_REGS;
    flash_status_t status;

    /* Wait until not busy */
    status = flash_wait_busy();
    if (status != FLASH_OK) {
        return status;
    }

    /* Set PG bit */
    flash->CR |= FLASH_CR_PG;

    /* Write to flash address */
    *(volatile uint32_t *)(uintptr_t)address = data;

    /* Memory barrier to ensure write before checking flags */
    DSB();

    /* Wait for completion */
    status = flash_wait_busy();

    /* Clear PG bit */
    flash->CR &= ~FLASH_CR_PG;

    return status;
}

/* ──────────────────────────────────────────────
 * flash_program_buffer — program a buffer into flash
 *
 * Knowledge: Sequential flash programming.
 *
 * Programs data word by word. Padding bytes are programmed as 0xFFFFFFFF
 * (which has no effect since flash is already all-1s after erase).
 *
 * For production code, use dual-buffering with the flash controller's
 * FIFO to pipeline writes and achieve higher throughput.
 * ────────────────────────────────────────────── */

flash_status_t flash_program_buffer(uint32_t dest, const uint8_t *src, size_t len)
{
    flash_status_t status;
    size_t i;
    uint32_t word;
    size_t aligned_len;

    if (src == NULL || len == 0) {
        return FLASH_OK;
    }

    if (dest < FLASH_BASE_ADDR) {
        return FLASH_ERR_ADDRESS;
    }

    /* Must be 4-byte aligned */
    if ((dest & 0x3U) != 0U) {
        return FLASH_ERR_ALIGNMENT;
    }

    /* Program word by word. Source may be unaligned, but destination is aligned. */
    aligned_len = (len + 3U) & ~3U;  /* Round up to 4-byte boundary */

    for (i = 0; i < aligned_len; i += 4U) {
        /* Assemble a word from up to 4 source bytes (little-endian) */
        word = 0xFFFFFFFFU;  /* Default: no change if beyond source length */

        if (i < len) {
            word = (uint32_t)src[i];
        }
        if ((i + 1U) < len) {
            word |= ((uint32_t)src[i + 1U]) << 8U;
        }
        if ((i + 2U) < len) {
            word |= ((uint32_t)src[i + 2U]) << 16U;
        }
        if ((i + 3U) < len) {
            word |= ((uint32_t)src[i + 3U]) << 24U;
        }

        status = flash_program_word(dest + (uint32_t)i, word);
        if (status != FLASH_OK) {
            return status;
        }
    }

    return FLASH_OK;
}

/* ──────────────────────────────────────────────
 * flash_verify_buffer — verify flash contents
 *
 * Knowledge: Post-program read-back verification.
 *
 * After programming, every word should be read back and compared
 * against the expected value. This catches:
 *   - Programming errors (PGPERR, PGSERR)
 *   - Write protection errors (WRPERR)
 *   - Alignment errors (PGAERR)
 *
 * On STM32F4 with ECC (Error Correction Code), single-bit errors
 * in flash are automatically corrected, but the SR flags them for
 * monitoring. Double-bit errors cause a bus fault or NMI.
 * ────────────────────────────────────────────── */

bool flash_verify_buffer(uint32_t addr, const uint8_t *data, size_t len)
{
    size_t i;
    uint32_t word_expected;
    uint32_t word_actual;
    size_t aligned_len = (len + 3U) & ~3U;

    if (data == NULL) {
        return false;
    }

    for (i = 0; i < aligned_len; i += 4U) {
        /* Expected word */
        word_expected = 0xFFFFFFFFU;
        if (i < len) {
            word_expected = (uint32_t)data[i];
        }
        if ((i + 1U) < len) {
            word_expected |= ((uint32_t)data[i + 1U]) << 8U;
        }
        if ((i + 2U) < len) {
            word_expected |= ((uint32_t)data[i + 2U]) << 16U;
        }
        if ((i + 3U) < len) {
            word_expected |= ((uint32_t)data[i + 3U]) << 24U;
        }

        /* Read actual from flash */
        word_actual = *(volatile uint32_t *)(uintptr_t)(addr + i);

        if (word_actual != word_expected) {
            return false;
        }
    }

    return true;
}

/* ──────────────────────────────────────────────
 * bootloader_validate_image — validate firmware image
 *
 * Knowledge: Multi-factor firmware validation.
 *
 * Checks:
 *   1. Magic number — identifies this as a valid firmware image
 *   2. Image size — within flash bounds for this slot
 *   3. CRC-32 — integrity check covering entire image
 *   4. Entry point — within the image boundaries
 *   5. Load address — valid flash address
 *
 * A valid image must pass ALL checks. Any failure means the image
 * is corrupt and must not be executed.
 * ────────────────────────────────────────────── */

bool bootloader_validate_image(uint32_t image_base, const uint32_t crc_table[256])
{
    const firmware_header_t *header;
    const uint8_t *image;

    if (crc_table == NULL) {
        return false;
    }

    header = (const firmware_header_t *)(uintptr_t)image_base;
    image = (const uint8_t *)(uintptr_t)image_base;

    /* 1. Magic number */
    if (header->magic != FIRMWARE_MAGIC) {
        return false;
    }

    /* 2. Image size bounds */
    if (header->image_size < sizeof(firmware_header_t) ||
        header->image_size > (1024U * 1024U)) {  /* Max 1 MB */
        return false;
    }

    /* 3. CRC-32 verification */
    if (!crc32_verify(crc_table, header, image)) {
        return false;
    }

    /* 4. Entry point within image */
    if (header->entry_point >= header->image_size) {
        return false;
    }

    /* 5. Load address is within flash */
    if (header->load_address < FLASH_BASE_ADDR) {
        return false;
    }

    return true;
}

/* ──────────────────────────────────────────────
 * bootloader_jump_to_application — transfer control
 *
 * Knowledge: ARM Cortex-M boot chain.
 *
 * When the MCU resets:
 *   1. Hardware loads SP from address 0x00000000 (aliased to flash)
 *   2. Hardware loads PC from address 0x00000004 (Reset_Handler)
 *
 * The bootloader simulates this reset sequence for the application:
 *   1. Disable all interrupts (__disable_irq / CPSID I)
 *   2. Set VTOR to application's vector table base
 *   3. Load SP_main from application's vector table[0]
 *   4. Load PC from application's vector table[1]
 *   5. Branch to the application's reset handler
 *
 * The vector table format is a requirement of the ARMv7-M
 * architecture: word[0] = initial SP value, word[1] = reset vector.
 *
 * After the jump, the application never returns to the bootloader.
 *
 * Reference: ARMv7-M Architecture Manual §B1.5.2 — Exception vectors
 * ────────────────────────────────────────────── */

void bootloader_jump_to_application(uint32_t app_base)
{
    uint32_t app_sp;
    uint32_t app_pc;
    uint32_t *vector_table;

    /* Disable global interrupts */
    cortex_m_disable_irq();

    /* The application vector table is at the start of the image */
    vector_table = (uint32_t *)(uintptr_t)app_base;

    /* Load the application stack pointer (vector_table[0]) */
    app_sp = vector_table[0];

    /* Load the application reset handler (vector_table[1]) */
    app_pc = vector_table[1];

    /* Relocate the vector table to the application's base */
    {
        scb_regs_t *scb = (scb_regs_t *)0xE000ED00U;
        scb->VTOR = app_base & ~(VTOR_ALIGN - 1U);
    }

    /* Set the main stack pointer to the application's SP */
    SET_MSP(app_sp);

    /* Jump to the application reset handler.
     * This loads PC with app_pc, setting the T-bit (thumb mode).
     * The LSb must be 1 to indicate Thumb state. */
    BX(app_pc);

    /* Unreachable */
    for (;;);
}

/* ──────────────────────────────────────────────
 * bootloader_decide_mode — determine boot image
 *
 * Knowledge: Boot decision logic with fallback chain.
 *
 * Priority: Boot pin → App A → App B → Golden → Stay in bootloader.
 * ────────────────────────────────────────────── */

boot_mode_t bootloader_decide_mode(const uint32_t crc_table[256])
{
    /* Application slot addresses (based on flash layout) */
    const uint32_t app_a_base = 0x08020000U;
    const uint32_t app_b_base = 0x08040000U;
    const uint32_t golden_base = 0x08060000U;

    /* Priority: Application A (newer), then B, then Golden */
    if (bootloader_validate_image(app_a_base, crc_table)) {
        const firmware_header_t *hdr_a = (const firmware_header_t *)(uintptr_t)app_a_base;
        if (bootloader_validate_image(app_b_base, crc_table)) {
            const firmware_header_t *hdr_b = (const firmware_header_t *)(uintptr_t)app_b_base;
            /* Compare versions: assume version encoded as major << 16 | minor */
            uint32_t ver_a = (hdr_a->version_major << 16) | hdr_a->version_minor;
            uint32_t ver_b = (hdr_b->version_major << 16) | hdr_b->version_minor;
            if (ver_a >= ver_b) {
                return BOOT_MODE_APP_A;
            } else {
                return BOOT_MODE_APP_B;
            }
        }
        return BOOT_MODE_APP_A;
    }

    if (bootloader_validate_image(app_b_base, crc_table)) {
        return BOOT_MODE_APP_B;
    }

    if (bootloader_validate_image(golden_base, crc_table)) {
        return BOOT_MODE_GOLDEN;
    }

    /* No valid image found — stay in bootloader */
    return BOOT_MODE_BOOTLOADER;
}

/* ──────────────────────────────────────────────
 * bootloader_copy_image — firmware update (flash programming)
 * ────────────────────────────────────────────── */

flash_status_t bootloader_copy_image(uint32_t dest_addr, const uint8_t *src,
                                     size_t size)
{
    flash_status_t status;

    if (src == NULL || size == 0) {
        return FLASH_ERR_ADDRESS;
    }

    /* 1. Unlock flash */
    status = flash_unlock();
    if (status != FLASH_OK) {
        return status;
    }

    /* 2. Erase target sector */
    status = flash_erase_sector(dest_addr);
    if (status != FLASH_OK) {
        flash_lock();
        return status;
    }

    /* 3. Program image */
    status = flash_program_buffer(dest_addr, src, size);
    if (status != FLASH_OK) {
        flash_lock();
        return status;
    }

    /* 4. Verify */
    if (!flash_verify_buffer(dest_addr, src, size)) {
        flash_lock();
        return FLASH_ERR_VERIFY;
    }

    /* 5. Lock flash */
    status = flash_lock();

    return status;
}

/* ──────────────────────────────────────────────
 * bootloader_get_reset_reason — determine reset cause
 * ────────────────────────────────────────────── */

reset_reason_t bootloader_get_reset_reason(void)
{
    /* RCC CSR is at 0x40023800 + 0x74 = 0x40023874 */
    volatile uint32_t *rcc_csr = (volatile uint32_t *)0x40023874U;
    uint32_t csr = *rcc_csr;

    /* Clear reset flags for next detection */
    *rcc_csr |= (1U << 24);  /* RMVF: Remove reset flag */

    if (csr & RCC_CSR_PORRSTF)    return RESET_REASON_POWER_ON;
    if (csr & RCC_CSR_PINRSTF)    return RESET_REASON_EXTERNAL;
    if (csr & RCC_CSR_BORRSTF)    return RESET_REASON_BROWNOUT;
    if (csr & RCC_CSR_SFTRSTF)    return RESET_REASON_SOFTWARE;
    if (csr & RCC_CSR_IWDGRSTF)   return RESET_REASON_WATCHDOG;
    if (csr & RCC_CSR_WWDGRSTF)   return RESET_REASON_WATCHDOG;
    if (csr & RCC_CSR_LPWRRSTF)   return RESET_REASON_POWER_ON;

    return RESET_REASON_UNKNOWN;
}

/* ──────────────────────────────────────────────
 * bootloader_enter — main bootloader entry
 * ────────────────────────────────────────────── */

void bootloader_enter(uint8_t update_port)
{
    uint32_t crc_table[256];
    boot_mode_t mode;

    (void)update_port;  /* Used for firmware reception UART index */

    /* Initialize CRC-32 table (one-time) */
    crc32_init_table(crc_table);

    /* Decide which image to boot */
    mode = bootloader_decide_mode(crc_table);

    switch (mode) {
    case BOOT_MODE_APP_A:
        bootloader_jump_to_application(0x08020000U);
        break;
    case BOOT_MODE_APP_B:
        bootloader_jump_to_application(0x08040000U);
        break;
    case BOOT_MODE_GOLDEN:
        bootloader_jump_to_application(0x08060000U);
        break;
    case BOOT_MODE_BOOTLOADER:
    default:
        /* Stay in bootloader — wait for firmware update over UART */
        /* (In a real implementation, this would start a communication
         *  protocol like XMODEM/YMODEM to receive the new firmware.) */
        while (1) {
            /* Bootloader idle loop — waiting for update command */
        }
        break;
    }

    /* Unreachable */
}
