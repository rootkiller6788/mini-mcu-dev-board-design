/*
 * usb_dfu_core.c -- USB DFU Protocol State Machine Implementation
 *
 * Full implementation of USB DFU 1.1 class-specific request handling,
 * state transitions, and manifestation logic.
 *
 * Knowledge Points:
 *   L2: DFU state machine with all 11 states and transitions
 *   L5: DFU download/upload protocol implementation
 *   L6: DFU manifestation and firmware activation
 */

#include "usb_dfu_core.h"
#include <string.h>
#include <stdio.h>

/* ─── DFU State Transition Table (§A.1) ───
 * Defines which transitions are valid from each state.
 * 1 = valid, 0 = invalid.
 */
static const uint8_t dfu_transition_table[11][11] = {
    /*               APP_IDLE APP_DET DFU_IDLE DNLD_SYNC DNBUSY DNLD_IDLE MANIF_SYNC MANIFEST MANIF_WAIT UPLOAD_IDLE ERROR */
    /* APP_IDLE    */ { 0,     1,      0,       0,        0,     0,        0,          0,       0,         0,          0    },
    /* APP_DETACH  */ { 0,     0,      1,       0,        0,     0,        0,          0,       0,         0,          0    },
    /* DFU_IDLE    */ { 0,     0,      0,       1,        0,     0,        0,          0,       0,         1,          1    },
    /* DNLD_SYNC   */ { 0,     0,      0,       0,        1,     0,        0,          0,       0,         0,          1    },
    /* DNBUSY      */ { 0,     0,      0,       0,        0,     1,        0,          0,       0,         0,          1    },
    /* DNLD_IDLE   */ { 0,     0,      0,       1,        0,     0,        1,          0,       0,         0,          1    },
    /* MANIF_SYNC  */ { 0,     0,      0,       0,        0,     0,        0,          1,       0,         0,          1    },
    /* MANIFEST    */ { 0,     0,      0,       0,        0,     0,        0,          0,       1,         0,          0    },
    /* MANIF_WAIT  */ { 0,     0,      1,       0,        0,     0,        0,          0,       0,         0,          0    },
    /* UPLOAD_IDLE */ { 0,     0,      1,       0,        0,     0,        0,          0,       0,         0,          1    },
    /* ERROR       */ { 0,     0,      1,       0,        0,     0,        0,          0,       0,         0,          0    },
};

/* State → name mapping */
static const char *dfu_state_names[] = {
    "appIDLE", "appDETACH", "dfuIDLE",
    "dfuDNLOAD-SYNC", "dfuDNBUSY", "dfuDNLOAD-IDLE",
    "dfuMANIFEST-SYNC", "dfuMANIFEST",
    "dfuMANIFEST-WAIT-RESET", "dfuUPLOAD-IDLE",
    "dfuERROR"
};

static const char *dfu_status_names[] = {
    "OK", "errTARGET", "errFILE", "errWRITE", "errERASE",
    "errCHECK_ERASED", "errPROG", "errVERIFY", "errADDRESS",
    "errNOTDONE", "errFIRMWARE", "errVENDOR", "errUSBR",
    "errPOR", "errUNKNOWN", "errSTALLEDPKT"
};

/* ─── DFU State Transition Validation ─── */

void dfu_init(dfu_context_t *ctx, uint16_t transfer_size,
              uint8_t attributes)
{
    if (!ctx) return;
    memset(ctx, 0, sizeof(*ctx));
    ctx->state          = DFU_STATE_DFU_IDLE;
    ctx->status         = DFU_STATUS_OK;
    ctx->transfer_size  = transfer_size;
    ctx->download_supported = (attributes & DFU_ATTR_CAN_DNLOAD) ? 1 : 0;
    ctx->upload_supported   = (attributes & DFU_ATTR_CAN_UPLOAD) ? 1 : 0;
    ctx->will_detach        = (attributes & DFU_ATTR_WILL_DETACH) ? 1 : 0;
    ctx->manifestation_tolerant = (attributes & DFU_ATTR_MANIFESTATION_TOLERANT) ? 1 : 0;
    ctx->poll_timeout_ms    = 1;  /* Default: 1ms for idle states */
}

bool dfu_is_valid_transition(dfu_state_t current, dfu_state_t next)
{
    if (current > DFU_STATE_DFU_ERROR || next > DFU_STATE_DFU_ERROR)
        return false;
    return dfu_transition_table[current][next] == 1;
}

/* ─── DFU_DETACH Handler (§5.1) ─── */

