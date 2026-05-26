/**
 * @file rgtp_parser.c
 * @brief RGTP packet parser state machine.
 *
 * Non-allocating: all output is written into the caller-supplied rgtp_packet_t.
 * Pointer fields (payload, proof, chunk_indices) point directly into the
 * input buffer — zero-copy parsing.
 *
 * State machine:
 *   READING_HEADER (4 bytes)
 *     → validate Version, Type, Length
 *     → if Length > len: RGTP_ERR_TRUNCATED
 *     → if Type unknown: RGTP_ERR_INVALID_ARG
 *     → if Version > RGTP_PROTOCOL_VERSION: RGTP_ERR_NOT_SUPPORTED
 *   READING_BODY (Length - 4 bytes)
 *     → per-type field validation
 *     → populate rgtp_packet_t
 *
 * Requirements: 7.3, 7.6, 7.7, 7.8, 7.9
 */

#include "rgtp_wire_internal.h"
#include "rgtp_packet_types.h"

#include <string.h>
#include <stdint.h>

/* ── Big-endian read helpers ────────────────────────────────────────────── */

static inline uint16_t read_u16_be(const uint8_t* p)
{
    return (uint16_t)(((uint16_t)p[0] << 8) | p[1]);
}

static inline uint32_t read_u32_be(const uint8_t* p)
{
    return ((uint32_t)p[0] << 24)
         | ((uint32_t)p[1] << 16)
         | ((uint32_t)p[2] <<  8)
         |  (uint32_t)p[3];
}

static inline uint64_t read_u64_be(const uint8_t* p)
{
    return ((uint64_t)p[0] << 56)
         | ((uint64_t)p[1] << 48)
         | ((uint64_t)p[2] << 40)
         | ((uint64_t)p[3] << 32)
         | ((uint64_t)p[4] << 24)
         | ((uint64_t)p[5] << 16)
         | ((uint64_t)p[6] <<  8)
         |  (uint64_t)p[7];
}

/* ── Per-type parsers ───────────────────────────────────────────────────── */

static rgtp_error_t parse_pull_request(const uint8_t* body, size_t body_len,
                                        rgtp_pull_request_t* out)
{
    /* body = exposure_id(16) + window_size(4) + loss_rate_q16(4)
     *      + flags(4) + version_min(2) + version_max(2) = 32 bytes */
    if (body_len < 32u) return RGTP_ERR_TRUNCATED;

    memcpy(out->exposure_id, body, 16);
    out->window_size   = read_u32_be(body + 16);
    out->loss_rate_q16 = read_u32_be(body + 20);
    out->flags         = read_u32_be(body + 24);
    out->version_min   = read_u16_be(body + 28);
    out->version_max   = read_u16_be(body + 30);
    return RGTP_OK;
}

static rgtp_error_t parse_manifest(const uint8_t* body, size_t body_len,
                                    rgtp_manifest_t* out)
{
    /* body = exposure_id(16) + total_size(8) + chunk_count(4)
     *      + chunk_size(4) + merkle_root(32) + fec_n(1) + fec_k(1) + flags(2) = 68 */
    if (body_len < 68u) return RGTP_ERR_TRUNCATED;

    memcpy(out->exposure_id, body, 16);
    out->total_size  = read_u64_be(body + 16);
    out->chunk_count = read_u32_be(body + 24);
    out->chunk_size  = read_u32_be(body + 28);
    memcpy(out->merkle_root, body + 32, 32);
    out->fec_n = body[64];
    out->fec_k = body[65];
    out->flags = read_u16_be(body + 66);

    if (out->chunk_count == 0 || out->chunk_size == 0) {
        return RGTP_ERR_INVALID_ARG;
    }
    return RGTP_OK;
}

static rgtp_error_t parse_chunk_data(const uint8_t* body, size_t body_len,
                                      uint32_t manifest_chunk_count,
                                      rgtp_chunk_data_t* out)
{
    /* body = exposure_id(16) + chunk_index(4) + payload(variable) */
    if (body_len < 20u) return RGTP_ERR_TRUNCATED;

    memcpy(out->exposure_id, body, 16);
    out->chunk_index = read_u32_be(body + 16);

    if (out->chunk_index >= manifest_chunk_count) {
        return RGTP_ERR_CHUNK_INDEX_OOB;
    }

    out->payload     = body + 20;
    out->payload_len = (uint16_t)(body_len - 20u);
    return RGTP_OK;
}

static rgtp_error_t parse_chunk_with_proof(const uint8_t* body, size_t body_len,
                                            uint32_t manifest_chunk_count,
                                            rgtp_chunk_with_proof_t* out)
{
    /* body = exposure_id(16) + chunk_index(4) + proof_depth(1)
     *      + proof(proof_depth*32) + payload(variable) */
    if (body_len < 21u) return RGTP_ERR_TRUNCATED;

    memcpy(out->exposure_id, body, 16);
    out->chunk_index = read_u32_be(body + 16);
    out->proof_depth = body[20];

    if (out->chunk_index >= manifest_chunk_count) {
        return RGTP_ERR_CHUNK_INDEX_OOB;
    }

    const size_t proof_bytes = (size_t)out->proof_depth * 32u;
    if (body_len < 21u + proof_bytes) return RGTP_ERR_TRUNCATED;

    out->proof       = body + 21;
    out->payload     = body + 21 + proof_bytes;
    out->payload_len = (uint16_t)(body_len - 21u - proof_bytes);
    return RGTP_OK;
}

