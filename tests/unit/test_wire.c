/**
 * @file test_wire.c
 * @brief Unit tests for the RGTP wire protocol parser and serializer.
 *
 * Tests: 15 cases covering valid parse for all 8 packet types, error
 * conditions, and big-endian field verification.
 *
 * Requirements: 17.1, 17.2
 */

#include "rgtp/rgtp.h"
#include "../helpers/rgtp_test.h"
#include "../../src/wire/rgtp_wire_internal.h"
#include "../../src/wire/rgtp_packet_types.h"

#include <string.h>
#include <stdint.h>

/* ── Round-trip helper ──────────────────────────────────────────────────── */

static void assert_roundtrip(const rgtp_packet_t* pkt, uint32_t chunk_count)
{
    uint8_t buf[4096];
    size_t  len = 0;

    rgtp_error_t err = rgtp_serialize_packet(pkt, buf, sizeof(buf), &len);
    RGTP_ASSERT(err == RGTP_OK, "Serialize must succeed");
    RGTP_ASSERT(len >= RGTP_HEADER_SIZE, "Serialized length must be >= 4");

    rgtp_packet_t parsed;
    err = rgtp_parse_packet(buf, len, chunk_count, &parsed);
    RGTP_ASSERT(err == RGTP_OK, "Parse of serialized packet must succeed");
    RGTP_ASSERT(parsed.type == pkt->type, "Parsed type must match original");
}

/* ── Test: Valid Pull_Request ───────────────────────────────────────────── */
static void test_parse_pull_request_valid(void)
{
    rgtp_packet_t pkt = {0};
    pkt.type = RGTP_PKT_PULL_REQUEST;
    memset(pkt.pull_request.exposure_id, 0xAB, 16);
    pkt.pull_request.window_size   = 64;
    pkt.pull_request.loss_rate_q16 = 100;
    pkt.pull_request.flags         = RGTP_PULL_FLAG_WANT_PROOF;
    pkt.pull_request.version_min   = 1;
    pkt.pull_request.version_max   = 1;
    assert_roundtrip(&pkt, UINT32_MAX);
}

/* ── Test: Valid Manifest ───────────────────────────────────────────────── */
static void test_parse_manifest_valid(void)
{
    rgtp_packet_t pkt = {0};
    pkt.type = RGTP_PKT_MANIFEST;
    memset(pkt.manifest.exposure_id, 0x11, 16);
    pkt.manifest.total_size  = 1024 * 1024;
    pkt.manifest.chunk_count = 100;
    pkt.manifest.chunk_size  = 1200;
    memset(pkt.manifest.merkle_root, 0x22, 32);
    pkt.manifest.fec_n = 255;
    pkt.manifest.fec_k = 223;
    assert_roundtrip(&pkt, UINT32_MAX);
}

/* ── Test: Valid Chunk_Data ─────────────────────────────────────────────── */
static void test_parse_chunk_data_valid(void)
{
    static uint8_t payload[64];
    memset(payload, 0x55, sizeof(payload));

    rgtp_packet_t pkt = {0};
    pkt.type = RGTP_PKT_CHUNK_DATA;
    memset(pkt.chunk_data.exposure_id, 0x33, 16);
    pkt.chunk_data.chunk_index = 5;
    pkt.chunk_data.payload     = payload;
    pkt.chunk_data.payload_len = sizeof(payload);
    assert_roundtrip(&pkt, 100);
}

/* ── Test: Valid Chunk_Data_With_Proof ──────────────────────────────────── */
static void test_parse_chunk_with_proof_valid(void)
{
    static uint8_t proof[32];
    static uint8_t payload[32];
    memset(proof,   0xAA, 32);
    memset(payload, 0xBB, 32);

    rgtp_packet_t pkt = {0};
    pkt.type = RGTP_PKT_CHUNK_DATA_WITH_PROOF;
    memset(pkt.chunk_with_proof.exposure_id, 0x44, 16);
    pkt.chunk_with_proof.chunk_index  = 3;
    pkt.chunk_with_proof.proof_depth  = 1;
    pkt.chunk_with_proof.proof        = proof;
    pkt.chunk_with_proof.payload      = payload;
    pkt.chunk_with_proof.payload_len  = 32;
    assert_roundtrip(&pkt, 100);
}

