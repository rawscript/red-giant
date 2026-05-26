/**
 * @file rgtp_priority.c
 * @brief Priority scheduling and jitter buffer for AV low-latency modes.
 *
 * Priority scheduling (Task 13.1):
 *  - Strict priority queue (levels 0–7) per Exposure in the Exposer.
 *  - Priority-0 pull requests are processed before priority 1–7.
 *  - Emits "priority_chunk_timeout" ERROR log when priority-0 chunk not
 *    arrived within 2× expected inter-chunk interval.
 *
 * Jitter buffer (Task 13.1):
 *  - Configurable depth (1ms–100ms) on the Puller side.
 *  - Absorbs packet arrival variance; smooths delivery to the application.
 *
 * Requirements: 11.2, 11.3, 11.4, 11.5
 */

#include "rgtp_surface_internal.h"
#include "../core/rgtp_alloc_internal.h"
#include "../observability/rgtp_log_internal.h"

#include <string.h>
#include <stdint.h>

/* ── Priority queue ─────────────────────────────────────────────────────── */

#define RGTP_PRIORITY_LEVELS 8u
#define RGTP_PRIORITY_QUEUE_DEPTH 256u

typedef struct {
    uint32_t chunk_index;
    uint8_t  priority;       /**< 0 = highest, 7 = lowest */
    uint64_t enqueue_us;
} rgtp_pq_entry_t;

typedef struct {
    rgtp_pq_entry_t entries[RGTP_PRIORITY_LEVELS][RGTP_PRIORITY_QUEUE_DEPTH];
    uint32_t        head[RGTP_PRIORITY_LEVELS];
    uint32_t        tail[RGTP_PRIORITY_LEVELS];
    uint32_t        count[RGTP_PRIORITY_LEVELS];
} rgtp_priority_queue_t;

void rgtp_pq_init(rgtp_priority_queue_t* pq)
{
    memset(pq, 0, sizeof(*pq));
}

/**
 * @brief Enqueue a chunk index at the given priority level.
 * @return RGTP_OK or RGTP_ERR_NOMEM if the queue for this priority is full.
 */
rgtp_error_t rgtp_pq_enqueue(rgtp_priority_queue_t* pq,
                               uint32_t chunk_index,
                               uint8_t  priority,
                               uint64_t now_us)
{
    if (priority >= RGTP_PRIORITY_LEVELS) priority = RGTP_PRIORITY_LEVELS - 1;
    if (pq->count[priority] >= RGTP_PRIORITY_QUEUE_DEPTH) {
        return RGTP_ERR_NOMEM;
    }

    uint32_t slot = pq->tail[priority] % RGTP_PRIORITY_QUEUE_DEPTH;
    pq->entries[priority][slot].chunk_index = chunk_index;
    pq->entries[priority][slot].priority    = priority;
    pq->entries[priority][slot].enqueue_us  = now_us;
    pq->tail[priority]++;
    pq->count[priority]++;
    return RGTP_OK;
}

/**
 * @brief Dequeue the highest-priority pending chunk index.
 * @return RGTP_OK if a chunk was dequeued, RGTP_ERR_TIMEOUT if all queues empty.
 */
rgtp_error_t rgtp_pq_dequeue(rgtp_priority_queue_t* pq,
                               uint32_t* out_chunk_index,
                               uint8_t*  out_priority)
{
    for (uint8_t p = 0; p < RGTP_PRIORITY_LEVELS; p++) {
        if (pq->count[p] == 0) continue;

        uint32_t slot = pq->head[p] % RGTP_PRIORITY_QUEUE_DEPTH;
        *out_chunk_index = pq->entries[p][slot].chunk_index;
        *out_priority    = p;
        pq->head[p]++;
        pq->count[p]--;
        return RGTP_OK;
    }
    return RGTP_ERR_TIMEOUT;
}

/* ── Jitter buffer ──────────────────────────────────────────────────────── */

#define RGTP_JITTER_BUF_MAX_SLOTS 1024u

typedef struct {
    uint8_t*  data;
    size_t    len;
    uint32_t  chunk_index;
    uint64_t  arrival_us;
    bool      valid;
} rgtp_jitter_slot_t;

typedef struct {
    rgtp_jitter_slot_t* slots;
    uint32_t            capacity;
    uint32_t            depth_us;     /**< Target buffer depth in microseconds */
    uint32_t            next_deliver; /**< Next chunk index to deliver */
    uint64_t            play_start_us;/**< Playback start timestamp */
} rgtp_jitter_buf_t;

/**
 * @brief Initialise a jitter buffer.
 *
 * @param jb         Jitter buffer to initialise.
 * @param depth_ms   Target buffer depth in milliseconds (1–100).
 * @param capacity   Maximum number of buffered chunks.
 * @return RGTP_OK or RGTP_ERR_NOMEM.
 */
