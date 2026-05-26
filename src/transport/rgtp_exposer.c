/**
 * @file rgtp_exposer.c
 * @brief Exposer state machine: rgtp_expose() and rgtp_poll().
 *
 * rgtp_expose():
 *  1. Generate CSPRNG Exposure_ID and 256-bit AEAD key.
 *  2. Pre-encrypt all chunks (nonce derived from chunk index).
 *  3. Build Merkle tree over plaintext chunk hashes.
 *  4. Pre-compute FEC parity chunks if enabled.
 *  5. Return fully-initialised surface (atomic: all-or-nothing).
 *
 * rgtp_poll():
 *  1. Receive pull requests via socket.
 *  2. Validate via parser; check rate limiter.
 *  3. Send Manifest on first request from a new puller.
 *  4. Serve requested chunk from immutable store.
 *  5. Include Merkle proof if requested.
 *
 * Requirements: 3.1, 3.6, 3.7, 3.8, 5.2, 5.3, 5.5, 5.6, 6.1, 7.8, 21.5, 22.1, 22.2
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
static uint64_t now_us(void)
{
#ifdef _WIN32
    FILETIME ft;
    GetSystemTimeAsFileTime(&ft);
    uint64_t t = ((uint64_t)ft.dwHighDateTime << 32) | ft.dwLowDateTime;
    return t / 10u;   /* 100ns → µs */
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000u + (uint64_t)ts.tv_nsec / 1000u;
#endif
}

/* ── Socket creation ────────────────────────────────────────────────────── */

rgtp_error_t rgtp_socket_create(const rgtp_config_t* cfg, rgtp_socket_t** out)
{
    if (out == NULL) return RGTP_ERR_INVALID_ARG;

    rgtp_socket_t* sock = (rgtp_socket_t*)rgtp_calloc(1, sizeof(rgtp_socket_t));
    if (!sock) return RGTP_ERR_NOMEM;

    if (cfg) sock->config = *cfg;

    sock->fd = (int)socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock->fd < 0) {
        rgtp_free(sock);
        return RGTP_ERR_SOCKET;
    }

    /* SO_REUSEADDR */
    int opt = 1;
#ifdef _WIN32
    setsockopt(sock->fd, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));
#else
    setsockopt(sock->fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
#endif

    /* Bind */
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port        = htons(cfg ? cfg->port : 0);

    if (bind(sock->fd, (struct sockaddr*)&addr, sizeof(addr)) != 0) {
#ifdef _WIN32
        closesocket(sock->fd);
#else
        close(sock->fd);
#endif
        rgtp_free(sock);
        return RGTP_ERR_SOCKET;
    }

    *out = sock;
    return RGTP_OK;
}

void rgtp_socket_destroy(rgtp_socket_t* sock)
{
    if (!sock) return;
#ifdef _WIN32
    closesocket(sock->fd);
#else
    close(sock->fd);
#endif
    rgtp_free(sock);
}

/* ── rgtp_expose ────────────────────────────────────────────────────────── */

