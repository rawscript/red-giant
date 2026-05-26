/**
 * @file rgtp_crypto.c
 * @brief AEAD encrypt/decrypt and nonce construction.
 *
 * Critical fixes from the original code:
 *  - Uses crypto_aead_chacha20poly1305_IETF (12-byte nonce), NOT the original
 *    variant that requires an 8-byte nonce (the old code passed 24 bytes).
 *  - Nonce is derived deterministically from the chunk index, guaranteeing
 *    uniqueness within an Exposure without a per-chunk random nonce.
 *  - Plaintext is NEVER written before the authentication tag is verified.
 *
 * Requirements: 3.2, 3.3, 3.4, 3.5, 3.15
 */

#include "rgtp_crypto_internal.h"

#include <string.h>   /* memset, memcpy */
#include <stdint.h>

/* ── Backend headers ────────────────────────────────────────────────────── */
#if defined(RGTP_CRYPTO_BACKEND_LIBSODIUM)
#  include <sodium.h>
   /* Verify the IETF variant nonce size matches our constant */
   _Static_assert(crypto_aead_chacha20poly1305_IETF_NPUBBYTES == RGTP_AEAD_NONCE_BYTES,
                  "libsodium IETF nonce size mismatch");
   _Static_assert(crypto_aead_chacha20poly1305_IETF_ABYTES == RGTP_AEAD_TAG_BYTES,
                  "libsodium IETF tag size mismatch");
   _Static_assert(crypto_aead_chacha20poly1305_IETF_KEYBYTES == RGTP_AEAD_KEY_BYTES,
                  "libsodium IETF key size mismatch");
#elif defined(RGTP_CRYPTO_BACKEND_OPENSSL)
#  include <openssl/evp.h>
#  include <openssl/err.h>
#else
#  error "rgtp_crypto: no crypto backend. Build with RGTP_CRYPTO_BACKEND=libsodium or openssl."
#endif

/* ── Nonce construction ─────────────────────────────────────────────────── */

void rgtp_build_nonce(uint32_t chunk_index, uint8_t nonce[RGTP_AEAD_NONCE_BYTES])
{
    /*
     * nonce[12] = { chunk_index_LE[0..3], 0x00 × 8 }
     *
     * Little-endian encoding of the 32-bit chunk index in the first 4 bytes,
     * remaining 8 bytes zero-padded.  This is safe because:
     *   1. Each Exposure uses a fresh random 256-bit key.
     *   2. Chunk indices are unique within an Exposure (0 .. chunk_count-1).
     *   3. The key is never reused across Exposures.
     */
    memset(nonce, 0, RGTP_AEAD_NONCE_BYTES);
    nonce[0] = (uint8_t)( chunk_index        & 0xFFu);
    nonce[1] = (uint8_t)((chunk_index >>  8) & 0xFFu);
    nonce[2] = (uint8_t)((chunk_index >> 16) & 0xFFu);
    nonce[3] = (uint8_t)((chunk_index >> 24) & 0xFFu);
}

/* ── AEAD encrypt ───────────────────────────────────────────────────────── */

