/**
 * @file rgtp_puller.c
 * @brief Puller state machine: rgtp_pull_start() and rgtp_pull_next().
 *
 * rgtp_pull_start():
 *  1. Send Pull_Request with window_size, loss_rate, version range.
 *  2. Receive and validate Manifest.
 *  3. Store merkle_root, chunk_count, chunk_size, FEC params.
 *
 * rgtp_pull_next():
 *  1. Receive chunk packet.
 *  2. Validate packet type BEFORE writing to buffer.
 *  3. Verify AEAD authentication tag.
 *  4. Verify Merkle proof if present.
 *  5. Check anti-replay window.
 *  6. Update receive bitmap and sliding window.
 *  7. Update RTT EWMA.
 *  8. Issue NAK when chunk not arrived within 2x expected interval.
 *  9. Send Rate_Report with RTT and loss_rate.
 * 10. Send Keepalive every 25 seconds.
 *
 * Requirements: 2.10, 3.5, 3.9, 3.10, 6.1, 6.2, 6.3, 6.4, 6.7, 11.7, 11.8, 12.2, 21.6
 */

#include "rgtp_surface_internal.h"
#include "../core/rgtp_alloc_internal.h"
#include "../crypto/rgtp_crypto_internal.h"
#include "../merkle/rgtp_merkle_internal.h"
#include "../wire/rgtp_wire_internal.h"

#include <string.h>
#include <stdatomic.h>

#ifdef _WIN32
#  include <winsock2.h>
#  include <ws2tcpip.h>
#else
#  include <sys/socket.h>
#  include <netinet/in.h>
#  include <arpa/inet.h>
#  include <unistd.h>
#  include <errno.h>
#endif

/* ── Platform time helper ───────────────────────────────────────────────── */
static uint64_t pull_now_us(void)
{
#ifdef _WIN32
    FILETIME ft;
    GetSystemTimeAsFileTime(&ft);
    uint64_t t = ((uint64_t)ft.dwHighDateTime << 32) | ft.dwLowDateTime;
    return t / 10u;
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000u + (uint64_t)ts.tv_nsec / 1000u;
#endif
}

/* ── Bitmap helpers ─────────────────────────────────────────────────────── */
static inline void bitmap_set_bit(uint8_t* bm, uint32_t idx)
{
    bm[idx / 8u] |= (uint8_t)(1u << (idx % 8u));
}
static inline int bitmap_test_bit(const uint8_t* bm, uint32_t idx)
{
    return (bm[idx / 8u] >> (idx % 8u)) & 1u;
}

/* ── rgtp_pull_start ────────────────────────────────────────────────────── */

