/*
 * example_dfu_update.c -- Complete USB DFU Firmware Update Demo
 *
 * Demonstrates a full DFU firmware update cycle:
 *   1. Device enumerates as DFU
 *   2. Host downloads firmware blocks
 *   3. Manifestation activates new firmware
 *   4. Device reboots into new application
 *
 * L6: Classic DFU firmware update problem
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "usb_dfu_core.h"
#include "usb_descriptors.h"
#include "firmware_image.h"
#include "flash_manager.h"
#include "crypto_verify.h"

int main(void)
{
    printf("=== USB DFU Firmware Update Example ===\n\n");

    /* Step 1: Build DFU descriptors */
    printf("Step 1: Building DFU descriptors...\n");
    usb_device_descriptor_t dev_desc;
    usb_build_device_descriptor(&dev_desc, 0x0483, 0xDF11, 0x0200, 64);
    printf("  VID=0x%04X PID=0x%04X bcdUSB=0x%04X\n",
           dev_desc.idVendor, dev_desc.idProduct, dev_desc.bcdUSB);

    dfu_config_bundle_t cfg;
    usb_build_dfu_config(&cfg, 1024,
                         DFU_ATTR_CAN_DNLOAD | DFU_ATTR_MANIFESTATION_TOLERANT,
                         255, 0);
    printf("  DFU config: %u interfaces, transfer=%u bytes\n",
           cfg.config.bNumInterfaces, cfg.dfu_func.wTransferSize);

    /* Step 2: Initialize DFU engine */
    printf("\nStep 2: Initializing DFU state machine...\n");
    dfu_context_t dfu;
    dfu_init(&dfu, 1024, DFU_ATTR_CAN_DNLOAD);
    printf("  State: %s\n", dfu_state_name(dfu.state));

    /* Step 3: Simulate firmware download */
    printf("\nStep 3: Downloading firmware image...\n");
    uint8_t firmware[4096];
    for (int i = 0; i < 4096; i++) firmware[i] = (uint8_t)(i & 0xFF);

    uint32_t total = 0;
    for (int block = 0; block < 4; block++) {
        dfu_status_t st = dfu_handle_dnload(&dfu, &firmware[block * 1024],
                                             1024, (uint16_t)block);
        printf("  Block %d: %s (%u bytes)\n", block, dfu_status_name(st), 1024);
        total += 1024;
    }
    printf("  Total downloaded: %u bytes\n", total);

    /* Step 4: Trigger manifestation (zero-length DNLOAD) */
    printf("\nStep 4: Triggering manifestation...\n");
    dfu_status_t st = dfu_handle_dnload(&dfu, NULL, 0, 4);
    printf("  Status: %s, State: %s\n",
           dfu_status_name(st), dfu_state_name(dfu.state));

    bool manifest_ok = dfu_manifest(&dfu);
    printf("  Manifestation: %s\n", manifest_ok ? "SUCCESS" : "FAILED");
    printf("  Final state: %s\n", dfu_state_name(dfu.state));

    /* Step 5: Compute firmware hash */
    printf("\nStep 5: Verifying firmware integrity...\n");
    uint8_t hash[32];
    sha256_hash(firmware, 4096, hash);
    char hex[65];
    hash_to_hex(hash, 32, hex, sizeof(hex));
    printf("  SHA-256: %s\n", hex);

    printf("\n=== DFU Update Complete ===\n");
    return 0;
}