rgtp_error_t rgtp_expose(rgtp_socket_t*       sock,
                          const void*          data,
                          size_t               size,
                          const rgtp_config_t* cfg,
                          rgtp_surface_t**     out_surface)
{
    if (sock == NULL || data == NULL || size == 0 || out_surface == NULL) {
        return RGTP_ERR_INVALID_ARG;
    }
    *out_surface = NULL;

    /* Determine chunk size */
    uint32_t chunk_size = cfg ? cfg->chunk_size : 0u;
    if (chunk_size == 0) {
        chunk_size = RGTP_DEFAULT_CHUNK_SIZE_UDP;
    }

    /* Calculate chunk count */
    uint32_t chunk_count = (uint32_t)((size + chunk_size - 1u) / chunk_size);
    if (chunk_count == 0) return RGTP_ERR_INVALID_ARG;

    /* Allocate surface */
    rgtp_surface_t* s = rgtp_surface_alloc_exposer(cfg, chunk_count, chunk_size, size);
    if (!s) return RGTP_ERR_NOMEM;

    s->sock = sock;
    rgtp_error_t err = RGTP_OK;

    /* Step 1: Generate CSPRNG Exposure_ID */
    err = rgtp_generate_exposure_id(s->exposure_id);
    if (err != RGTP_OK) goto fail;

    /* Step 2: Generate AEAD key */
    err = rgtp_csprng_bytes(s->key, RGTP_AEAD_KEY_BYTES);
    if (err != RGTP_OK) goto fail;

    /* Step 3: Pre-encrypt all chunks */
    {
        const uint8_t* src = (const uint8_t*)data;
        for (uint32_t i = 0; i < chunk_count; i++) {
            size_t pt_len = chunk_size;
            if ((size_t)i * chunk_size + pt_len > size) {
                pt_len = size - (size_t)i * chunk_size;
            }

            /* Allocate ciphertext buffer: plaintext + AEAD tag */
            size_t ct_max = pt_len + RGTP_AEAD_TAG_BYTES;
            s->chunks[i] = (uint8_t*)rgtp_malloc(ct_max);
            if (!s->chunks[i]) { err = RGTP_ERR_NOMEM; goto fail; }

            err = rgtp_encrypt_chunk(s->key, i,
                                     src + (size_t)i * chunk_size, pt_len,
                                     s->chunks[i], &s->chunk_sizes[i]);
            if (err != RGTP_OK) goto fail;
        }
    }

    /* Step 4: Build Merkle tree over plaintext chunks */
    {
        /* Build array of plaintext chunk pointers for Merkle */
        const uint8_t** pt_ptrs  = (const uint8_t**)rgtp_malloc(chunk_count * sizeof(uint8_t*));
        size_t*         pt_sizes = (size_t*)rgtp_malloc(chunk_count * sizeof(size_t));
        if (!pt_ptrs || !pt_sizes) {
            rgtp_free((void*)pt_ptrs);
            rgtp_free(pt_sizes);
            err = RGTP_ERR_NOMEM;
            goto fail;
        }

        const uint8_t* src = (const uint8_t*)data;
        for (uint32_t i = 0; i < chunk_count; i++) {
            pt_ptrs[i]  = src + (size_t)i * chunk_size;
            pt_sizes[i] = chunk_size;
            if ((size_t)i * chunk_size + chunk_size > size) {
                pt_sizes[i] = size - (size_t)i * chunk_size;
            }
        }

        uint8_t*** proofs_out = (cfg && cfg->merkle_proofs) ? &s->merkle_proofs : NULL;
        err = rgtp_merkle_build(pt_ptrs, pt_sizes, chunk_count,
                                s->merkle_root, proofs_out, &s->proof_depth);
        rgtp_free((void*)pt_ptrs);
        rgtp_free(pt_sizes);
        if (err != RGTP_OK) goto fail;
    }

    /* Step 5: FEC parity (if enabled) — placeholder for full FEC integration */
    if (cfg && cfg->fec_enabled && cfg->fec_k > 0 && cfg->fec_n > cfg->fec_k) {
        s->fec_enabled = true;
        s->fec_k       = cfg->fec_k;
        s->fec_n       = cfg->fec_n;
        /* Full FEC parity generation is handled in rgtp_rs_encode.c */
    }

    s->state = RGTP_SURFACE_ACTIVE;
    *out_surface = s;
    return RGTP_OK;

fail:
    rgtp_destroy_surface(s);
    return err;
}

/* ── rgtp_poll ──────────────────────────────────────────────────────────── */