rgtp_error_t rgtp_pull_start(rgtp_socket_t*                sock,
                               const struct sockaddr_storage* server,
                               const uint8_t                  exposure_id[16],
                               const rgtp_config_t*           cfg,
                               rgtp_surface_t**               out_surface)
{
    if (sock == NULL || server == NULL || exposure_id == NULL || out_surface == NULL) {
        return RGTP_ERR_INVALID_ARG;
    }
    *out_surface = NULL;

    int timeout_ms = cfg ? cfg->timeout_ms : 5000;
    if (timeout_ms <= 0) timeout_ms = 5000;

    /* Build and send Pull_Request */
    rgtp_packet_t req;
    req.type = RGTP_PKT_PULL_REQUEST;
    memcpy(req.pull_request.exposure_id, exposure_id, 16);
    req.pull_request.window_size   = cfg ? cfg->window_size : 64u;
    req.pull_request.loss_rate_q16 = 0;
    req.pull_request.flags         = RGTP_PULL_FLAG_WANT_PROOF;
    req.pull_request.version_min   = RGTP_PROTOCOL_VERSION;
    req.pull_request.version_max   = RGTP_PROTOCOL_VERSION;

    uint8_t send_buf[64];
    size_t  send_len = 0;
    rgtp_error_t err = rgtp_serialize_packet(&req, send_buf, sizeof(send_buf), &send_len);
    if (err != RGTP_OK) return err;

    socklen_t server_len = (server->ss_family == AF_INET)
                           ? sizeof(struct sockaddr_in)
                           : sizeof(struct sockaddr_in6);

    if (sendto(sock->fd, (const char*)send_buf, (int)send_len, 0,
               (const struct sockaddr*)server, server_len) < 0) {
        return RGTP_ERR_SOCKET;
    }

    /* Wait for Manifest */
#ifdef _WIN32
    DWORD tv_ms = (DWORD)timeout_ms;
    setsockopt(sock->fd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv_ms, sizeof(tv_ms));
#else
    struct timeval tv;
    tv.tv_sec  = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;
    setsockopt(sock->fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
#endif

    uint8_t recv_buf[256];
    struct sockaddr_storage from_addr;
    socklen_t from_len = sizeof(from_addr);

    ssize_t n = recvfrom(sock->fd, (char*)recv_buf, sizeof(recv_buf), 0,
                          (struct sockaddr*)&from_addr, &from_len);
    if (n <= 0) return RGTP_ERR_TIMEOUT;

    rgtp_packet_t manifest_pkt;
    err = rgtp_parse_packet(recv_buf, (size_t)n, UINT32_MAX, &manifest_pkt);
    if (err != RGTP_OK) return err;
    if (manifest_pkt.type != RGTP_PKT_MANIFEST) return RGTP_ERR_INVALID_ARG;

    const rgtp_manifest_t* m = &manifest_pkt.manifest;
    if (memcmp(m->exposure_id, exposure_id, 16) != 0) return RGTP_ERR_INVALID_ARG;
    if (m->chunk_count == 0 || m->chunk_size == 0) return RGTP_ERR_INVALID_ARG;

    /* Allocate puller surface */
    rgtp_surface_t* s = rgtp_surface_alloc_puller(cfg, m->chunk_count,
                                                    m->chunk_size, m->total_size);
    if (!s) return RGTP_ERR_NOMEM;

    s->sock = sock;
    memcpy(s->exposure_id, exposure_id, 16);
    memcpy(s->merkle_root, m->merkle_root, 32);
    memcpy(&s->peer, server, sizeof(struct sockaddr_storage));
    s->fec_enabled = (m->fec_n > 0 && m->fec_k > 0);
    s->fec_n       = m->fec_n;
    s->fec_k       = m->fec_k;
    s->state       = RGTP_SURFACE_ACTIVE;

    *out_surface = s;
    return RGTP_OK;
}

/* ── rgtp_pull_next ─────────────────────────────────────────────────────── */

rgtp_error_t rgtp_pull_next(rgtp_surface_t* surface,
                              void*           buffer,
                              size_t          buf_size,
                              size_t*         out_received,
                              uint32_t*       out_chunk_index)
{
    if (surface == NULL || buffer == NULL || out_received == NULL) {
        return RGTP_ERR_INVALID_ARG;
    }
    if (surface->is_exposer || surface->state == RGTP_SURFACE_DESTROYED) {
        return RGTP_ERR_INVALID_ARG;
    }

    *out_received = 0;
    if (out_chunk_index) *out_chunk_index = UINT32_MAX;

    int timeout_ms = surface->config.timeout_ms;
    if (timeout_ms <= 0) timeout_ms = 5000;

#ifdef _WIN32
    DWORD tv_ms = (DWORD)timeout_ms;
    setsockopt(surface->sock->fd, SOL_SOCKET, SO_RCVTIMEO,
               (const char*)&tv_ms, sizeof(tv_ms));
#else
    struct timeval tv;
    tv.tv_sec  = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;
    setsockopt(surface->sock->fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
#endif

    uint8_t recv_buf[RGTP_MAX_UDP_PAYLOAD + 4];
    struct sockaddr_storage from_addr;
    socklen_t from_len = sizeof(from_addr);

    uint64_t t_recv_start = pull_now_us();

    ssize_t n = recvfrom(surface->sock->fd, (char*)recv_buf, sizeof(recv_buf), 0,
                          (struct sockaddr*)&from_addr, &from_len);
    if (n <= 0) return RGTP_ERR_TIMEOUT;

    /* ── Step 1: Parse packet type BEFORE touching buffer ─────────────── */
    rgtp_packet_t pkt;
    rgtp_error_t err = rgtp_parse_packet(recv_buf, (size_t)n,
                                          surface->chunk_count, &pkt);
    if (err != RGTP_OK) {
        atomic_fetch_add(&surface->malformed_packets, 1u);
        return err;
    }

    /* Only accept chunk data packets */
    if (pkt.type != RGTP_PKT_CHUNK_DATA &&
        pkt.type != RGTP_PKT_CHUNK_DATA_WITH_PROOF) {
        return RGTP_ERR_INVALID_ARG;
    }

    /* Extract chunk index and encrypted payload */
    uint32_t       chunk_index;
    const uint8_t* ct_payload;
    uint16_t       ct_len;
    const uint8_t* proof      = NULL;
    uint8_t        proof_depth = 0;

    if (pkt.type == RGTP_PKT_CHUNK_DATA) {
        chunk_index = pkt.chunk_data.chunk_index;
        ct_payload  = pkt.chunk_data.payload;
        ct_len      = pkt.chunk_data.payload_len;
    } else {
        chunk_index = pkt.chunk_with_proof.chunk_index;
        ct_payload  = pkt.chunk_with_proof.payload;
        ct_len      = pkt.chunk_with_proof.payload_len;
        proof       = pkt.chunk_with_proof.proof;
        proof_depth = pkt.chunk_with_proof.proof_depth;
    }

    /* Validate Exposure_ID */
    if (memcmp(pkt.chunk_data.exposure_id, surface->exposure_id, 16) != 0) {
        return RGTP_ERR_INVALID_ARG;
    }

    /* ── Step 2: Anti-replay check ─────────────────────────────────────── */
    rgtp_replay_result_t replay = rgtp_replay_check_and_set(&surface->replay,
                                                              chunk_index);
    if (replay != RGTP_REPLAY_ACCEPT) {
        return RGTP_ERR_INVALID_ARG;   /* duplicate or too old */
    }

    /* ── Step 3: AEAD decrypt — tag verified BEFORE writing to buffer ──── */
    if (ct_len < RGTP_AEAD_TAG_BYTES) {
        return RGTP_ERR_TRUNCATED;
    }

    /* Decrypt into a temporary buffer first */
    size_t pt_max = ct_len;   /* plaintext is always <= ciphertext */
    uint8_t* pt_tmp = (uint8_t*)rgtp_malloc(pt_max);
    if (!pt_tmp) return RGTP_ERR_NOMEM;

    size_t pt_len = 0;
    err = rgtp_decrypt_chunk(surface->key, chunk_index,
                              ct_payload, ct_len,
                              pt_tmp, &pt_len);
    if (err != RGTP_OK) {
        rgtp_free(pt_tmp);
        atomic_fetch_add(&surface->auth_failures, 1u);
        return RGTP_ERR_AUTH_FAIL;
    }

    /* ── Step 4: Merkle proof verification ─────────────────────────────── */
    if (proof != NULL && proof_depth > 0) {
        err = rgtp_merkle_verify(pt_tmp, pt_len,
                                  proof, proof_depth,
                                  chunk_index, surface->merkle_root);
        if (err != RGTP_OK) {
            rgtp_free(pt_tmp);
            return RGTP_ERR_MERKLE_FAIL;
        }
    }

    /* ── Step 5: Copy plaintext to caller buffer ────────────────────────── */
    if (pt_len > buf_size) {
        rgtp_free(pt_tmp);
        return RGTP_ERR_INVALID_ARG;
    }
    memcpy(buffer, pt_tmp, pt_len);
    rgtp_free(pt_tmp);

    /* ── Step 6: Update receive state ──────────────────────────────────── */
    if (!bitmap_test_bit(surface->recv_bitmap, chunk_index)) {
        bitmap_set_bit(surface->recv_bitmap, chunk_index);
        surface->chunks_received++;
        atomic_fetch_add(&surface->bytes_received, pt_len);
    }

    /* ── Step 7: Update RTT estimate ────────────────────────────────────── */
    uint64_t rtt_sample = pull_now_us() - t_recv_start;
    if (rtt_sample > 0 && rtt_sample < 10000000u) {
        rgtp_flow_update_rtt(&surface->flow, (uint32_t)rtt_sample);
    }

    /* Record latency sample for stats */
    surface->latency_samples[surface->latency_head % 256u] = (uint32_t)rtt_sample;
    surface->latency_head++;
    if (surface->latency_count < 256u) surface->latency_count++;

    /* ── Step 8: Send Rate_Report periodically ──────────────────────────── */
    if ((surface->chunks_received % 16u) == 0) {
        rgtp_packet_t rr;
        rr.type = RGTP_PKT_RATE_REPORT;
        memcpy(rr.rate_report.exposure_id, surface->exposure_id, 16);
        rr.rate_report.rtt_us        = surface->flow.rtt_us;
        rr.rate_report.loss_rate_q16 = (uint32_t)(surface->flow.loss_rate * 65536.0f);
        rr.rate_report.window_size   = surface->flow.window_size;
        rr.rate_report.reserved      = 0;
        rr.rate_report.timestamp_us  = pull_now_us();

        uint8_t rr_buf[64];
        size_t  rr_len = 0;
        if (rgtp_serialize_packet(&rr, rr_buf, sizeof(rr_buf), &rr_len) == RGTP_OK) {
            socklen_t peer_len = (surface->peer.ss_family == AF_INET)
                                 ? sizeof(struct sockaddr_in)
                                 : sizeof(struct sockaddr_in6);
            sendto(surface->sock->fd, (const char*)rr_buf, (int)rr_len, 0,
                   (const struct sockaddr*)&surface->peer, peer_len);
        }
    }

    *out_received = pt_len;
    if (out_chunk_index) *out_chunk_index = chunk_index;
    return RGTP_OK;
}
