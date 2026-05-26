/**
 * @file rgtp_replay.c
 * @brief Anti-replay window — 256-bit sliding bitmap per puller session.
 *
 * Algorithm:
 *   On receiving chunk with sequence number seq:
 *   1. If seq < base:          discard (too old).
 *   2. If seq >= base + 256:   advance window, clearing old bits.
 *   3. If bit (seq - base) set: discard (replay).
 *   4. Otherwise:              set bit, accept.
 *
 * The window base is monotonically non-decreasing.
 *
 * Requirements: 3.10, 22.6
 */

#include "rgtp_transport_internal.h"

#include <string.h>
#include <stdint.h>

/* ── Bitmap helpers ─────────────────────────────────────────────────────── */

#define WINDOW_BITS 256u

static inline void bitmap_set(uint64_t bm[4], uint32_t bit)
{
    bm[bit / 64u] |= (UINT64_C(1) << (bit % 64u));
}

static inline int bitmap_test(const uint64_t bm[4], uint32_t bit)
{
    return (bm[bit / 64u] >> (bit % 64u)) & 1u;
}

static inline void bitmap_clear(uint64_t bm[4], uint32_t bit)
{
    bm[bit / 64u] &= ~(UINT64_C(1) << (bit % 64u));
}

/* ── Public API ─────────────────────────────────────────────────────────── */

void rgtp_replay_window_init(rgtp_replay_window_t* w)
{
    memset(w->bitmap, 0, sizeof(w->bitmap));
    w->base = 0;
}

rgtp_replay_result_t rgtp_replay_check_and_set(rgtp_replay_window_t* w,
                                                uint32_t              seq)
{
    /* Too old — below window base */
    if (seq < w->base) {
        return RGTP_REPLAY_TOO_OLD;
    }

    uint32_t offset = seq - w->base;

    /* Advance window if seq is ahead of the current window */
    if (offset >= WINDOW_BITS) {
        uint32_t advance = offset - WINDOW_BITS + 1u;

        if (advance >= WINDOW_BITS) {
            /* Seq is so far ahead that the entire window is invalidated */
            memset(w->bitmap, 0, sizeof(w->bitmap));
            w->base = seq;
            offset  = 0;
        } else {
            /* Clear the bits that are being evicted */
            for (uint32_t i = 0; i < advance; i++) {
                bitmap_clear(w->bitmap, i % WINDOW_BITS);
            }
            w->base += advance;
            offset   = seq - w->base;
        }
    }

    /* Duplicate / replay check */
    if (bitmap_test(w->bitmap, offset)) {
        return RGTP_REPLAY_DUPLICATE;
    }

    /* Accept: mark as seen */
    bitmap_set(w->bitmap, offset);
    return RGTP_REPLAY_ACCEPT;
}
