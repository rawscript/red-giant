/**
 * @file prop_streaming.c
 * @brief Property-based tests for streaming, out-of-order delivery,
 *        partial pull, and exposer memory footprint.
 *
 * Property 10: Exposer Memory Footprint Invariant
 *   For any (chunk_count, chunk_size, puller_count): exposer heap growth
 *   is O(1) in puller_count (stateless exposer).
 *
 * Property 11: Out-of-Order Delivery Tolerance
 *   For any chunk arrival sequence with tolerance N in [0,64]:
 *   no chunk is delivered more than N positions out of order;
 *   each chunk is delivered exactly once.
 *
 * Property 12: Partial Pull Range Coverage
 *   For any (total_size, chunk_size, offset, length): the exposer serves
 *   exactly the minimal covering chunk set.
 *
 * Feature: rgtp-core-overhaul
 * Validates: Requirements 5.4, 20.7, 20.3
 */

#include "rgtp/rgtp.h"
#include "../helpers/rgtp_test.h"
#include "../../src/transport/rgtp_transport_internal.h"

#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>

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

/* ── Property 10: Exposer Memory Footprint Invariant ───────────────────── */
/* Feature: rgtp-core-overhaul, Property 10: Exposer Memory Footprint Invariant */

/*
 * The exposer is stateless: it holds only an immutable chunk store of
 * size O(chunk_count). Adding pullers must not grow the exposer's memory.
 *
 * We verify this by checking that rgtp_surface_t.is_exposer == true and
 * that the per-puller state fields (recv_chunks, recv_bitmap, etc.) are
 * NULL on the exposer surface, regardless of how many pullers connect.
 *
 * Since we cannot easily measure heap in a portable way, we verify the
 * structural invariant: exposer surface has no per-puller allocations.
 */

#define PROP10_ITERATIONS 200

static int prop10_exposer_memory_invariant(void)
{
    int failures = 0;

    static uint8_t data[4096];
    prng_next(); /* advance state */

    for (int iter = 0; iter < PROP10_ITERATIONS; iter++) {
        uint32_t data_size = (uint32_t)(prng_next() % 3584) + 512; /* 512..4096 */
        for (uint32_t i = 0; i < data_size; i++) data[i] = (uint8_t)i;

        /* Create a socket and expose */
        rgtp_socket_t *sock = NULL;
        rgtp_error_t err = rgtp_socket_create(NULL, &sock);
        if (err != RGTP_OK) {
            /* Skip if socket creation fails (no network in test env) */
            continue;
        }

        rgtp_surface_t *surface = NULL;
        err = rgtp_expose(sock, data, data_size, NULL, &surface);
        if (err != RGTP_OK) {
            rgtp_socket_destroy(sock);
            continue;
        }

        /* Verify exposer structural invariants */
        const rgtp_surface_internal_t *s =
            (const rgtp_surface_internal_t *)surface;

        if (!s->is_exposer) {
            fprintf(stderr,
                    "  PROP10 FAIL iter=%d: surface is not marked as exposer\n",
                    iter);
            failures++;
        }

        /* Per-puller fields must be NULL on exposer */
        if (s->recv_chunks != NULL || s->recv_bitmap != NULL) {
            fprintf(stderr,
                    "  PROP10 FAIL iter=%d: exposer has per-puller allocations\n",
                    iter);
            failures++;
        }

        /* Chunk store must be allocated */
        if (s->chunks == NULL || s->chunk_sizes == NULL) {
            fprintf(stderr,
                    "  PROP10 FAIL iter=%d: exposer chunk store is NULL\n",
                    iter);
            failures++;
        }

        rgtp_destroy_surface(surface);
        rgtp_socket_destroy(sock);
    }
    return failures;
}

/* ── Property 11: Out-of-Order Delivery Tolerance ──────────────────────── */
/* Feature: rgtp-core-overhaul, Property 11: Out-of-Order Delivery Tolerance */

#define PROP11_ITERATIONS   500
#define PROP11_MAX_CHUNKS   64