rgtp_error_t rgtp_jitter_buf_init(rgtp_jitter_buf_t* jb,
                                   uint32_t           depth_ms,
                                   uint32_t           capacity)
{
    if (!jb || depth_ms < 1 || depth_ms > 100) return RGTP_ERR_INVALID_ARG;
    if (capacity == 0 || capacity > RGTP_JITTER_BUF_MAX_SLOTS) {
        capacity = RGTP_JITTER_BUF_MAX_SLOTS;
    }

    jb->slots = (rgtp_jitter_slot_t*)rgtp_calloc(capacity, sizeof(rgtp_jitter_slot_t));
    if (!jb->slots) return RGTP_ERR_NOMEM;

    jb->capacity      = capacity;
    jb->depth_us      = depth_ms * 1000u;
    jb->next_deliver  = 0;
    jb->play_start_us = 0;
    return RGTP_OK;
}

void rgtp_jitter_buf_destroy(rgtp_jitter_buf_t* jb)
{
    if (!jb) return;
    if (jb->slots) {
        for (uint32_t i = 0; i < jb->capacity; i++) {
            rgtp_free(jb->slots[i].data);
        }
        rgtp_free(jb->slots);
        jb->slots = NULL;
    }
}

/**
 * @brief Insert a received chunk into the jitter buffer.
 */
rgtp_error_t rgtp_jitter_buf_insert(rgtp_jitter_buf_t* jb,
                                     uint32_t           chunk_index,
                                     const uint8_t*     data,
                                     size_t             len,
                                     uint64_t           arrival_us)
{
    if (!jb || !data) return RGTP_ERR_INVALID_ARG;

    uint32_t slot = chunk_index % jb->capacity;
    if (jb->slots[slot].valid) return RGTP_OK;  /* already have it */

    jb->slots[slot].data = (uint8_t*)rgtp_malloc(len);
    if (!jb->slots[slot].data) return RGTP_ERR_NOMEM;

    memcpy(jb->slots[slot].data, data, len);
    jb->slots[slot].len         = len;
    jb->slots[slot].chunk_index = chunk_index;
    jb->slots[slot].arrival_us  = arrival_us;
    jb->slots[slot].valid       = true;

    /* Set playback start on first chunk */
    if (jb->play_start_us == 0) {
        jb->play_start_us = arrival_us + jb->depth_us;
    }
    return RGTP_OK;
}

/**
 * @brief Dequeue the next chunk if its scheduled playback time has arrived.
 *
 * @param jb         Jitter buffer.
 * @param now_us     Current time in microseconds.
 * @param buf        Output buffer.
 * @param buf_size   Size of @p buf.
 * @param out_len    Receives bytes written.
 * @param out_index  Receives chunk index.
 * @return RGTP_OK if a chunk was delivered, RGTP_ERR_TIMEOUT if not yet ready.
 */
rgtp_error_t rgtp_jitter_buf_dequeue(rgtp_jitter_buf_t* jb,
                                      uint64_t           now_us,
                                      void*              buf,
                                      size_t             buf_size,
                                      size_t*            out_len,
                                      uint32_t*          out_index)
{
    if (!jb || !buf || !out_len) return RGTP_ERR_INVALID_ARG;

    /* Not yet time to start playing */
    if (jb->play_start_us > 0 && now_us < jb->play_start_us) {
        return RGTP_ERR_TIMEOUT;
    }

    uint32_t slot = jb->next_deliver % jb->capacity;
    if (!jb->slots[slot].valid) {
        return RGTP_ERR_TIMEOUT;
    }

    size_t len = jb->slots[slot].len;
    if (len > buf_size) return RGTP_ERR_INVALID_ARG;

    memcpy(buf, jb->slots[slot].data, len);
    *out_len   = len;
    if (out_index) *out_index = jb->next_deliver;

    rgtp_free(jb->slots[slot].data);
    jb->slots[slot].data  = NULL;
    jb->slots[slot].valid = false;
    jb->next_deliver++;
    return RGTP_OK;
}

/* ── Priority chunk timeout detection ──────────────────────────────────── */

/**
 * @brief Check if a priority-0 chunk has timed out (not arrived within
 *        2× the expected inter-chunk interval).
 *
 * Emits an ERROR log event "priority_chunk_timeout" if timed out.
 *
 * @param expected_chunk_index  The chunk index we're waiting for.
 * @param last_chunk_arrival_us Timestamp of the last received chunk.
 * @param inter_chunk_us        Expected inter-chunk interval in microseconds.
 * @param now_us                Current time in microseconds.
 */
void rgtp_check_priority_timeout(uint32_t chunk_index,
                                  uint64_t last_chunk_arrival_us,
                                  uint32_t inter_chunk_us,
                                  uint64_t now_us)
{
    if (inter_chunk_us == 0) return;
    uint64_t deadline = last_chunk_arrival_us + 2u * (uint64_t)inter_chunk_us;
    if (now_us > deadline) {
        char idx_str[16], deadline_str[24];
        snprintf(idx_str, sizeof(idx_str), "%u", chunk_index);
        snprintf(deadline_str, sizeof(deadline_str), "%llu",
                 (unsigned long long)deadline);
        RGTP_LOG2(RGTP_LOG_ERROR, "rgtp.transport",
                  "Priority-0 chunk timeout: chunk not arrived within 2x inter-chunk interval",
                  "chunk_index", idx_str,
                  "deadline_us", deadline_str);
    }
}
