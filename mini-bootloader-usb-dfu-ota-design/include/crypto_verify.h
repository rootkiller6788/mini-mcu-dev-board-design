/*
 * crypto_verify.h -- Cryptographic Verification for Secure Bootloader
 *
 * Implements SHA-256 hashing, AES-128-CBC/GCM encryption, ECDSA P-256
 * signature verification, HMAC-SHA256, and constant-time comparison.
 *
 * Knowledge Coverage:
 *   L1: Hash functions (SHA-256), symmetric encryption (AES-128),
 *       asymmetric signatures (ECDSA P-256), MAC (HMAC)
 *   L2: Merkle-Damgard construction, Feistel network, ECC point arithmetic
 *   L3: Finite field GF(2^8) for AES, prime field GF(p) for ECDSA
 *   L4: Collision resistance, preimage resistance, DLP hardness (secp256r1)
 *   L5: AES key schedule, SHA-256 compression, ECDSA verify algorithm
 *
 * Reference: FIPS 180-4 (SHA-256), FIPS 197 (AES), FIPS 186-5 (ECDSA)
 * NIST SP 800-38D (GCM), RFC 2104 (HMAC)
 * Stanford CS255 -- Applied Cryptography
 */

#ifndef CRYPTO_VERIFY_H
#define CRYPTO_VERIFY_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* SHA-256 Constants */
#define SHA256_BLOCK_SIZE   64
#define SHA256_DIGEST_SIZE  32
#define SHA256_HASH_SIZE    32

/* SHA-256 Context */
typedef struct {
    uint32_t state[8];
    uint64_t bit_count;
    uint8_t  buffer[SHA256_BLOCK_SIZE];
    uint32_t buffer_idx;
} sha256_context_t;

/* AES Constants */
#define AES_BLOCK_SIZE      16
#define AES128_KEY_SIZE     16
#define AES256_KEY_SIZE     32
#define AES_NUM_ROUNDS_128  10
#define AES_NUM_ROUNDS_256  14

/* AES Key */
typedef struct {
    uint8_t  key_bytes[32];
    uint32_t round_keys[60];
    uint8_t  key_size;
    uint8_t  num_rounds;
} aes_key_t;

/* GCM Context */
typedef struct {
    aes_key_t     cipher_key;
    uint8_t       iv[12];
    uint32_t      iv_len;
    uint8_t       hash_key[16];
    uint8_t       tag[16];
    uint32_t      tag_len;
    uint64_t      aad_len;
    uint64_t      ct_len;
    uint8_t       j0[16];
} gcm_context_t;

/* ECDSA P-256 Constants */
#define ECDSA_P256_KEY_SIZE     32
#define ECDSA_P256_SIG_SIZE     64
#define ECDSA_P256_PUBKEY_SIZE  64

/* ECDSA Public Key (uncompressed) */
typedef struct {
    uint8_t  x[ECDSA_P256_KEY_SIZE];
    uint8_t  y[ECDSA_P256_KEY_SIZE];
} ecdsa_p256_pubkey_t;

/* ECDSA Signature */
typedef struct {
    uint8_t  r[ECDSA_P256_KEY_SIZE];
    uint8_t  s[ECDSA_P256_KEY_SIZE];
} ecdsa_p256_signature_t;

/* Secure Storage Key Slot Identifier */
typedef enum {
    KEY_SLOT_BOOT_PUBKEY   = 0,
    KEY_SLOT_OTA_PUBKEY    = 1,
    KEY_SLOT_APP_SIGNKEY   = 2,
    KEY_SLOT_AES_OTP       = 3,
    KEY_SLOT_HMAC_KEY      = 4,
    KEY_SLOT_MAX           = 8
} key_slot_id_t;

/* Key Slot Descriptor */
typedef struct {
    uint8_t  data[64];
    uint8_t  size;
    uint8_t  type;
    uint8_t  writable;
    uint8_t  locked;
    uint32_t version;
} key_slot_t;

/* Security counter (anti-rollback) */
typedef struct {
    uint32_t security_counter;
    uint32_t min_version;
} security_counter_t;

/* API Declarations */

