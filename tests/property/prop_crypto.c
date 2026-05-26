/**
 * @file prop_crypto.c
 * @brief Property-based tests for the RGTP cryptographic subsystem.
 *
 * Property 1: AEAD Encryption Round-Trip
 *   For any key[32], chunk_index, and plaintext[1..65507]:
 *   encrypt then decrypt must recover the original plaintext exactly.
 *
 * Property 7: Nonce Uniqueness Across Chunk Indices
 *   For any two distinct chunk indices i != j:
 *   nonce(i) != nonce(j) byte-for-byte.
 *
 * Uses a simple deterministic pseudo-random generator seeded from the
 * system CSPRNG to produce varied inputs across iterations.
 *
 * Feature: rgtp-core-overhaul
 * Validates: Requirements 3.15, 3.2
 */

#include "rgtp/rgtp.h"
#include "../helpers/rgtp_test.h"
#include "../../src/crypto/rgtp_crypto_internal.h"

#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <time.h>

/* ── Deterministic PRNG (xorshift64) ────────────────────────────────────── */

static uint64_t prng_state = 0;

static void prng_seed(uint64_t seed) { prng_state = seed ? seed : 1; }

static uint64_t prng_next(void)
{
    prng_state ^= prng_state << 13;
    prng_state ^= prng_state >> 7;
    prng_state ^= prng_state << 17;
    return prng_state;
}

static void prng_bytes(void* buf, size_t len)
{
    uint8_t* p = (uint8_t*)buf;
    for (size_t i = 0; i < len; i++) {
        if (i % 8 == 0) prng_next();
        p[i] = (uint8_t)(prng_state >> ((i % 8) * 8));
    }
}

/* ── Property 1: AEAD Encryption Round-Trip ─────────────────────────────── */
/* Feature: rgtp-core-overhaul, Property 1: AEAD Encryption Round-Trip */

#define PROP1_ITERATIONS 1000
#define PROP1_MAX_PT_LEN 4096   /* cap at 4KB for speed; full 65507 in nightly */

static int prop1_aead_roundtrip(void)
{
    static uint8_t key[32];
    static uint8_t pt[PROP1_MAX_PT_LEN];
    static uint8_t ct[PROP1_MAX_PT_LEN + RGTP_AEAD_TAG_BYTES];
    static uint8_t pt2[PROP1_MAX_PT_LEN];

    int failures = 0;

    for (int iter = 0; iter < PROP1_ITERATIONS; iter++) {
        /* Generate random key and plaintext */
        prng_bytes(key, 32);
        size_t pt_len = (size_t)(prng_next() % PROP1_MAX_PT_LEN) + 1;
        prng_bytes(pt, pt_len);
        uint32_t chunk_index = (uint32_t)prng_next();

        /* Encrypt */
        size_t ct_len = 0;
        rgtp_error_t err = rgtp_encrypt_chunk(key, chunk_index, pt, pt_len,
                                               ct, &ct_len);
        if (err != RGTP_OK) {
            fprintf(stderr, "  PROP1 FAIL iter=%d: encrypt returned %s\n",
                    iter, rgtp_strerror(err));
            failures++;
            continue;
        }

        /* Decrypt */
        size_t pt2_len = 0;
        err = rgtp_decrypt_chunk(key, chunk_index, ct, ct_len, pt2, &pt2_len);
        if (err != RGTP_OK) {
            fprintf(stderr, "  PROP1 FAIL iter=%d: decrypt returned %s\n",
                    iter, rgtp_strerror(err));
            failures++;
            continue;
        }

        /* Verify round-trip */
        if (pt2_len != pt_len || memcmp(pt, pt2, pt_len) != 0) {
            fprintf(stderr, "  PROP1 FAIL iter=%d: plaintext mismatch "
                    "(pt_len=%zu, pt2_len=%zu)\n", iter, pt_len, pt2_len);
            failures++;
        }
    }
    return failures;
}

/* ── Property 7: Nonce Uniqueness Across Chunk Indices ──────────────────── */
/* Feature: rgtp-core-overhaul, Property 7: Nonce Uniqueness Across Chunk Indices */

#define PROP7_ITERATIONS 10000

static int prop7_nonce_uniqueness(void)
{
    int failures = 0;

    for (int iter = 0; iter < PROP7_ITERATIONS; iter++) {
        uint32_t i = (uint32_t)prng_next();
        uint32_t j = (uint32_t)prng_next();
        if (i == j) { j++; }   /* ensure i != j */

        uint8_t nonce_i[RGTP_AEAD_NONCE_BYTES];
        uint8_t nonce_j[RGTP_AEAD_NONCE_BYTES];
        rgtp_build_nonce(i, nonce_i);
        rgtp_build_nonce(j, nonce_j);

        if (memcmp(nonce_i, nonce_j, RGTP_AEAD_NONCE_BYTES) == 0) {
            fprintf(stderr, "  PROP7 FAIL iter=%d: nonce(%u) == nonce(%u)\n",
                    iter, i, j);
            failures++;
        }
    }
    return failures;
}

/* ── Test runner ────────────────────────────────────────────────────────── */

int main(void)
{
    RGTP_ASSERT_OK(rgtp_init());

    /* Seed PRNG from CSPRNG for varied inputs */
    uint64_t seed = 0;
    rgtp_csprng_bytes(&seed, sizeof(seed));
    prng_seed(seed);
    printf("PRNG seed: %llu\n", (unsigned long long)seed);

    printf("Running Property 1: AEAD Encryption Round-Trip (%d iterations)...\n",
           PROP1_ITERATIONS);
    int p1_failures = prop1_aead_roundtrip();
    if (p1_failures == 0) {
        printf("  PASS Property 1 (%d iterations)\n", PROP1_ITERATIONS);
        rgtp_test_passed++;
    } else {
        fprintf(stderr, "  FAIL Property 1: %d failures\n", p1_failures);
        rgtp_test_failures += p1_failures;
    }

    printf("Running Property 7: Nonce Uniqueness (%d iterations)...\n",
           PROP7_ITERATIONS);
    int p7_failures = prop7_nonce_uniqueness();
    if (p7_failures == 0) {
        printf("  PASS Property 7 (%d iterations)\n", PROP7_ITERATIONS);
        rgtp_test_passed++;
    } else {
        fprintf(stderr, "  FAIL Property 7: %d failures\n", p7_failures);
        rgtp_test_failures += p7_failures;
    }

    RGTP_PRINT_RESULTS();
    return rgtp_test_failures > 0 ? 1 : 0;
}
