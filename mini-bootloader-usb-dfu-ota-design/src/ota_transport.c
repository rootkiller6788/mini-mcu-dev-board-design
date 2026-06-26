/*
 * ota_transport.c -- OTA Transport Protocol Implementations
 *
 * Implements XMODEM (checksum and CRC variants), YMODEM batch
 * protocol, and HTTP-based OTA download simulation.
 *
 * Knowledge Points:
 *   L4: XMODEM stop-and-wait ARQ error recovery
 *   L5: XMODEM-CRC (CRC-16-CCITT), YMODEM batch transfer
 *   L6: Multi-protocol OTA with XMODEM/YMODEM fallback
 *   L7: Internet OTA via HTTP download simulation
 */

#include "ota_transport.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

static const uint16_t crc16_table[256] = {
    0x0000,0x1021,0x2042,0x3063,0x4084,0x50A5,0x60C6,0x70E7,
    0x8108,0x9129,0xA14A,0xB16B,0xC18C,0xD1AD,0xE1CE,0xF1EF,
    0x1231,0x0210,0x3273,0x2252,0x52B5,0x4294,0x72F7,0x62D6,
    0x9339,0x8318,0xB37B,0xA35A,0xD3BD,0xC39C,0xF3FF,0xE3DE,
    0x2462,0x3443,0x0420,0x1401,0x64E6,0x74C7,0x44A4,0x5485,
    0xA56A,0xB54B,0x8528,0x9509,0xE5EE,0xF5CF,0xC5AC,0xD58D,
    0x3653,0x2672,0x1611,0x0630,0x76D7,0x66F6,0x5695,0x46B4,
    0xB75B,0xA77A,0x9719,0x8738,0xF7DF,0xE7FE,0xD79D,0xC7BC,
    0x48C4,0x58E5,0x6886,0x78A7,0x0840,0x1861,0x2802,0x3823,
    0xC9CC,0xD9ED,0xE98E,0xF9AF,0x8948,0x9969,0xA90A,0xB92B,
    0x5AF5,0x4AD4,0x7AB7,0x6A96,0x1A71,0x0A50,0x3A33,0x2A12,
    0xDBFD,0xCBDC,0xFBBF,0xEB9E,0x9B79,0x8B58,0xBB3B,0xAB1A,
    0x6CA6,0x7C87,0x4CE4,0x5CC5,0x2C22,0x3C03,0x0C60,0x1C41,
    0xEDAE,0xFD8F,0xCDEC,0xDDCD,0xAD2A,0xBD0B,0x8D68,0x9D49,
    0x7E97,0x6EB6,0x5ED5,0x4EF4,0x3E13,0x2E32,0x1E51,0x0E70,
    0xFF9F,0xEFBE,0xDFDD,0xCFFC,0xBF1B,0xAF3A,0x9F59,0x8F78,
    0x9188,0x81A9,0xB1CA,0xA1EB,0xD10C,0xC12D,0xF14E,0xE16F,
    0x1080,0x00A1,0x30C2,0x20E3,0x5004,0x4025,0x7046,0x6067,
    0x83B9,0x9398,0xA3FB,0xB3DA,0xC33D,0xD31C,0xE37F,0xF35E,
    0x02B1,0x1290,0x22F3,0x32D2,0x4235,0x5214,0x6277,0x7256,
    0xB5EA,0xA5CB,0x95A8,0x8589,0xF56E,0xE54F,0xD52C,0xC50D,
    0x34E2,0x24C3,0x14A0,0x0481,0x7466,0x6447,0x5424,0x4405,
    0xA7DB,0xB7FA,0x8799,0x97B8,0xE75F,0xF77E,0xC71D,0xD73C,
    0x26D3,0x36F2,0x0691,0x16B0,0x6657,0x7676,0x4615,0x5634,
    0xD94C,0xC96D,0xF90E,0xE92F,0x99C8,0x89E9,0xB98A,0xA9AB,
    0x5844,0x4865,0x7806,0x6827,0x18C0,0x08E1,0x3882,0x28A3,
    0xCB7D,0xDB5C,0xEB3F,0xFB1E,0x8BF9,0x9BD8,0xABBB,0xBB9A,
    0x4A75,0x5A54,0x6A37,0x7A16,0x0AF1,0x1AD0,0x2AB3,0x3A92,
    0xFD2E,0xED0F,0xDD6C,0xCD4D,0xBDAA,0xAD8B,0x9DE8,0x8DC9,
    0x7C26,0x6C07,0x5C64,0x4C45,0x3CA2,0x2C83,0x1CE0,0x0CC1,
    0xEF1F,0xFF3E,0xCF5D,0xDF7C,0xAF9B,0xBFBA,0x8FD9,0x9FF8,
    0x6E17,0x7E36,0x4E55,0x5E74,0x2E93,0x3EB2,0x0ED1,0x1EF0
};

