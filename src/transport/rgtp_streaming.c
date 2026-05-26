/**
 * @file rgtp_streaming.c
 * @brief Sliding-window exposure, partial pull, and out-of-order delivery.
 *
 * Tasks 11.1, 11.2, 11.3
 *
 * Sliding-window exposure (11.1):
 *  - rgtp_expose_sliding_window(): continuously append new chunks; advance
 *    window; evict oldest chunks; zeroize evicted chunk memory before freeing.
 *  - Puller discards pull requests for evicted chunks (chunk_index < window_base).
 *
 * Partial pull (11.2):
 *  - rgtp_pull_range(): puller requests byte range [offset, offset+length).
 *  - Exposer computes minimal covering chunk set and serves only those chunks.
 *
 * Out-of-order delivery tolerance (11.3):
 *  - rgtp_pull_next_ordered(): delivers chunks to the application in order,
 *    buffering up to max_out_of_order chunks ahead of the lowest undelivered.
 *
 * Requirements: 20.1, 20.2, 20.3, 20.4, 20.5, 20.6, 20.7
 */

#include "rgtp_surface_internal.h"
#include "../core/rgtp_alloc_internal.h"
#include "../crypto/rgtp_crypto_internal.h"

#include <string.h>
#include <stdatomic.h>

/* ═══════════════════════════════════════════════════════════════════════════
 * 11.1 — Sliding-window exposure
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Append a new chunk to a sliding-window Exposure.
 *
 * Encrypts the new chunk, appends it to the chunk store, and evicts the
 * oldest chunk if the window is full. Evicted chunk memory is zeroized
 * before freeing.
 *
 * @param surface      An exposer surface created by rgtp_expose().
 * @param chunk_data   Plaintext data for the new chunk.
 * @param chunk_len    Length of @p chunk_data in bytes.
 * @param window_size  Maximum number of chunks to keep in the window.
 * @return RGTP_OK, RGTP_ERR_NOMEM, or RGTP_ERR_ENCRYPT.
 */
