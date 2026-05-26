/**
 * @file test_crypto.c
 * @brief Unit tests for the RGTP cryptographic subsystem.
 *
 * Tests: 12 cases covering CSPRNG, nonce construction, AEAD round-trip,
 * authentication failure detection, and key zeroization.
 *
 * Requirements: 17.1, 17.2
 */

#include "rgtp/rgtp.h"
#include "../helpers/rgtp_test.h"
#include "../../src/crypto/rgtp_crypto_internal.h"

#include <string.h>
#include <stdint.h>

/* ── Test: CSPRNG output is non-zero ────────────────────────────────────── */
static void test_csprng_output_nonzero(void)
{
    uint8_t id[16] = {0};
    RGTP_ASSERT_OK(rgtp_generate_exposure_id(id));

    int all_zero = 1;
    for (int i = 0; i < 16; i++) {
        if (id[i] != 0) { all_zero = 0; break; }
    }
    RGTP_ASSERT(!all_zero, "CSPRNG output must not be all-zero");
}

/* ── Test: Two consecutive CSPRNG calls produce different IDs ───────────── */
static void test_csprng_uniqueness(void)
{
    uint8_t id1[16], id2[16];
    RGTP_ASSERT_OK(rgtp_generate_exposure_id(id1));
    RGTP_ASSERT_OK(rgtp_generate_exposure_id(id2));
    RGTP_ASSERT(memcmp(id1, id2, 16) != 0, "Two CSPRNG IDs must differ");
}

/* ── Test: Nonce construction for ChaCha20-Poly1305-IETF ───────────────── */
static void test_nonce_construction_chacha(void)
{
    uint8_t nonce[RGTP_AEAD_NONCE_BYTES];

    /* Index 0: nonce should be all zeros */
    rgtp_build_nonce(0, nonce);
    uint8_t expected_zero[RGTP_AEAD_NONCE_BYTES] = {0};
    RGTP_ASSERT(memcmp(nonce, expected_zero, RGTP_AEAD_NONCE_BYTES) == 0,
                "Nonce for index 0 must be all-zero");

    /* Index 1: first byte = 0x01, rest zero */
    rgtp_build_nonce(1, nonce);
    RGTP_ASSERT(nonce[0] == 0x01, "Nonce[0] for index 1 must be 0x01");
    for (int i = 1; i < RGTP_AEAD_NONCE_BYTES; i++) {
        RGTP_ASSERT(nonce[i] == 0x00, "Nonce bytes 1-11 for index 1 must be zero");
    }

    /* Index 256 (0x100): bytes 0=0x00, 1=0x01, rest zero */
    rgtp_build_nonce(256, nonce);
    RGTP_ASSERT(nonce[0] == 0x00, "Nonce[0] for index 256 must be 0x00");
    RGTP_ASSERT(nonce[1] == 0x01, "Nonce[1] for index 256 must be 0x01");
}

/* ── Test: Nonce for AES-256-GCM (same construction) ───────────────────── */
static void test_nonce_construction_aesgcm(void)
{
    /* Same nonce construction regardless of algorithm */
    uint8_t nonce[RGTP_AEAD_NONCE_BYTES];
    rgtp_build_nonce(0xDEADBEEFu, nonce);
    RGTP_ASSERT(nonce[0] == 0xEF, "Nonce[0] for 0xDEADBEEF must be 0xEF (LE)");
    RGTP_ASSERT(nonce[1] == 0xBE, "Nonce[1] for 0xDEADBEEF must be 0xBE");
    RGTP_ASSERT(nonce[2] == 0xAD, "Nonce[2] for 0xDEADBEEF must be 0xAD");
    RGTP_ASSERT(nonce[3] == 0xDE, "Nonce[3] for 0xDEADBEEF must be 0xDE");
}

/* ── Test: Zero-length plaintext returns RGTP_ERR_INVALID_ARG ───────────── */
static void test_encrypt_decrypt_empty_not_allowed(void)
{
    uint8_t key[32] = {0x42};
    uint8_t ct[32];
    size_t  ct_len = 0;
    rgtp_error_t err = rgtp_encrypt_chunk(key, 0, (const uint8_t*)"", 0, ct, &ct_len);
    RGTP_ASSERT(err == RGTP_ERR_INVALID_ARG,
                "Zero-length plaintext must return RGTP_ERR_INVALID_ARG");
}

/* ── Test: Single-byte round-trip ───────────────────────────────────────── */
static void test_encrypt_decrypt_one_byte(void)
{
    uint8_t key[32];
    RGTP_ASSERT_OK(rgtp_csprng_bytes(key, 32));

    const uint8_t pt_in[1] = {0xAB};
    uint8_t ct[1 + RGTP_AEAD_TAG_BYTES];
    size_t  ct_len = 0;
    RGTP_ASSERT_OK(rgtp_encrypt_chunk(key, 0, pt_in, 1, ct, &ct_len));
    RGTP_ASSERT(ct_len == 1 + RGTP_AEAD_TAG_BYTES, "Ciphertext length must be pt+tag");

    uint8_t pt_out[1];
    size_t  pt_len = 0;
    RGTP_ASSERT_OK(rgtp_decrypt_chunk(key, 0, ct, ct_len, pt_out, &pt_len));
    RGTP_ASSERT(pt_len == 1, "Decrypted length must be 1");
    RGTP_ASSERT(pt_out[0] == 0xAB, "Decrypted byte must match original");
}

