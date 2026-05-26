/**
 * @file rgtp_packet.c
 * @brief RGTP packet serializer (pretty-printer).
 *
 * Serializes any valid rgtp_packet_t into a byte buffer suitable for
 * transmission. All multi-byte integers are written in big-endian order.
 *
 * Requirements: 7.4
 */

#include "rgtp_wire_internal.h"
#include "rgtp_packet_types.h"

#include <string.h>
#include <stdint.h>

/* ── Big-endian write helpers ───────────────────────────────────────────── */

static inline void write_u16_be(uint8_t* p, uint16_t v)
{
    p[0] = (uint8_t)(v >> 8);
    p[1] = (uint8_t)(v & 0xFFu);
}

static inline void write_u32_be(uint8_t* p, uint32_t v)
{
    p[0] = (uint8_t)(v >> 24);
    p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >>  8);
    p[3] = (uint8_t)(v & 0xFFu);
}

static inline void write_u64_be(uint8_t* p, uint64_t v)
{
    p[0] = (uint8_t)(v >> 56);
    p[1] = (uint8_t)(v >> 48);
    p[2] = (uint8_t)(v >> 40);
    p[3] = (uint8_t)(v >> 32);
    p[4] = (uint8_t)(v >> 24);
    p[5] = (uint8_t)(v >> 16);
    p[6] = (uint8_t)(v >>  8);
    p[7] = (uint8_t)(v & 0xFFu);
}

/* ── Common header ──────────────────────────────────────────────────────── */

static void write_header(uint8_t* buf, rgtp_pkt_type_t type, uint16_t total_len)
{
    buf[0] = RGTP_PROTOCOL_VERSION;
    buf[1] = (uint8_t)type;
    write_u16_be(buf + 2, total_len);
}

/* ── Per-type serializers ───────────────────────────────────────────────── */

static rgtp_error_t serialize_pull_request(const rgtp_pull_request_t* p,
                                            uint8_t* buf, size_t buf_size,
                                            size_t* out_len)
{
    /* Header(4) + exposure_id(16) + window_size(4) + loss_rate_q16(4)
     * + flags(4) + version_min(2) + version_max(2) = 36 bytes */
    const size_t LEN = 36u;
    if (buf_size < LEN) return RGTP_ERR_INVALID_ARG;

    write_header(buf, RGTP_PKT_PULL_REQUEST, (uint16_t)LEN);
    memcpy(buf + 4, p->exposure_id, 16);
    write_u32_be(buf + 20, p->window_size);
    write_u32_be(buf + 24, p->loss_rate_q16);
    write_u32_be(buf + 28, p->flags);
    write_u16_be(buf + 32, p->version_min);
    write_u16_be(buf + 34, p->version_max);
    *out_len = LEN;
    return RGTP_OK;
}

static rgtp_error_t serialize_manifest(const rgtp_manifest_t* p,
                                        uint8_t* buf, size_t buf_size,
                                        size_t* out_len)
{
    /* Header(4) + exposure_id(16) + total_size(8) + chunk_count(4)
     * + chunk_size(4) + merkle_root(32) + fec_n(1) + fec_k(1) + flags(2) = 72 */
    const size_t LEN = 72u;
    if (buf_size < LEN) return RGTP_ERR_INVALID_ARG;

    write_header(buf, RGTP_PKT_MANIFEST, (uint16_t)LEN);
    memcpy(buf + 4,  p->exposure_id, 16);
    write_u64_be(buf + 20, p->total_size);
    write_u32_be(buf + 28, p->chunk_count);
    write_u32_be(buf + 32, p->chunk_size);
    memcpy(buf + 36, p->merkle_root, 32);
    buf[68] = p->fec_n;
    buf[69] = p->fec_k;
    write_u16_be(buf + 70, p->flags);
    *out_len = LEN;
    return RGTP_OK;
}

static rgtp_error_t serialize_chunk_data(const rgtp_chunk_data_t* p,
                                          uint8_t* buf, size_t buf_size,
                                          size_t* out_len)
{
    /* Header(4) + exposure_id(16) + chunk_index(4) + payload(variable) */
    const size_t FIXED = 24u;
    const size_t LEN   = FIXED + p->payload_len;
    if (buf_size < LEN || LEN > 65535u) return RGTP_ERR_INVALID_ARG;

    write_header(buf, RGTP_PKT_CHUNK_DATA, (uint16_t)LEN);
    memcpy(buf + 4, p->exposure_id, 16);
    write_u32_be(buf + 20, p->chunk_index);
    if (p->payload_len > 0 && p->payload != NULL) {
        memcpy(buf + FIXED, p->payload, p->payload_len);
    }
    *out_len = LEN;
    return RGTP_OK;
}

static rgtp_error_t serialize_chunk_with_proof(const rgtp_chunk_with_proof_t* p,
                                                uint8_t* buf, size_t buf_size,
                                                size_t* out_len)
{
    /* Header(4) + exposure_id(16) + chunk_index(4) + proof_depth(1)
     * + proof(proof_depth*32) + payload(variable) */
    const size_t FIXED      = 25u;
    const size_t proof_bytes = (size_t)p->proof_depth * 32u;
    const size_t LEN         = FIXED + proof_bytes + p->payload_len;
    if (buf_size < LEN || LEN > 65535u) return RGTP_ERR_INVALID_ARG;

    write_header(buf, RGTP_PKT_CHUNK_DATA_WITH_PROOF, (uint16_t)LEN);
    memcpy(buf + 4, p->exposure_id, 16);
    write_u32_be(buf + 20, p->chunk_index);
    buf[24] = p->proof_depth;
    if (proof_bytes > 0 && p->proof != NULL) {
        memcpy(buf + FIXED, p->proof, proof_bytes);
    }
    if (p->payload_len > 0 && p->payload != NULL) {
        memcpy(buf + FIXED + proof_bytes, p->payload, p->payload_len);
    }
    *out_len = LEN;
    return RGTP_OK;
}

