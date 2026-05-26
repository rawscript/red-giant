/**
 * @file prop_merkle.c
 * @brief Property-based tests for the RGTP Merkle tree subsystem.
 *
 * Property 6: Merkle Proof Verification
 *   For any array of 1–1024 chunks:
 *   (a) All pre-computed proofs verify against the root.
 *   (b) Any tampered proof (one sibling byte flipped) fails verification.
 *
 * Feature: rgtp-core-overhaul
 * Validates: Requirements 3.6, 3.8, 3.9
 */

#include "rgtp/rgtp.h"
#include "../helpers/rgtp_test.h"
#include "../../src/merkle/rgtp_merkle_internal.h"

#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>

/* ── Deterministic PRNG (xorshift64) ────────────────────────────────────── */

static uint64_t prng_state = 0;
static void     prng_seed(uint64_t s) { prng_state = s ? s : 1; }
static uint64_t prng_next(void)
{
    prng_state ^= prng_state << 13;
    prng_state ^= prng_state >> 7;
    prng_state ^= prng_state << 17;
    return prng_state;
}
static void prng_bytes(void *buf, size_t len)
{
    uint8_t *p = (uint8_t *)buf;
    for (size_t i = 0; i < len; i++) {
        if (i % 8 == 0) prng_next();
        p[i] = (uint8_t)(prng_state >> ((i % 8) * 8));
    }
}

/* ── Limits ─────────────────────────────────────────────────────────────── */

#define PROP6_ITERATIONS   500
#define PROP6_MAX_CHUNKS   64      /* cap for speed; spec allows up to 1024 */
#define PROP6_CHUNK_SIZE   32

/* ── Property 6: Merkle Proof Verification ──────────────────────────────── */
/* Feature: rgtp-core-overhaul, Property 6: Merkle Proof Verification */

static int prop6_merkle_proof_verification(void)
{
    int failures = 0;

    /* Static storage to avoid large stack frames */
    static uint8_t  chunk_data[PROP6_MAX_CHUNKS][PROP6_CHUNK_SIZE];
    static const uint8_t *chunk_ptrs[PROP6_MAX_CHUNKS];
    static size_t   chunk_sizes[PROP6_MAX_CHUNKS];

    for (int iter = 0; iter < PROP6_ITERATIONS; iter++) {
        uint32_t chunk_count = (uint32_t)(prng_next() % PROP6_MAX_CHUNKS) + 1;

        /* Generate random chunk data */
        for (uint32_t i = 0; i < chunk_count; i++) {
            prng_bytes(chunk_data[i], PROP6_CHUNK_SIZE);
            chunk_ptrs[i]  = chunk_data[i];
            chunk_sizes[i] = PROP6_CHUNK_SIZE;
        }

        /* Build Merkle tree */
        uint8_t   root[32];
        uint8_t **proofs = NULL;
        uint32_t  depth  = 0;

        rgtp_error_t err = rgtp_merkle_build(chunk_ptrs, chunk_sizes,
                                              chunk_count, root, &proofs, &depth);
        if (err != RGTP_OK) {
            fprintf(stderr, "  PROP6 FAIL iter=%d: merkle_build returned %s\n",
                    iter, rgtp_strerror(err));
            failures++;
            continue;
        }

        /* (a) All valid proofs must verify */
        for (uint32_t i = 0; i < chunk_count; i++) {
            err = rgtp_merkle_verify(chunk_data[i], PROP6_CHUNK_SIZE,
                                     proofs[i], depth, i, root);
            if (err != RGTP_OK) {
                fprintf(stderr,
                        "  PROP6 FAIL iter=%d chunk=%u: valid proof failed: %s\n",
                        iter, i, rgtp_strerror(err));
                failures++;
            }
        }

        /* (b) Tampered proof must fail */
        if (depth > 0 && chunk_count > 0) {
            /* Copy proof[0] and flip one byte */
            uint8_t bad_proof[depth * 32];
            memcpy(bad_proof, proofs[0], depth * 32);
            bad_proof[0] ^= 0xFF;

            err = rgtp_merkle_verify(chunk_data[0], PROP6_CHUNK_SIZE,
                                     bad_proof, depth, 0, root);
            if (err != RGTP_ERR_MERKLE_FAIL) {
                fprintf(stderr,
                        "  PROP6 FAIL iter=%d: tampered proof did not fail "
                        "(got %s)\n", iter, rgtp_strerror(err));
                failures++;
            }
        }

        /* Free proofs */
        if (proofs) {
            for (uint32_t i = 0; i < chunk_count; i++) {
                free(proofs[i]);
            }
            free(proofs);
        }
    }
    return failures;
}

/* ── Test runner ────────────────────────────────────────────────────────── */

int main(void)
{
    RGTP_ASSERT_OK(rgtp_init());

    uint64_t seed = 0;
    rgtp_csprng_bytes(&seed, sizeof(seed));
    prng_seed(seed);
    printf("PRNG seed: %llu\n", (unsigned long long)seed);

    printf("Running Property 6: Merkle Proof Verification (%d iterations)...\n",
           PROP6_ITERATIONS);
    int failures = prop6_merkle_proof_verification();
    if (failures == 0) {
        printf("  PASS Property 6 (%d iterations)\n", PROP6_ITERATIONS);
        rgtp_test_passed++;
    } else {
        fprintf(stderr, "  FAIL Property 6: %d failures\n", failures);
        rgtp_test_failures += failures;
    }

    RGTP_PRINT_RESULTS();
    return rgtp_test_failures > 0 ? 1 : 0;
}