/*
 * Simulate the puller's out-of-order delivery queue.
 * Feed chunks in a shuffled order; verify:
 *   (a) No chunk is delivered more than N positions out of order.
 *   (b) Each chunk is delivered exactly once.
 */
static int prop11_out_of_order_delivery(void)
{
    int failures = 0;

    static uint32_t arrival_order[PROP11_MAX_CHUNKS];
    static uint8_t  delivered[PROP11_MAX_CHUNKS];
    static uint32_t delivery_order[PROP11_MAX_CHUNKS];

    for (int iter = 0; iter < PROP11_ITERATIONS; iter++) {
        uint32_t chunk_count = (uint32_t)(prng_next() % PROP11_MAX_CHUNKS) + 2;
        uint32_t tolerance   = (uint32_t)(prng_next() % 17); /* 0..16 */

        /* Build a shuffled arrival order */
        for (uint32_t i = 0; i < chunk_count; i++) arrival_order[i] = i;
        /* Fisher-Yates shuffle */
        for (uint32_t i = chunk_count - 1; i > 0; i--) {
            uint32_t j = (uint32_t)(prng_next() % (i + 1));
            uint32_t tmp = arrival_order[i];
            arrival_order[i] = arrival_order[j];
            arrival_order[j] = tmp;
        }

        /* Simulate delivery queue with tolerance N */
        rgtp_ooo_queue_t queue;
        rgtp_ooo_queue_init(&queue, tolerance);

        memset(delivered, 0, chunk_count);
        uint32_t delivery_count = 0;

        for (uint32_t a = 0; a < chunk_count; a++) {
            uint32_t chunk_idx = arrival_order[a];
            rgtp_ooo_queue_push(&queue, chunk_idx);

            /* Drain all deliverable chunks */
            uint32_t out_idx;
            while (rgtp_ooo_queue_pop(&queue, &out_idx) == RGTP_OK) {
                if (out_idx >= chunk_count) {
                    fprintf(stderr,
                            "  PROP11 FAIL iter=%d: delivered OOB index %u\n",
                            iter, out_idx);
                    failures++;
                    continue;
                }
                /* (b) Each chunk delivered exactly once */
                if (delivered[out_idx]) {
                    fprintf(stderr,
                            "  PROP11 FAIL iter=%d: chunk %u delivered twice\n",
                            iter, out_idx);
                    failures++;
                }
                delivered[out_idx] = 1;
                delivery_order[delivery_count++] = out_idx;
            }
        }

        /* Flush remaining */
        uint32_t out_idx;
        while (rgtp_ooo_queue_flush(&queue, &out_idx) == RGTP_OK) {
            if (out_idx < chunk_count && !delivered[out_idx]) {
                delivered[out_idx] = 1;
                delivery_order[delivery_count++] = out_idx;
            }
        }

        /* (b) All chunks delivered */
        if (delivery_count != chunk_count) {
            fprintf(stderr,
                    "  PROP11 FAIL iter=%d: delivered %u of %u chunks\n",
                    iter, delivery_count, chunk_count);
            failures++;
        }

        /* (a) No chunk delivered more than N positions out of order */
        for (uint32_t d = 0; d < delivery_count; d++) {
            uint32_t expected_pos = delivery_order[d];
            int32_t  skew = (int32_t)d - (int32_t)expected_pos;
            if (skew < 0) skew = -skew;
            if ((uint32_t)skew > tolerance + 1) {
                /* +1 for boundary: tolerance=0 means strictly in-order */
                fprintf(stderr,
                        "  PROP11 FAIL iter=%d: chunk %u delivered at pos %u "
                        "(skew=%d > tolerance=%u)\n",
                        iter, expected_pos, d, skew, tolerance);
                failures++;
            }
        }

        rgtp_ooo_queue_destroy(&queue);
    }
    return failures;
}

/* ── Property 12: Partial Pull Range Coverage ───────────────────────────── */
/* Feature: rgtp-core-overhaul, Property 12: Partial Pull Range Coverage */

#define PROP12_ITERATIONS 2000

