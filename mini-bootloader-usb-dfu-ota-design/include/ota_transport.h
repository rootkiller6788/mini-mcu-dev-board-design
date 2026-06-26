/*
 * ota_transport.h -- OTA Update Transport Layer Abstraction
 *
 * Provides transport-agnostic OTA firmware download over multiple
 * protocols: USB DFU, UART (XMODEM/YMODEM), BLE, WiFi/HTTP.
 *
 * Knowledge Coverage:
 *   L1: OTA transport protocols, packet framing, error detection
 *   L2: Stop-and-wait ARQ (XMODEM), sliding window (YMODEM), REST (HTTP)
 *   L4: XMODEM error detection (checksum/CRC), YMODEM batch transfer
 *   L5: HTTP Range requests for resumable download
 *   L6: Multi-transport OTA with fallback
 *
 * Reference: Christensen XMODEM Protocol (1977), Forsberg YMODEM (1985)
 * RFC 7230 HTTP/1.1, RFC 7252 CoAP
 * Stanford EE359 -- Wireless communications (OTA channels)
 */

#ifndef OTA_TRANSPORT_H
#define OTA_TRANSPORT_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* Transport Types */
typedef enum {
    OTA_TRANSPORT_USB_DFU     = 0,
    OTA_TRANSPORT_UART_XMODEM = 1,
    OTA_TRANSPORT_UART_YMODEM = 2,
    OTA_TRANSPORT_BLE         = 3,
    OTA_TRANSPORT_WIFI_HTTP   = 4,
    OTA_TRANSPORT_WIFI_COAP   = 5,
    OTA_TRANSPORT_WIFI_MQTT   = 6,
    OTA_TRANSPORT_CAN         = 7,
    OTA_TRANSPORT_SPI         = 8
} ota_transport_type_t;

/* OTA Update State Machine */
typedef enum {
    OTA_STATE_IDLE              = 0,
    OTA_STATE_CHECKING          = 1,
    OTA_STATE_DOWNLOADING       = 2,
    OTA_STATE_VERIFYING         = 3,
    OTA_STATE_PROGRAMMING       = 4,
    OTA_STATE_SWAPPING          = 5,
    OTA_STATE_COMPLETE          = 6,
    OTA_STATE_FAILED            = 7,
    OTA_STATE_ROLLBACK          = 8
} ota_state_t;

/* OTA Update Result */
typedef enum {
    OTA_RESULT_OK               = 0,
    OTA_RESULT_ERR_NETWORK      = -1,
    OTA_RESULT_ERR_TIMEOUT      = -2,
    OTA_RESULT_ERR_CHECKSUM     = -3,
    OTA_RESULT_ERR_SIGNATURE    = -4,
    OTA_RESULT_ERR_FLASH        = -5,
    OTA_RESULT_ERR_NO_UPDATE    = -6,
    OTA_RESULT_ERR_VERSION      = -7,
    OTA_RESULT_ERR_STORAGE      = -8,
    OTA_RESULT_ERR_ABORTED      = -9
} ota_result_t;

/* XMODEM Constants */
#define XMODEM_SOH      0x01
#define XMODEM_STX      0x02
#define XMODEM_EOT      0x04
#define XMODEM_ACK      0x06
#define XMODEM_NAK      0x15
#define XMODEM_CAN      0x18
#define XMODEM_CRC_C    0x43
#define XMODEM_PKT_SIZE 128
#define XMODEM_PKT1K_SIZE 1024
#define XMODEM_MAX_RETRIES 10

/* YMODEM Constants */
#define YMODEM_HEADER_PKT_SIZE 128
#define YMODEM_MAX_FILENAME    64
#define YMODEM_STX_PKT_SIZE    1024

/* XMODEM Packet */
typedef struct {
    uint8_t  header;
    uint8_t  packet_num;
    uint8_t  packet_num_cmp;
    uint8_t  data[1024];
    uint16_t crc;
    uint16_t data_len;
} xmodem_packet_t;

/* YMODEM File Info Header (Packet 0) */
typedef struct {
    char     filename[YMODEM_MAX_FILENAME];
    uint32_t file_size;
    uint32_t mtime;
    uint16_t mode;
    uint8_t  serial_num;
} ymodem_header_t;

/* HTTP OTA Configuration */
typedef struct {
    char     server_url[256];
    char     firmware_path[128];
    char     version_path[128];
    uint16_t server_port;
    uint8_t  use_tls;
    uint32_t connect_timeout_ms;
    uint32_t read_timeout_ms;
    uint32_t max_retries;
} ota_http_config_t;

/* OTA Download Context */
typedef struct {
    ota_transport_type_t  transport;
    ota_state_t           state;
    ota_result_t          last_result;
    uint32_t              total_size;
    uint32_t              downloaded_size;
    uint32_t              block_size;
    uint32_t              block_count;
    uint32_t              retry_count;
    uint32_t              timeout_ms;
    uint8_t              *download_buffer;
    uint32_t              buffer_size;
    uint32_t              resume_offset;
    uint8_t               checksum_type;
    uint8_t               transport_active;
    uint8_t               last_packet_ack;
} ota_context_t;

/* API Declarations */
void ota_init(ota_context_t *ctx, ota_transport_type_t transport);
ota_result_t ota_begin_download(ota_context_t *ctx, uint32_t expected_size);
ota_result_t ota_receive_block(ota_context_t *ctx,
                                const uint8_t *data, uint32_t len,
                                uint32_t block_num);
ota_result_t ota_finish_download(ota_context_t *ctx);
ota_result_t ota_abort_download(ota_context_t *ctx);

void xmodem_init_packet(xmodem_packet_t *pkt, uint8_t pkt_num,
                         const uint8_t *data, uint16_t len, uint8_t use_crc);
bool xmodem_verify_packet(const xmodem_packet_t *pkt, uint8_t expected_num);
uint16_t xmodem_calc_crc16(const uint8_t *data, uint16_t len);

void ymodem_build_header_packet(xmodem_packet_t *pkt,
                                 const char *filename, uint32_t file_size);
bool ymodem_parse_header(const xmodem_packet_t *pkt, ymodem_header_t *info);

void ota_http_set_config(ota_context_t *ctx, const ota_http_config_t *config);
ota_result_t ota_http_check_update(ota_context_t *ctx,
                                    const char *current_version,
                                    char *new_version, int max_len);
ota_result_t ota_http_download_firmware(ota_context_t *ctx,
                                         uint8_t *buffer, uint32_t buf_size,
                                         uint32_t *out_size);

uint16_t ota_crc16_ccitt(const uint8_t *data, uint32_t len);
uint8_t ota_checksum8(const uint8_t *data, uint16_t len);
const char *ota_state_name(ota_state_t state);
const char *ota_result_name(ota_result_t result);
bool ota_transport_supports_resume(ota_transport_type_t transport);
uint32_t ota_get_block_size(ota_transport_type_t transport);

#endif /* OTA_TRANSPORT_H */
