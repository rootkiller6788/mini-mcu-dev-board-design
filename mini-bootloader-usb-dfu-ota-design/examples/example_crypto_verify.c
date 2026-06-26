/*
 * example_crypto_verify.c -- Secure Boot Signature Verification Demo
 *
 * Demonstrates the secure boot chain:
 *   1. Compute firmware hash (SHA-256)
 *   2. Verify ECDSA P-256 signature
 *   3. Check security counter (anti-rollback)
 *   4. HMAC-based firmware authentication
 *
 * L6: Secure boot chain problem
 * L7: Medical device firmware management (security counter)
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "crypto_verify.h"
#include "firmware_image.h"

int main(void)
{
    printf("=== Secure Boot Crypto Verification Example ===\n\n");

    /* Test firmware image data */
    const char *firmware_data = "MINI-BOOTLOADER-OTA-FIRMWARE-V2.1";
    uint32_t fw_len = (uint32_t)strlen(firmware_data);

    /* Step 1: Compute firmware hash */
    printf("Step 1: Computing firmware hash...\n");
    uint8_t fw_hash[32];
    sha256_hash((const uint8_t *)firmware_data, fw_len, fw_hash);
    char hex[65];
    hash_to_hex(fw_hash, 32, hex, sizeof(hex));
    printf("  SHA-256(firmware) = %s\n", hex);

    /* Step 2: ECDSA P-256 signature verification (simulated) */
    printf("\nStep 2: Verifying ECDSA P-256 signature...\n");
    ecdsa_p256_pubkey_t pubkey;
    memset(&pubkey, 0, sizeof(pubkey));
    /* In real device: public key is in OTP or secure storage */
    for (int i = 0; i < 32; i++) pubkey.x[i] = (uint8_t)(0x10 + i);
    for (int i = 0; i < 32; i++) pubkey.y[i] = (uint8_t)(0x30 + i);

    ecdsa_p256_signature_t sig;
    memset(&sig, 0, sizeof(sig));
    /* In real device: signature comes with the firmware image */

    bool sig_valid = ecdsa_p256_verify(&pubkey, fw_hash, 32, &sig);
    printf("  Signature verification: %s \n",
           sig_valid ? "PASS" : "FAIL");

    /* Step 3: Security counter check */
    printf("\nStep 3: Checking security counter (anti-rollback)...\n");
    security_counter_t sec_cnt;
    sec_counter_init(&sec_cnt, 42);
    printf("  Current counter: %u\n", sec_cnt.security_counter);

    /* Firmware version requires counter >= 50 */
    if (!sec_counter_verify(&sec_cnt, 50)) {
        printf("  WARNING: Security counter too low!\n");
        printf("  Updating counter to 100...\n");
        sec_counter_update(&sec_cnt, 100);
    }
    printf("  Counter after update: %u\n", sec_cnt.security_counter);

    /* Step 4: HMAC-SHA256 firmware authentication */
    printf("\nStep 4: HMAC-SHA256 firmware authentication...\n");
    const char *auth_key = "secure-boot-key-2026";
    uint8_t hmac_result[32];
    hmac_sha256((const uint8_t *)auth_key, (uint32_t)strlen(auth_key),
                (const uint8_t *)firmware_data, fw_len,
                hmac_result);
    hash_to_hex(hmac_result, 32, hex, sizeof(hex));
    printf("  HMAC-SHA256: %s\n", hex);

    /* Step 5: CRC-32 integrity check */
    printf("\nStep 5: CRC-32 integrity check...\n");
    uint32_t crc = crc32_compute((const uint8_t *)firmware_data, fw_len);
    printf("  CRC-32: 0x%08X\n", crc);

    /* Step 6: Constant-time comparison demo */
    printf("\nStep 6: Constant-time comparison (side-channel resistant)...\n");
    const char *expected = "MINI-BOOTLOADER-OTA-FIRMWARE-V2.1";
    bool match = ct_memcmp((const uint8_t *)firmware_data,
                            (const uint8_t *)expected,
                            (uint32_t)strlen(expected));
    printf("  Firmware ID match: %s\n", match ? "YES" : "NO");

    printf("\n=== Secure Boot Verification Complete ===\n");
    return 0;
}
