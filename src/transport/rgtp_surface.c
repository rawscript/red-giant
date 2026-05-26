/**
 * @file rgtp_surface.c
 * @brief Surface lifecycle: create, destroy, stats, progress, exposure ID.
 *
 * Key invariants:
 *  - rgtp_destroy_surface() is safe to call with NULL.
 *  - All heap fields are freed in reverse allocation order (no double-free).
 *  - Key material is zeroized before freeing.
 *  - No global mutable state — all state lives in the surface struct.
 *
 * Requirements: 2.4, 2.8, 2.9, 5.1, 5.4, 5.7
 */

#include "rgtp_surface_internal.h"
#include "../core/rgtp_alloc_internal.h"
#include "../crypto/rgtp_crypto_internal.h"

#include <string.h>
#include <stdatomic.h>

/* ── Internal helpers ───────────────────────────────────────────────────── */

/** Allocate a zeroed surface using the provided allocator. */
static rgtp_surface_t* surface_alloc(const rgtp_allocator_t* alloc)
{
    rgtp_surface_t* s;
    if (alloc && alloc->alloc) {
        s = (rgtp_surface_t*)alloc->alloc(sizeof(rgtp_surface_t), alloc->ctx);
    } else {
        s = (rgtp_surface_t*)rgtp_calloc(1, sizeof(rgtp_surface_t));
    }
    if (s) {
        memset(s, 0, sizeof(rgtp_surface_t));
    }
    return s;
}

static void surface_free_ptr(rgtp_surface_t* s, void* ptr)
{
    if (!ptr) return;
    if (s->alloc.free) {
        s->alloc.free(ptr, s->alloc.ctx);
    } else {
        rgtp_free(ptr);
    }
}

static void* surface_malloc(rgtp_surface_t* s, size_t sz)
{
    if (s->alloc.alloc) {
        return s->alloc.alloc(sz, s->alloc.ctx);
    }
    return rgtp_malloc(sz);
}

static void* surface_calloc(rgtp_surface_t* s, size_t n, size_t sz)
{
    void* p = surface_malloc(s, n * sz);
    if (p) memset(p, 0, n * sz);
    return p;
}

/* ── Public: destroy ────────────────────────────────────────────────────── */

void rgtp_destroy_surface(rgtp_surface_t* surface)
{
    if (surface == NULL) return;
    if (surface->state == RGTP_SURFACE_DESTROYED) return;

    surface->state = RGTP_SURFACE_DESTROYED;

    /* Zeroize key material FIRST, before freeing any memory */
    rgtp_zeroize(surface->key, sizeof(surface->key));

    /* Free exposer chunk store */
    if (surface->chunks) {
        for (uint32_t i = 0; i < surface->chunk_count; i++) {
            surface_free_ptr(surface, surface->chunks[i]);
        }
        surface_free_ptr(surface, surface->chunks);
        surface->chunks = NULL;
    }
    if (surface->chunk_sizes) {
        surface_free_ptr(surface, surface->chunk_sizes);
        surface->chunk_sizes = NULL;
    }

    /* Free pre-computed Merkle proofs */
    if (surface->merkle_proofs) {
        for (uint32_t i = 0; i < surface->chunk_count; i++) {
            surface_free_ptr(surface, surface->merkle_proofs[i]);
        }
        surface_free_ptr(surface, surface->merkle_proofs);
        surface->merkle_proofs = NULL;
    }

    /* Free FEC parity chunks */
    if (surface->fec_parity) {
        uint32_t parity_count = (surface->fec_n > surface->fec_k)
                                ? (uint32_t)(surface->fec_n - surface->fec_k)
                                : 0u;
        for (uint32_t i = 0; i < parity_count; i++) {
            surface_free_ptr(surface, surface->fec_parity[i]);
        }
        surface_free_ptr(surface, surface->fec_parity);
        surface->fec_parity = NULL;
    }

    /* Free puller receive buffers */
    if (surface->recv_chunks) {
        for (uint32_t i = 0; i < surface->chunk_count; i++) {
            surface_free_ptr(surface, surface->recv_chunks[i]);
        }
        surface_free_ptr(surface, surface->recv_chunks);
        surface->recv_chunks = NULL;
    }
    if (surface->recv_sizes) {
        surface_free_ptr(surface, surface->recv_sizes);
        surface->recv_sizes = NULL;
    }
    if (surface->recv_bitmap) {
        surface_free_ptr(surface, surface->recv_bitmap);
        surface->recv_bitmap = NULL;
    }

    /* Free the surface struct itself */
    rgtp_allocator_t alloc = surface->alloc;
    if (alloc.free) {
        alloc.free(surface, alloc.ctx);
    } else {
        rgtp_free(surface);
    }
}

