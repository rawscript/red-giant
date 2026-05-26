/**
 * @file rgtp_crypto_internal.h
 * @brief Internal declarations for the RGTP cryptographic subsystem.
 *
 * Not part of the public API — do not install this header.
 */

#ifndef RGTP_CRYPTO_INTERNAL_H
#define RGTP_CRYPTO_INTERNAL_H

#include "rgtp/rgtp.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── AEAD tag and nonce sizes ───────────────────────────────────────────── */
#define RGTP_AEAD_TAG_BYTES   16u   /* Poly1305 / GCM authentication tag */
#define RGTP_AEAD_NONCE_BYTES 12u   /* ChaCha20-Poly1305-IETF / AES-256-GCM nonce */
#define RGTP_AEAD_KEY_BYTES   32u   /* 256-bit key */

/* ── Zeroization ────────────────────────────────────────────────────────── */

/**
 * @brief Securely zero @p len bytes at @p ptr.
 *
 * Uses sodium_memzero (libsodium) or OPENSSL_cleanse (OpenSSL).
 * A compile-time error fires if neither backend is available.
 */
void rgtp_zeroize(void* ptr, size_t len);

/* ── CSPRNG ─────────────────────────────────────────────────────────────── */

/**
 * @brief Fill @p buf with @p len cryptographically random bytes.
 * @return RGTP_OK or RGTP_ERR_CRYPTO_INIT on PRNG failure.
 */
rgtp_error_t rgtp_csprng_bytes(void* buf, size_t len);

/**
 * @brief Generate a 128-bit CSPRNG Exposure_ID.
 * @param out_id  16-byte output buffer.
 * @return RGTP_OK or RGTP_ERR_CRYPTO_INIT.
 */
rgtp_error_t rgtp_generate_exposure_id(uint8_t out_id[16]);

/* ── Nonce construction ─────────────────────────────────────────────────── */

/**
 * @brief Build the 12-byte AEAD nonce for chunk @p chunk_index.
 *
 * nonce[12] = { chunk_index_LE[0..3], 0x00 × 8 }
 *
 * Nonce uniqueness is guaranteed because:
 *  - Each Exposure uses a fresh random 256-bit key.
 *  - Chunk indices are unique within an Exposure.
 *  - The key is never reused across Exposures.
 */
void rgtp_build_nonce(uint32_t chunk_index, uint8_t nonce[RGTP_AEAD_NONCE_BYTES]);

/* ── AEAD encrypt / decrypt ─────────────────────────────────────────────── */

/**
 * @brief Encrypt a plaintext chunk with AEAD.
 *
 * @param key          32-byte AEAD key.
 * @param chunk_index  Chunk index (used to derive the nonce).
 * @param plaintext    Input plaintext buffer.
 * @param pt_len       Length of @p plaintext in bytes (1 – 65507).
 * @param ciphertext   Output buffer; must be at least pt_len + RGTP_AEAD_TAG_BYTES bytes.
 * @param ct_len_out   Receives the ciphertext length (pt_len + RGTP_AEAD_TAG_BYTES).
 * @return RGTP_OK or RGTP_ERR_ENCRYPT.
 */
rgtp_error_t rgtp_encrypt_chunk(const uint8_t  key[RGTP_AEAD_KEY_BYTES],
                                 uint32_t       chunk_index,
                                 const uint8_t* plaintext,
                                 size_t         pt_len,
                                 uint8_t*       ciphertext,
                                 size_t*        ct_len_out);

/**
 * @brief Decrypt and authenticate a ciphertext chunk.
 *
 * The authentication tag is verified BEFORE any plaintext is written.
 *
 * @param key          32-byte AEAD key.
 * @param chunk_index  Chunk index (used to derive the nonce).
 * @param ciphertext   Input ciphertext buffer (includes appended tag).
 * @param ct_len       Length of @p ciphertext in bytes.
 * @param plaintext    Output buffer; must be at least ct_len - RGTP_AEAD_TAG_BYTES bytes.
 * @param pt_len_out   Receives the plaintext length.
 * @return RGTP_OK, RGTP_ERR_AUTH_FAIL, or RGTP_ERR_DECRYPT.
 */
rgtp_error_t rgtp_decrypt_chunk(const uint8_t  key[RGTP_AEAD_KEY_BYTES],
                                 uint32_t       chunk_index,
                                 const uint8_t* ciphertext,
                                 size_t         ct_len,
                                 uint8_t*       plaintext,
                                 size_t*        pt_len_out);

#ifdef __cplusplus
}
#endif

#endif /* RGTP_CRYPTO_INTERNAL_H */
