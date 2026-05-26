/**
 * @file rgtp_metrics.c
 * @brief Prometheus-compatible metrics registry.
 *
 * Implements counter, gauge, and histogram types for all RGTP metrics.
 * Exposes a text-format scrape endpoint via rgtp_metrics_scrape().
 *
 * Metrics:
 *   bytes_sent_total          (counter)
 *   bytes_received_total      (counter)
 *   chunks_sent_total         (counter)
 *   chunks_received_total     (counter)
 *   fec_parity_chunks_total   (counter)
 *   auth_failures_total       (counter)
 *   malformed_packets_total   (counter)
 *   active_exposures          (gauge)
 *   active_pullers            (gauge)
 *   rtt_seconds               (histogram)
 *   throughput_bytes_per_sec  (gauge)
 *   packet_loss_rate          (gauge)
 *
 * Requirements: 13.1
 */

#include "rgtp_metrics_internal.h"
#include "../core/rgtp_alloc_internal.h"

#include <string.h>
#include <stdio.h>
#include <stdatomic.h>
#include <math.h>

/* ── Global registry ────────────────────────────────────────────────────── */

static rgtp_metrics_t g_metrics;
static _Atomic int    g_metrics_init = 0;

void rgtp_metrics_init(void)
{
    memset(&g_metrics, 0, sizeof(g_metrics));
    atomic_store(&g_metrics_init, 1);
}

rgtp_metrics_t* rgtp_metrics_get(void)
{
    return &g_metrics;
}

/* ── Counter operations ─────────────────────────────────────────────────── */

void rgtp_metrics_counter_add(rgtp_counter_t* c, uint64_t delta)
{
    if (c) atomic_fetch_add(&c->value, delta);
}

uint64_t rgtp_metrics_counter_get(const rgtp_counter_t* c)
{
    return c ? atomic_load(&c->value) : 0u;
}

/* ── Gauge operations ───────────────────────────────────────────────────── */

void rgtp_metrics_gauge_set(rgtp_gauge_t* g, double value)
{
    if (!g) return;
    /* Store as integer micros to avoid non-atomic double writes */
    int64_t v = (int64_t)(value * 1e6);
    atomic_store(&g->value_micros, v);
}

void rgtp_metrics_gauge_add(rgtp_gauge_t* g, double delta)
{
    if (!g) return;
    int64_t d = (int64_t)(delta * 1e6);
    atomic_fetch_add(&g->value_micros, d);
}

double rgtp_metrics_gauge_get(const rgtp_gauge_t* g)
{
    return g ? (double)atomic_load(&g->value_micros) / 1e6 : 0.0;
}

/* ── Histogram operations ───────────────────────────────────────────────── */

/* RTT histogram buckets in seconds: 0.0001, 0.001, 0.01, 0.1, 1.0, +Inf */
static const double RTT_BUCKETS[] = { 0.0001, 0.001, 0.01, 0.1, 1.0 };
#define RTT_BUCKET_COUNT 5

void rgtp_metrics_histogram_observe(rgtp_histogram_t* h, double value)
{
    if (!h) return;
    atomic_fetch_add(&h->count, 1u);
    int64_t v_micros = (int64_t)(value * 1e6);
    atomic_fetch_add(&h->sum_micros, v_micros);

    for (int i = 0; i < RTT_BUCKET_COUNT; i++) {
        if (value <= RTT_BUCKETS[i]) {
            atomic_fetch_add(&h->buckets[i], 1u);
        }
    }
}

/* ── Prometheus text format scrape ──────────────────────────────────────── */

/**
 * @brief Write Prometheus text format metrics into @p buf.
 *
 * @param buf      Output buffer.
 * @param buf_size Size of @p buf.
 * @return Number of bytes written (not including null terminator),
 *         or -1 if the buffer is too small.
 */
