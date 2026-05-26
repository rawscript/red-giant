/**
 * @file rgtp_metrics_internal.h
 * @brief Internal metrics types and registry.
 */

#ifndef RGTP_METRICS_INTERNAL_H
#define RGTP_METRICS_INTERNAL_H

#include <stdint.h>
#include <stddef.h>
#include <stdatomic.h>

#ifdef __cplusplus
extern "C" {
#endif

#define RGTP_HISTOGRAM_BUCKET_COUNT 6u

typedef struct { _Atomic uint64_t value; }        rgtp_counter_t;
typedef struct { _Atomic int64_t  value_micros; } rgtp_gauge_t;
typedef struct {
    _Atomic uint64_t count;
    _Atomic int64_t  sum_micros;
    _Atomic uint64_t buckets[RGTP_HISTOGRAM_BUCKET_COUNT];
} rgtp_histogram_t;

typedef struct {
    rgtp_counter_t   bytes_sent_total;
    rgtp_counter_t   bytes_received_total;
    rgtp_counter_t   chunks_sent_total;
    rgtp_counter_t   chunks_received_total;
    rgtp_counter_t   fec_parity_chunks_total;
    rgtp_counter_t   auth_failures_total;
    rgtp_counter_t   malformed_packets_total;
    rgtp_gauge_t     active_exposures;
    rgtp_gauge_t     active_pullers;
    rgtp_gauge_t     packet_loss_rate;
    rgtp_gauge_t     throughput_bps;
    rgtp_histogram_t rtt_histogram;
} rgtp_metrics_t;

void             rgtp_metrics_init(void);
rgtp_metrics_t*  rgtp_metrics_get(void);

void     rgtp_metrics_counter_add(rgtp_counter_t* c, uint64_t delta);
uint64_t rgtp_metrics_counter_get(const rgtp_counter_t* c);

void   rgtp_metrics_gauge_set(rgtp_gauge_t* g, double value);
void   rgtp_metrics_gauge_add(rgtp_gauge_t* g, double delta);
double rgtp_metrics_gauge_get(const rgtp_gauge_t* g);

void rgtp_metrics_histogram_observe(rgtp_histogram_t* h, double value);

int rgtp_metrics_scrape(char* buf, size_t buf_size);

#ifdef __cplusplus
}
#endif

#endif /* RGTP_METRICS_INTERNAL_H */