/* ── Test: Valid FEC_Parity ─────────────────────────────────────────────── */
static void test_parse_fec_parity_valid(void)
{
    static uint8_t payload[32];
    memset(payload, 0xCC, 32);

    rgtp_packet_t pkt = {0};
    pkt.type = RGTP_PKT_FEC_PARITY;
    memset(pkt.fec_parity.exposure_id, 0x55, 16);
    pkt.fec_parity.block_index  = 0;
    pkt.fec_parity.parity_index = 0;
    pkt.fec_parity.fec_n        = 255;
    pkt.fec_parity.fec_k        = 223;
    pkt.fec_parity.payload      = payload;
    pkt.fec_parity.payload_len  = 32;
    assert_roundtrip(&pkt, UINT32_MAX);
}

/* ── Test: Valid NAK ────────────────────────────────────────────────────── */
static void test_parse_nak_valid(void)
{
    static uint32_t indices[3] = {1, 5, 10};

    rgtp_packet_t pkt = {0};
    pkt.type = RGTP_PKT_NAK;
    memset(pkt.nak.exposure_id, 0x66, 16);
    pkt.nak.nak_count      = 3;
    pkt.nak.chunk_indices  = indices;
    assert_roundtrip(&pkt, UINT32_MAX);
}

/* ── Test: Valid Rate_Report ────────────────────────────────────────────── */
static void test_parse_rate_report_valid(void)
{
    rgtp_packet_t pkt = {0};
    pkt.type = RGTP_PKT_RATE_REPORT;
    memset(pkt.rate_report.exposure_id, 0x77, 16);
    pkt.rate_report.rtt_us        = 1000;
    pkt.rate_report.loss_rate_q16 = 500;
    pkt.rate_report.window_size   = 64;
    pkt.rate_report.timestamp_us  = 1700000000000000ULL;
    assert_roundtrip(&pkt, UINT32_MAX);
}

/* ── Test: Valid Keepalive ──────────────────────────────────────────────── */
static void test_parse_keepalive_valid(void)
{
    rgtp_packet_t pkt = {0};
    pkt.type = RGTP_PKT_KEEPALIVE;
    memset(pkt.keepalive.exposure_id, 0x88, 16);
    pkt.keepalive.timestamp_us = 1700000000000000ULL;
    assert_roundtrip(&pkt, UINT32_MAX);
}

/* ── Test: Truncated header (3 bytes) ───────────────────────────────────── */
static void test_parse_truncated_header(void)
{
    uint8_t buf[3] = {0x01, 0x01, 0x00};
    rgtp_packet_t pkt;
    rgtp_error_t err = rgtp_parse_packet(buf, 3, UINT32_MAX, &pkt);
    RGTP_ASSERT(err == RGTP_ERR_TRUNCATED,
                "3-byte buffer must return RGTP_ERR_TRUNCATED");
}

/* ── Test: Length field exceeds buffer ──────────────────────────────────── */
static void test_parse_length_exceeds_buffer(void)
{
    uint8_t buf[8] = {0x01, 0x01, 0x00, 0x64, 0,0,0,0};  /* Length=100, buf=8 */
    rgtp_packet_t pkt;
    rgtp_error_t err = rgtp_parse_packet(buf, 8, UINT32_MAX, &pkt);
    RGTP_ASSERT(err == RGTP_ERR_TRUNCATED,
                "Length > buffer must return RGTP_ERR_TRUNCATED");
}

/* ── Test: Unknown packet type ──────────────────────────────────────────── */
static void test_parse_unknown_type(void)
{
    uint8_t buf[8] = {0x01, 0xFF, 0x00, 0x08, 0,0,0,0};  /* Type=0xFF */
    rgtp_packet_t pkt;
    rgtp_error_t err = rgtp_parse_packet(buf, 8, UINT32_MAX, &pkt);
    RGTP_ASSERT(err == RGTP_ERR_INVALID_ARG,
                "Unknown type must return RGTP_ERR_INVALID_ARG");
}

/* ── Test: Chunk index out of bounds ────────────────────────────────────── */
static void test_parse_chunk_index_oob(void)
{
    /* Build a valid Chunk_Data packet with chunk_index = 50 */
    rgtp_packet_t pkt = {0};
    pkt.type = RGTP_PKT_CHUNK_DATA;
    pkt.chunk_data.chunk_index = 50;
    pkt.chunk_data.payload     = NULL;
    pkt.chunk_data.payload_len = 0;

    uint8_t buf[64];
    size_t  len = 0;
    RGTP_ASSERT_OK(rgtp_serialize_packet(&pkt, buf, sizeof(buf), &len));

    rgtp_packet_t parsed;
    /* manifest_chunk_count = 10, so index 50 is OOB */
    rgtp_error_t err = rgtp_parse_packet(buf, len, 10, &parsed);
    RGTP_ASSERT(err == RGTP_ERR_CHUNK_INDEX_OOB,
                "chunk_index >= chunk_count must return RGTP_ERR_CHUNK_INDEX_OOB");
}

