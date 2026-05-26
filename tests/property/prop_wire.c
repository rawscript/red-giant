/**
 * @file prop_wire.c
 * @brief Property-based tests for the RGTP wire protocol.
 *
 * Property 3: Wire Protocol Parse/Serialize Round-Trip
 *   For any valid rgtp_packet_t of any of the 8 packet types:
 *   serialize → parse must produce a field-for-field equivalent packet.
 *
 * Feature: rgtp-core-overhaul
 * Validates: Requirements 7.5
 */

#include "rgtp/rgtp.h"
#include "../helpers/rgtp_test.h"
#include "../../src/wire/rgtp_wire_internal.h"
#include "../../src/wire/rgtp_packet_types.h"

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
static void prng_bytes(void *buf, size_t len)
{
    uint8_t *p = (uint8_t *)buf;
    for (size_t i = 0; i < len; i++) {
        if (i % 8 == 0) prng_next();
        p[i] = (uint8_t)(prng_state >> ((i % 8) * 8));
    }
}

/* ── Packet generators ──────────────────────────────────────────────────── */

static void gen_pull_request(rgtp_packet_t *pkt)
{
    memset(pkt, 0, sizeof(*pkt));
    pkt->type = RGTP_PKT_PULL_REQUEST;
    prng_bytes(pkt->pull_request.exposure_id, 16);
    pkt->pull_request.window_size   = (uint32_t)(prng_next() & 0xFFFF) + 1;
    pkt->pull_request.loss_rate_q16 = (uint32_t)(prng_next() & 0xFFFF);
    pkt->pull_request.flags         = (uint32_t)(prng_next() & 0x3);
    pkt->pull_request.version_min   = 1;
    pkt->pull_request.version_max   = 1;
}

static void gen_manifest(rgtp_packet_t *pkt)
{
    memset(pkt, 0, sizeof(*pkt));
    pkt->type = RGTP_PKT_MANIFEST;
    prng_bytes(pkt->manifest.exposure_id, 16);
    pkt->manifest.total_size  = (uint64_t)(prng_next() & 0xFFFFFFFF) + 1;
    pkt->manifest.chunk_count = (uint32_t)(prng_next() % 1000) + 1;
    pkt->manifest.chunk_size  = 1200;
    prng_bytes(pkt->manifest.merkle_root, 32);
    pkt->manifest.fec_n = 255;
    pkt->manifest.fec_k = 223;
}

/* Static payload buffers — reused across iterations */
static uint8_t s_payload[256];
static uint8_t s_proof[32 * 8];   /* up to depth 8 */
static uint32_t s_nak_indices[16];

static void gen_chunk_data(rgtp_packet_t *pkt, uint32_t chunk_count)
{
    memset(pkt, 0, sizeof(*pkt));
    pkt->type = RGTP_PKT_CHUNK_DATA;
    prng_bytes(pkt->chunk_data.exposure_id, 16);
    pkt->chunk_data.chunk_index = (uint32_t)(prng_next() % chunk_count);
    size_t plen = (size_t)(prng_next() % 64) + 1;
    prng_bytes(s_payload, plen);
    pkt->chunk_data.payload     = s_payload;
    pkt->chunk_data.payload_len = plen;
}

static void gen_chunk_with_proof(rgtp_packet_t *pkt, uint32_t chunk_count)
{
    memset(pkt, 0, sizeof(*pkt));
    pkt->type = RGTP_PKT_CHUNK_DATA_WITH_PROOF;
    prng_bytes(pkt->chunk_with_proof.exposure_id, 16);
    pkt->chunk_with_proof.chunk_index = (uint32_t)(prng_next() % chunk_count);
    uint8_t depth = (uint8_t)((prng_next() % 4) + 1);
    prng_bytes(s_proof, depth * 32);
    pkt->chunk_with_proof.proof_depth = depth;
    pkt->chunk_with_proof.proof       = s_proof;
    size_t plen = (size_t)(prng_next() % 32) + 1;
    prng_bytes(s_payload, plen);
    pkt->chunk_with_proof.payload     = s_payload;
    pkt->chunk_with_proof.payload_len = plen;
}

static void gen_fec_parity(rgtp_packet_t *pkt)
{
    memset(pkt, 0, sizeof(*pkt));
    pkt->type = RGTP_PKT_FEC_PARITY;
    prng_bytes(pkt->fec_parity.exposure_id, 16);
    pkt->fec_parity.block_index  = (uint32_t)(prng_next() & 0xFF);
    pkt->fec_parity.parity_index = (uint8_t)(prng_next() % 32);
    pkt->fec_parity.fec_n        = 255;
    pkt->fec_parity.fec_k        = 223;
    size_t plen = (size_t)(prng_next() % 32) + 1;
    prng_bytes(s_payload, plen);
    pkt->fec_parity.payload     = s_payload;
    pkt->fec_parity.payload_len = plen;
}