/* ── Public: get_exposure_id ────────────────────────────────────────────── */

rgtp_error_t rgtp_get_exposure_id(const rgtp_surface_t* surface,
                                   uint8_t out_id[16])
{
    if (surface == NULL || out_id == NULL) {
        return RGTP_ERR_INVALID_ARG;
    }
    memcpy(out_id, surface->exposure_id, 16);
    return RGTP_OK;
}

/* ── Public: progress ───────────────────────────────────────────────────── */

float rgtp_progress(const rgtp_surface_t* surface)
{
    if (surface == NULL || surface->is_exposer || surface->chunk_count == 0) {
        return 0.0f;
    }
    return (float)surface->chunks_received / (float)surface->chunk_count;
}

/* ── Public: get_stats ──────────────────────────────────────────────────── */

rgtp_error_t rgtp_get_stats(const rgtp_surface_t* surface, rgtp_stats_t* out)
{
    if (surface == NULL || out == NULL) {
        return RGTP_ERR_INVALID_ARG;
    }
    out->bytes_sent        = atomic_load(&surface->bytes_sent);
    out->bytes_received    = atomic_load(&surface->bytes_received);
    out->chunks_sent       = atomic_load(&surface->chunks_sent);
    out->chunks_received   = surface->chunks_received;
    out->auth_failures     = atomic_load(&surface->auth_failures);
    out->malformed_packets = atomic_load(&surface->malformed_packets);
    out->fec_recoveries    = atomic_load(&surface->fec_recoveries);
    out->nak_sent          = atomic_load(&surface->nak_sent);
    out->packet_loss_rate  = surface->flow.loss_rate;
    out->rtt_us            = surface->flow.rtt_us;
    out->pull_pressure     = surface->flow.pull_pressure;
    return RGTP_OK;
}

/* ── Public: get_latency_stats ──────────────────────────────────────────── */

rgtp_error_t rgtp_get_latency_stats(const rgtp_surface_t* surface,
                                     rgtp_latency_stats_t* out)
{
    if (surface == NULL || out == NULL) {
        return RGTP_ERR_INVALID_ARG;
    }

    uint32_t count = surface->latency_count;
    if (count == 0) {
        memset(out, 0, sizeof(*out));
        return RGTP_OK;
    }

    /* Compute mean, min, max */
    uint64_t sum = 0;
    uint32_t min_us = UINT32_MAX, max_us = 0;
    uint32_t n = (count < 256u) ? count : 256u;

    for (uint32_t i = 0; i < n; i++) {
        uint32_t v = surface->latency_samples[i];
        sum += v;
        if (v < min_us) min_us = v;
        if (v > max_us) max_us = v;
    }
    out->mean_us     = (uint32_t)(sum / n);
    out->min_us      = min_us;
    out->max_us      = max_us;
    out->sample_count = n;

    /* Jitter: mean absolute deviation from mean */
    uint64_t jitter_sum = 0;
    for (uint32_t i = 0; i < n; i++) {
        uint32_t v = surface->latency_samples[i];
        uint32_t diff = (v > out->mean_us) ? (v - out->mean_us)
                                           : (out->mean_us - v);
        jitter_sum += diff;
    }
    out->jitter_us = (uint32_t)(jitter_sum / n);

    /* p99: sort a copy and pick the 99th percentile */
    uint32_t sorted[256];
    memcpy(sorted, surface->latency_samples, n * sizeof(uint32_t));
    /* Simple insertion sort — n <= 256 */
    for (uint32_t i = 1; i < n; i++) {
        uint32_t key = sorted[i];
        int j = (int)i - 1;
        while (j >= 0 && sorted[j] > key) {
            sorted[j + 1] = sorted[j];
            j--;
        }
        sorted[j + 1] = key;
    }
    uint32_t p99_idx = (uint32_t)((n * 99u) / 100u);
    if (p99_idx >= n) p99_idx = n - 1u;
    out->p99_us = sorted[p99_idx];

    return RGTP_OK;
}