static rgtp_error_t parse_fec_parity(const uint8_t* body, size_t body_len,
                                      rgtp_fec_parity_t* out)
{
    /* body = exposure_id(16) + block_index(4) + parity_index(1)
     *      + fec_n(1) + fec_k(1) + reserved(1) + payload(variable) = 24 + payload */
    if (body_len < 24u) return RGTP_ERR_TRUNCATED;

    memcpy(out->exposure_id, body, 16);
    out->block_index  = read_u32_be(body + 16);
    out->parity_index = body[20];
    out->fec_n        = body[21];
    out->fec_k        = body[22];
    out->reserved     = body[23];
    out->payload      = body + 24;
    out->payload_len  = (uint16_t)(body_len - 24u);
    return RGTP_OK;
}

static rgtp_error_t parse_nak(const uint8_t* body, size_t body_len,
                               rgtp_nak_t* out)
{
    /* body = exposure_id(16) + nak_count(2) + reserved(2)
     *      + chunk_indices(nak_count*4) */
    if (body_len < 20u) return RGTP_ERR_TRUNCATED;

    memcpy(out->exposure_id, body, 16);
    out->nak_count = read_u16_be(body + 16);
    /* body[18..19] = reserved */

    const size_t indices_bytes = (size_t)out->nak_count * 4u;
    if (body_len < 20u + indices_bytes) return RGTP_ERR_TRUNCATED;

    /* Point directly into the buffer — caller must keep buf alive */
    out->chunk_indices = (const uint32_t*)(body + 20);
    return RGTP_OK;
}

static rgtp_error_t parse_rate_report(const uint8_t* body, size_t body_len,
                                       rgtp_rate_report_t* out)
{
    /* body = exposure_id(16) + rtt_us(4) + loss_rate_q16(4)
     *      + window_size(4) + reserved(4) + timestamp_us(8) = 40 bytes */
    if (body_len < 40u) return RGTP_ERR_TRUNCATED;

    memcpy(out->exposure_id, body, 16);
    out->rtt_us        = read_u32_be(body + 16);
    out->loss_rate_q16 = read_u32_be(body + 20);
    out->window_size   = read_u32_be(body + 24);
    out->reserved      = read_u32_be(body + 28);
    out->timestamp_us  = read_u64_be(body + 32);
    return RGTP_OK;
}

static rgtp_error_t parse_keepalive(const uint8_t* body, size_t body_len,
                                     rgtp_keepalive_t* out)
{
    /* body = exposure_id(16) + timestamp_us(8) + reserved(4) = 28 bytes */
    if (body_len < 28u) return RGTP_ERR_TRUNCATED;

    memcpy(out->exposure_id, body, 16);
    out->timestamp_us = read_u64_be(body + 16);
    out->reserved     = read_u32_be(body + 24);
    return RGTP_OK;
}

/* ── Public API ─────────────────────────────────────────────────────────── */

rgtp_error_t rgtp_parse_packet(const uint8_t* buf,
                                size_t         len,
                                uint32_t       manifest_chunk_count,
                                rgtp_packet_t* out)
{
    if (buf == NULL || out == NULL) {
        return RGTP_ERR_INVALID_ARG;
    }

    /* ── Phase 1: Read and validate common header (4 bytes) ─────────── */
    if (len < RGTP_HEADER_SIZE) {
        return RGTP_ERR_TRUNCATED;
    }

    const uint8_t  version    = buf[0];
    const uint8_t  type_byte  = buf[1];
    const uint16_t pkt_length = read_u16_be(buf + 2);

    /* Version check */
    if (version > RGTP_PROTOCOL_VERSION) {
        return RGTP_ERR_NOT_SUPPORTED;
    }

    /* Length check: declared length must not exceed actual received bytes */
    if ((size_t)pkt_length > len) {
        return RGTP_ERR_TRUNCATED;
    }
    if (pkt_length < RGTP_HEADER_SIZE) {
        return RGTP_ERR_INVALID_ARG;
    }

    /* Type check */
    switch ((rgtp_pkt_type_t)type_byte) {
    case RGTP_PKT_PULL_REQUEST:
    case RGTP_PKT_MANIFEST:
    case RGTP_PKT_CHUNK_DATA:
    case RGTP_PKT_CHUNK_DATA_WITH_PROOF:
    case RGTP_PKT_FEC_PARITY:
    case RGTP_PKT_NAK:
    case RGTP_PKT_RATE_REPORT:
    case RGTP_PKT_KEEPALIVE:
        break;
    default:
        return RGTP_ERR_INVALID_ARG;
    }

    /* ── Phase 2: Parse body ─────────────────────────────────────────── */
    const uint8_t* body     = buf + RGTP_HEADER_SIZE;
    const size_t   body_len = (size_t)pkt_length - RGTP_HEADER_SIZE;

    out->type = (rgtp_pkt_type_t)type_byte;

    switch (out->type) {
    case RGTP_PKT_PULL_REQUEST:
        return parse_pull_request(body, body_len, &out->pull_request);
    case RGTP_PKT_MANIFEST:
        return parse_manifest(body, body_len, &out->manifest);
    case RGTP_PKT_CHUNK_DATA:
        return parse_chunk_data(body, body_len, manifest_chunk_count, &out->chunk_data);
    case RGTP_PKT_CHUNK_DATA_WITH_PROOF:
        return parse_chunk_with_proof(body, body_len, manifest_chunk_count,
                                       &out->chunk_with_proof);
    case RGTP_PKT_FEC_PARITY:
        return parse_fec_parity(body, body_len, &out->fec_parity);
    case RGTP_PKT_NAK:
        return parse_nak(body, body_len, &out->nak);
    case RGTP_PKT_RATE_REPORT:
        return parse_rate_report(body, body_len, &out->rate_report);
    case RGTP_PKT_KEEPALIVE:
        return parse_keepalive(body, body_len, &out->keepalive);
    default:
        return RGTP_ERR_INVALID_ARG;
    }
}