bool dfu_handle_detach(dfu_context_t *ctx, uint16_t timeout_ms)
{
    if (!ctx) return false;

    /* DETACH valid from APP_IDLE or DFU_IDLE */
    if (ctx->state != DFU_STATE_APP_IDLE &&
        ctx->state != DFU_STATE_DFU_IDLE)
        return false;

    ctx->detach_requested = 1;
    ctx->poll_timeout_ms  = (uint32_t)timeout_ms;
    ctx->state = DFU_STATE_APP_DETACH;

    /* If will_detach: device initiates USB re-enumeration after timeout */
    if (ctx->will_detach) {
        /* After timeout, the device should disconnect and re-enumerate
         * as a DFU-only device. The bootloader handles this transition. */
        ctx->state = DFU_STATE_DFU_IDLE;
    }

    return true;
}

/* ─── DFU_DNLOAD Handler (§6.1.1) ─── */

dfu_status_t dfu_handle_dnload(dfu_context_t *ctx,
                                const uint8_t *data, uint16_t len,
                                uint16_t block_num)
{
    if (!ctx) return DFU_STATUS_ERR_UNKNOWN;

    /* DNLOAD valid from DFU_IDLE or DNLOAD_IDLE */
    if (ctx->state == DFU_STATE_DFU_IDLE) {
        ctx->state = DFU_STATE_DFU_DNLOAD_SYNC;
    } else if (ctx->state != DFU_STATE_DFU_DNLOAD_IDLE) {
        ctx->status = DFU_STATUS_ERR_NOTDONE;
        ctx->state  = DFU_STATE_DFU_ERROR;
        return ctx->status;
    }

    /* wLength=0 in DNLOAD_IDLE means manifestation request */
    if (len == 0 && ctx->state == DFU_STATE_DFU_DNLOAD_IDLE) {
        ctx->state = DFU_STATE_DFU_MANIFEST_SYNC;
        ctx->manifest_started = 1;
        return DFU_STATUS_OK;
    }

    /* Normal download: data is firmware being programmed */
    if (data && len > 0) {
        if (len > ctx->transfer_size) {
            ctx->status = DFU_STATUS_ERR_FILE;
            ctx->state  = DFU_STATE_DFU_ERROR;
            return ctx->status;
        }

        /* Simulate flash programming: update counters */
        ctx->total_downloaded += len;
        ctx->block_count++;
        ctx->current_address += len;

        /* Transition: DNLOAD_SYNC → DNBUSY → DNLOAD_IDLE */
        if (ctx->state == DFU_STATE_DFU_DNLOAD_SYNC) {
            ctx->state = DFU_STATE_DFU_DNBUSY;
            ctx->poll_timeout_ms = dfu_compute_poll_timeout(ctx->state);
        }
        /* After "programming" completes: */
        ctx->state = DFU_STATE_DFU_DNLOAD_IDLE;
        ctx->status = DFU_STATUS_OK;

        /* If this is the last block (len < transfer_size), note it */
        if (len < ctx->transfer_size && ctx->expected_total == 0) {
            ctx->expected_total = ctx->total_downloaded;
        }
    }

    ctx->poll_timeout_ms = dfu_compute_poll_timeout(ctx->state);
    return ctx->status;
}

/* ─── DFU_UPLOAD Handler (§6.2) ─── */

dfu_status_t dfu_handle_upload(dfu_context_t *ctx,
                                uint8_t *buf, uint16_t max_len,
                                uint16_t block_num, uint16_t *out_len)
{
    if (!ctx || !buf || !out_len) return DFU_STATUS_ERR_UNKNOWN;

    /* UPLOAD valid from DFU_IDLE, UPLOAD_IDLE, or DFU_ERROR */
    if (ctx->state != DFU_STATE_DFU_IDLE &&
        ctx->state != DFU_STATE_DFU_UPLOAD_IDLE &&
        ctx->state != DFU_STATE_DFU_ERROR) {
        ctx->status = DFU_STATUS_ERR_NOTDONE;
        return ctx->status;
    }

    /* In a real bootloader, this would read from the current firmware
     * image in flash. For simulation, we return the downloaded data. */

    if (ctx->state == DFU_STATE_DFU_IDLE) {
        ctx->state = DFU_STATE_DFU_UPLOAD_IDLE;
    }

    /* Return minimal data: just indicate upload is possible */
    uint16_t to_copy = (max_len < ctx->transfer_size) ? max_len : ctx->transfer_size;
    memset(buf, 0, to_copy);  /* In real impl: read from flash */
    if (out_len) *out_len = to_copy;

    ctx->status = DFU_STATUS_OK;
    ctx->state  = DFU_STATE_DFU_UPLOAD_IDLE;
    return ctx->status;
}

/* ─── DFU_GETSTATUS Handler (§6.1.2) ─── */