/* ── Internal: surface_alloc_exposer ────────────────────────────────────── */

rgtp_surface_t* rgtp_surface_alloc_exposer(const rgtp_config_t* cfg,
                                            uint32_t chunk_count,
                                            uint32_t chunk_size,
                                            uint64_t total_size)
{
    const rgtp_allocator_t* alloc = cfg ? &cfg->allocator : NULL;
    rgtp_surface_t* s = surface_alloc(alloc);
    if (!s) return NULL;

    if (alloc) s->alloc = *alloc;
    if (cfg)   s->config = *cfg;

    s->is_exposer  = true;
    s->state       = RGTP_SURFACE_INITIALIZING;
    s->chunk_count = chunk_count;
    s->chunk_size  = chunk_size;
    s->total_size  = total_size;

    /* Allocate chunk store arrays */
    s->chunks = (uint8_t**)surface_calloc(s, chunk_count, sizeof(uint8_t*));
    if (!s->chunks) goto fail;

    s->chunk_sizes = (size_t*)surface_calloc(s, chunk_count, sizeof(size_t));
    if (!s->chunk_sizes) goto fail;

    /* Initialise rate limiter and flow control */
    rgtp_ratelimit_init(&s->ratelimit);
    rgtp_flow_init(&s->flow, cfg ? cfg->window_size : 64u);

    return s;

fail:
    rgtp_destroy_surface(s);
    return NULL;
}

/* ── Internal: surface_alloc_puller ─────────────────────────────────────── */

rgtp_surface_t* rgtp_surface_alloc_puller(const rgtp_config_t* cfg,
                                           uint32_t chunk_count,
                                           uint32_t chunk_size,
                                           uint64_t total_size)
{
    const rgtp_allocator_t* alloc = cfg ? &cfg->allocator : NULL;
    rgtp_surface_t* s = surface_alloc(alloc);
    if (!s) return NULL;

    if (alloc) s->alloc = *alloc;
    if (cfg)   s->config = *cfg;

    s->is_exposer  = false;
    s->state       = RGTP_SURFACE_INITIALIZING;
    s->chunk_count = chunk_count;
    s->chunk_size  = chunk_size;
    s->total_size  = total_size;

    /* Allocate receive buffers */
    s->recv_chunks = (uint8_t**)surface_calloc(s, chunk_count, sizeof(uint8_t*));
    if (!s->recv_chunks) goto fail;

    s->recv_sizes = (size_t*)surface_calloc(s, chunk_count, sizeof(size_t));
    if (!s->recv_sizes) goto fail;

    /* Bitmap: 1 bit per chunk, rounded up to bytes */
    uint32_t bitmap_bytes = (chunk_count + 7u) / 8u;
    s->recv_bitmap = (uint8_t*)surface_calloc(s, bitmap_bytes, 1);
    if (!s->recv_bitmap) goto fail;

    /* Initialise anti-replay window and flow control */
    rgtp_replay_window_init(&s->replay);
    rgtp_flow_init(&s->flow, cfg ? cfg->window_size : 64u);

    return s;

fail:
    rgtp_destroy_surface(s);
    return NULL;
}
