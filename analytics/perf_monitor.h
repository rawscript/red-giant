#ifndef PERF_MONITOR_H
#define PERF_MONITOR_H

#include "../include/rgtp/rgtp.h"
#include <stdint.h>
#include <stddef.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

/* -------------------------------------------------------------------------- */
/* Performance Monitor Data Structures                                       */
/* -------------------------------------------------------------------------- */

typedef enum {
    RGTP_EVENT_TRANSFER_START,
    RGTP_EVENT_TRANSFER_END,
    RGTP_EVENT_PACKET_SENT,
    RGTP_EVENT_PACKET_RECEIVED,
    RGTP_EVENT_RETRANSMIT,
    RGTP_EVENT_CONGESTION_ADJUSTED,
    RGTP_EVENT_RTT_MEASURED,
    RGTP_EVENT_BANDWIDTH_ESTIMATED,
    RGTP_EVENT_ERROR_OCCURRED
} rgtp_event_type_t;

typedef struct {
    rgtp_event_type_t type;
    struct timespec timestamp;
    uint64_t transfer_id;
    uint64_t session_id;
    uint32_t chunk_id;
    uint32_t bytes;
    float value;  // Generic value field for metrics
    char details[256];  // Event-specific details
} rgtp_event_t;

typedef struct {
    // Transfer metrics
    uint64_t total_bytes_sent;
    uint64_t total_bytes_received;
    uint64_t total_packets_sent;
    uint64_t total_packets_received;
    uint64_t total_retransmissions;
    uint64_t total_errors;
    
    // Time-based metrics
    struct timespec start_time;
    struct timespec end_time;
    double total_duration_ms;
    
    // Performance metrics
    double avg_throughput_mbps;
    double peak_throughput_mbps;
    float avg_rtt_ms;
    float min_rtt_ms;
    float max_rtt_ms;
    float packet_loss_rate;
    float jitter_ms;
    
    // Congestion control metrics
    uint32_t avg_rate_adjustments;
    uint32_t total_rate_decreases;
    uint32_t total_rate_increases;
    
    // Resource utilization
    uint32_t peak_memory_usage_kb;
    uint32_t avg_memory_usage_kb;
    float cpu_utilization_percent;
} rgtp_performance_metrics_t;

typedef struct rgtp_perf_monitor rgtp_perf_monitor_t;

/* -------------------------------------------------------------------------- */
/* Performance Monitor Configuration                                         */
/* -------------------------------------------------------------------------- */

typedef struct {
    int enable_realtime_monitoring;
    int enable_event_logging;
    int enable_statistics_aggregation;
    int log_level;  // 0=off, 1=errors, 2=warnings, 3=info, 4=debug
    char* log_file_path;
    int max_log_file_size_mb;
    int flush_interval_ms;
    int sample_interval_ms;
    void* user_context;
} rgtp_perf_config_t;

/* -------------------------------------------------------------------------- */
/* Performance Monitor API Functions                                         */
/* -------------------------------------------------------------------------- */

// Monitor lifecycle functions
rgtp_perf_monitor_t* rgtp_perf_monitor_create(const rgtp_perf_config_t* config);
int rgtp_perf_monitor_start(rgtp_perf_monitor_t* monitor);
int rgtp_perf_monitor_stop(rgtp_perf_monitor_t* monitor);
void rgtp_perf_monitor_destroy(rgtp_perf_monitor_t* monitor);

// Event recording functions
int rgtp_perf_record_event(rgtp_perf_monitor_t* monitor, const rgtp_event_t* event);
int rgtp_perf_record_transfer_start(rgtp_perf_monitor_t* monitor, uint64_t transfer_id, uint64_t session_id, uint64_t total_size);
int rgtp_perf_record_transfer_end(rgtp_perf_monitor_t* monitor, uint64_t transfer_id, uint64_t session_id, int success);
int rgtp_perf_record_packet_sent(rgtp_perf_monitor_t* monitor, uint64_t transfer_id, uint32_t chunk_id, uint32_t size);
int rgtp_perf_record_packet_received(rgtp_perf_monitor_t* monitor, uint64_t transfer_id, uint32_t chunk_id, uint32_t size);
int rgtp_perf_record_retransmit(rgtp_perf_monitor_t* monitor, uint64_t transfer_id, uint32_t chunk_id);
int rgtp_perf_record_congestion_adjustment(rgtp_perf_monitor_t* monitor, uint64_t transfer_id, float old_rate, float new_rate);
int rgtp_perf_record_rtt_measurement(rgtp_perf_monitor_t* monitor, uint64_t transfer_id, float rtt_ms);
int rgtp_perf_record_error(rgtp_perf_monitor_t* monitor, uint64_t transfer_id, int error_code, const char* error_msg);

// Metrics retrieval functions
int rgtp_perf_get_current_metrics(rgtp_perf_monitor_t* monitor, rgtp_performance_metrics_t* metrics);
int rgtp_perf_get_transfer_metrics(rgtp_perf_monitor_t* monitor, uint64_t transfer_id, rgtp_performance_metrics_t* metrics);
int rgtp_perf_get_session_metrics(rgtp_perf_monitor_t* monitor, uint64_t session_id, rgtp_performance_metrics_t* metrics);

// Analytics and reporting functions
int rgtp_perf_generate_report(rgtp_perf_monitor_t* monitor, const char* output_path);
char* rgtp_perf_format_json_report(rgtp_perf_monitor_t* monitor);
char* rgtp_perf_format_csv_report(rgtp_perf_monitor_t* monitor);
int rgtp_perf_export_events(rgtp_perf_monitor_t* monitor, const char* output_path);

// Configuration functions
void rgtp_perf_config_init(rgtp_perf_config_t* config);
void rgtp_perf_config_set_defaults(rgtp_perf_config_t* config);
int rgtp_perf_config_set_log_level(rgtp_perf_config_t* config, int level);
int rgtp_perf_config_set_log_file(rgtp_perf_config_t* config, const char* path);
int rgtp_perf_config_set_sample_interval(rgtp_perf_config_t* config, int interval_ms);

// Utility functions
double rgtp_perf_timestamp_diff_ms(const struct timespec* start, const struct timespec* end);
uint64_t rgtp_perf_generate_id();
float rgtp_perf_calculate_moving_avg(float current_avg, float new_value, int sample_count);

#ifdef __cplusplus
}
#endif

#endif // PERF_MONITOR_H