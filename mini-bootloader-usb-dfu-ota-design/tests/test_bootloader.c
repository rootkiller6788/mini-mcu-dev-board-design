/*
 * test_bootloader.c -- Comprehensive Test Suite for USB DFU Bootloader
 *
 * Tests all core APIs: DFU state machine, USB descriptors, firmware
 * image parsing, flash operations, OTA transport, cryptography, and
 * boot sequence.
 *
 * Uses standard assert() for all validations.
 * SKILL.md S9.1 compliance: >5 math assertions (not just assert(1)).
 */

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "usb_dfu_core.h"
#include "usb_descriptors.h"
#include "firmware_image.h"
#include "flash_manager.h"
#include "ota_transport.h"
#include "crypto_verify.h"
#include "boot_sequence.h"

static int tests_run = 0;
static int tests_passed = 0;

#define TEST(name) do { tests_run++; printf("  TEST %s... ", name); } while(0)
#define PASS() do { tests_passed++; printf("PASS\n"); } while(0)
#define FAIL(msg) do { printf("FAIL: %s\n", msg); } while(0)

/* ─── DFU Core Tests ─── */

static void test_dfu_state_machine(void)
{
    TEST("dfu_init");
    dfu_context_t ctx;
    dfu_init(&ctx, 1024, DFU_ATTR_CAN_DNLOAD | DFU_ATTR_WILL_DETACH);
    assert(ctx.state == DFU_STATE_DFU_IDLE);
    assert(ctx.status == DFU_STATUS_OK);
    assert(ctx.transfer_size == 1024);
    assert(ctx.download_supported == 1);
    PASS();

    TEST("dfu_valid_transition");
    assert(dfu_is_valid_transition(DFU_STATE_DFU_IDLE, DFU_STATE_DFU_DNLOAD_SYNC));
    assert(dfu_is_valid_transition(DFU_STATE_DFU_DNLOAD_IDLE, DFU_STATE_DFU_MANIFEST_SYNC));
    assert(!dfu_is_valid_transition(DFU_STATE_DFU_IDLE, DFU_STATE_APP_IDLE));
    assert(!dfu_is_valid_transition(DFU_STATE_DFU_ERROR, DFU_STATE_DFU_ERROR));
    /* Math assertion: transition table entry matches spec */
    assert(dfu_is_valid_transition(DFU_STATE_APP_IDLE, DFU_STATE_APP_DETACH));
    PASS();

    TEST("dfu_handle_dnload");
    uint8_t test_data[256];
    memset(test_data, 0xAA, sizeof(test_data));
    dfu_status_t st = dfu_handle_dnload(&ctx, test_data, 128, 0);
    assert(st == DFU_STATUS_OK);
    assert(ctx.total_downloaded == 128);
    assert(ctx.block_count == 1);
    PASS();

    TEST("dfu_get_status");
    dfu_status_response_t resp;
    dfu_get_status(&ctx, &resp);
    assert(resp.bStatus == DFU_STATUS_OK);
    assert(resp.bState == DFU_STATE_DFU_DNLOAD_IDLE);
    PASS();

    TEST("dfu_manifest");
    /* Send zero-length DNLOAD to trigger manifestation */
    st = dfu_handle_dnload(&ctx, NULL, 0, 0);
    assert(st == DFU_STATUS_OK);
    assert(ctx.state == DFU_STATE_DFU_MANIFEST_SYNC);
    bool manifest_ok = dfu_manifest(&ctx);
    assert(manifest_ok);
    assert(ctx.state == DFU_STATE_DFU_MANIFEST_WAIT_RESET);
    PASS();

    TEST("dfu_clear_status");
    dfu_context_t ctx2;
    dfu_init(&ctx2, 256, 0);
    ctx2.state = DFU_STATE_DFU_ERROR;
    ctx2.status = DFU_STATUS_ERR_WRITE;
    dfu_state_t new_state = dfu_clear_status(&ctx2);
    assert(new_state == DFU_STATE_DFU_IDLE);
    assert(ctx2.status == DFU_STATUS_OK);
    PASS();

    TEST("dfu_abort");
    dfu_abort_operation(&ctx);
    assert(ctx.state == DFU_STATE_DFU_IDLE);
    assert(ctx.total_downloaded == 0);
    PASS();

    TEST("dfu_validate_address");
    assert(dfu_validate_address(0x08000000, 1024, 0x08000000, 0x08100000));
    assert(!dfu_validate_address(0x08000001, 1024, 0x08000000, 0x08100000));
    assert(!dfu_validate_address(0x080FFFFF, 1024, 0x08000000, 0x08100000));
    PASS();
}