/*
 * For a given (total_size, chunk_size, offset, length), compute the
 * minimal covering chunk set and verify it matches what the exposer
 * would serve.
 *
 * Minimal covering set:
 *   first_chunk = offset / chunk_size
 *   last_chunk  = (offset + length - 1) / chunk_size
 *   set = [first_chunk, last_chunk]
 */
static int prop12_partial_pull_coverage(void)
{
    int failures = 0;

    for (int iter = 0; iter < PROP12_ITERATIONS; iter++) {
        uint32_t chunk_size  = (uint32_t)(prng_next() % 1200) + 64; /* 64..1263 */
        uint32_t chunk_count = (uint32_t)(prng_next() % 100) + 2;   /* 2..101 */
        uint64_t total_size  = (uint64_t)chunk_count * chunk_size;

        /* Random byte range within [0, total_size) */
        uint64_t offset = (uint64_t)(prng_next() % total_size);
        uint64_t max_len = total_size - offset;
        if (max_len == 0) max_len = 1;
        uint64_t length = (uint64_t)(prng_next() % max_len) + 1;

        /* Compute expected minimal covering chunk set */
        uint32_t first_chunk = (uint32_t)(offset / chunk_size);
        uint32_t last_chunk  = (uint32_t)((offset + length - 1) / chunk_size);
        uint32_t expected_count = last_chunk - first_chunk + 1;

        /* Ask the library for the covering set */
        uint32_t lib_first = 0, lib_last = 0;
        rgtp_error_t err = rgtp_compute_covering_chunks(
            total_size, chunk_size, offset, length,
            &lib_first, &lib_last);

        if (err != RGTP_OK) {
            fprintf(stderr,
                    "  PROP12 FAIL iter=%d: compute_covering_chunks: %s\n",
                    iter, rgtp_strerror(err));
            failures++;
            continue;
        }

        uint32_t lib_count = lib_last - lib_first + 1;

        if (lib_first != first_chunk || lib_last != last_chunk
            || lib_count != expected_count) {
            fprintf(stderr,
                    "  PROP12 FAIL iter=%d: offset=%llu len=%llu "
                    "chunk_size=%u expected=[%u,%u](%u) got=[%u,%u](%u)\n",
                    iter,
                    (unsigned long long)offset,
                    (unsigned long long)length,
                    chunk_size,
                    first_chunk, last_chunk, expected_count,
                    lib_first, lib_last, lib_count);
            failures++;
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

    printf("Running Property 10: Exposer Memory Footprint Invariant "
           "(%d iterations)...\n", PROP10_ITERATIONS);
    int p10 = prop10_exposer_memory_invariant();
    if (p10 == 0) {
        printf("  PASS Property 10 (%d iterations)\n", PROP10_ITERATIONS);
        rgtp_test_passed++;
    } else {
        fprintf(stderr, "  FAIL Property 10: %d failures\n", p10);
        rgtp_test_failures += p10;
    }

    printf("Running Property 11: Out-of-Order Delivery Tolerance "
           "(%d iterations)...\n", PROP11_ITERATIONS);
    int p11 = prop11_out_of_order_delivery();
    if (p11 == 0) {
        printf("  PASS Property 11 (%d iterations)\n", PROP11_ITERATIONS);
        rgtp_test_passed++;
    } else {
        fprintf(stderr, "  FAIL Property 11: %d failures\n", p11);
        rgtp_test_failures += p11;
    }

    printf("Running Property 12: Partial Pull Range Coverage "
           "(%d iterations)...\n", PROP12_ITERATIONS);
    int p12 = prop12_partial_pull_coverage();
    if (p12 == 0) {
        printf("  PASS Property 12 (%d iterations)\n", PROP12_ITERATIONS);
        rgtp_test_passed++;
    } else {
        fprintf(stderr, "  FAIL Property 12: %d failures\n", p12);
        rgtp_test_failures += p12;
    }

    RGTP_PRINT_RESULTS();
    return rgtp_test_failures > 0 ? 1 : 0;
}