rgtp_error_t rgtp_expose_append_chunk(rgtp_surface_t* surface,
                                       const uint8_t*  chunk_data,
                                       size_t          chunk_len,
                                       uint32_t        window_size)
{
    if (surface == NULL || chunk_data == NULL || chunk_len == 0) {
        return RGTP_ERR_INVALID_ARG;
    }
    if (!surface->is_exposer || surface->state != RGTP_SURFACE_ACTIVE) {
        return RGTP_ERR_INVALID_ARG;
    }

    /* Determine the new chunk index */
    uint32_t new_idx = surface->chunk_count;

    /* Grow chunk store arrays if needed */
    uint8_t** new_chunks = (uint8_t**)rgtp_malloc(
        (size_t)(new_idx + 1u) * sizeof(uint8_t*));
    size_t* new_sizes = (size_t*)rgtp_malloc(
        (size_t)(new_idx + 1u) * sizeof(size_t));
    if (!new_chunks || !new_sizes) {
        rgtp_free(new_chunks);
        rgtp_free(new_sizes);
        return RGTP_ERR_NOMEM;
    }

    if (surface->chunks) {
        memcpy(new_chunks, surface->chunks, new_idx * sizeof(uint8_t*));
        rgtp_free(surface->chunks);
    }
    if (surface->chunk_sizes) {
        memcpy(new_sizes, surface->chunk_sizes, new_idx * sizeof(size_t));
        rgtp_free(surface->chunk_sizes);
    }
    surface->chunks      = new_chunks;
    surface->chunk_sizes = new_sizes;

    /* Encrypt the new chunk */
    size_t ct_max = chunk_len + RGTP_AEAD_TAG_BYTES;
    surface->chunks[new_idx] = (uint8_t*)rgtp_malloc(ct_max);
    if (!surface->chunks[new_idx]) return RGTP_ERR_NOMEM;

    rgtp_error_t err = rgtp_encrypt_chunk(surface->key, new_idx,
                                           chunk_data, chunk_len,
                                           surface->chunks[new_idx],
                                           &surface->chunk_sizes[new_idx]);
    if (err != RGTP_OK) {
        rgtp_free(surface->chunks[new_idx]);
        surface->chunks[new_idx] = NULL;
        return err;
    }

    surface->chunk_count++;
    surface->total_size += chunk_len;

    /* Evict oldest chunk if window is full */
    if (window_size > 0 && surface->chunk_count > window_size) {
        uint32_t evict_idx = surface->window_base;
        if (surface->chunks[evict_idx]) {
            /* Zeroize before freeing */
            rgtp_zeroize(surface->chunks[evict_idx],
                          surface->chunk_sizes[evict_idx]);
            rgtp_free(surface->chunks[evict_idx]);
            surface->chunks[evict_idx] = NULL;
        }
        surface->window_base++;
    }

    return RGTP_OK;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * 11.2 — Partial pull: compute minimal covering chunk set
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Compute the minimal set of chunk indices covering byte range
 *        [offset, offset + length).
 *
 * @param chunk_size    Bytes per chunk.
 * @param chunk_count   Total number of chunks.
 * @param offset        Start byte offset within the Exposure.
 * @param length        Number of bytes to cover.
 * @param first_chunk   Receives the index of the first covering chunk.
 * @param last_chunk    Receives the index of the last covering chunk (inclusive).
 * @return RGTP_OK or RGTP_ERR_INVALID_ARG.
 */
rgtp_error_t rgtp_compute_chunk_range(uint32_t  chunk_size,
                                       uint32_t  chunk_count,
                                       uint64_t  offset,
                                       uint64_t  length,
                                       uint32_t* first_chunk,
                                       uint32_t* last_chunk)
{
    if (chunk_size == 0 || chunk_count == 0 ||
        first_chunk == NULL || last_chunk == NULL) {
        return RGTP_ERR_INVALID_ARG;
    }
    if (length == 0) {
        return RGTP_ERR_INVALID_ARG;
    }

    uint64_t end = offset + length - 1u;
    uint32_t fc  = (uint32_t)(offset / chunk_size);
    uint32_t lc  = (uint32_t)(end    / chunk_size);

    if (fc >= chunk_count) return RGTP_ERR_INVALID_ARG;
    if (lc >= chunk_count) lc = chunk_count - 1u;

    *first_chunk = fc;
    *last_chunk  = lc;
    return RGTP_OK;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * 11.3 — Out-of-order delivery tolerance
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Reorder buffer for out-of-order chunk delivery.
 *
 * Holds up to max_out_of_order chunks ahead of the lowest undelivered index.
 * Delivers each chunk to the application exactly once in near-sequential order.
 */
typedef struct {
    uint32_t  max_out_of_order;   /**< Maximum reorder depth */
    uint32_t  next_deliver;       /**< Next chunk index to deliver */
    uint32_t  capacity;           /**< Allocated slot count */
    uint8_t** slots;              /**< Buffered plaintext chunks */
    size_t*   slot_sizes;         /**< Sizes of buffered chunks */
    uint8_t*  slot_valid;         /**< Bitmap: slot i is populated */
} rgtp_reorder_buf_t;

rgtp_error_t rgtp_reorder_buf_init(rgtp_reorder_buf_t* rb,
                                    uint32_t            max_out_of_order)
{
    if (rb == NULL) return RGTP_ERR_INVALID_ARG;

    uint32_t cap = max_out_of_order + 1u;
    rb->max_out_of_order = max_out_of_order;
    rb->next_deliver     = 0;
    rb->capacity         = cap;

    rb->slots      = (uint8_t**)rgtp_calloc(cap, sizeof(uint8_t*));
    rb->slot_sizes = (size_t*)  rgtp_calloc(cap, sizeof(size_t));
    rb->slot_valid = (uint8_t*) rgtp_calloc(cap, 1u);

    if (!rb->slots || !rb->slot_sizes || !rb->slot_valid) {
        rgtp_free(rb->slots);
        rgtp_free(rb->slot_sizes);
        rgtp_free(rb->slot_valid);
        return RGTP_ERR_NOMEM;
    }
    return RGTP_OK;
}

void rgtp_reorder_buf_destroy(rgtp_reorder_buf_t* rb)
{
    if (!rb) return;
    for (uint32_t i = 0; i < rb->capacity; i++) {
        rgtp_free(rb->slots[i]);
    }
    rgtp_free(rb->slots);
    rgtp_free(rb->slot_sizes);
    rgtp_free(rb->slot_valid);
    memset(rb, 0, sizeof(*rb));
}

/**
 * @brief Insert a received chunk into the reorder buffer.
 *
 * @return RGTP_OK, RGTP_ERR_INVALID_ARG (chunk too far ahead), or RGTP_ERR_NOMEM.
 */
rgtp_error_t rgtp_reorder_buf_insert(rgtp_reorder_buf_t* rb,
                                      uint32_t            chunk_index,
                                      const uint8_t*      data,
                                      size_t              len)
{
    if (rb == NULL || data == NULL) return RGTP_ERR_INVALID_ARG;
    if (chunk_index < rb->next_deliver) return RGTP_ERR_INVALID_ARG;  /* already delivered */

    uint32_t ahead = chunk_index - rb->next_deliver;
    if (ahead >= rb->capacity) return RGTP_ERR_INVALID_ARG;  /* too far ahead */

    uint32_t slot = chunk_index % rb->capacity;
    if (rb->slot_valid[slot]) return RGTP_OK;  /* already have it */

    rb->slots[slot] = (uint8_t*)rgtp_malloc(len);
    if (!rb->slots[slot]) return RGTP_ERR_NOMEM;

    memcpy(rb->slots[slot], data, len);
    rb->slot_sizes[slot] = len;
    rb->slot_valid[slot] = 1;
    return RGTP_OK;
}

/**
 * @brief Dequeue the next in-order chunk from the reorder buffer.
 *
 * @param rb          Reorder buffer.
 * @param buffer      Caller-supplied output buffer.
 * @param buf_size    Size of @p buffer.
 * @param out_len     Receives the number of bytes written.
 * @param out_index   Receives the chunk index delivered.
 * @return RGTP_OK if a chunk was delivered, RGTP_ERR_TIMEOUT if none ready.
 */
rgtp_error_t rgtp_reorder_buf_dequeue(rgtp_reorder_buf_t* rb,
                                       void*               buffer,
                                       size_t              buf_size,
                                       size_t*             out_len,
                                       uint32_t*           out_index)
{
    if (rb == NULL || buffer == NULL || out_len == NULL) {
        return RGTP_ERR_INVALID_ARG;
    }

    uint32_t slot = rb->next_deliver % rb->capacity;
    if (!rb->slot_valid[slot]) {
        return RGTP_ERR_TIMEOUT;  /* next chunk not yet received */
    }

    size_t len = rb->slot_sizes[slot];
    if (len > buf_size) return RGTP_ERR_INVALID_ARG;

    memcpy(buffer, rb->slots[slot], len);
    *out_len = len;
    if (out_index) *out_index = rb->next_deliver;

    /* Free the slot */
    rgtp_free(rb->slots[slot]);
    rb->slots[slot]      = NULL;
    rb->slot_sizes[slot] = 0;
    rb->slot_valid[slot] = 0;
    rb->next_deliver++;

    return RGTP_OK;
}