void dfu_get_status(const dfu_context_t *ctx,
                    dfu_status_response_t *response)
{
    if (!ctx || !response) return;

    response->bStatus = (uint8_t)ctx->status;
    response->bState  = (uint8_t)ctx->state;

    /* 24-bit little-endian poll timeout */
    response->bwPollTimeout[0] = (uint8_t)(ctx->poll_timeout_ms & 0xFF);
    response->bwPollTimeout[1] = (uint8_t)((ctx->poll_timeout_ms >> 8) & 0xFF);
    response->bwPollTimeout[2] = (uint8_t)((ctx->poll_timeout_ms >> 16) & 0xFF);

    /* String index: use DFU mode string when in DFU states */
    if (ctx->state >= DFU_STATE_DFU_IDLE) {
        response->iString = DFU_STRING_DFU_MODE;
    } else {
        response->iString = 0;
    }
}

/* ─── DFU_CLRSTATUS Handler (§A.1) ─── */

dfu_state_t dfu_clear_status(dfu_context_t *ctx)
{
    if (!ctx) return DFU_STATE_DFU_ERROR;

    /* CLRSTATUS only valid in ERROR state */
    if (ctx->state == DFU_STATE_DFU_ERROR) {
        ctx->status = DFU_STATUS_OK;
        ctx->state  = DFU_STATE_DFU_IDLE;
    }
    return ctx->state;
}

/* ─── DFU_GETSTATE Handler (§6.1.5) ─── */

dfu_state_t dfu_get_state(const dfu_context_t *ctx)
{
    if (!ctx) return DFU_STATE_DFU_ERROR;
    return ctx->state;
}

/* ─── DFU_ABORT Handler ─── */

dfu_state_t dfu_abort_operation(dfu_context_t *ctx)
{
    if (!ctx) return DFU_STATE_DFU_ERROR;

    /* Reset download/upload progress */
    ctx->total_downloaded = 0;
    ctx->expected_total   = 0;
    ctx->block_count      = 0;
    ctx->manifest_started = 0;
    ctx->status = DFU_STATUS_OK;
    ctx->state  = DFU_STATE_DFU_IDLE;
    ctx->poll_timeout_ms = 1;

    return ctx->state;
}

/* ─── DFU Manifestation (§7) ─── */

bool dfu_manifest(dfu_context_t *ctx)
{
    if (!ctx) return false;

    if (ctx->state != DFU_STATE_DFU_MANIFEST_SYNC &&
        ctx->state != DFU_STATE_DFU_MANIFEST) {
        return false;
    }

    /* Enter MANIFEST state */
    ctx->state = DFU_STATE_DFU_MANIFEST;
    ctx->poll_timeout_ms = 5000;  /* Manifestation can take seconds */

    /* Manifestation: validate and activate new firmware.
     * In a real bootloader, this involves:
     *   1. Verify CRC/SHA of downloaded image
     *   2. Verify digital signature
     *   3. Set boot flags to boot new image
     *   4. Mark swap status for A/B update */

    /* Simulate successful manifestation */
    ctx->state = DFU_STATE_DFU_MANIFEST_WAIT_RESET;

    return true;
}

/* ─── Poll Timeout Computation ─── */

uint32_t dfu_compute_poll_timeout(dfu_state_t state)
{
    switch (state) {
        case DFU_STATE_DFU_IDLE:
        case DFU_STATE_DFU_DNLOAD_IDLE:
        case DFU_STATE_DFU_UPLOAD_IDLE:
            return 1;        /* 1 ms — ready for next command */

        case DFU_STATE_DFU_DNBUSY:
            return 100;      /* 100 ms — flash programming */

        case DFU_STATE_DFU_MANIFEST:
            return 5000;     /* 5 seconds — firmware validation */

        case DFU_STATE_DFU_MANIFEST_WAIT_RESET:
            return 500;      /* 500 ms — reset pending */

        default:
            return 1;
    }
}

/* ─── Address Validation ─── */

bool dfu_validate_address(uint32_t address, uint32_t size,
                          uint32_t flash_base, uint32_t flash_end)
{
    /* Check for overflow */
    if (address + size < address) return false;

    /* Check address range within flash bounds */
    if (address < flash_base) return false;
    if (address + size > flash_end) return false;

    /* Check word alignment (4-byte for ARM Cortex-M) */
    if (address & 0x3) return false;

    return true;
}

/* ─── String Helpers ─── */

const char *dfu_state_name(dfu_state_t state)
{
    if (state > DFU_STATE_DFU_ERROR) return "UNKNOWN";
    return dfu_state_names[state];
}

const char *dfu_status_name(dfu_status_t status)
{
    if (status > DFU_STATUS_ERR_STALLEDPKT) return "UNKNOWN";
    return dfu_status_names[status];
}