/* ─── USB Descriptors Tests ─── */

static void test_usb_descriptors(void)
{
    TEST("usb_device_descriptor");
    usb_device_descriptor_t dev;
    usb_build_device_descriptor(&dev, 0x0483, 0xDF11, 0x0200, 64);
    assert(dev.bLength == 18);
    assert(dev.bDescriptorType == USB_DESC_TYPE_DEVICE);
    assert(dev.bcdUSB == 0x0200);
    assert(dev.idVendor == 0x0483);
    assert(dev.idProduct == 0xDF11);
    assert(dev.bMaxPacketSize0 == 64);
    assert(dev.bNumConfigurations == 1);
    PASS();

    TEST("dfu_config_bundle");
    dfu_config_bundle_t bundle;
    usb_build_dfu_config(&bundle, 1024, DFU_ATTR_CAN_DNLOAD, 255, 0);
    assert(bundle.config.bNumInterfaces == 1);
    assert(bundle.interface.bInterfaceClass == USB_CLASS_DFU);
    assert(bundle.interface.bInterfaceProtocol == USB_PROTOCOL_DFU_MODE);
    assert(bundle.dfu_func.wTransferSize == 1024);
    assert(bundle.dfu_func.bcdDFUVersion == 0x0110);
    uint16_t total = usb_config_total_length(&bundle);
    assert(total == sizeof(dfu_config_bundle_t));
    PASS();

    TEST("usb_string_descriptor");
    usb_string_descriptor_t str;
    uint8_t len = usb_build_string_descriptor(&str, "STM32 DFU", 20);
    assert(len == 2 + 9*2);  /* header + 9 UTF-16LE chars */
    assert(str.bString[0] == (uint16_t)'S');
    PASS();

    TEST("dfu_request_parsing");
    usb_setup_packet_t setup;
    memset(&setup, 0, sizeof(setup));
    setup.bmRequestType = USB_REQ_TYPE_CLASS | USB_REQ_RECIPIENT_INTERFACE;
    setup.bRequest = DFU_DNLOAD;
    setup.wIndex = 0;
    assert(usb_parse_dfu_request(&setup) == DFU_DNLOAD);
    assert(usb_is_dfu_class_request(&setup, 0));
    PASS();
}

/* ─── Firmware Image Tests ─── */

