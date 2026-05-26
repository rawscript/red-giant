/**
 * @file rgtp_ratelimit.c
 * @brief Per-source token bucket rate limiter.
 *
 * Limits pull requests from a single source IP to RGTP_RATELIMIT_MAX_RPS
 * (1000) requests per second per Exposure. Requests exceeding this limit
 * receive no response (RGTP_ERR_RATE_LIMITED is returned to the caller).
 *
 * Implementation: open-addressing hash table with linear probing.
 * Table size is RGTP_RATELIMIT_BUCKET_COUNT (256) — bounded to prevent DoS.
 * When the table is full, the oldest entry (lowest last_refill_us) is evicted.
 *
 * Requirements: 3.11
 */

#include "rgtp_transport_internal.h"

#include <string.h>
#include <stdint.h>

/* ── Hash function ──────────────────────────────────────────────────────── */

static uint32_t hash_key(const uint8_t exposure_id[16], uint32_t source_ip)
{
    /* FNV-1a over the 20-byte key */
    uint32_t h = 2166136261u;
    for (int i = 0; i < 16; i++) {
        h ^= exposure_id[i];
        h *= 16777619u;
    }
    h ^= (source_ip & 0xFFu);        h *= 16777619u;
    h ^= ((source_ip >>  8) & 0xFFu); h *= 16777619u;
    h ^= ((source_ip >> 16) & 0xFFu); h *= 16777619u;
    h ^= ((source_ip >> 24) & 0xFFu); h *= 16777619u;
    return h & (RGTP_RATELIMIT_BUCKET_COUNT - 1u);
}

/* ── Public API ─────────────────────────────────────────────────────────── */

void rgtp_ratelimit_init(rgtp_ratelimit_t* rl)
{
    memset(rl, 0, sizeof(*rl));
}

rgtp_error_t rgtp_ratelimit_check(rgtp_ratelimit_t* rl,
                                   const uint8_t     exposure_id[16],
                                   uint32_t          source_ip,
                                   uint64_t          now_us)
{
    if (rl == NULL || exposure_id == NULL) {
        return RGTP_ERR_INVALID_ARG;
    }

    const double RATE     = (double)RGTP_RATELIMIT_MAX_RPS;  /* tokens/sec */
    const double MAX_TOKENS = RATE;

    uint32_t slot = hash_key(exposure_id, source_ip);

    /* Linear probe to find existing entry or empty slot */
    rgtp_ratelimit_entry_t* found   = NULL;
    rgtp_ratelimit_entry_t* oldest  = NULL;
    uint64_t                oldest_ts = UINT64_MAX;

    for (uint32_t i = 0; i < RGTP_RATELIMIT_BUCKET_COUNT; i++) {
        uint32_t idx = (slot + i) & (RGTP_RATELIMIT_BUCKET_COUNT - 1u);
        rgtp_ratelimit_entry_t* e = &rl->entries[idx];

        if (e->last_refill_us == 0) {
            /* Empty slot — use it if we haven't found the key yet */
            if (found == NULL) {
                found = e;
                memcpy(e->exposure_id, exposure_id, 16);
                e->source_ip      = source_ip;
                e->tokens         = MAX_TOKENS - 1.0;  /* consume one token */
                e->last_refill_us = now_us;
                return RGTP_OK;
            }
            break;
        }

        if (memcmp(e->exposure_id, exposure_id, 16) == 0 &&
            e->source_ip == source_ip) {
            found = e;
            break;
        }

        /* Track oldest entry for eviction */
        if (e->last_refill_us < oldest_ts) {
            oldest_ts = e->last_refill_us;
            oldest    = e;
        }
    }

    if (found == NULL) {
        /* Table full — evict oldest entry */
        if (oldest == NULL) {
            /* Shouldn't happen with a power-of-2 table, but be safe */
            return RGTP_ERR_RATE_LIMITED;
        }
        found = oldest;
        memcpy(found->exposure_id, exposure_id, 16);
        found->source_ip      = source_ip;
        found->tokens         = MAX_TOKENS - 1.0;
        found->last_refill_us = now_us;
        return RGTP_OK;
    }

    /* Refill tokens based on elapsed time */
    if (now_us > found->last_refill_us) {
        double elapsed_sec = (double)(now_us - found->last_refill_us) / 1e6;
        found->tokens += elapsed_sec * RATE;
        if (found->tokens > MAX_TOKENS) {
            found->tokens = MAX_TOKENS;
        }
        found->last_refill_us = now_us;
    }

    /* Consume one token */
    if (found->tokens < 1.0) {
        return RGTP_ERR_RATE_LIMITED;
    }
    found->tokens -= 1.0;
    return RGTP_OK;
}