uint16_t ota_crc16_ccitt(const uint8_t *data, uint32_t len)
{
    uint16_t crc = 0;
    for (uint32_t i = 0; i < len; i++) {
        uint8_t idx = (uint8_t)(((crc >> 8) ^ data[i]) & 0xFF);
        crc = (uint16_t)((crc << 8) ^ crc16_table[idx]);
    }
    return crc;
}

uint16_t xmodem_calc_crc16(const uint8_t *data, uint16_t len)
{
    return ota_crc16_ccitt(data, len);
}

uint8_t ota_checksum8(const uint8_t *data, uint16_t len)
{
    uint8_t sum = 0;
    for (uint16_t i = 0; i < len; i++) sum += data[i];
    return sum;
}

void ota_init(ota_context_t *ctx, ota_transport_type_t transport)
{
    if (!ctx) return;
    memset(ctx, 0, sizeof(*ctx));
    ctx->transport = transport;
    ctx->state = OTA_STATE_IDLE;
    ctx->block_size = ota_get_block_size(transport);
    ctx->timeout_ms = 1000;
    ctx->checksum_type = 1;
}

uint32_t ota_get_block_size(ota_transport_type_t transport)
{
    switch (transport) {
    case OTA_TRANSPORT_UART_XMODEM:  return XMODEM_PKT_SIZE;
    case OTA_TRANSPORT_UART_YMODEM:  return YMODEM_STX_PKT_SIZE;
    case OTA_TRANSPORT_USB_DFU:      return 1024;
    case OTA_TRANSPORT_BLE:          return 20;
    case OTA_TRANSPORT_WIFI_HTTP:    return 4096;
    case OTA_TRANSPORT_WIFI_COAP:    return 1024;
    case OTA_TRANSPORT_WIFI_MQTT:    return 256;
    case OTA_TRANSPORT_CAN:          return 8;
    case OTA_TRANSPORT_SPI:          return 256;
    default:                         return 128;
    }
}

bool ota_transport_supports_resume(ota_transport_type_t transport)
{
    return (transport == OTA_TRANSPORT_WIFI_HTTP ||
            transport == OTA_TRANSPORT_USB_DFU);
}

ota_result_t ota_begin_download(ota_context_t *ctx, uint32_t expected_size)
{
    if (!ctx) return OTA_RESULT_ERR_NETWORK;
    ctx->state = OTA_STATE_DOWNLOADING;
    ctx->total_size = expected_size;
    ctx->downloaded_size = 0;
    ctx->block_count = 0;
    ctx->retry_count = 0;
    ctx->last_result = OTA_RESULT_OK;
    return OTA_RESULT_OK;
}

ota_result_t ota_receive_block(ota_context_t *ctx,
                                const uint8_t *data, uint32_t len,
                                uint32_t block_num)
{
    if (!ctx) return OTA_RESULT_ERR_NETWORK;
    if (ctx->state != OTA_STATE_DOWNLOADING)
        return OTA_RESULT_ERR_NETWORK;
    if (block_num != ctx->block_count) {
        ctx->last_result = OTA_RESULT_ERR_CHECKSUM;
        ctx->state = OTA_STATE_FAILED;
        return ctx->last_result;
    }
    if (ctx->download_buffer && ctx->downloaded_size + len <= ctx->buffer_size)
        memcpy(ctx->download_buffer + ctx->downloaded_size, data, len);
    ctx->downloaded_size += len;
    ctx->block_count++;
    ctx->retry_count = 0;
    return OTA_RESULT_OK;
}

ota_result_t ota_finish_download(ota_context_t *ctx)
{
    if (!ctx) return OTA_RESULT_ERR_NETWORK;
    ctx->state = OTA_STATE_COMPLETE;
    return OTA_RESULT_OK;
}

ota_result_t ota_abort_download(ota_context_t *ctx)
{
    if (!ctx) return OTA_RESULT_ERR_ABORTED;
    ctx->state = OTA_STATE_FAILED;
    ctx->last_result = OTA_RESULT_ERR_ABORTED;
    return OTA_RESULT_ERR_ABORTED;
}

void xmodem_init_packet(xmodem_packet_t *pkt, uint8_t pkt_num,
                         const uint8_t *data, uint16_t len, uint8_t use_crc)
{
    if (!pkt) return;
    memset(pkt, 0, sizeof(*pkt));
    if (len > XMODEM_PKT_SIZE) {
        pkt->header = XMODEM_STX;
        pkt->data_len = XMODEM_PKT1K_SIZE;
    } else {
        pkt->header = XMODEM_SOH;
        pkt->data_len = XMODEM_PKT_SIZE;
    }
    pkt->packet_num = pkt_num;
    pkt->packet_num_cmp = (uint8_t)(~pkt_num);
    if (data && len > 0) {
        uint16_t cl = (len < pkt->data_len) ? len : pkt->data_len;
        memcpy(pkt->data, data, cl);
        if (cl < pkt->data_len)
            memset(pkt->data + cl, 0x1A, pkt->data_len - cl);
    }
    pkt->crc = use_crc ? xmodem_calc_crc16(pkt->data, pkt->data_len)
                       : ota_checksum8(pkt->data, pkt->data_len);
}

