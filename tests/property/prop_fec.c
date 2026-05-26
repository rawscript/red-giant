/**
 * @file prop_fec.c
 * @brief Property-based tests for the RGTP Reed-Solomon FEC subsystem.
 *
 * Property 2: FEC Encode/Erase/Decode Round-Trip
 *   For any valid (n, k) with 1 <= k < n <= 255, any k data symbols,
 *   and any erasure pattern of size <= n-k:
 *   encode → erase → decode must recover the original k data symbols.
 *
 * Property 8: Adaptive FEC Strength Bounds
 *   For any (overhead in [0.0,0.5], loss_rate in [0.0,1.0]):
 *   after applying the adaptation rule the result stays within [0.0,0.5]
 *   and satisfies the three-case spec exactly.
 *
 * Feature: rgtp-core-overhaul
 * Validates: Requirements 4.5, 4.9
 */

#include "rgtp/rgtp.h"
#include "../helpers/rgtp_test.h"
#include "../../src/fec/rgtp_fec_internal.h"
#include "../../src/transport/rgtp_transport_internal.h"

#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <math.h>

/* ── Deterministic PRNG ─────────────────────────────────────────────────── */

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

/* ── Property 2: FEC Encode/Erase/Decode Round-Trip ────────────────────── */
/* Feature: rgtp-core-overhaul, Property 2: FEC Encode/Erase/Decode Round-Trip */

#define PROP2_ITERATIONS 2000

static int prop2_fec_roundtrip(void)
{
    int failures = 0;

    static uint8_t data[255];
    static uint8_t parity[255];
    static uint8_t received[255];
    static uint8_t recovered[255];
    static uint8_t erasure_pos[255];

    for (int iter = 0; iter < PROP2_ITERATIONS; iter++) {
        /* Pick valid (n, k): 1 <= k < n <= 32 (cap for speed) */
        uint8_t n = (uint8_t)((prng_next() % 31) + 2);   /* 2..32 */
        uint8_t k = (uint8_t)((prng_next() % (n - 1)) + 1); /* 1..n-1 */
        uint8_t parity_count = n - k;

        /* Generate random k data symbols */
        prng_bytes(data, k);

        /* Encode */
        rgtp_error_t err = rgtp_rs_encode(k, n, data, parity);
        if (err != RGTP_OK) {
            fprintf(stderr, "  PROP2 FAIL iter=%d (n=%u,k=%u): encode: %s\n",
                    iter, n, k, rgtp_strerror(err));
            failures++;
            continue;
        }

        /* Build received array: data + parity */
        memcpy(received,     data,   k);
        memcpy(received + k, parity, parity_count);

        /* Choose a random erasure set of size <= parity_count */
        uint8_t erase_count = (uint8_t)(prng_next() % (parity_count + 1));

        /* Pick erase_count distinct positions in [0, n) */
        uint8_t used[255] = {0};
        uint8_t actual_erased = 0;
        for (uint8_t e = 0; e < erase_count; e++) {
            uint8_t pos;
            int attempts = 0;
            do {
                pos = (uint8_t)(prng_next() % n);
                attempts++;
            } while (used[pos] && attempts < 512);
            if (!used[pos]) {
                used[pos] = 1;
                erasure_pos[actual_erased++] = pos;
                received[pos] = 0;   /* erase */
            }
        }

        /* Decode */
        memset(recovered, 0, k);
        err = rgtp_rs_decode(k, n, received, erasure_pos, actual_erased, recovered);
        if (err != RGTP_OK) {
            fprintf(stderr,
                    "  PROP2 FAIL iter=%d (n=%u,k=%u,erased=%u): decode: %s\n",
                    iter, n, k, actual_erased, rgtp_strerror(err));
            failures++;
            continue;
        }

        /* Verify recovery */
        if (memcmp(data, recovered, k) != 0) {
            fprintf(stderr,
                    "  PROP2 FAIL iter=%d (n=%u,k=%u,erased=%u): "
                    "data mismatch after decode\n",
                    iter, n, k, actual_erased);
            failures++;
        }
    }
    return failures;
}