static rgtp_error_t serialize_fec_parity(const rgtp_fec_parity_t* p,
                                          uint8_t* buf, size_t buf_size,
                                          size_t* out_len)
{
    /* Header(4) + exposure_id(16) + block_index(4) + parity_index(1)
     * + fec_n(1) + fec_k(1) + reserved(1) + payload(variable) = 28 + payload */
    const size_t FIXED = 28u;
    const size_t LEN   = FIXED + p->payload_len;
    if (buf_size < LEN || LEN > 65535u) return RGTP_ERR_INVALID_ARG;

    write_header(buf, RGTP_PKT_FEC_PARITY, (uint16_t)LEN);
    memcpy(buf + 4, p->exposure_id, 16);
    write_u32_be(buf + 20, p->block_index);
    buf[24] = p->parity_index;
    buf[25] = p->fec_n;
    buf[26] = p->fec_k;
    buf[27] = p->reserved;
    if (p->payload_len > 0 && p->payload != NULL) {
        memcpy(buf + FIXED, p->payload, p->payload_len);
    }
    *out_len = LEN;
    return RGTP_OK;
}

static rgtp_error_t serialize_nak(const rgtp_nak_t* p,
                                   uint8_t* buf, size_t buf_size,
                                   size_t* out_len)
{
    /* Header(4) + exposure_id(16) + nak_count(2) + reserved(2)
     * + chunk_indices(nak_count*4) */
    const size_t FIXED = 24u;
    const size_t LEN   = FIXED + (size_t)p->nak_count * 4u;
    if (buf_size < LEN || LEN > 65535u) return RGTP_ERR_INVALID_ARG;

    write_header(buf, RGTP_PKT_NAK, (uint16_t)LEN);
    memcpy(buf + 4, p->exposure_id, 16);
    write_u16_be(buf + 20, p->nak_count);
    buf[22] = 0; buf[23] = 0;   /* reserved */
    for (uint16_t i = 0; i < p->nak_count; i++) {
        write_u32_be(buf + FIXED + (size_t)i * 4u, p->chunk_indices[i]);
    }
    *out_len = LEN;
    return RGTP_OK;
}

static rgtp_error_t serialize_rate_report(const rgtp_rate_report_t* p,
                                           uint8_t* buf, size_t buf_size,
                                           size_t* out_len)
{
    /* Header(4) + exposure_id(16) + rtt_us(4) + loss_rate_q16(4)
     * + window_size(4) + reserved(4) + timestamp_us(8) = 44 bytes */
    const size_t LEN = 44u;
    if (buf_size < LEN) return RGTP_ERR_INVALID_ARG;

    write_header(buf, RGTP_PKT_RATE_REPORT, (uint16_t)LEN);
    memcpy(buf + 4, p->exposure_id, 16);
    write_u32_be(buf + 20, p->rtt_us);
    write_u32_be(buf + 24, p->loss_rate_q16);
    write_u32_be(buf + 28, p->window_size);
    write_u32_be(buf + 32, p->reserved);
    write_u64_be(buf + 36, p->timestamp_us);
    *out_len = LEN;
    return RGTP_OK;
}

static rgtp_error_t serialize_keepalive(const rgtp_keepalive_t* p,
                                         uint8_t* buf, size_t buf_size,
                                         size_t* out_len)
{
    /* Header(4) + exposure_id(16) + timestamp_us(8) + reserved(4) = 32 bytes */
    const size_t LEN = 32u;
    if (buf_size < LEN) return RGTP_ERR_INVALID_ARG;

    write_header(buf, RGTP_PKT_KEEPALIVE, (uint16_t)LEN);
    memcpy(buf + 4, p->exposure_id, 16);
    write_u64_be(buf + 20, p->timestamp_us);
    write_u32_be(buf + 28, p->reserved);
    *out_len = LEN;
    return RGTP_OK;
}

/* ── Public API ─────────────────────────────────────────────────────────── */

rgtp_error_t rgtp_serialize_packet(const rgtp_packet_t* pkt,
                                    uint8_t*             buf,
                                    size_t               buf_size,
                                    size_t*              out_len)
{
    if (pkt == NULL || buf == NULL || out_len == NULL) {
        return RGTP_ERR_INVALID_ARG;
    }

    switch (pkt->type) {
    case RGTP_PKT_PULL_REQUEST:
        return serialize_pull_request(&pkt->pull_request, buf, buf_size, out_len);
    case RGTP_PKT_MANIFEST:
        return serialize_manifest(&pkt->manifest, buf, buf_size, out_len);
    case RGTP_PKT_CHUNK_DATA:
        return serialize_chunk_data(&pkt->chunk_data, buf, buf_size, out_len);
    case RGTP_PKT_CHUNK_DATA_WITH_PROOF:
        return serialize_chunk_with_proof(&pkt->chunk_with_proof, buf, buf_size, out_len);
    case RGTP_PKT_FEC_PARITY:
        return serialize_fec_parity(&pkt->fec_parity, buf, buf_size, out_len);
    case RGTP_PKT_NAK:
        return serialize_nak(&pkt->nak, buf, buf_size, out_len);
    case RGTP_PKT_RATE_REPORT:
        return serialize_rate_report(&pkt->rate_report, buf, buf_size, out_len);
    case RGTP_PKT_KEEPALIVE:
        return serialize_keepalive(&pkt->keepalive, buf, buf_size, out_len);
    default:
        return RGTP_ERR_INVALID_ARG;
    }
}