int rgtp_metrics_scrape(char* buf, size_t buf_size)
{
    if (!buf || buf_size == 0) return -1;

    rgtp_metrics_t* m = &g_metrics;
    int written = 0;
    int remaining = (int)buf_size;

#define APPEND(...) do { \
    int r = snprintf(buf + written, (size_t)remaining, __VA_ARGS__); \
    if (r < 0 || r >= remaining) return -1; \
    written += r; remaining -= r; \
} while(0)

    APPEND("# HELP rgtp_bytes_sent_total Total bytes sent by exposers\n");
    APPEND("# TYPE rgtp_bytes_sent_total counter\n");
    APPEND("rgtp_bytes_sent_total %llu\n",
           (unsigned long long)rgtp_metrics_counter_get(&m->bytes_sent_total));

    APPEND("# HELP rgtp_bytes_received_total Total bytes received by pullers\n");
    APPEND("# TYPE rgtp_bytes_received_total counter\n");
    APPEND("rgtp_bytes_received_total %llu\n",
           (unsigned long long)rgtp_metrics_counter_get(&m->bytes_received_total));

    APPEND("# HELP rgtp_chunks_sent_total Total chunks sent\n");
    APPEND("# TYPE rgtp_chunks_sent_total counter\n");
    APPEND("rgtp_chunks_sent_total %llu\n",
           (unsigned long long)rgtp_metrics_counter_get(&m->chunks_sent_total));

    APPEND("# HELP rgtp_chunks_received_total Total chunks received\n");
    APPEND("# TYPE rgtp_chunks_received_total counter\n");
    APPEND("rgtp_chunks_received_total %llu\n",
           (unsigned long long)rgtp_metrics_counter_get(&m->chunks_received_total));

    APPEND("# HELP rgtp_auth_failures_total AEAD authentication failures\n");
    APPEND("# TYPE rgtp_auth_failures_total counter\n");
    APPEND("rgtp_auth_failures_total %llu\n",
           (unsigned long long)rgtp_metrics_counter_get(&m->auth_failures_total));

    APPEND("# HELP rgtp_malformed_packets_total Packets discarded by parser\n");
    APPEND("# TYPE rgtp_malformed_packets_total counter\n");
    APPEND("rgtp_malformed_packets_total %llu\n",
           (unsigned long long)rgtp_metrics_counter_get(&m->malformed_packets_total));

    APPEND("# HELP rgtp_active_exposures Number of active exposures\n");
    APPEND("# TYPE rgtp_active_exposures gauge\n");
    APPEND("rgtp_active_exposures %.0f\n",
           rgtp_metrics_gauge_get(&m->active_exposures));

    APPEND("# HELP rgtp_active_pullers Number of active pullers\n");
    APPEND("# TYPE rgtp_active_pullers gauge\n");
    APPEND("rgtp_active_pullers %.0f\n",
           rgtp_metrics_gauge_get(&m->active_pullers));

    APPEND("# HELP rgtp_packet_loss_rate Current packet loss rate\n");
    APPEND("# TYPE rgtp_packet_loss_rate gauge\n");
    APPEND("rgtp_packet_loss_rate %g\n",
           rgtp_metrics_gauge_get(&m->packet_loss_rate));

    APPEND("# HELP rgtp_throughput_bytes_per_second Current throughput\n");
    APPEND("# TYPE rgtp_throughput_bytes_per_second gauge\n");
    APPEND("rgtp_throughput_bytes_per_second %g\n",
           rgtp_metrics_gauge_get(&m->throughput_bps));

    /* RTT histogram */
    APPEND("# HELP rgtp_rtt_seconds Round-trip time histogram\n");
    APPEND("# TYPE rgtp_rtt_seconds histogram\n");
    for (int i = 0; i < RTT_BUCKET_COUNT; i++) {
        APPEND("rgtp_rtt_seconds_bucket{le=\"%g\"} %llu\n",
               RTT_BUCKETS[i],
               (unsigned long long)atomic_load(&m->rtt_histogram.buckets[i]));
    }
    APPEND("rgtp_rtt_seconds_bucket{le=\"+Inf\"} %llu\n",
           (unsigned long long)atomic_load(&m->rtt_histogram.count));
    APPEND("rgtp_rtt_seconds_sum %g\n",
           (double)atomic_load(&m->rtt_histogram.sum_micros) / 1e6);
    APPEND("rgtp_rtt_seconds_count %llu\n",
           (unsigned long long)atomic_load(&m->rtt_histogram.count));

#undef APPEND

    buf[written] = '\0';
    return written;
}