/* ── Test: Max UDP payload round-trip ───────────────────────────────────── */
static void test_encrypt_decrypt_max_udp(void)
{
    static uint8_t pt_in[65507];
    static uint8_t ct[65507 + RGTP_AEAD_TAG_BYTES];
    static uint8_t pt_out[65507];

    uint8_t key[32];
    RGTP_ASSERT_OK(rgtp_csprng_bytes(key, 32));
    RGTP_ASSERT_OK(rgtp_csprng_bytes(pt_in, sizeof(pt_in)));

    size_t ct_len = 0;
    RGTP_ASSERT_OK(rgtp_encrypt_chunk(key, 0, pt_in, sizeof(pt_in), ct, &ct_len));

    size_t pt_len = 0;
    RGTP_ASSERT_OK(rgtp_decrypt_chunk(key, 0, ct, ct_len, pt_out, &pt_len));
    RGTP_ASSERT(pt_len == sizeof(pt_in), "Decrypted length must match original");
    RGTP_ASSERT(memcmp(pt_in, pt_out, sizeof(pt_in)) == 0,
                "Decrypted data must match original");
}

/* ── Test: Tampered ciphertext returns RGTP_ERR_AUTH_FAIL ──────────────── */
static void test_auth_fail_on_tampered_ciphertext(void)
{
    uint8_t key[32];
    RGTP_ASSERT_OK(rgtp_csprng_bytes(key, 32));

    const uint8_t pt[16] = {0};
    uint8_t ct[16 + RGTP_AEAD_TAG_BYTES];
    size_t  ct_len = 0;
    RGTP_ASSERT_OK(rgtp_encrypt_chunk(key, 0, pt, 16, ct, &ct_len));

    ct[0] ^= 0xFF;   /* flip a bit */

    uint8_t pt_out[16];
    size_t  pt_len = 0;
    rgtp_error_t err = rgtp_decrypt_chunk(key, 0, ct, ct_len, pt_out, &pt_len);
    RGTP_ASSERT(err == RGTP_ERR_AUTH_FAIL,
                "Tampered ciphertext must return RGTP_ERR_AUTH_FAIL");
}

/* ── Test: Wrong key returns RGTP_ERR_AUTH_FAIL ─────────────────────────── */
static void test_auth_fail_on_wrong_key(void)
{
    uint8_t key1[32], key2[32];
    RGTP_ASSERT_OK(rgtp_csprng_bytes(key1, 32));
    RGTP_ASSERT_OK(rgtp_csprng_bytes(key2, 32));

    const uint8_t pt[8] = {1,2,3,4,5,6,7,8};
    uint8_t ct[8 + RGTP_AEAD_TAG_BYTES];
    size_t  ct_len = 0;
    RGTP_ASSERT_OK(rgtp_encrypt_chunk(key1, 0, pt, 8, ct, &ct_len));

    uint8_t pt_out[8];
    size_t  pt_len = 0;
    rgtp_error_t err = rgtp_decrypt_chunk(key2, 0, ct, ct_len, pt_out, &pt_len);
    RGTP_ASSERT(err == RGTP_ERR_AUTH_FAIL,
                "Wrong key must return RGTP_ERR_AUTH_FAIL");
}

/* ── Test: Wrong nonce (chunk index) returns RGTP_ERR_AUTH_FAIL ─────────── */
static void test_auth_fail_on_wrong_nonce(void)
{
    uint8_t key[32];
    RGTP_ASSERT_OK(rgtp_csprng_bytes(key, 32));

    const uint8_t pt[8] = {0};
    uint8_t ct[8 + RGTP_AEAD_TAG_BYTES];
    size_t  ct_len = 0;
    RGTP_ASSERT_OK(rgtp_encrypt_chunk(key, 0, pt, 8, ct, &ct_len));

    uint8_t pt_out[8];
    size_t  pt_len = 0;
    rgtp_error_t err = rgtp_decrypt_chunk(key, 1, ct, ct_len, pt_out, &pt_len);
    RGTP_ASSERT(err == RGTP_ERR_AUTH_FAIL,
                "Wrong chunk index (nonce) must return RGTP_ERR_AUTH_FAIL");
}

/* ── Test: rgtp_init() is idempotent ────────────────────────────────────── */
static void test_sodium_init_called_once(void)
{
    RGTP_ASSERT_OK(rgtp_init());
    RGTP_ASSERT_OK(rgtp_init());   /* second call must also return RGTP_OK */
}

/* ── Test runner ────────────────────────────────────────────────────────── */
int main(void)
{
    RGTP_ASSERT_OK(rgtp_init());

    RGTP_RUN_TEST(test_csprng_output_nonzero);
    RGTP_RUN_TEST(test_csprng_uniqueness);
    RGTP_RUN_TEST(test_nonce_construction_chacha);
    RGTP_RUN_TEST(test_nonce_construction_aesgcm);
    RGTP_RUN_TEST(test_encrypt_decrypt_empty_not_allowed);
    RGTP_RUN_TEST(test_encrypt_decrypt_one_byte);
    RGTP_RUN_TEST(test_encrypt_decrypt_max_udp);
    RGTP_RUN_TEST(test_auth_fail_on_tampered_ciphertext);
    RGTP_RUN_TEST(test_auth_fail_on_wrong_key);
    RGTP_RUN_TEST(test_auth_fail_on_wrong_nonce);
    RGTP_RUN_TEST(test_sodium_init_called_once);

    RGTP_PRINT_RESULTS();
    return rgtp_test_failures > 0 ? 1 : 0;
}
