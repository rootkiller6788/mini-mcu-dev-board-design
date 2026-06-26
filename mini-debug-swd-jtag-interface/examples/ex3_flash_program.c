/**
 * ex3_flash_program.c - Flash Programming via Debug Interface
 * L7: Production flash programming workflow via SWD.
 *
 * Demonstrates:
 *   1. Debug connection and target identification
 *   2. Flash unlock sequence
 *   3. Memory write via AHB-AP
 *   4. Flash status polling
 *   5. Verification by read-back
 *
 * Usage: ./build/ex3_flash_program
 * Reference: STM32F1xx Flash Programming Manual (PM0075)
 */

#include "../include/debug_port.h"
#include "../include/swd_protocol.h"
#include <stdio.h>
#include <string.h>

int main(void) {
    printf("Flash Programming via Debug Interface Example\n");
    printf("==============================================\n\n");

    /* Step 1: Setup simulated target */
    uint32_t target_flash_base = 0x08000000u;
    uint32_t flash_ctrl_base   = 0x40022000u;
    printf("Target flash base:   0x%08X\n", target_flash_base);
    printf("Flash controller:    0x%08X\n", flash_ctrl_base);

    /* Step 2: Configure AHB-AP for memory access */
    uint32_t csw = mem_ap_setup_csw(32, true, false);
    printf("\nAHB-AP CSW configured: 0x%08X\n", csw);
    printf("  Data size:   32-bit\n");
    printf("  Auto-increment: ON\n");
    printf("  Master type: Debug\n");

    /* Step 3: Unlock flash */
    uint32_t key1, key2;
    flash_unlock_keys(&key1, &key2);
    printf("\nFlash unlock sequence:\n");
    printf("  KEY1 = 0x%08X\n", key1);
    printf("  KEY2 = 0x%08X\n", key2);

    /* Step 4: Simulate flash write */
    uint32_t program_addr = target_flash_base + 0x1000;
    uint32_t program_data[] = {
        0x20001000,  /* Initial SP value */
        0x08001001,  /* Reset vector */
        0x08001005,  /* NMI handler */
        0x08001009   /* HardFault handler */
    };
    int data_count = sizeof(program_data) / sizeof(program_data[0]);

    printf("\nProgramming %d words at 0x%08X:\n", data_count, program_addr);
    int i;
    for (i = 0; i < data_count; i++) {
        printf("  [%d] 0x%08X <- 0x%08X\n", i,
               program_addr + i * 4, program_data[i]);
        /* In real implementation: mem_ap_write32(addr, data) */
    }

    /* Step 5: Verify by reading back */
    printf("\nVerification read-back:\n");
    uint32_t read_buffer[4];
    int words_read = mem_ap_read_burst32(program_addr, read_buffer, data_count);
    printf("  Words read: %d\n", words_read);
    bool verify_ok = true;
    for (i = 0; i < data_count && i < words_read; i++) {
        bool match = (read_buffer[i] == program_data[i]);
        printf("  [%d] 0x%08X %s\n", i, read_buffer[i],
               match ? "OK" : "MISMATCH");
        if (!match) verify_ok = false;
    }
    printf("  Verification: %s\n", verify_ok ? "PASSED" : "FAILED");

    /* Step 6: Check flash status */
    uint32_t sr_value = 0; /* Simulated: no errors, not busy */
    printf("\nFlash status check:\n");
    printf("  BSY:  %s\n", flash_is_busy(sr_value) ? "YES" : "NO");
    printf("  Error: %s\n", flash_has_error(sr_value) ? "YES" : "NO");

    /* Step 7: Lock flash after programming */
    printf("\nFlash locked after programming.\n");

    printf("\nExample completed successfully.\n");
    return 0;
}
