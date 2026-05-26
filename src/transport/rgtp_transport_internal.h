/**
 * @file rgtp_transport_internal.h
 * @brief Internal declarations for the RGTP transport layer.
 */

#ifndef RGTP_TRANSPORT_INTERNAL_H
#define RGTP_TRANSPORT_INTERNAL_H

#include "rgtp/rgtp.h"
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════════════════
 * Anti-replay window
 * ═══════════════════════════════════════════════════════════════════════════ */

/** 256-bit sliding bitmap anti-replay window. */
typedef struct {
    uint64_t bitmap[4];   /**< 256 bits: bit i = seq (base+i) has been seen */
    uint32_t base;        /**< Sequence number of bit 0 */
} rgtp_replay_window_t;

typedef enum {
    RGTP_REPLAY_ACCEPT    = 0,  /**< New sequence number — accept */
    RGTP_REPLAY_DUPLICATE = 1,  /**< Already seen — discard */
    RGTP_REPLAY_TOO_OLD   = 2,  /**< Below window base — discard */
} rgtp_replay_result_t;

void                 rgtp_replay_window_init(rgtp_replay_window_t* w);
rgtp_replay_result_t rgtp_replay_check_and_set(rgtp_replay_window_t* w,
                                                uint32_t              seq);

/* ═══════════════════════════════════════════════════════════════════════════
 * Rate limiter (token bucket, per source IP)
 * ═══════════════════════════════════════════════════════════════════════════ */

/** Token bucket entry for one (exposure_id, source_ip) pair. */
typedef struct {
    uint8_t  exposure_id[16];
    uint32_t source_ip;          /**< IPv4 address in host byte order */
    double   tokens;             /**< Current token count */
    uint64_t last_refill_us;     /**< Timestamp of last refill (microseconds) */
} rgtp_ratelimit_entry_t;

#define RGTP_RATELIMIT_BUCKET_COUNT 256u   /**< Hash table size (power of 2) */
#define RGTP_RATELIMIT_MAX_RPS      1000u  /**< Max pull requests per second */

typedef struct {
    rgtp_ratelimit_entry_t entries[RGTP_RATELIMIT_BUCKET_COUNT];
} rgtp_ratelimit_t;

void         rgtp_ratelimit_init(rgtp_ratelimit_t* rl);
rgtp_error_t rgtp_ratelimit_check(rgtp_ratelimit_t* rl,
                                   const uint8_t     exposure_id[16],
                                   uint32_t          source_ip,
                                   uint64_t          now_us);

/* ═══════════════════════════════════════════════════════════════════════════
 * Flow control (AIMD congestion control + RTT estimation)
 * ═══════════════════════════════════════════════════════════════════════════ */

typedef struct {
    uint32_t rtt_us;             /**< EWMA RTT estimate in microseconds */
    uint32_t rtt_baseline_us;    /**< Baseline RTT (minimum observed) */
    uint32_t rtt_sample_count;   /**< Number of RTT samples collected */
    uint32_t window_size;        /**< Current pull window size (chunks) */
    uint32_t window_min;         /**< Minimum window size (always >= 1) */
    uint32_t window_max;         /**< Maximum window size */
    uint32_t consecutive_low_rtt; /**< Consecutive RTT periods within 1.1x baseline */
    float    loss_rate;          /**< EWMA packet loss rate [0.0, 1.0] */
    float    fec_overhead;       /**< Current FEC parity overhead [0.0, 0.5] */
    uint32_t pull_pressure;      /**< Pull requests in last 100ms window */
    uint64_t pull_pressure_window_start_us; /**< Start of 100ms window */
    uint32_t pull_pressure_count;           /**< Count in current window */
} rgtp_flow_t;

void rgtp_flow_init(rgtp_flow_t* f, uint32_t initial_window);
void rgtp_flow_update_rtt(rgtp_flow_t* f, uint32_t sample_us);
void rgtp_flow_update_loss(rgtp_flow_t* f, float loss_sample);
void rgtp_flow_on_congestion(rgtp_flow_t* f);
void rgtp_flow_on_ack(rgtp_flow_t* f);
void rgtp_flow_update_fec(rgtp_flow_t* f);
void rgtp_flow_record_pull(rgtp_flow_t* f, uint64_t now_us);

#ifdef __cplusplus
}
#endif

#endif /* RGTP_TRANSPORT_INTERNAL_H */