rgtp_error_t rgtp_encrypt_chunk(const uint8_t  key[RGTP_AEAD_KEY_BYTES],
                                 uint32_t       chunk_index,
                                 const uint8_t* plaintext,
                                 size_t         pt_len,
                                 uint8_t*       ciphertext,
                                 size_t*        ct_len_out)
{
    if (key == NULL || plaintext == NULL || ciphertext == NULL || ct_len_out == NULL) {
        return RGTP_ERR_INVALID_ARG;
    }
    if (pt_len == 0 || pt_len > 65507u) {
        return RGTP_ERR_INVALID_ARG;
    }

    uint8_t nonce[RGTP_AEAD_NONCE_BYTES];
    rgtp_build_nonce(chunk_index, nonce);

#if defined(RGTP_CRYPTO_BACKEND_LIBSODIUM)
    unsigned long long ct_len = 0;
    int rc = crypto_aead_chacha20poly1305_ietf_encrypt(
        ciphertext, &ct_len,
        plaintext,  (unsigned long long)pt_len,
        NULL, 0,          /* no additional data */
        NULL,             /* nsec — unused by this variant */
        nonce, key);
    if (rc != 0) {
        return RGTP_ERR_ENCRYPT;
    }
    *ct_len_out = (size_t)ct_len;
    return RGTP_OK;

#elif defined(RGTP_CRYPTO_BACKEND_OPENSSL)
    EVP_AEAD_CTX* ctx = NULL;
    int           ok  = 0;
    size_t        out_len = 0;

    ctx = EVP_AEAD_CTX_new(EVP_aead_chacha20_poly1305(),
                            key, RGTP_AEAD_KEY_BYTES,
                            RGTP_AEAD_TAG_BYTES);
    if (ctx == NULL) {
        return RGTP_ERR_ENCRYPT;
    }

    ok = EVP_AEAD_CTX_seal(ctx,
                            ciphertext, &out_len, pt_len + RGTP_AEAD_TAG_BYTES,
                            nonce, RGTP_AEAD_NONCE_BYTES,
                            plaintext, pt_len,
                            NULL, 0);
    EVP_AEAD_CTX_free(ctx);

    if (!ok) {
        return RGTP_ERR_ENCRYPT;
    }
    *ct_len_out = out_len;
    return RGTP_OK;
#endif
}

/* ── AEAD decrypt ───────────────────────────────────────────────────────── */

rgtp_error_t rgtp_decrypt_chunk(const uint8_t  key[RGTP_AEAD_KEY_BYTES],
                                 uint32_t       chunk_index,
                                 const uint8_t* ciphertext,
                                 size_t         ct_len,
                                 uint8_t*       plaintext,
                                 size_t*        pt_len_out)
{
    if (key == NULL || ciphertext == NULL || plaintext == NULL || pt_len_out == NULL) {
        return RGTP_ERR_INVALID_ARG;
    }
    if (ct_len <= RGTP_AEAD_TAG_BYTES) {
        return RGTP_ERR_TRUNCATED;
    }

    uint8_t nonce[RGTP_AEAD_NONCE_BYTES];
    rgtp_build_nonce(chunk_index, nonce);

#if defined(RGTP_CRYPTO_BACKEND_LIBSODIUM)
    unsigned long long pt_len = 0;
    /*
     * crypto_aead_chacha20poly1305_ietf_decrypt verifies the tag BEFORE
     * writing any plaintext — this is the correct, safe calling convention.
     */
    int rc = crypto_aead_chacha20poly1305_ietf_decrypt(
        plaintext, &pt_len,
        NULL,                /* nsec — unused */
        ciphertext, (unsigned long long)ct_len,
        NULL, 0,             /* no additional data */
        nonce, key);
    if (rc != 0) {
        return RGTP_ERR_AUTH_FAIL;
    }
    *pt_len_out = (size_t)pt_len;
    return RGTP_OK;

#elif defined(RGTP_CRYPTO_BACKEND_OPENSSL)
    EVP_AEAD_CTX* ctx = NULL;
    int           ok  = 0;
    size_t        out_len = 0;

    ctx = EVP_AEAD_CTX_new(EVP_aead_chacha20_poly1305(),
                            key, RGTP_AEAD_KEY_BYTES,
                            RGTP_AEAD_TAG_BYTES);
    if (ctx == NULL) {
        return RGTP_ERR_DECRYPT;
    }

    ok = EVP_AEAD_CTX_open(ctx,
                            plaintext, &out_len, ct_len - RGTP_AEAD_TAG_BYTES,
                            nonce, RGTP_AEAD_NONCE_BYTES,
                            ciphertext, ct_len,
                            NULL, 0);
    EVP_AEAD_CTX_free(ctx);

    if (!ok) {
        return RGTP_ERR_AUTH_FAIL;
    }
    *pt_len_out = out_len;
    return RGTP_OK;
#endif
}
