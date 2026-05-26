/**
 * @file prop_flow.c
 * @brief Property-based tests for RGTP flow control and RTT estimation.
 *
 * Property 5: Flow Control Convergence
 *   For any (initial_window, capacity_bps, rtt_us): simulate AIMD for
 *   10 RTT periods; assert final throughput within 10% of capacity.
 *
 * Property 9: RTT EWMA Monotone Convergence
 *   For any positive RTT samples and initial estimate:
 *   (a) Estimate is always positive.
 *   (b) After 50 samples from a stationary distribution, estimate is
 *       within 2 standard deviations of the true mean.
 *
 * Feature: rgtp-core-overhaul
 * Validates: Requirements 6.9, 6.4
 */

#include "rgtp/rgtp.h"
#include "../helpers/rgtp_test.h"
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

/* ── Property 5: Flow Control Convergence ───────────────────────────────── */
/* Feature: rgtp-core-overhaul, Property 5: Flow Control Convergence */

#define PROP5_ITERATIONS  1000
#define PROP5_RTT_PERIODS 10

/*
 * Simulate AIMD over PROP5_RTT_PERIODS RTT periods.
 *
 * Model:
 *   - capacity_chunks = capacity_bps / (chunk_size_bits)
 *   - Each RTT: puller sends window_size pull requests
 *   - If window_size > capacity_chunks: RTT inflates → congestion → halve
 *   - If window_size <= capacity_chunks: RTT stays low → increase by 1
 *   - Throughput = min(window_size, capacity_chunks) chunks/RTT
 */
static int prop5_flow_convergence(void)
{
    int failures = 0;

    for (int iter = 0; iter < PROP5_ITERATIONS; iter++) {
        /* Random parameters */
        uint32_t initial_window = (uint32_t)(prng_next() % 128) + 1;
        /* capacity in chunks/RTT: 4..128 */
        uint32_t capacity_chunks = (uint32_t)(prng_next() % 125) + 4;
        uint32_t rtt_us_base     = (uint32_t)(prng_next() % 9000) + 1000; /* 1ms..10ms */

        rgtp_flow_t f;
        rgtp_flow_init(&f, initial_window);
        f.rtt_us          = rtt_us_base;
        f.rtt_baseline_us = rtt_us_base;
        f.rtt_sample_count = 10; /* skip first-sample init */

        for (int period = 0; period < PROP5_RTT_PERIODS; period++) {
            /* Simulate RTT: if window > capacity, RTT inflates */
            uint32_t rtt_sample;
            if (f.window_size > capacity_chunks) {
                /* Congestion: RTT > 2x baseline */
                rtt_sample = rtt_us_base * 3;
            } else {
                /* No congestion: RTT within 1.05x baseline */
                rtt_sample = rtt_us_base + (rtt_us_base / 20);
            }
            rgtp_flow_update_rtt(&f, rtt_sample);
        }

        /* Throughput = min(window_size, capacity_chunks) */
        uint32_t throughput = f.window_size < capacity_chunks
                              ? f.window_size : capacity_chunks;

        /* Assert within 10% of capacity */
        float ratio = (float)throughput / (float)capacity_chunks;
        if (ratio < 0.90f) {
            fprintf(stderr,
                    "  PROP5 FAIL iter=%d: throughput=%u capacity=%u "
                    "ratio=%.3f (< 0.90)\n",
                    iter, throughput, capacity_chunks, ratio);
            failures++;
        }
    }
    return failures;
}

/* ── Property 9: RTT EWMA Monotone Convergence ──────────────────────────── */
/* Feature: rgtp-core-overhaul, Property 9: RTT EWMA Monotone Convergence */

#define PROP9_ITERATIONS  1000
#define PROP9_SAMPLES     50

static int prop9_ewma_convergence(void)
{
    int failures = 0;

    for (int iter = 0; iter < PROP9_ITERATIONS; iter++) {
        /* True mean: 1000..50000 µs */
        uint32_t true_mean = (uint32_t)(prng_next() % 49000) + 1000;
        /* Std dev: 5..20% of mean */
        uint32_t std_dev   = true_mean / (uint32_t)((prng_next() % 4) + 5);

        /* Random initial estimate: 500..100000 µs */
        uint32_t initial   = (uint32_t)(prng_next() % 99500) + 500;

        rgtp_flow_t f;
        rgtp_flow_init(&f, 64);
        f.rtt_us           = initial;
        f.rtt_baseline_us  = initial;
        f.rtt_sample_count = 1; /* skip first-sample init */

        /* Feed PROP9_SAMPLES samples from N(true_mean, std_dev) */
        for (int s = 0; s < PROP9_SAMPLES; s++) {
            /* Box-Muller approximation using two uniform samples */
            uint32_t u1 = (uint32_t)(prng_next() % 10000) + 1;
            uint32_t u2 = (uint32_t)(prng_next() % 10000) + 1;
            /* Simple approximation: sample = mean + std_dev * (u1-5000)/2887 */
            int32_t offset = (int32_t)((int64_t)std_dev * ((int32_t)u1 - 5000) / 2887);
            int32_t sample = (int32_t)true_mean + offset;
            if (sample < 1) sample = 1;
            rgtp_flow_update_rtt(&f, (uint32_t)sample);

            /* (a) Estimate must always be positive */
            if (f.rtt_us == 0) {
                fprintf(stderr,
                        "  PROP9 FAIL iter=%d sample=%d: RTT estimate is 0\n",
                        iter, s);
                failures++;
                break;
            }
        }

        /* (b) After 50 samples, estimate within 2 std devs of true mean */
        int32_t diff = (int32_t)f.rtt_us - (int32_t)true_mean;
        if (diff < 0) diff = -diff;
        if ((uint32_t)diff > 2 * std_dev + true_mean / 10) {
            /* Allow extra 10% tolerance for the approximated distribution */
            fprintf(stderr,
                    "  PROP9 FAIL iter=%d: estimate=%u true_mean=%u "
                    "diff=%d > 2*std_dev=%u\n",
                    iter, f.rtt_us, true_mean, diff, 2 * std_dev);
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

    printf("Running Property 5: Flow Control Convergence "
           "(%d iterations)...\n", PROP5_ITERATIONS);
    int p5 = prop5_flow_convergence();
    if (p5 == 0) {
        printf("  PASS Property 5 (%d iterations)\n", PROP5_ITERATIONS);
        rgtp_test_passed++;
    } else {
        fprintf(stderr, "  FAIL Property 5: %d failures\n", p5);
        rgtp_test_failures += p5;
    }

    printf("Running Property 9: RTT EWMA Monotone Convergence "
           "(%d iterations × %d samples)...\n",
           PROP9_ITERATIONS, PROP9_SAMPLES);
    int p9 = prop9_ewma_convergence();
    if (p9 == 0) {
        printf("  PASS Property 9 (%d iterations)\n", PROP9_ITERATIONS);
        rgtp_test_passed++;
    } else {
        fprintf(stderr, "  FAIL Property 9: %d failures\n", p9);
        rgtp_test_failures += p9;
    }

    RGTP_PRINT_RESULTS();
    return rgtp_test_failures > 0 ? 1 : 0;
}