static void gen_nak(rgtp_packet_t *pkt, uint32_t chunk_count)
{
    memset(pkt, 0, sizeof(*pkt));
    pkt->type = RGTP_PKT_NAK;
    prng_bytes(pkt->nak.exposure_id, 16);
    uint16_t count = (uint16_t)((prng_next() % 4) + 1);
    for (uint16_t i = 0; i < count; i++) {
        s_nak_indices[i] = (uint32_t)(prng_next() % chunk_count);
    }
    pkt->nak.nak_count     = count;
    pkt->nak.chunk_indices = s_nak_indices;
}

static void gen_rate_report(rgtp_packet_t *pkt)
{
    memset(pkt, 0, sizeof(*pkt));
    pkt->type = RGTP_PKT_RATE_REPORT;
    prng_bytes(pkt->rate_report.exposure_id, 16);
    pkt->rate_report.rtt_us        = (uint32_t)(prng_next() & 0xFFFF);
    pkt->rate_report.loss_rate_q16 = (uint32_t)(prng_next() & 0xFFFF);
    pkt->rate_report.window_size   = (uint32_t)(prng_next() % 256) + 1;
    pkt->rate_report.timestamp_us  = prng_next();
}

static void gen_keepalive(rgtp_packet_t *pkt)
{
    memset(pkt, 0, sizeof(*pkt));
    pkt->type = RGTP_PKT_KEEPALIVE;
    prng_bytes(pkt->keepalive.exposure_id, 16);
    pkt->keepalive.timestamp_us = prng_next();
}

/* ── Equivalence check ──────────────────────────────────────────────────── */

static int packets_equivalent(const rgtp_packet_t *a, const rgtp_packet_t *b)
{
    if (a->type != b->type) return 0;
    switch (a->type) {
    case RGTP_PKT_PULL_REQUEST:
        return memcmp(a->pull_request.exposure_id,
                      b->pull_request.exposure_id, 16) == 0
            && a->pull_request.window_size   == b->pull_request.window_size
            && a->pull_request.loss_rate_q16 == b->pull_request.loss_rate_q16
            && a->pull_request.flags         == b->pull_request.flags
            && a->pull_request.version_min   == b->pull_request.version_min
            && a->pull_request.version_max   == b->pull_request.version_max;
    case RGTP_PKT_MANIFEST:
        return memcmp(a->manifest.exposure_id,
                      b->manifest.exposure_id, 16) == 0
            && a->manifest.total_size  == b->manifest.total_size
            && a->manifest.chunk_count == b->manifest.chunk_count
            && a->manifest.chunk_size  == b->manifest.chunk_size
            && memcmp(a->manifest.merkle_root,
                      b->manifest.merkle_root, 32) == 0
            && a->manifest.fec_n == b->manifest.fec_n
            && a->manifest.fec_k == b->manifest.fec_k;
    case RGTP_PKT_CHUNK_DATA:
        return memcmp(a->chunk_data.exposure_id,
                      b->chunk_data.exposure_id, 16) == 0
            && a->chunk_data.chunk_index == b->chunk_data.chunk_index
            && a->chunk_data.payload_len == b->chunk_data.payload_len
            && memcmp(a->chunk_data.payload,
                      b->chunk_data.payload,
                      a->chunk_data.payload_len) == 0;
    case RGTP_PKT_CHUNK_DATA_WITH_PROOF:
        return memcmp(a->chunk_with_proof.exposure_id,
                      b->chunk_with_proof.exposure_id, 16) == 0
            && a->chunk_with_proof.chunk_index == b->chunk_with_proof.chunk_index
            && a->chunk_with_proof.proof_depth == b->chunk_with_proof.proof_depth
            && memcmp(a->chunk_with_proof.proof,
                      b->chunk_with_proof.proof,
                      a->chunk_with_proof.proof_depth * 32) == 0
            && a->chunk_with_proof.payload_len == b->chunk_with_proof.payload_len
            && memcmp(a->chunk_with_proof.payload,
                      b->chunk_with_proof.payload,
                      a->chunk_with_proof.payload_len) == 0;
    case RGTP_PKT_FEC_PARITY:
        return memcmp(a->fec_parity.exposure_id,
                      b->fec_parity.exposure_id, 16) == 0
            && a->fec_parity.block_index  == b->fec_parity.block_index
            && a->fec_parity.parity_index == b->fec_parity.parity_index
            && a->fec_parity.fec_n        == b->fec_parity.fec_n
            && a->fec_parity.fec_k        == b->fec_parity.fec_k
            && a->fec_parity.payload_len  == b->fec_parity.payload_len
            && memcmp(a->fec_parity.payload,
                      b->fec_parity.payload,
                      a->fec_parity.payload_len) == 0;
    case RGTP_PKT_NAK:
        if (memcmp(a->nak.exposure_id, b->nak.exposure_id, 16) != 0) return 0;
        if (a->nak.nak_count != b->nak.nak_count) return 0;
        for (uint16_t i = 0; i < a->nak.nak_count; i++) {
            if (a->nak.chunk_indices[i] != b->nak.chunk_indices[i]) return 0;
        }
        return 1;
    case RGTP_PKT_RATE_REPORT:
        return memcmp(a->rate_report.exposure_id,
                      b->rate_report.exposure_id, 16) == 0
            && a->rate_report.rtt_us        == b->rate_report.rtt_us
            && a->rate_report.loss_rate_q16 == b->rate_report.loss_rate_q16
            && a->rate_report.window_size   == b->rate_report.window_size
            && a->rate_report.timestamp_us  == b->rate_report.timestamp_us;
    case RGTP_PKT_KEEPALIVE:
        return memcmp(a->keepalive.exposure_id,
                      b->keepalive.exposure_id, 16) == 0
            && a->keepalive.timestamp_us == b->keepalive.timestamp_us;
    default:
        return 0;
    }
}

