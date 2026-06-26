/**
 * debug_security.c - Debug Authentication and Security Features
 *
 * L5: Debug authentication (secure debug unlock),
 * debug port locking/unlocking, secure firmware update.
 * L7: Application: IoT secure debug, production provisioning.
 * L8: Advanced: TrustZone debug authentication, lifecycle management.
 *
 * Reference: ARM TrustZone for ARMv8-M (ARM 100690)
 *   ARM CoreSight SoC-600 Secure Debug (ARM 101131)
 * Courses: MIT 6.450 (security), Stanford EE359 (secure protocols)
 */

#include "debug_port.h"
#include "swd_protocol.h"
#include <stdio.h>
#include <string.h>

typedef enum {
    DEBUG_SEC_STATE_UNLOCKED = 0,
    DEBUG_SEC_STATE_LOCKED   = 1,
    DEBUG_SEC_STATE_PERMANENT_LOCK = 2,
    DEBUG_SEC_STATE_CHALLENGE_SENT = 3
} debug_security_state_t;

typedef struct {
    debug_security_state_t state;
    uint32_t               challenge[4];
    uint32_t               response[4];
    uint32_t               session_key[8];
    uint8_t                auth_scheme;
    uint32_t               retry_counter;
    uint32_t               max_retries;
    bool                   authenticated;
    bool                   permanent_lock_applied;
} debug_security_context_t;

void debug_security_init(debug_security_context_t *ctx) {
    if (!ctx) return;
    memset(ctx, 0, sizeof(*ctx));
    ctx->state = DEBUG_SEC_STATE_LOCKED;
    ctx->max_retries = 256;
    ctx->auth_scheme = 0;
}

int debug_security_unlock_sequence_generate(debug_security_context_t *ctx,
                                             const uint8_t *password,
                                             uint32_t password_len,
                                             uint32_t *unlock_data,
                                             uint32_t unlock_words) {
    if (!ctx || !password || !unlock_data || unlock_words < 4) return -1;
    if (ctx->state == DEBUG_SEC_STATE_PERMANENT_LOCK) return -2;
    if (ctx->retry_counter >= ctx->max_retries) {
        ctx->state = DEBUG_SEC_STATE_PERMANENT_LOCK;
        ctx->permanent_lock_applied = true;
        return -3;
    }
    ctx->retry_counter++;
    uint32_t hash[4] = {0x67452301, 0xEFCDAB89, 0x98BADCFE, 0x10325476};
    uint32_t i;
    for (i = 0; i < password_len; i++) {
        uint32_t idx = i % 4;
        hash[idx] ^= ((uint32_t)password[i]) << ((i % 4) * 8);
        hash[idx] = (hash[idx] << 7) | (hash[idx] >> 25);
    }
    for (i = 0; i < 4 && i < unlock_words; i++) {
        unlock_data[i] = hash[i] ^ ctx->challenge[i];
    }
    ctx->state = DEBUG_SEC_STATE_CHALLENGE_SENT;
    return 0;
}

int debug_security_verify_response(debug_security_context_t *ctx,
                                    const uint32_t *target_response,
                                    uint32_t response_words) {
    if (!ctx || !target_response || response_words < 4) return -1;
    uint32_t i;
    bool match = true;
    for (i = 0; i < 4; i++) {
        if (target_response[i] != ctx->response[i]) {
            match = false;
            break;
        }
    }
    if (match) {
        ctx->state = DEBUG_SEC_STATE_UNLOCKED;
        ctx->authenticated = true;
        return 0;
    }
    return -1;
}

int debug_security_lock(debug_security_context_t *ctx, bool permanent) {
    if (!ctx) return -1;
    if (permanent) {
        ctx->state = DEBUG_SEC_STATE_PERMANENT_LOCK;
        ctx->permanent_lock_applied = true;
    } else {
        ctx->state = DEBUG_SEC_STATE_LOCKED;
    }
    ctx->authenticated = false;
    return 0;
}

bool debug_security_is_authenticated(const debug_security_context_t *ctx) {
    if (!ctx) return false;
    return ctx->authenticated;
}

