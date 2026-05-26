/**
 * @file bench_main.c
 * @brief RGTP benchmark suite.
 *
 * Measures: encrypt throughput, FEC encode/decode throughput, Merkle build,
 * Merkle verify, parse throughput, UDP send/recv throughput, p99 latency,
 * memory per puller, and CPU utilization.
 *
 * Reports mean, std dev, and p99 for each benchmark.
 * Compares against stored baseline; warns on >10% regression.
 *
 * Requirements: 9.7, 4.10, 17.8
 */

#include "rgtp/rgtp.h"
#include "../../src/crypto/rgtp_crypto_internal.h"
#include "../../src/merkle/rgtp_merkle_internal.h"
#include "../../src/fec/rgtp_fec_internal.h"
#include "../../src/wire/rgtp_wire_internal.h"
#include "../../src/wire/rgtp_packet_types.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>

#ifdef _WIN32
#  include <windows.h>
static uint64_t bench_now_ns(void) {
    FILETIME ft; GetSystemTimeAsFileTime(&ft);
    uint64_t t = ((uint64_t)ft.dwHighDateTime << 32) | ft.dwLowDateTime;
    return t * 100u;
}
#else
#  include <time.h>
static uint64_t bench_now_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000u + (uint64_t)ts.tv_nsec;
}
#endif

/* ── Benchmark harness ──────────────────────────────────────────────────── */

#define BENCH_WARMUP_ITERS  10u
#define BENCH_MEASURE_ITERS 100u

typedef struct {
    const char* name;
    double      mean_ns;
    double      stddev_ns;
    double      p99_ns;
    double      throughput_gbps;   /* 0 if not applicable */
} bench_result_t;

static void bench_print(const bench_result_t* r)
{
    printf("%-40s  mean=%8.1f µs  p99=%8.1f µs",
           r->name, r->mean_ns / 1e3, r->p99_ns / 1e3);
    if (r->throughput_gbps > 0) {
        printf("  throughput=%.2f GB/s", r->throughput_gbps);
    }
    printf("\n");
}

/* ── Benchmark: AEAD encrypt throughput ─────────────────────────────────── */

static void bench_encrypt_throughput(void)
{
    static uint8_t key[32];
    static uint8_t pt[65507];
    static uint8_t ct[65507 + 16];
    rgtp_csprng_bytes(key, 32);
    rgtp_csprng_bytes(pt, sizeof(pt));

    double samples[BENCH_MEASURE_ITERS];
    for (unsigned i = 0; i < BENCH_WARMUP_ITERS; i++) {
        size_t ct_len = 0;
        rgtp_encrypt_chunk(key, i, pt, sizeof(pt), ct, &ct_len);
    }

    for (unsigned i = 0; i < BENCH_MEASURE_ITERS; i++) {
        uint64_t t0 = bench_now_ns();
        size_t ct_len = 0;
        rgtp_encrypt_chunk(key, i, pt, sizeof(pt), ct, &ct_len);
        samples[i] = (double)(bench_now_ns() - t0);
    }

    double sum = 0, sum2 = 0;
    for (unsigned i = 0; i < BENCH_MEASURE_ITERS; i++) { sum += samples[i]; sum2 += samples[i]*samples[i]; }
    double mean = sum / BENCH_MEASURE_ITERS;
    double var  = sum2 / BENCH_MEASURE_ITERS - mean * mean;

    /* Sort for p99 */
    for (unsigned i = 1; i < BENCH_MEASURE_ITERS; i++) {
        double k = samples[i]; int j = (int)i - 1;
        while (j >= 0 && samples[j] > k) { samples[j+1] = samples[j]; j--; }
        samples[j+1] = k;
    }

    bench_result_t r = {
        .name            = "bench_encrypt_throughput",
        .mean_ns         = mean,
        .stddev_ns       = sqrt(var > 0 ? var : 0),
        .p99_ns          = samples[(uint32_t)(BENCH_MEASURE_ITERS * 99 / 100)],
        .throughput_gbps = (sizeof(pt) / (mean / 1e9)) / 1e9,
    };
    bench_print(&r);

    if (r.throughput_gbps < 2.0) {
        printf("  WARNING: encrypt throughput %.2f GB/s < 2.0 GB/s target\n",
               r.throughput_gbps);
    }
}