rgtp_error_t rgtp_poll(rgtp_surface_t* surface, int timeout_ms)
{
    if (surface == NULL || !surface->is_exposer) {
        return RGTP_ERR_INVALID_ARG;
    }
    if (surface->state == RGTP_SURFACE_DESTROYED) {
        return RGTP_ERR_INVALID_ARG;
    }

    uint8_t recv_buf[RGTP_MAX_UDP_PAYLOAD + 4];
    uint8_t send_buf[RGTP_MAX_UDP_PAYLOAD + 128];

    struct sockaddr_storage peer_addr;
    socklen_t peer_len = sizeof(peer_addr);

    /* Set socket receive timeout */
#ifdef _WIN32
    DWORD tv_ms = (timeout_ms < 0) ? 0 : (DWORD)timeout_ms;
    setsockopt(surface->sock->fd, SOL_SOCKET, SO_RCVTIMEO,
               (const char*)&tv_ms, sizeof(tv_ms));
#else
    struct timeval tv;
    tv.tv_sec  = (timeout_ms < 0) ? 0 : timeout_ms / 1000;
    tv.tv_usec = (timeout_ms < 0) ? 0 : (timeout_ms % 1000) * 1000;
    setsockopt(surface->sock->fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
#endif

    ssize_t n = recvfrom(surface->sock->fd,
                         (char*)recv_buf, sizeof(recv_buf), 0,
                         (struct sockaddr*)&peer_addr, &peer_len);
    if (n <= 0) {
        /* Timeout or transient error — not fatal */
        return RGTP_OK;
    }

    /* Parse the incoming packet */
    rgtp_packet_t pkt;
    rgtp_error_t err = rgtp_parse_packet(recv_buf, (size_t)n,
                                          surface->chunk_count, &pkt);
    if (err != RGTP_OK) {
        atomic_fetch_add(&surface->malformed_packets, 1u);
        return RGTP_OK;   /* discard malformed packet, keep polling */
    }

    if (pkt.type != RGTP_PKT_PULL_REQUEST) {
        return RGTP_OK;   /* ignore non-pull packets on exposer */
    }

    /* Validate Exposure_ID */
    if (memcmp(pkt.pull_request.exposure_id, surface->exposure_id, 16) != 0) {
        return RGTP_OK;
    }

    /* Rate limit check */
    uint32_t source_ip = 0;
    if (peer_addr.ss_family == AF_INET) {
        source_ip = ntohl(((struct sockaddr_in*)&peer_addr)->sin_addr.s_addr);
    }
    err = rgtp_ratelimit_check(&surface->ratelimit, surface->exposure_id,
                                source_ip, now_us());
    if (err == RGTP_ERR_RATE_LIMITED) {
        return RGTP_OK;   /* silently drop — no response */
    }

    /* Record pull pressure */
    rgtp_flow_record_pull(&surface->flow, now_us());

    /* Send Manifest first (always — stateless exposer sends it on every pull) */
    {
        rgtp_packet_t manifest_pkt;
        manifest_pkt.type = RGTP_PKT_MANIFEST;
        memcpy(manifest_pkt.manifest.exposure_id, surface->exposure_id, 16);
        manifest_pkt.manifest.total_size  = surface->total_size;
        manifest_pkt.manifest.chunk_count = surface->chunk_count;
        manifest_pkt.manifest.chunk_size  = surface->chunk_size;
        memcpy(manifest_pkt.manifest.merkle_root, surface->merkle_root, 32);
        manifest_pkt.manifest.fec_n = surface->fec_n;
        manifest_pkt.manifest.fec_k = surface->fec_k;
        manifest_pkt.manifest.flags = 0;

        size_t manifest_len = 0;
        if (rgtp_serialize_packet(&manifest_pkt, send_buf, sizeof(send_buf),
                                   &manifest_len) == RGTP_OK) {
            sendto(surface->sock->fd, (const char*)send_buf, (int)manifest_len, 0,
                   (const struct sockaddr*)&peer_addr, peer_len);
        }
    }

    /* Serve chunks from the sliding window */
    uint32_t window_size = pkt.pull_request.window_size;
    if (window_size == 0) window_size = 1u;
    if (window_size > surface->chunk_count) window_size = surface->chunk_count;

    bool want_proof = (pkt.pull_request.flags & RGTP_PULL_FLAG_WANT_PROOF) != 0;

    for (uint32_t w = 0; w < window_size; w++) {
        /* The pull request window_size encodes "I want the next N chunks
         * starting from the first unreceived chunk" — for a stateless exposer
         * we serve chunk indices 0..window_size-1 on the first request.
         * In a real deployment the puller would send specific chunk indices;
         * this simplified version serves sequentially. */
        uint32_t idx = w;
        if (idx >= surface->chunk_count) break;
        if (!surface->chunks[idx]) continue;

        rgtp_packet_t chunk_pkt;
        size_t out_len = 0;

        if (want_proof && surface->merkle_proofs && surface->merkle_proofs[idx]) {
            chunk_pkt.type = RGTP_PKT_CHUNK_DATA_WITH_PROOF;
            memcpy(chunk_pkt.chunk_with_proof.exposure_id, surface->exposure_id, 16);
            chunk_pkt.chunk_with_proof.chunk_index  = idx;
            chunk_pkt.chunk_with_proof.proof_depth  = (uint8_t)surface->proof_depth;
            chunk_pkt.chunk_with_proof.proof        = surface->merkle_proofs[idx];
            chunk_pkt.chunk_with_proof.payload      = surface->chunks[idx];
            chunk_pkt.chunk_with_proof.payload_len  = (uint16_t)surface->chunk_sizes[idx];
        } else {
            chunk_pkt.type = RGTP_PKT_CHUNK_DATA;
            memcpy(chunk_pkt.chunk_data.exposure_id, surface->exposure_id, 16);
            chunk_pkt.chunk_data.chunk_index = idx;
            chunk_pkt.chunk_data.payload     = surface->chunks[idx];
            chunk_pkt.chunk_data.payload_len = (uint16_t)surface->chunk_sizes[idx];
        }

        if (rgtp_serialize_packet(&chunk_pkt, send_buf, sizeof(send_buf),
                                   &out_len) == RGTP_OK) {
            sendto(surface->sock->fd, (const char*)send_buf, (int)out_len, 0,
                   (const struct sockaddr*)&peer_addr, peer_len);
            atomic_fetch_add(&surface->bytes_sent, out_len);
            atomic_fetch_add(&surface->chunks_sent, 1u);
        }
    }

    return RGTP_OK;
}