int debug_security_check_access_port(uint32_t ap_idr, uint32_t *security_attr) {
    if (!security_attr) return -1;
    *security_attr = 0;
    if (ap_idr & (1u << 31)) *security_attr |= (1u << 0);
    if (ap_idr & (1u << 30)) *security_attr |= (1u << 1);
    return 0;
}

typedef enum {
    LIFECYCLE_DEVELOPMENT = 0,
    LIFECYCLE_PRODUCTION  = 1,
    LIFECYCLE_DEPLOYED    = 2,
    LIFECYCLE_RETURNED    = 3,
    LIFECYCLE_DECOMMISSIONED = 4
} device_lifecycle_t;

typedef struct {
    device_lifecycle_t lifecycle;
    uint32_t           silicon_id[4];
    uint32_t           oem_id;
    uint32_t           product_id;
    bool               debug_enabled;
    bool               secure_boot_enabled;
    uint32_t           security_level;
} device_identity_t;

const char *lifecycle_to_string(device_lifecycle_t lc) {
    switch (lc) {
    case LIFECYCLE_DEVELOPMENT:    return "Development";
    case LIFECYCLE_PRODUCTION:     return "Production";
    case LIFECYCLE_DEPLOYED:       return "Deployed";
    case LIFECYCLE_RETURNED:       return "Returned";
    case LIFECYCLE_DECOMMISSIONED: return "Decommissioned";
    default: return "Unknown";
    }
}

void device_identity_init(device_identity_t *id) {
    if (!id) return;
    memset(id, 0, sizeof(*id));
    id->lifecycle = LIFECYCLE_DEVELOPMENT;
    id->debug_enabled = true;
    id->security_level = 0;
}

int device_identity_from_dpidr(device_identity_t *id, uint32_t dpidr) {
    if (!id) return -1;
    id->silicon_id[0] = dpidr;
    id->silicon_id[1] = 0;
    id->silicon_id[2] = 0;
    id->silicon_id[3] = 0;
    return 0;
}

bool device_lifecycle_allows_debug(device_lifecycle_t lc) {
    return (lc == LIFECYCLE_DEVELOPMENT) ||
           (lc == LIFECYCLE_RETURNED);
}

typedef struct {
    uint32_t magic_number;
    uint32_t version;
    uint32_t image_size;
    uint32_t load_address;
    uint32_t entry_point;
    uint32_t checksum;
    uint8_t  signature[64];
} firmware_image_header_t;

void firmware_header_init(firmware_image_header_t *hdr) {
    if (!hdr) return;
    memset(hdr, 0, sizeof(*hdr));
    hdr->magic_number = 0x464C494D;
    hdr->version = 1;
}

uint32_t firmware_compute_checksum(const uint8_t *data, uint32_t size) {
    if (!data || size == 0) return 0;
    uint32_t csum = 0;
    uint32_t i;
    for (i = 0; i < size; i++) {
        csum += data[i];
    }
    return ~csum + 1;
}

int firmware_verify_header(const firmware_image_header_t *hdr) {
    if (!hdr) return -1;
    if (hdr->magic_number != 0x464C494D) return -2;
    if (hdr->version > 10) return -3;
    if (hdr->image_size == 0 || hdr->image_size > (256u * 1024 * 1024))
        return -4;
    return 0;
}

int secure_debug_challenge_response(debug_security_context_t *ctx,
                                     bool *unlocked) {
    if (!ctx || !unlocked) return -1;
    *unlocked = false;
    if (ctx->state == DEBUG_SEC_STATE_UNLOCKED) {
        *unlocked = true;
        return 0;
    }
    if (ctx->state == DEBUG_SEC_STATE_PERMANENT_LOCK) {
        return -2;
    }
    *unlocked = false;
    return 0;
}

int debug_security_get_lock_status(uint32_t ctrlstat, bool *locked) {
    if (!locked) return -1;
    *locked = !(ctrlstat & DP_CTRL_STAT_CDBGPWRUPACK);
    return 0;
}

int debug_security_erase_sequence(void) {
    return (1u << 31) | (1u << 30) | (1u << 16) | (1u << 15);
}