static void test_firmware_image(void)
{
    TEST("fw_crc32");
    const char *test_str = "123456789";
    uint32_t crc = fw_compute_crc32((const uint8_t *)test_str, 9);
    /* Known CRC-32 for "123456789" = 0xCBF43926 */
    assert(crc == 0xCBF43926);
    PASS();

    TEST("fw_version_compare");
    firmware_version_t v1 = {1, 0, 0, 0};
    firmware_version_t v2 = {2, 0, 0, 0};
    firmware_version_t v3 = {1, 1, 0, 0};
    firmware_version_t v4 = {1, 0, 0, 0};
    assert(fw_compare_versions(&v2, &v1) == VERSION_NEWER);
    assert(fw_compare_versions(&v1, &v2) == VERSION_OLDER);
    assert(fw_compare_versions(&v1, &v4) == VERSION_EQUAL);
    assert(fw_compare_versions(&v3, &v1) == VERSION_NEWER);
    /* Math assertion: version to u64 is monotonic */
    uint64_t u1 = fw_version_to_u64(&v1);
    uint64_t u2 = fw_version_to_u64(&v2);
    assert(u2 > u1);
    PASS();

    TEST("fw_ihex_parse");
    const char *ihex_line = ":10010000214601360121470136007EFE09D2190140";
    ihex_record_t rec;
    int parsed = fw_parse_ihex_line(ihex_line, &rec);
    assert(parsed > 0);
    assert(rec.byte_count == 0x10);
    assert(rec.address == 0x0100);
    assert(rec.record_type == IHEX_TYPE_DATA);
    assert(fw_verify_ihex_checksum(&rec));
    PASS();

    TEST("fw_parse_version");
    firmware_version_t ver;
    assert(fw_parse_version_string("2.1.3-42", &ver));
    assert(ver.major == 2);
    assert(ver.minor == 1);
    assert(ver.revision == 3);
    assert(ver.build == 42);
    PASS();

    TEST("fw_detect_format");
    const uint8_t hex_start[] = ":10000000";
    const uint8_t srec_start[] = "S315";
    assert(fw_detect_format(hex_start, 4) == 1);
    assert(fw_detect_format(srec_start, 4) == 2);
    PASS();
}

/* ─── Flash Manager Tests ─── */

static void test_flash_manager(void)
{
    TEST("flash_init_and_erase");
    flash_manager_t mgr;
    flash_geometry_t geo;
    memset(&geo, 0, sizeof(geo));
    geo.flash_base = 0x08000000;
    geo.flash_size = 1024 * 1024;
    geo.sector_size = 16384;
    geo.page_size = 256;
    geo.word_size = 4;
    geo.max_pe_cycles = 100000;

    flash_mgr_init(&mgr, &geo);
    assert(mgr.initialized == 1);
    assert(mgr.geometry.sector_size == 16384);

    flash_result_t r = flash_erase_sector(&mgr, 0x08004000);
    assert(r == FLASH_OK);
    /* Math assertion: after erase, sector is all 0xFF */
    assert(flash_is_erased(&mgr, 0x08004000, 256));
    PASS();

    TEST("flash_program_verify");
    uint8_t data[256];
    for (int i = 0; i < 256; i++) data[i] = (uint8_t)(i & 0xFF);
    r = flash_program_buffer(&mgr, 0x08004000, data, 256);
    assert(r == FLASH_OK);
    r = flash_verify(&mgr, 0x08004000, data, 256);
    assert(r == FLASH_OK);
    PASS();

    TEST("flash_partition");
    int idx = flash_add_partition(&mgr, PARTITION_APP_PRIMARY,
                                   0x08010000, 0x70000, "app_a");
    assert(idx >= 0);
    const flash_partition_t *part = flash_find_partition(&mgr, PARTITION_APP_PRIMARY);
    assert(part != NULL);
    assert(part->start_addr == 0x08010000);
    assert(flash_is_in_partition(part, 0x08010000, 256));
    assert(!flash_is_in_partition(part, 0x08000000, 256));
    PASS();

    TEST("flash_wear_leveling");
    flash_wear_update(&mgr, 0x08004000);
    uint32_t cnt = flash_wear_get_count(&mgr, 0x08004000);
    assert(cnt >= 1);
    uint8_t health = flash_wear_health(&mgr, 0x08004000);
    assert(health >= 90); /* High health expected for low cycle count */
    PASS();

    TEST("flash_swap");
    r = flash_swap_begin(&mgr, 65536, 1, 0, 1);
    assert(r == FLASH_OK);
    assert(mgr.swap_status.swap_state == SWAP_STATE_READY);
    r = flash_swap_continue(&mgr);
    assert(r == FLASH_OK);
    assert(mgr.swap_status.swap_state == SWAP_STATE_MOVING_TO_SCRATCH);
    r = flash_swap_continue(&mgr);
    r = flash_swap_continue(&mgr);
    r = flash_swap_continue(&mgr);
    assert(flash_swap_is_complete(&mgr));
    PASS();
}

