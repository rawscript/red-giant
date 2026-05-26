/**
 * @file rgtp_merkle_hash.c
 * @brief Merkle leaf and internal node hash functions with domain separation.
 *
 * Hash function selection:
 *  - libsodium backend: BLAKE2b-256 (crypto_generichash)
 *  - OpenSSL backend:   SHA-256 (EVP_sha256)
 *
 * Domain separation prefixes prevent second-preimage attacks:
 *  - Leaf:     H(0x00 || chunk_data)
 *  - Internal: H(0x01 || left_hash || right_hash)
 *
 * Requirements: 3.6
 */

#include "rgtp_merkle_internal.h"

#include <string.h>   /* memcpy */

#if defined(RGTP_CRYPTO_BACKEND_LIBSODIUM)
#  include <sodium.h>
   _Static_assert(crypto_generichash_BYTES == RGTP_MERKLE_HASH_BYTES,
                  "libsodium generichash default output size mismatch");
#elif defined(RGTP_CRYPTO_BACKEND_OPENSSL)
#  include <openssl/evp.h>
#else
#  error "rgtp_merkle_hash: no crypto backend."
#endif

/* ── Leaf hash: H(0x00 || data) ─────────────────────────────────────────── */

rgtp_error_t rgtp_leaf_hash(const uint8_t* data, size_t len,
                             uint8_t out[RGTP_MERKLE_HASH_BYTES])
{
    if (data == NULL || out == NULL) {
        return RGTP_ERR_INVALID_ARG;
    }

    static const uint8_t PREFIX = 0x00u;

#if defined(RGTP_CRYPTO_BACKEND_LIBSODIUM)
    crypto_generichash_state state;
    crypto_generichash_init(&state, NULL, 0, RGTP_MERKLE_HASH_BYTES);
    crypto_generichash_update(&state, &PREFIX, 1);
    crypto_generichash_update(&state, data, len);
    crypto_generichash_final(&state, out, RGTP_MERKLE_HASH_BYTES);
    return RGTP_OK;

#elif defined(RGTP_CRYPTO_BACKEND_OPENSSL)
    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    if (ctx == NULL) return RGTP_ERR_CRYPTO_INIT;

    int ok = EVP_DigestInit_ex(ctx, EVP_sha256(), NULL)
          && EVP_DigestUpdate(ctx, &PREFIX, 1)
          && EVP_DigestUpdate(ctx, data, len);

    unsigned int out_len = 0;
    ok = ok && EVP_DigestFinal_ex(ctx, out, &out_len);
    EVP_MD_CTX_free(ctx);

    return (ok && out_len == RGTP_MERKLE_HASH_BYTES) ? RGTP_OK : RGTP_ERR_CRYPTO_INIT;
#endif
}

/* ── Internal node hash: H(0x01 || left || right) ───────────────────────── */

rgtp_error_t rgtp_internal_hash(const uint8_t left[RGTP_MERKLE_HASH_BYTES],
                                 const uint8_t right[RGTP_MERKLE_HASH_BYTES],
                                 uint8_t       out[RGTP_MERKLE_HASH_BYTES])
{
    if (left == NULL || right == NULL || out == NULL) {
        return RGTP_ERR_INVALID_ARG;
    }

    static const uint8_t PREFIX = 0x01u;

#if defined(RGTP_CRYPTO_BACKEND_LIBSODIUM)
    crypto_generichash_state state;
    crypto_generichash_init(&state, NULL, 0, RGTP_MERKLE_HASH_BYTES);
    crypto_generichash_update(&state, &PREFIX, 1);
    crypto_generichash_update(&state, left,  RGTP_MERKLE_HASH_BYTES);
    crypto_generichash_update(&state, right, RGTP_MERKLE_HASH_BYTES);
    crypto_generichash_final(&state, out, RGTP_MERKLE_HASH_BYTES);
    return RGTP_OK;

#elif defined(RGTP_CRYPTO_BACKEND_OPENSSL)
    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    if (ctx == NULL) return RGTP_ERR_CRYPTO_INIT;

    int ok = EVP_DigestInit_ex(ctx, EVP_sha256(), NULL)
          && EVP_DigestUpdate(ctx, &PREFIX, 1)
          && EVP_DigestUpdate(ctx, left,  RGTP_MERKLE_HASH_BYTES)
          && EVP_DigestUpdate(ctx, right, RGTP_MERKLE_HASH_BYTES);

    unsigned int out_len = 0;
    ok = ok && EVP_DigestFinal_ex(ctx, out, &out_len);
    EVP_MD_CTX_free(ctx);

    return (ok && out_len == RGTP_MERKLE_HASH_BYTES) ? RGTP_OK : RGTP_ERR_CRYPTO_INIT;
#endif
}
