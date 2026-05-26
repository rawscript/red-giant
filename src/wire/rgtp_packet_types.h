/**
 * @file rgtp_packet_types.h
 * @brief RGTP wire protocol packet type definitions.
 *
 * All multi-byte integer fields are big-endian (network byte order).
 * All packets begin with the 4-byte common header.
 *
 * Requirements: 7.1, 7.2
 */

#ifndef RGTP_PACKET_TYPES_H
#define RGTP_PACKET_TYPES_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── Packet type bytes ──────────────────────────────────────────────────── */

typedef enum rgtp_pkt_type {
    RGTP_PKT_PULL_REQUEST          = 0x01,
    RGTP_PKT_MANIFEST              = 0x02,
    RGTP_PKT_CHUNK_DATA            = 0x03,
    RGTP_PKT_CHUNK_DATA_WITH_PROOF = 0x04,
    RGTP_PKT_FEC_PARITY            = 0x05,
    RGTP_PKT_NAK                   = 0x06,
    RGTP_PKT_RATE_REPORT           = 0x07,
    RGTP_PKT_KEEPALIVE             = 0x08,
} rgtp_pkt_type_t;

/* ── Protocol constants ─────────────────────────────────────────────────── */

#define RGTP_PROTOCOL_VERSION      0x01u
#define RGTP_HEADER_SIZE           4u      /* common header bytes */
#define RGTP_MAX_UDP_PAYLOAD       65507u
#define RGTP_MAX_RAWETH_PAYLOAD    1486u   /* 1500 - 14 byte Ethernet header */
#define RGTP_DEFAULT_CHUNK_SIZE_UDP    1200u
#define RGTP_DEFAULT_CHUNK_SIZE_ETH    1400u
#define RGTP_MIN_MTU               576u    /* RFC 791 minimum */

/* Pull_Request flags */
#define RGTP_PULL_FLAG_WANT_PROOF  (1u << 0)
#define RGTP_PULL_FLAG_FEC_ENABLED (1u << 1)

/* ── Per-type parsed structs (host byte order after parsing) ────────────── */

/** Pull_Request (0x01) — 36 bytes on wire */
typedef struct {
    uint8_t  exposure_id[16];  /**< Exposure to pull */
    uint32_t window_size;      /**< Current pull window size (chunks) */
    uint32_t loss_rate_q16;    /**< Packet loss rate as Q16 fixed-point [0, 65536] */
    uint32_t flags;            /**< RGTP_PULL_FLAG_* bitmask */
    uint16_t version_min;      /**< Minimum protocol version supported */
    uint16_t version_max;      /**< Maximum protocol version supported */
} rgtp_pull_request_t;

/** Manifest (0x02) — 72 bytes on wire */
typedef struct {
    uint8_t  exposure_id[16];  /**< Exposure being described */
    uint64_t total_size;       /**< Total plaintext size in bytes */
    uint32_t chunk_count;      /**< Number of data chunks */
    uint32_t chunk_size;       /**< Bytes per data chunk */
    uint8_t  merkle_root[32];  /**< Merkle root hash (32 bytes) */
    uint8_t  fec_n;            /**< RS total symbols (0 = FEC disabled) */
    uint8_t  fec_k;            /**< RS data symbols */
    uint16_t flags;            /**< Reserved, must be 0 */
} rgtp_manifest_t;

/** Chunk_Data (0x03) — variable */
typedef struct {
    uint8_t        exposure_id[16];  /**< Exposure this chunk belongs to */
    uint32_t       chunk_index;      /**< 0-based chunk index */
    const uint8_t* payload;          /**< Pointer into receive buffer (not owned) */
    uint16_t       payload_len;      /**< Encrypted payload length in bytes */
} rgtp_chunk_data_t;

/** Chunk_Data_With_Proof (0x04) — variable */
typedef struct {
    uint8_t        exposure_id[16];
    uint32_t       chunk_index;
    uint8_t        proof_depth;      /**< Number of sibling hashes in proof */
    const uint8_t* proof;            /**< proof_depth * 32 bytes (not owned) */
    const uint8_t* payload;          /**< Encrypted payload (not owned) */
    uint16_t       payload_len;
} rgtp_chunk_with_proof_t;

/** FEC_Parity (0x05) — variable */
typedef struct {
    uint8_t        exposure_id[16];
    uint32_t       block_index;      /**< FEC block index */
    uint8_t        parity_index;     /**< Parity symbol index within block */
    uint8_t        fec_n;
    uint8_t        fec_k;
    uint8_t        reserved;
    const uint8_t* payload;          /**< Parity symbol data (not owned) */
    uint16_t       payload_len;
} rgtp_fec_parity_t;

/** NAK (0x06) — variable */
typedef struct {
    uint8_t         exposure_id[16];
    uint16_t        nak_count;       /**< Number of chunk indices in this NAK */
    const uint32_t* chunk_indices;   /**< Array of nak_count chunk indices (not owned) */
} rgtp_nak_t;

/** Rate_Report (0x07) — 40 bytes on wire */
typedef struct {
    uint8_t  exposure_id[16];
    uint32_t rtt_us;           /**< EWMA RTT estimate in microseconds */
    uint32_t loss_rate_q16;    /**< Packet loss rate as Q16 fixed-point */
    uint32_t window_size;      /**< Current pull window size */
    uint32_t reserved;
    uint64_t timestamp_us;     /**< Sender timestamp in microseconds */
} rgtp_rate_report_t;

/** Keepalive (0x08) — 28 bytes on wire */
typedef struct {
    uint8_t  exposure_id[16];
    uint64_t timestamp_us;     /**< Sender timestamp in microseconds */
    uint32_t reserved;
} rgtp_keepalive_t;

/* ── Tagged union ───────────────────────────────────────────────────────── */

/** Parsed representation of any RGTP packet. */
typedef struct rgtp_packet {
    rgtp_pkt_type_t type;
    union {
        rgtp_pull_request_t   pull_request;
        rgtp_manifest_t       manifest;
        rgtp_chunk_data_t     chunk_data;
        rgtp_chunk_with_proof_t chunk_with_proof;
        rgtp_fec_parity_t     fec_parity;
        rgtp_nak_t            nak;
        rgtp_rate_report_t    rate_report;
        rgtp_keepalive_t      keepalive;
    };
} rgtp_packet_t;

#ifdef __cplusplus
}
#endif

#endif /* RGTP_PACKET_TYPES_H */
