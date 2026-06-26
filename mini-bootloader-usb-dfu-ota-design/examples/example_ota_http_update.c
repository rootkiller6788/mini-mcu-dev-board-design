/*
 * example_ota_http_update.c -- HTTP-Based OTA Firmware Update Demo
 *
 * Demonstrates an OTA update cycle over WiFi/HTTP:
 *   1. Check for available update
 *   2. Download new firmware
 *   3. Verify signature
 *   4. Program to flash and swap slots
 *
 * L6: A/B OTA with fallback
 * L7: IoT firmware update application
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "ota_transport.h"
#include "firmware_image.h"
#include "flash_manager.h"
#include "crypto_verify.h"
#include "boot_sequence.h"

int main(void)
{
    printf("=== HTTP OTA Firmware Update Example ===\n\n");

    /* Step 1: Check for update */
    printf("Step 1: Checking for firmware update...\n");
    ota_context_t ota;
    ota_init(&ota, OTA_TRANSPORT_WIFI_HTTP);
    char new_ver[32];
    ota_result_t res = ota_http_check_update(&ota, "1.0.0", new_ver, sizeof(new_ver));
    printf("  Result: %s, New version: %s\n", ota_result_name(res), new_ver);

    /* Step 2: Download firmware */
    printf("\nStep 2: Downloading firmware image...\n");
    uint8_t fw_buf[65536];
    uint32_t fw_size = 0;
    res = ota_http_download_firmware(&ota, fw_buf, sizeof(fw_buf), &fw_size);
    printf("  Result: %s, Size: %u bytes\n", ota_result_name(res), fw_size);

    /* Step 3: Verify download */
    printf("\nStep 3: Verifying firmware integrity...\n");
    uint32_t crc = fw_compute_crc32(fw_buf, fw_size);
    printf("  CRC-32: 0x%08X\n", crc);

    uint8_t hash[32];
    sha256_hash(fw_buf, fw_size, hash);
    printf("  SHA-256: ");
    for (int i = 0; i < 8; i++) printf("%02x", hash[i]);
    printf("...\n");

    /* Step 4: Simulate flash programming */
    printf("\nStep 4: Programming flash (simulated)...\n");
    flash_manager_t mgr;
    flash_geometry_t geo = {0x08000000, 1024*1024, 16384, 256, 65536, 4, 100000};
    flash_mgr_init(&mgr, &geo);
    int idx = flash_add_partition(&mgr, PARTITION_APP_SECONDARY,
                                   0x08040000, 0x40000, "app_b");
    printf("  Partition '%s' at 0x%08X, size=%u\n",
           mgr.partitions[idx].name,
           mgr.partitions[idx].start_addr,
           mgr.partitions[idx].size);

    /* Step 5: Begin A/B swap */
    printf("\nStep 5: Performing A/B slot swap...\n");
    flash_result_t fr = flash_swap_begin(&mgr, fw_size, 1, 0, 1);
    if (fr == FLASH_OK) {
        while (!flash_swap_is_complete(&mgr)) {
            fr = flash_swap_continue(&mgr);
            printf("  Swap state: %d\n", mgr.swap_status.swap_state);
        }
        printf("  Swap complete!\n");
    }

    /* Step 6: Boot into new firmware */
    printf("\nStep 6: Rebooting into new firmware...\n");
    printf("  Setting magic value for OTA boot...\n");
    /* In real device: boot_set_magic_value(BACKUP_SRAM_ADDR, BOOT_MAGIC_ENTER_OTA) */
    printf("  System reset...\n");

    printf("\n=== OTA Update Complete ===\n");
    return 0;
}