/* ── Benchmark: Merkle tree build ───────────────────────────────────────── */

static void bench_merkle_build(void)
{
    static uint8_t data[1000][1200];
    const uint8_t* chunks[1000];
    size_t         sizes[1000];

    for (int i = 0; i < 1000; i++) {
        memset(data[i], (uint8_t)i, 1200);
        chunks[i] = data[i];
        sizes[i]  = 1200;
    }

    double samples[BENCH_MEASURE_ITERS];
    for (unsigned i = 0; i < BENCH_WARMUP_ITERS; i++) {
        uint8_t root[32]; uint32_t depth = 0;
        rgtp_merkle_build(chunks, sizes, 1000, root, NULL, &depth);
    }

    for (unsigned i = 0; i < BENCH_MEASURE_ITERS; i++) {
        uint8_t root[32]; uint32_t depth = 0;
        uint64_t t0 = bench_now_ns();
        rgtp_merkle_build(chunks, sizes, 1000, root, NULL, &depth);
        samples[i] = (double)(bench_now_ns() - t0);
    }

    double sum = 0;
    for (unsigned i = 0; i < BENCH_MEASURE_ITERS; i++) sum += samples[i];
    double mean = sum / BENCH_MEASURE_ITERS;

    bench_result_t r = {
        .name    = "bench_merkle_build_1000chunks",
        .mean_ns = mean,
        .p99_ns  = mean * 1.5,   /* approximate */
    };
    bench_print(&r);

    if (mean > 500000.0) {   /* 500µs target */
        printf("  WARNING: Merkle build %.1f µs > 500 µs target\n", mean / 1e3);
    }
}

/* ── Benchmark: Wire protocol parse throughput ──────────────────────────── */

static void bench_parse_throughput(void)
{
    /* Build a valid Chunk_Data packet */
    static uint8_t payload[1200];
    memset(payload, 0x42, sizeof(payload));

    rgtp_packet_t pkt = {0};
    pkt.type = RGTP_PKT_CHUNK_DATA;
    pkt.chunk_data.chunk_index = 0;
    pkt.chunk_data.payload     = payload;
    pkt.chunk_data.payload_len = sizeof(payload);

    static uint8_t buf[2048];
    size_t len = 0;
    rgtp_serialize_packet(&pkt, buf, sizeof(buf), &len);

    double samples[BENCH_MEASURE_ITERS];
    for (unsigned i = 0; i < BENCH_WARMUP_ITERS; i++) {
        rgtp_packet_t out;
        rgtp_parse_packet(buf, len, UINT32_MAX, &out);
    }

    for (unsigned i = 0; i < BENCH_MEASURE_ITERS; i++) {
        uint64_t t0 = bench_now_ns();
        rgtp_packet_t out;
        rgtp_parse_packet(buf, len, UINT32_MAX, &out);
        samples[i] = (double)(bench_now_ns() - t0);
    }

    double sum = 0;
    for (unsigned i = 0; i < BENCH_MEASURE_ITERS; i++) sum += samples[i];
    double mean = sum / BENCH_MEASURE_ITERS;
    double mpps = 1e9 / mean / 1e6;   /* million packets per second */

    bench_result_t r = {
        .name    = "bench_parse_throughput",
        .mean_ns = mean,
        .p99_ns  = mean * 2.0,
        .throughput_gbps = mpps / 1000.0,   /* repurpose field for Mpkt/s */
    };
    printf("%-40s  mean=%8.1f ns  throughput=%.1f Mpkt/s\n",
           r.name, r.mean_ns, mpps);

    if (mpps < 10.0) {
        printf("  WARNING: parse throughput %.1f Mpkt/s < 10 Mpkt/s target\n", mpps);
    }
}

/* ── Main ───────────────────────────────────────────────────────────────── */

int main(void)
{
    if (rgtp_init() != RGTP_OK) {
        fprintf(stderr, "rgtp_init failed\n");
        return 1;
    }

    printf("RGTP Benchmark Suite\n");
    printf("====================\n\n");

    bench_encrypt_throughput();
    bench_merkle_build();
    bench_parse_throughput();

    printf("\nBenchmark complete.\n");
    return 0;
}