/* SHA-256 */
void sha256_init(sha256_context_t *ctx);
void sha256_update(sha256_context_t *ctx, const uint8_t *data, uint32_t len);
void sha256_final(sha256_context_t *ctx, uint8_t digest[SHA256_DIGEST_SIZE]);
void sha256_hash(const uint8_t *data, uint32_t len,
                 uint8_t digest[SHA256_DIGEST_SIZE]);

/* AES-128 */
void aes128_key_expand(aes_key_t *key, const uint8_t *key_bytes, uint8_t size);
void aes128_encrypt_block(const aes_key_t *key,
                           const uint8_t plain[16], uint8_t cipher[16]);
void aes128_decrypt_block(const aes_key_t *key,
                           const uint8_t cipher[16], uint8_t plain[16]);

/* AES-CBC mode */
void aes_cbc_encrypt(const aes_key_t *key, const uint8_t iv[16],
                      const uint8_t *plain, uint32_t len, uint8_t *cipher);
void aes_cbc_decrypt(const aes_key_t *key, const uint8_t iv[16],
                      const uint8_t *cipher, uint32_t len, uint8_t *plain);

/* AES-GCM mode */
void gcm_init(gcm_context_t *gcm, const aes_key_t *key,
              const uint8_t *iv, uint32_t iv_len);
void gcm_update_aad(gcm_context_t *gcm, const uint8_t *aad, uint32_t len);
void gcm_encrypt(gcm_context_t *gcm, const uint8_t *plain, uint32_t len,
                 uint8_t *cipher);
void gcm_decrypt(gcm_context_t *gcm, const uint8_t *cipher, uint32_t len,
                 uint8_t *plain);
void gcm_final(gcm_context_t *gcm, uint8_t tag[16]);

/* ECDSA P-256 */
bool ecdsa_p256_verify(const ecdsa_p256_pubkey_t *pubkey,
                        const uint8_t *hash, uint32_t hash_len,
                        const ecdsa_p256_signature_t *sig);
bool ecdsa_p256_import_pubkey(ecdsa_p256_pubkey_t *pubkey,
                               const uint8_t *der_data, uint32_t der_len);

/* HMAC-SHA256 (RFC 2104) */
void hmac_sha256(const uint8_t *key, uint32_t key_len,
                 const uint8_t *message, uint32_t msg_len,
                 uint8_t mac[SHA256_DIGEST_SIZE]);

/* Constant-time memory comparison (no early exit) */
bool ct_memcmp(const uint8_t *a, const uint8_t *b, uint32_t len);

/* HKDF (RFC 5869): HMAC-based Key Derivation Function */
void hkdf_extract(const uint8_t *salt, uint32_t salt_len,
                  const uint8_t *ikm, uint32_t ikm_len,
                  uint8_t prk[SHA256_DIGEST_SIZE]);
void hkdf_expand(const uint8_t *prk, uint32_t prk_len,
                 const uint8_t *info, uint32_t info_len,
                 uint8_t *okm, uint32_t okm_len);

/* CRC-32 (IEEE 802.3, polynomial 0xEDB88320) */
uint32_t crc32_compute(const uint8_t *data, uint32_t len);
uint32_t crc32_continue(uint32_t crc, const uint8_t *data, uint32_t len);

/* CRC-16-CCITT (polynomial 0x1021) */
uint16_t crc16_ccitt_compute(const uint8_t *data, uint32_t len);

/* Key slot management */
void key_slot_init(key_slot_t slots[KEY_SLOT_MAX]);
bool key_slot_write(key_slot_t *slot, const uint8_t *data, uint8_t size);
bool key_slot_lock(key_slot_t *slot);
bool key_slot_is_locked(const key_slot_t *slot);

/* Security counter (anti-rollback) */
void sec_counter_init(security_counter_t *counter, uint32_t initial);
bool sec_counter_update(security_counter_t *counter, uint32_t new_value);
bool sec_counter_verify(const security_counter_t *counter,
                         uint32_t required_min);

/* Convert hash to hex string */
void hash_to_hex(const uint8_t *hash, uint32_t len,
                 char *hex_out, uint32_t hex_size);

#endif /* CRYPTO_VERIFY_H */
