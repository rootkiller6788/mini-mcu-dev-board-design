/*
 * usb_dfu_core.h — USB Device Firmware Upgrade (DFU) Core Definitions
 *
 * Implements the USB DFU Class Specification, Revision 1.1 (Aug 5, 2004)
 * USB-IF Device Class Specification for Device Firmware Upgrade.
 *
 * Knowledge Coverage:
 *   L1: DFU protocol definitions, state machine, request codes
 *   L2: DFU mode entry/exit, manifestation, detach-attach sequence
 *   L4: DFU state transition invariants, status propagation
 *
 * Reference: USB DFU Class Specification v1.1, Section 3-6
 * MIT 6.004 — Computation Structures (boot state machines)
 * CMU 18-349 — Embedded Real-Time (firmware update protocols)
 */

#ifndef USB_DFU_CORE_H
#define USB_DFU_CORE_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* ─── USB DFU 1.1 Class-Specific Request Codes (§4) ─── */
#define DFU_DETACH      0x00
#define DFU_DNLOAD      0x01
#define DFU_UPLOAD      0x02
#define DFU_GETSTATUS   0x03
#define DFU_CLRSTATUS   0x04
#define DFU_GETSTATE    0x05
#define DFU_ABORT       0x06

/* ─── DFU Protocol Attributes (bmAttributes field) ─── */
#define DFU_ATTR_CAN_DNLOAD              (1u << 0)
#define DFU_ATTR_CAN_UPLOAD              (1u << 1)
#define DFU_ATTR_MANIFESTATION_TOLERANT  (1u << 2)
#define DFU_ATTR_WILL_DETACH             (1u << 3)

/* ─── DFU State Enumeration (§A.1, Table A.1) ─── */
typedef enum {
    DFU_STATE_APP_IDLE                = 0,
    DFU_STATE_APP_DETACH              = 1,
    DFU_STATE_DFU_IDLE                = 2,
    DFU_STATE_DFU_DNLOAD_SYNC         = 3,
    DFU_STATE_DFU_DNBUSY              = 4,
    DFU_STATE_DFU_DNLOAD_IDLE         = 5,
    DFU_STATE_DFU_MANIFEST_SYNC       = 6,
    DFU_STATE_DFU_MANIFEST            = 7,
    DFU_STATE_DFU_MANIFEST_WAIT_RESET = 8,
    DFU_STATE_DFU_UPLOAD_IDLE         = 9,
    DFU_STATE_DFU_ERROR               = 10
} dfu_state_t;

/* ─── DFU Status Enumeration (§A.2, Table A.2) ─── */
typedef enum {
    DFU_STATUS_OK                = 0x00,
    DFU_STATUS_ERR_TARGET        = 0x01,
    DFU_STATUS_ERR_FILE          = 0x02,
    DFU_STATUS_ERR_WRITE         = 0x03,
    DFU_STATUS_ERR_ERASE         = 0x04,
    DFU_STATUS_ERR_CHECK_ERASED  = 0x05,
    DFU_STATUS_ERR_PROG          = 0x06,
    DFU_STATUS_ERR_VERIFY        = 0x07,
    DFU_STATUS_ERR_ADDRESS       = 0x08,
    DFU_STATUS_ERR_NOTDONE       = 0x09,
    DFU_STATUS_ERR_FIRMWARE      = 0x0A,
    DFU_STATUS_ERR_VENDOR        = 0x0B,
    DFU_STATUS_ERR_USBR          = 0x0C,
    DFU_STATUS_ERR_POR           = 0x0D,
    DFU_STATUS_ERR_UNKNOWN       = 0x0E,
    DFU_STATUS_ERR_STALLEDPKT    = 0x0F
} dfu_status_t;

/* ─── DFU GetStatus Response (§6.1.2, Table 6.1) ─── */
typedef struct {
    uint8_t  bStatus;
    uint8_t  bwPollTimeout[3];
    uint8_t  bState;
    uint8_t  iString;
} __attribute__((packed)) dfu_status_response_t;

/* ─── DFU Functional Descriptor (§4.1.1, Table 4.1) ─── */
typedef struct {
    uint8_t  bLength;
    uint8_t  bDescriptorType;
    uint8_t  bmAttributes;
    uint16_t wDetachTimeOut;
    uint16_t wTransferSize;
    uint16_t bcdDFUVersion;
} __attribute__((packed)) dfu_functional_descriptor_t;

/* ─── DFU Download Block Header ─── */
typedef struct {
    uint32_t base_address;
    uint32_t block_index;
    uint32_t block_size;
    uint32_t total_size;
    uint16_t block_crc16;
    uint8_t  block_flags;
} dfu_dnload_header_t;

#define DFU_BLOCK_FLAG_LAST      0x01
#define DFU_BLOCK_FLAG_ENCRYPTED 0x02

/* ─── DFU Operation Context ─── */
typedef struct {
    dfu_state_t   state;
    dfu_status_t  status;
    uint32_t      poll_timeout_ms;
    uint32_t      transfer_size;
    uint32_t      total_downloaded;
    uint32_t      expected_total;
    uint32_t      current_address;
    uint16_t      block_count;
    uint8_t       manifest_started;
    uint8_t       detach_requested;
    uint8_t       will_detach;
    uint8_t       manifestation_tolerant;
    uint8_t       upload_supported;
    uint8_t       download_supported;
} dfu_context_t;

/* ─── DFU String Descriptor Index ─── */
typedef enum {
    DFU_STRING_MANUFACTURER = 1,
    DFU_STRING_PRODUCT      = 2,
    DFU_STRING_SERIAL       = 3,
    DFU_STRING_DFU_MODE     = 4,
    DFU_STRING_STATUS_OK    = 5,
    DFU_STRING_STATUS_ERROR = 6
} dfu_string_index_t;

/* ─── API Declarations ─── */
void dfu_init(dfu_context_t *ctx, uint16_t transfer_size,
              uint8_t attributes);

bool dfu_handle_detach(dfu_context_t *ctx, uint16_t timeout_ms);

dfu_status_t dfu_handle_dnload(dfu_context_t *ctx,
                                const uint8_t *data, uint16_t len,
                                uint16_t block_num);

dfu_status_t dfu_handle_upload(dfu_context_t *ctx,
                                uint8_t *buf, uint16_t max_len,
                                uint16_t block_num, uint16_t *out_len);

void dfu_get_status(const dfu_context_t *ctx,
                    dfu_status_response_t *response);

dfu_state_t dfu_clear_status(dfu_context_t *ctx);

dfu_state_t dfu_get_state(const dfu_context_t *ctx);

dfu_state_t dfu_abort_operation(dfu_context_t *ctx);

bool dfu_manifest(dfu_context_t *ctx);

bool dfu_is_valid_transition(dfu_state_t current, dfu_state_t next);

uint32_t dfu_compute_poll_timeout(dfu_state_t state);

const char *dfu_state_name(dfu_state_t state);

const char *dfu_status_name(dfu_status_t status);

bool dfu_validate_address(uint32_t address, uint32_t size,
                          uint32_t flash_base, uint32_t flash_end);

#endif /* USB_DFU_CORE_H */
