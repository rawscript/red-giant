/**
 * @file rgtp_surface_internal.h
 * @brief Internal definition of rgtp_surface_s — the central data structure.
 *
 * This header is NOT installed. Callers hold an opaque rgtp_surface_t* pointer.
 * The full struct is only visible inside librgtp.
 */

#ifndef RGTP_SURFACE_INTERNAL_H
#define RGTP_SURFACE_INTERNAL_H

#include "rgtp/rgtp.h"
#include "rgtp_transport_internal.h"
#include "../wire/rgtp_packet_types.h"

#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#ifdef _WIN32
#  include <winsock2.h>
#  include <ws2tcpip.h>
#else
#  include <sys/socket.h>
#  include <netinet/in.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* ── Surface state machine ──────────────────────────────────────────────── */
typedef enum {
    RGTP_SURFACE_INITIALIZING = 0,
    RGTP_SURFACE_ACTIVE       = 1,
    RGTP_SURFACE_DEGRADED     = 2,
    RGTP_SURFACE_DESTROYED    = 3,
} rgtp_surface_state_t;

/* ── Internal surface struct ────────────────────────────────────────────── */
struct rgtp_surface_s {
    /* ── Identity ──────────────────────────────────────────────────────── */
    uint8_t          exposure_id[16];     /* 128-bit CSPRNG ID */
    uint64_t         total_size;          /* plaintext bytes */
    uint32_t         chunk_count;         /* ceil(total_size / chunk_size) */
    uint32_t         chunk_size;          /* bytes per chunk (plaintext) */

    /* ── Role ──────────────────────────────────────────────────────────── */
    bool             is_exposer;
    rgtp_surface_state_t state;

    /* ── Crypto ────────────────────────────────────────────────────────── */
    uint8_t          key[32];             /* AEAD key — zeroized on destroy */
    uint8_t          merkle_root[32];     /* BLAKE2b-256 or SHA-256 root */

    /* ── Immutable chunk store (exposer only) ──────────────────────────── */
    uint8_t**        chunks;              /* array[chunk_count] of encrypted blobs */
    size_t*          chunk_sizes;         /* encrypted size per chunk */
    uint8_t**        merkle_proofs;       /* optional pre-computed proofs */
    uint32_t         proof_depth;         /* log2(padded chunk count) */

    /* ── Puller receive state ───────────────────────────────────────────── */
    uint8_t**        recv_chunks;         /* received plaintext chunks */
    size_t*          recv_sizes;
    uint8_t*         recv_bitmap;         /* bit i = chunk i received */
    uint32_t         chunks_received;

    /* ── Sliding window (puller) ────────────────────────────────────────── */
    uint32_t         window_base;         /* first unreceived chunk */

    /* ── Anti-replay window (puller) ────────────────────────────────────── */
    rgtp_replay_window_t replay;

    /* ── Flow control (puller) ──────────────────────────────────────────── */
    rgtp_flow_t      flow;

    /* ── Rate limiter (exposer) ─────────────────────────────────────────── */
    rgtp_ratelimit_t ratelimit;

    /* ── FEC state ──────────────────────────────────────────────────────── */
    bool             fec_enabled;
    uint8_t          fec_k;
    uint8_t          fec_n;
    uint8_t**        fec_parity;          /* parity chunks (exposer) */

    /* ── I/O ────────────────────────────────────────────────────────────── */
    rgtp_socket_t*   sock;
    struct sockaddr_storage peer;

    /* ── Statistics (atomic for thread safety) ──────────────────────────── */
    _Atomic uint64_t bytes_sent;
    _Atomic uint64_t bytes_received;
    _Atomic uint32_t chunks_sent;
    _Atomic uint32_t auth_failures;
    _Atomic uint32_t malformed_packets;
    _Atomic uint32_t fec_recoveries;
    _Atomic uint32_t nak_sent;

    /* ── Latency tracking (puller) ──────────────────────────────────────── */
    uint32_t         latency_samples[256];
    uint32_t         latency_head;
    uint32_t         latency_count;

    /* ── Allocator ──────────────────────────────────────────────────────── */
    rgtp_allocator_t alloc;

    /* ── Config snapshot ────────────────────────────────────────────────── */
    rgtp_config_t    config;
};

/* ── Internal socket struct (opaque to callers) ─────────────────────────── */
struct rgtp_socket_s {
    int      fd;                          /* OS socket file descriptor */
    bool     raw_ethernet;
    uint16_t port;
    rgtp_config_t config;
};

#ifdef __cplusplus
}
#endif

#endif /* RGTP_SURFACE_INTERNAL_H */