/* ── Property 8: Adaptive FEC Strength Bounds ───────────────────────────── */
/* Feature: rgtp-core-overhaul, Property 8: Adaptive FEC Strength Bounds */

#define PROP8_ITERATIONS 10000

static int prop8_adaptive_fec_bounds(void)
{
    int failures = 0;

    for (int iter = 0; iter < PROP8_ITERATIONS; iter++) {
        /* Random overhead in [0.0, 0.5] */
        float overhead = (float)(prng_next() % 10001) / 20000.0f; /* 0..0.5 */
        /* Random loss_rate in [0.0, 1.0] */
        float loss_rate = (float)(prng_next() % 10001) / 10000.0f;

        rgtp_flow_t f;
        rgtp_flow_init(&f, 64);
        f.fec_overhead = overhead;
        f.loss_rate    = loss_rate;

        float before = f.fec_overhead;
        rgtp_flow_update_fec(&f);
        float after = f.fec_overhead;

        /* Bound check: result must stay in [0.0, 0.5] */
        if (after < -0.0001f || after > 0.5001f) {
            fprintf(stderr,
                    "  PROP8 FAIL iter=%d: overhead=%.4f loss=%.4f → %.4f "
                    "(out of [0,0.5])\n",
                    iter, before, loss_rate, after);
            failures++;
            continue;
        }

        /* Three-case spec check */
        if (loss_rate > 0.05f) {
            /* Must increase by 0.10, capped at 0.50 */
            float expected = before + 0.10f;
            if (expected > 0.50f) expected = 0.50f;
            if (fabsf(after - expected) > 0.001f) {
                fprintf(stderr,
                        "  PROP8 FAIL iter=%d: loss>5%% but overhead "
                        "%.4f → %.4f (expected %.4f)\n",
                        iter, before, after, expected);
                failures++;
            }
        } else if (loss_rate < 0.01f) {
            /* Must decrease by 0.05, floored at 0.00 */
            float expected = before - 0.05f;
            if (expected < 0.00f) expected = 0.00f;
            if (fabsf(after - expected) > 0.001f) {
                fprintf(stderr,
                        "  PROP8 FAIL iter=%d: loss<1%% but overhead "
                        "%.4f → %.4f (expected %.4f)\n",
                        iter, before, after, expected);
                failures++;
            }
        } else {
            /* Must be unchanged */
            if (fabsf(after - before) > 0.001f) {
                fprintf(stderr,
                        "  PROP8 FAIL iter=%d: 1%%<=loss<=5%% but overhead "
                        "changed %.4f → %.4f\n",
                        iter, before, after);
                failures++;
            }
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

    printf("Running Property 2: FEC Encode/Erase/Decode Round-Trip "
           "(%d iterations)...\n", PROP2_ITERATIONS);
    int p2 = prop2_fec_roundtrip();
    if (p2 == 0) {
        printf("  PASS Property 2 (%d iterations)\n", PROP2_ITERATIONS);
        rgtp_test_passed++;
    } else {
        fprintf(stderr, "  FAIL Property 2: %d failures\n", p2);
        rgtp_test_failures += p2;
    }

    printf("Running Property 8: Adaptive FEC Strength Bounds "
           "(%d iterations)...\n", PROP8_ITERATIONS);
    int p8 = prop8_adaptive_fec_bounds();
    if (p8 == 0) {
        printf("  PASS Property 8 (%d iterations)\n", PROP8_ITERATIONS);
        rgtp_test_passed++;
    } else {
        fprintf(stderr, "  FAIL Property 8: %d failures\n", p8);
        rgtp_test_failures += p8;
    }

    RGTP_PRINT_RESULTS();
    return rgtp_test_failures > 0 ? 1 : 0;
}