/* ── Test: Unsupported version ──────────────────────────────────────────── */
static void test_parse_unknown_version(void)
{
    uint8_t buf[36];
    memset(buf, 0, sizeof(buf));
    buf[0] = 0xFF;   /* version = 255 */
    buf[1] = 0x01;   /* type = Pull_Request */
    buf[2] = 0x00;
    buf[3] = 0x24;   /* length = 36 */

    rgtp_packet_t pkt;
    rgtp_error_t err = rgtp_parse_packet(buf, 36, UINT32_MAX, &pkt);
    RGTP_ASSERT(err == RGTP_ERR_NOT_SUPPORTED,
                "Version > supported must return RGTP_ERR_NOT_SUPPORTED");
}

/* ── Test: All packet types serialize without error ─────────────────────── */
static void test_serialize_all_types(void)
{
    uint8_t buf[4096];
    size_t  len = 0;

    /* Pull_Request */
    rgtp_packet_t pkt = {0};
    pkt.type = RGTP_PKT_PULL_REQUEST;
    pkt.pull_request.version_min = 1;
    pkt.pull_request.version_max = 1;
    RGTP_ASSERT_OK(rgtp_serialize_packet(&pkt, buf, sizeof(buf), &len));

    /* Manifest */
    memset(&pkt, 0, sizeof(pkt));
    pkt.type = RGTP_PKT_MANIFEST;
    pkt.manifest.chunk_count = 1;
    pkt.manifest.chunk_size  = 1200;
    RGTP_ASSERT_OK(rgtp_serialize_packet(&pkt, buf, sizeof(buf), &len));

    /* Keepalive */
    memset(&pkt, 0, sizeof(pkt));
    pkt.type = RGTP_PKT_KEEPALIVE;
    RGTP_ASSERT_OK(rgtp_serialize_packet(&pkt, buf, sizeof(buf), &len));
}

/* ── Test: Multi-byte fields are big-endian ─────────────────────────────── */
static void test_big_endian_fields(void)
{
    rgtp_packet_t pkt = {0};
    pkt.type = RGTP_PKT_MANIFEST;
    pkt.manifest.chunk_count = 0x01020304u;
    pkt.manifest.chunk_size  = 1200;
    pkt.manifest.total_size  = 1;

    uint8_t buf[128];
    size_t  len = 0;
    RGTP_ASSERT_OK(rgtp_serialize_packet(&pkt, buf, sizeof(buf), &len));

    /* chunk_count is at offset 28 (header=4, exposure_id=16, total_size=8) */
    RGTP_ASSERT(buf[28] == 0x01, "chunk_count byte 0 must be 0x01 (big-endian)");
    RGTP_ASSERT(buf[29] == 0x02, "chunk_count byte 1 must be 0x02");
    RGTP_ASSERT(buf[30] == 0x03, "chunk_count byte 2 must be 0x03");
    RGTP_ASSERT(buf[31] == 0x04, "chunk_count byte 3 must be 0x04");
}

/* ── Test runner ────────────────────────────────────────────────────────── */
int main(void)
{
    RGTP_ASSERT_OK(rgtp_init());

    RGTP_RUN_TEST(test_parse_pull_request_valid);
    RGTP_RUN_TEST(test_parse_manifest_valid);
    RGTP_RUN_TEST(test_parse_chunk_data_valid);
    RGTP_RUN_TEST(test_parse_chunk_with_proof_valid);
    RGTP_RUN_TEST(test_parse_fec_parity_valid);
    RGTP_RUN_TEST(test_parse_nak_valid);
    RGTP_RUN_TEST(test_parse_rate_report_valid);
    RGTP_RUN_TEST(test_parse_keepalive_valid);
    RGTP_RUN_TEST(test_parse_truncated_header);
    RGTP_RUN_TEST(test_parse_length_exceeds_buffer);
    RGTP_RUN_TEST(test_parse_unknown_type);
    RGTP_RUN_TEST(test_parse_chunk_index_oob);
    RGTP_RUN_TEST(test_parse_unknown_version);
    RGTP_RUN_TEST(test_serialize_all_types);
    RGTP_RUN_TEST(test_big_endian_fields);

    RGTP_PRINT_RESULTS();
    return rgtp_test_failures > 0 ? 1 : 0;
}
