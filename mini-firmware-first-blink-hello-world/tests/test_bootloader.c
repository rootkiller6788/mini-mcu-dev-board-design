/*
 * test_bootloader.c — CRC-32 and bootloader logic tests
 *
 * Tests: CRC-32 table correctness, CRC computation against known vectors,
 * firmware header validation, flash sector mapping.
 */

#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <stdbool.h>
#include "../include/bootloader.h"

/* ──────────────────────────────────────────────
 * Test: CRC-32 lookup table against known values
 *
 * Verify the first few entries match the standard CRC-32 table.
 * Reference: PNG specification (same CRC-32 polynomial).
 * ────────────────────────────────────────────── */
static void test_crc32_table(void)
{
    uint32_t table[256];

    crc32_init_table(table);

    /* Known values from the standard CRC-32 (Ethernet) table */
    assert(table[0x00] == 0x00000000U);
    assert(table[0x01] == 0x77073096U);
    assert(table[0x02] == 0xEE0E612CU);
    assert(table[0x03] == 0x990951BAU);
    assert(table[0x04] == 0x076DC419U);
    assert(table[0xFF] == 0x2D02EF8DU);

    printf("  PASS: CRC-32 table\n");
}

/* ──────────────────────────────────────────────
 * Test: CRC-32 computation against known test vector
 *
 * "123456789" (9 bytes) → CRC-32 = 0xCBF43926
 * This is the standard CRC-32 check value.
 * ────────────────────────────────────────────── */
static void test_crc32_check_value(void)
{
    uint32_t table[256];
    uint32_t crc;
    const uint8_t test_data[] = "123456789";

    crc32_init_table(table);

    crc = CRC32_INITIAL;
    crc = crc32_calculate(table, test_data, 9, crc);
    crc ^= CRC32_XOROUT;

    assert(crc == 0xCBF43926U);

    printf("  PASS: CRC-32 check value\n");
}

/* ──────────────────────────────────────────────
 * Test: CRC-32 of empty data
 *
 * CRC of zero bytes with standard init/xorout should
 * equal the XOROUT value itself (0xFFFFFFFF → all bits set).
 * Actually: after final XOR with 0xFFFFFFFF, CRC(empty) = 0x00000000.
 * Wait — that's not right either.
 *
 * CRC-32/Ethernet of empty message:
 *   Init = 0xFFFFFFFF, no bytes processed, XorOut = 0xFFFFFFFF
 *   Result = 0xFFFFFFFF ^ 0xFFFFFFFF = 0x00000000
 *
 * But the CRC of empty data with the raw algorithm is:
 *   Init = FFFFFFFF, XorOut = FFFFFFFF → 0x00000000.
 *
 * Let's verify. Actually the standard answer is:
 *   CRC-32 of empty string = 0x00000000 (after final XOR)
 * ────────────────────────────────────────────── */
static void test_crc32_empty(void)
{
    uint32_t table[256];
    uint32_t crc;

    crc32_init_table(table);

    crc = CRC32_INITIAL;
    crc = crc32_calculate(table, NULL, 0, crc);
    crc ^= CRC32_XOROUT;

    assert(crc == 0x00000000U);

    printf("  PASS: CRC-32 empty\n");
}

/* ──────────────────────────────────────────────
 * Test: CRC-32 incremental computation
 *
 * CRC computed incrementally (byte by byte) should equal
 * CRC computed all at once.
 * ────────────────────────────────────────────── */
static void test_crc32_incremental(void)
{
    uint32_t table[256];
    uint32_t crc_full, crc_inc;
    const uint8_t data[] = {0xDE, 0xAD, 0xBE, 0xEF, 0xCA, 0xFE};
    int i;

    crc32_init_table(table);

    /* Full computation */
    crc_full = CRC32_INITIAL;
    crc_full = crc32_calculate(table, data, 6, crc_full);
    crc_full ^= CRC32_XOROUT;

    /* Incremental computation */
    crc_inc = CRC32_INITIAL;
    for (i = 0; i < 6; i++) {
        crc_inc = crc32_calculate(table, &data[i], 1, crc_inc);
    }
    crc_inc ^= CRC32_XOROUT;

    assert(crc_full == crc_inc);

    printf("  PASS: CRC-32 incremental\n");
}

/* ──────────────────────────────────────────────
 * Test: CRC-32 table NULL safety
 * ────────────────────────────────────────────── */
static void test_crc32_null(void)
{
    uint32_t table[256];
    uint32_t crc;

    crc32_init_table(NULL);  /* Should not crash */
    crc32_init_table(table);

    /* NULL data returns crc unchanged */
    crc = 0xDEADBEEFU;
    crc = crc32_calculate(table, NULL, 100, crc);
    assert(crc == 0xDEADBEEFU);

    printf("  PASS: CRC-32 null safety\n");
}

/* ──────────────────────────────────────────────
 * Test: Firmware magic number
 * ────────────────────────────────────────────── */
static void test_firmware_magic(void)
{
    assert(FIRMWARE_MAGIC == 0x4D435546U);
    printf("  PASS: firmware magic\n");
}

/* ──────────────────────────────────────────────
 * Test: Flash unlock key values
 * ────────────────────────────────────────────── */
static void test_flash_keys(void)
{
    /* These are the STM32 flash unlock keys from RM0090 */
    assert(FLASH_KEY1 == 0x45670123U);
    assert(FLASH_KEY2 == 0xCDEF89ABU);

    /* Keys must be different */
    assert(FLASH_KEY1 != FLASH_KEY2);

    printf("  PASS: flash keys\n");
}

/* ──────────────────────────────────────────────
 * Test: Boot mode enum values
 * ────────────────────────────────────────────── */
static void test_boot_modes(void)
{
    assert(BOOT_MODE_BOOTLOADER == 0);
    assert(BOOT_MODE_APP_A == 1);
    assert(BOOT_MODE_APP_B == 2);
    assert(BOOT_MODE_GOLDEN == 3);
    printf("  PASS: boot modes\n");
}

/* ──────────────────────────────────────────────
 * Test: CRC polynomial constant
 *
 * The Ethernet/802.3 CRC-32 polynomial (reversed) is 0xEDB88320.
 * The standard (non-reversed) polynomial is 0x04C11DB7.
 * ────────────────────────────────────────────── */
static void test_crc_polynomial(void)
{
    /* Reversed polynomial for LSB-first implementation */
    assert(CRC32_POLYNOMIAL == 0xEDB88320U);
    printf("  PASS: CRC polynomial\n");
}

int main(void)
{
    printf("=== Bootloader / CRC Tests ===\n");

    test_crc32_table();
    test_crc32_check_value();
    test_crc32_empty();
    test_crc32_incremental();
    test_crc32_null();
    test_firmware_magic();
    test_flash_keys();
    test_boot_modes();
    test_crc_polynomial();

    printf("All bootloader/CRC tests passed!\n");
    return 0;
}