bool xmodem_verify_packet(const xmodem_packet_t *pkt, uint8_t expected_num)
{
    if (!pkt) return 0;
    if (pkt->header != XMODEM_SOH && pkt->header != XMODEM_STX) return 0;
    if (pkt->packet_num != expected_num) return 0;
    if (pkt->packet_num_cmp != (uint8_t)(~expected_num)) return 0;
    return 1;
}

void ymodem_build_header_packet(xmodem_packet_t *pkt,
                                 const char *filename, uint32_t file_size)
{
    if (!pkt || !filename) return;
    memset(pkt, 0, sizeof(*pkt));
    pkt->header = XMODEM_SOH;
    pkt->packet_num = 0;
    pkt->packet_num_cmp = 0xFF;
    pkt->data_len = XMODEM_PKT_SIZE;
    int wr = snprintf((char *)pkt->data, XMODEM_PKT_SIZE,
                      "%s%c%lu%c%o%c%u",
                      filename, 0, (unsigned long)file_size, ' ',
                      0644, ' ', 0);
    if (wr < XMODEM_PKT_SIZE)
        memset(pkt->data + wr, 0, XMODEM_PKT_SIZE - wr);
    pkt->crc = xmodem_calc_crc16(pkt->data, XMODEM_PKT_SIZE);
}

bool ymodem_parse_header(const xmodem_packet_t *pkt, ymodem_header_t *info)
{
    if (!pkt || !info || pkt->packet_num != 0) return 0;
    memset(info, 0, sizeof(*info));
    strncpy(info->filename, (const char *)pkt->data,
            YMODEM_MAX_FILENAME - 1);
    const char *meta = (const char *)pkt->data;
    size_t nl = strlen(meta);
    if (nl + 1 < XMODEM_PKT_SIZE)
        info->file_size = (uint32_t)strtoul(meta + nl + 1, NULL, 10);
    return 1;
}

void ota_http_set_config(ota_context_t *ctx, const ota_http_config_t *config)
{
    (void)ctx; (void)config;
}

ota_result_t ota_http_check_update(ota_context_t *ctx,
                                    const char *current_version,
                                    char *new_version, int max_len)
{
    if (!ctx || !new_version) return OTA_RESULT_ERR_NETWORK;
    ctx->state = OTA_STATE_CHECKING;
    (void)current_version;
    snprintf(new_version, max_len, "2.0.0-42");
    return OTA_RESULT_OK;
}

ota_result_t ota_http_download_firmware(ota_context_t *ctx,
                                         uint8_t *buffer, uint32_t buf_size,
                                         uint32_t *out_size)
{
    if (!ctx || !buffer || !out_size) return OTA_RESULT_ERR_NETWORK;
    ctx->state = OTA_STATE_DOWNLOADING;
    uint32_t sz = (buf_size < 65536) ? buf_size : 65536;
    for (uint32_t i = 0; i < sz; i++)
        buffer[i] = (uint8_t)(i & 0xFF);
    *out_size = sz;
    return OTA_RESULT_OK;
}

const char *ota_state_name(ota_state_t state)
{
    static const char *names[] = {
        "IDLE","CHECKING","DOWNLOADING","VERIFYING",
        "PROGRAMMING","SWAPPING","COMPLETE","FAILED","ROLLBACK"
    };
    if (state > OTA_STATE_ROLLBACK) return "UNKNOWN";
    return names[state];
}

const char *ota_result_name(ota_result_t result)
{
    switch (result) {
    case OTA_RESULT_OK:             return "OK";
    case OTA_RESULT_ERR_NETWORK:    return "ERR_NETWORK";
    case OTA_RESULT_ERR_TIMEOUT:    return "ERR_TIMEOUT";
    case OTA_RESULT_ERR_CHECKSUM:   return "ERR_CHECKSUM";
    case OTA_RESULT_ERR_SIGNATURE:  return "ERR_SIGNATURE";
    case OTA_RESULT_ERR_FLASH:      return "ERR_FLASH";
    case OTA_RESULT_ERR_NO_UPDATE:  return "ERR_NO_UPDATE";
    case OTA_RESULT_ERR_VERSION:    return "ERR_VERSION";
    case OTA_RESULT_ERR_STORAGE:    return "ERR_STORAGE";
    case OTA_RESULT_ERR_ABORTED:    return "ERR_ABORTED";
    default:                         return "UNKNOWN";
    }
}
