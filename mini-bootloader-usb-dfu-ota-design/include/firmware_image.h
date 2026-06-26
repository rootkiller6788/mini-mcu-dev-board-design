/*
 * firmware_image.h -- Firmware Image Format Definitions and Parsing
 *
 * Supports multiple firmware image formats: raw binary, Intel HEX,
 * Motorola S-Record, and MCUboot-compatible format.
 *
 * Knowledge Coverage:
 *   L1: Firmware image header, magic number, version vector, TLV
 *   L2: Image parsing, validation, integrity checking
 *   L3: Intel HEX checksum, SREC checksum, CRC-32 polynomial
 *   L4: Two's complement arithmetic for checksums
 *   L5: HEX/SREC parsing algorithms, TLV extraction
 *
 * Reference: Intel HEX Format (1988), Motorola SREC Format
 * MCUboot Image Format v1.0 (Zephyr Project)
 * MIT 6.004 -- Memory representations, binary formats
 */

#ifndef FIRMWARE_IMAGE_H
#define FIRMWARE_IMAGE_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* Magic Number Constants */
#define FW_MAGIC_MCUBOOT    0x96f3b83d
#define FW_MAGIC_ESP32      0xe9u
#define FW_MAGIC_STM32      0x544F4F42
#define FW_MAGIC_CUSTOM     0x4D435542

/* Firmware Image Header (MCUboot-compatible) */
typedef struct {
    uint32_t ih_magic;
    uint32_t ih_load_addr;
    uint16_t ih_hdr_size;
    uint16_t ih_protect_tlv_size;
    uint32_t ih_img_size;
    uint32_t ih_flags;
    struct {
        uint8_t  iv_major;
        uint8_t  iv_minor;
        uint16_t iv_revision;
        uint32_t iv_build_num;
    } ih_ver;
    uint8_t  ih_pad[12];
} firmware_header_t;

#define FW_FLAG_SECURE              (1u << 0)
#define FW_FLAG_ENCRYPTED           (1u << 1)
#define FW_FLAG_BOOTABLE            (1u << 2)
#define FW_FLAG_RAM_LOAD            (1u << 3)
#define FW_FLAG_SINGLE_APPLICATION  (1u << 4)

/* TLV (Type-Length-Value) Entry */
typedef struct {
    uint16_t it_type;
    uint16_t it_len;
} tlv_entry_t;

#define TLV_TYPE_KEYHASH         0x01
#define TLV_TYPE_PUBKEY          0x02
#define TLV_TYPE_SHA256          0x10
#define TLV_TYPE_SHA512          0x11
#define TLV_TYPE_RSA2048         0x20
#define TLV_TYPE_ECDSA256        0x21
#define TLV_TYPE_ECDSA_SIG       0x22
#define TLV_TYPE_ED25519         0x23
#define TLV_TYPE_ENCRSA2048      0x30
#define TLV_TYPE_ENCKW128        0x31
#define TLV_TYPE_ENCEC256        0x32
#define TLV_TYPE_DEPENDENCY      0x40
#define TLV_TYPE_SEC_CNT         0x50
#define TLV_TYPE_BOOT_RECORD     0x60

/* Intel HEX Record Types */
#define IHEX_TYPE_DATA            0x00
#define IHEX_TYPE_EOF             0x01
#define IHEX_TYPE_EXT_SEGMENT     0x02
#define IHEX_TYPE_START_SEGMENT   0x03
#define IHEX_TYPE_EXT_LINEAR      0x04
#define IHEX_TYPE_START_LINEAR    0x05

/* Intel HEX record */
typedef struct {
    uint8_t   byte_count;
    uint16_t  address;
    uint8_t   record_type;
    uint8_t   data[255];
    uint8_t   checksum;
    uint8_t   data_len;
} ihex_record_t;

/* SREC Record Types */
#define SREC_TYPE_S0  0x00
#define SREC_TYPE_S1  0x01
#define SREC_TYPE_S2  0x02
#define SREC_TYPE_S3  0x03
#define SREC_TYPE_S5  0x05
#define SREC_TYPE_S7  0x07
#define SREC_TYPE_S8  0x08
#define SREC_TYPE_S9  0x09

/* SREC record */
typedef struct {
    uint8_t   record_type;
    uint8_t   byte_count;
    uint32_t  address;
    uint8_t   data[255];
    uint8_t   data_len;
    uint8_t   checksum;
} srec_record_t;

/* Firmware Image Context */
typedef struct {
    firmware_header_t header;
    uint8_t          *payload;
    uint32_t          payload_size;
    uint32_t          total_size;
    uint32_t          crc32;
    uint8_t           sha256[32];
    uint8_t           format;
    uint8_t           valid;
    uint32_t          base_addr;
} firmware_image_t;

/* Firmware Version */
typedef struct {
    uint8_t  major;
    uint8_t  minor;
    uint16_t revision;
    uint32_t build;
} firmware_version_t;

typedef enum {
    VERSION_EQUAL        = 0,
    VERSION_NEWER        = 1,
    VERSION_OLDER        = -1,
    VERSION_INCOMPATIBLE = -2
} version_cmp_t;

/* API */
void fw_image_init(firmware_image_t *img);
bool fw_validate_header(const firmware_header_t *header);
int fw_parse_ihex_line(const char *line, ihex_record_t *record);
bool fw_verify_ihex_checksum(const ihex_record_t *record);
int fw_parse_srec_line(const char *line, srec_record_t *record);
bool fw_verify_srec_checksum(const srec_record_t *record);
version_cmp_t fw_compare_versions(const firmware_version_t *a,
                                   const firmware_version_t *b);
uint64_t fw_version_to_u64(const firmware_version_t *ver);
bool fw_parse_version_string(const char *str, firmware_version_t *ver);
int fw_extract_tlv(const uint8_t *tlv_start, uint16_t tlv_size,
                   tlv_entry_t *entries, int max_entries);
const uint8_t *fw_find_tlv(const uint8_t *tlv_area, uint16_t tlv_size,
                            uint16_t tlv_type, uint16_t *out_len);
bool fw_check_dependency(const firmware_version_t *installed,
                          const firmware_version_t *required_min,
                          const firmware_version_t *required_max);
uint32_t fw_compute_crc32(const uint8_t *data, uint32_t len);
int fw_encode_ihex_line(uint32_t address, const uint8_t *data,
                         uint8_t len, uint8_t record_type,
                         char *out_line, int max_line_len);
int fw_detect_format(const uint8_t *data, uint32_t len);

#endif /* FIRMWARE_IMAGE_H */