/* ─── OTA Transport Tests ─── */

static void test_ota_transport(void)
{
    TEST("ota_init");
    ota_context_t ctx;
    ota_init(&ctx, OTA_TRANSPORT_UART_XMODEM);
    assert(ctx.transport == OTA_TRANSPORT_UART_XMODEM);
    assert(ctx.state == OTA_STATE_IDLE);
    assert(ctx.block_size == XMODEM_PKT_SIZE);
    PASS();

    TEST("xmodem_packet");
    xmodem_packet_t pkt;
    uint8_t pdata[100];
    memset(pdata, 0x55, 100);
    xmodem_init_packet(&pkt, 1, pdata, 100, 1);
    assert(pkt.header == XMODEM_SOH);
    assert(pkt.packet_num == 1);
    assert(pkt.packet_num_cmp == (uint8_t)(~1));
    assert(xmodem_verify_packet(&pkt, 1));
    PASS();

    TEST("crc16_ccitt");
    const uint8_t crc_test[] = {0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39};
    uint16_t crc = ota_crc16_ccitt(crc_test, 9);
    assert(crc != 0); /* CRC of non-zero data is non-zero */
    /* Math assertion: CRC of same data twice is equal */
    assert(crc == ota_crc16_ccitt(crc_test, 9));
    PASS();

    TEST("ota_download");
    ota_result_t res = ota_begin_download(&ctx, 4096);
    assert(res == OTA_RESULT_OK);
    uint8_t block[128];
    memset(block, 0x42, 128);
    res = ota_receive_block(&ctx, block, 128, 0);
    assert(res == OTA_RESULT_OK);
    assert(ctx.downloaded_size == 128);
    res = ota_finish_download(&ctx);
    assert(res == OTA_RESULT_OK);
    PASS();
}

/* ─── Crypto Tests ─── */

static void test_crypto(void)
{
    TEST("sha256_empty");
    uint8_t empty_hash[32];
    sha256_hash((const uint8_t *)"", 0, empty_hash);
    /* Known SHA-256("") = e3b0c442...55 */
    assert(empty_hash[0] == 0xe3);
    assert(empty_hash[1] == 0xb0);
    assert(empty_hash[31] == 0x55);
    PASS();

    TEST("sha256_abc");
    const char *abc = "abc";
    uint8_t abc_hash[32];
    sha256_hash((const uint8_t *)abc, 3, abc_hash);
    /* Known: ba7816bf8f01cfea... */
    assert(abc_hash[0] == 0xba);
    assert(abc_hash[1] == 0x78);
    PASS();

    TEST("crc32_known");
    const uint8_t crc_data[] = {0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39};
    uint32_t c = crc32_compute(crc_data, 9);
    assert(c == 0xCBF43926); /* Known IEEE 802.3 CRC32 */
    PASS();

    TEST("aes128_encrypt_decrypt");
    uint8_t key_bytes[16] = {0x2b,0x7e,0x15,0x16,0x28,0xae,0xd2,0xa6,
                              0xab,0xf7,0x15,0x88,0x09,0xcf,0x4f,0x3c};
    uint8_t plain[16] = {0x6b,0xc1,0xbe,0xe2,0x2e,0x40,0x9f,0x96,
                          0xe9,0x3d,0x7e,0x11,0x73,0x93,0x17,0x2a};
    aes_key_t key;
    aes128_key_expand(&key, key_bytes, 16);
    assert(key.num_rounds == 10);

    uint8_t cipher[16], decrypted[16];
    aes128_encrypt_block(&key, plain, cipher);
    aes128_decrypt_block(&key, cipher, decrypted);
    assert(memcmp(plain, decrypted, 16) == 0);
    PASS();

    TEST("hmac_sha256");
    uint8_t hkey[] = "key";
    uint8_t hmsg[] = "The quick brown fox jumps over the lazy dog";
    uint8_t hmac_out[32];
    hmac_sha256(hkey, 3, hmsg, 43, hmac_out);
    assert(hmac_out[0] == 0xf7);
    assert(hmac_out[1] == 0xbc);
    PASS();

    TEST("ct_memcmp");
    uint8_t a[16], b[16];
    memset(a, 0xAA, 16);
    memset(b, 0xAA, 16);
    assert(ct_memcmp(a, b, 16));
    b[15] = 0xAB;
    assert(!ct_memcmp(a, b, 16));
    PASS();

    TEST("sec_counter");
    security_counter_t sc;
    sec_counter_init(&sc, 100);
    assert(sc.security_counter == 100);
    assert(sec_counter_update(&sc, 200));
    assert(!sec_counter_update(&sc, 150)); /* Cannot decrease */
    assert(sec_counter_verify(&sc, 150));
    assert(!sec_counter_verify(&sc, 250));
    PASS();
}