/* ── Property 3: Wire Protocol Parse/Serialize Round-Trip ──────────────── */
/* Feature: rgtp-core-overhaul, Property 3: Wire Protocol Parse/Serialize Round-Trip */

#define PROP3_ITERATIONS_PER_TYPE 1000
#define PROP3_BUF_SIZE            4096

static int prop3_wire_roundtrip(void)
{
    int failures = 0;
    static uint8_t buf[PROP3_BUF_SIZE];

    /* Iterate over all 8 packet types */
    for (int type_idx = 0; type_idx < 8; type_idx++) {
        for (int iter = 0; iter < PROP3_ITERATIONS_PER_TYPE; iter++) {
            rgtp_packet_t original, parsed;
            uint32_t chunk_count = (uint32_t)(prng_next() % 1000) + 10;

            switch (type_idx) {
            case 0: gen_pull_request(&original);                    break;
            case 1: gen_manifest(&original);                        break;
            case 2: gen_chunk_data(&original, chunk_count);         break;
            case 3: gen_chunk_with_proof(&original, chunk_count);   break;
            case 4: gen_fec_parity(&original);                      break;
            case 5: gen_nak(&original, chunk_count);                break;
            case 6: gen_rate_report(&original);                     break;
            case 7: gen_keepalive(&original);                       break;
            }

            /* Serialize */
            size_t len = 0;
            rgtp_error_t err = rgtp_serialize_packet(&original, buf,
                                                      PROP3_BUF_SIZE, &len);
            if (err != RGTP_OK) {
                fprintf(stderr,
                        "  PROP3 FAIL type=%d iter=%d: serialize: %s\n",
                        type_idx, iter, rgtp_strerror(err));
                failures++;
                continue;
            }

            /* Parse */
            err = rgtp_parse_packet(buf, len, chunk_count, &parsed);
            if (err != RGTP_OK) {
                fprintf(stderr,
                        "  PROP3 FAIL type=%d iter=%d: parse: %s\n",
                        type_idx, iter, rgtp_strerror(err));
                failures++;
                continue;
            }

            /* Equivalence */
            if (!packets_equivalent(&original, &parsed)) {
                fprintf(stderr,
                        "  PROP3 FAIL type=%d iter=%d: "
                        "parsed packet differs from original\n",
                        type_idx, iter);
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

    int total_iters = PROP3_ITERATIONS_PER_TYPE * 8;
    printf("Running Property 3: Wire Protocol Parse/Serialize Round-Trip "
           "(%d iterations across 8 packet types)...\n", total_iters);
    int failures = prop3_wire_roundtrip();
    if (failures == 0) {
        printf("  PASS Property 3 (%d iterations)\n", total_iters);
        rgtp_test_passed++;
    } else {
        fprintf(stderr, "  FAIL Property 3: %d failures\n", failures);
        rgtp_test_failures += failures;
    }

    RGTP_PRINT_RESULTS();
    return rgtp_test_failures > 0 ? 1 : 0;
}