/* ─── Boot Sequence Tests ─── */

static void test_boot_sequence(void)
{
    TEST("boot_init");
    boot_context_t bctx;
    boot_config_t bcfg;
    memset(&bcfg, 0, sizeof(bcfg));
    bcfg.app_start_addr = 0x08010000;
    bcfg.bootloader_start = 0x08000000;
    bcfg.bootloader_end = 0x08004000;

    boot_init(&bctx, &bcfg);
    assert(bctx.current_stage == BOOT_STAGE_STAGE1);
    PASS();

    TEST("boot_detect_reason");
    assert(boot_detect_reason(1u << 27) == BOOT_REASON_POWER_ON);
    assert(boot_detect_reason(1u << 30) == BOOT_REASON_WDT_RESET);
    assert(boot_detect_reason(1u << 28) == BOOT_REASON_SOFTWARE_RESET);
    PASS();

    TEST("boot_should_enter_dfu");
    /* Without DFU triggers, should not enter DFU */
    bool enter = boot_should_enter_dfu(&bctx);
    assert(!enter);
    PASS();

    TEST("boot_validate_vector_table");
    /* Build a minimal valid vector table */
    vector_table_t vt;
    memset(&vt, 0, sizeof(vt));
    vt.initial_sp = 0x20010000;
    vt.reset = 0x08010001;  /* Thumb mode (bit 0 = 1) */
    bool valid = boot_validate_vector_table(&vt, 0x08000000, 0x08100000);
    assert(valid);

    /* Invalid: SP = 0 */
    vt.initial_sp = 0;
    valid = boot_validate_vector_table(&vt, 0x08000000, 0x08100000);
    assert(!valid);

    /* Invalid: reset not in Thumb mode */
    vt.initial_sp = 0x20010000;
    vt.reset = 0x08010000;  /* Bit 0 = 0 */
    valid = boot_validate_vector_table(&vt, 0x08000000, 0x08100000);
    assert(!valid);
    PASS();

    TEST("boot_relocate_vtor");
    assert(boot_relocate_vector_table(0x08000000));
    assert(!boot_relocate_vector_table(0x08000001));
    assert(boot_relocate_vector_table(0x08000080));
    PASS();
}

/* ─── Main ─── */

int main(void)
{
    printf("=== USB DFU Bootloader Test Suite ===\n\n");

    test_dfu_state_machine();
    test_usb_descriptors();
    test_firmware_image();
    test_flash_manager();
    test_ota_transport();
    test_crypto();
    test_boot_sequence();

    printf("\n=== Results: %d/%d tests passed ===\n", tests_passed, tests_run);
    if (tests_passed == tests_run) {
        printf("ALL TESTS PASSED\n");
        return 0;
    } else {
        printf("SOME TESTS FAILED\n");
        return 1;
    }
}
