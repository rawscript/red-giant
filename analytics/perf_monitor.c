#include "perf_monitor.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#include <pthread.h>
#endif

// Internal structure for the performance monitor
struct rgtp_perf_monitor {
    rgtp_perf_config_t config;
    int running;
    
    // Metrics storage
    rgtp_performance_metrics_t current_metrics;
    
    // Thread safety
#ifdef _WIN32
    CRITICAL_SECTION mutex;
#else
    pthread_mutex_t mutex;
#endif
    
    // Event storage (simple circular buffer for now)
    rgtp_event_t* event_buffer;
    size_t event_buffer_size;
    size_t event_count;
    size_t event_index;
    
    // File logging
    FILE* log_file;
};

/* -------------------------------------------------------------------------- */
/* Utility Functions                                                          */
/* -------------------------------------------------------------------------- */

double rgtp_perf_timestamp_diff_ms(const struct timespec* start, const struct timespec* end) {
    double start_ms = start->tv_sec * 1000.0 + start->tv_nsec / 1000000.0;
    double end_ms = end->tv_sec * 1000.0 + end->tv_nsec / 1000000.0;
    return end_ms - start_ms;
}

uint64_t rgtp_perf_generate_id() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ((uint64_t)ts.tv_sec << 32) | (uint32_t)ts.tv_nsec;
}

float rgtp_perf_calculate_moving_avg(float current_avg, float new_value, int sample_count) {
    if (sample_count <= 1) {
        return new_value;
    }
    // Simple moving average formula
    return ((current_avg * (sample_count - 1)) + new_value) / sample_count;
}

/* -------------------------------------------------------------------------- */
/* Configuration Functions                                                    */
/* -------------------------------------------------------------------------- */

void rgtp_perf_config_init(rgtp_perf_config_t* config) {
    if (!config) return;
    
    memset(config, 0, sizeof(rgtp_perf_config_t));
    rgtp_perf_config_set_defaults(config);
}

void rgtp_perf_config_set_defaults(rgtp_perf_config_t* config) {
    if (!config) return;
    
    config->enable_realtime_monitoring = 1;
    config->enable_event_logging = 1;
    config->enable_statistics_aggregation = 1;
    config->log_level = 3;  // Info level
    config->log_file_path = NULL;
    config->max_log_file_size_mb = 10;
    config->flush_interval_ms = 1000;
    config->sample_interval_ms = 100;
    config->user_context = NULL;
}

int rgtp_perf_config_set_log_level(rgtp_perf_config_t* config, int level) {
    if (!config) return -1;
    if (level < 0 || level > 4) return -1;
    
    config->log_level = level;
    return 0;
}

int rgtp_perf_config_set_log_file(rgtp_perf_config_t* config, const char* path) {
    if (!config) return -1;
    if (!path) return -1;
    
    if (config->log_file_path) {
        free(config->log_file_path);
    }
    
    config->log_file_path = strdup(path);
    return config->log_file_path ? 0 : -1;
}

int rgtp_perf_config_set_sample_interval(rgtp_perf_config_t* config, int interval_ms) {
    if (!config) return -1;
    if (interval_ms <= 0) return -1;
    
    config->sample_interval_ms = interval_ms;
    return 0;
}

/* -------------------------------------------------------------------------- */
/* Monitor Lifecycle Functions                                                */
/* -------------------------------------------------------------------------- */

rgtp_perf_monitor_t* rgtp_perf_monitor_create(const rgtp_perf_config_t* config) {
    if (!config) return NULL;
    
    rgtp_perf_monitor_t* monitor = calloc(1, sizeof(rgtp_perf_monitor_t));
    if (!monitor) return NULL;
    
    // Copy configuration
    monitor->config = *config;
    if (config->log_file_path) {
        monitor->config.log_file_path = strdup(config->log_file_path);
    }
    
    monitor->running = 0;
    
    // Initialize metrics
    memset(&monitor->current_metrics, 0, sizeof(rgtp_performance_metrics_t));
    clock_gettime(CLOCK_MONOTONIC, &monitor->current_metrics.start_time);
    
    // Initialize event buffer (simple circular buffer)
    monitor->event_buffer_size = 10000; // 10k events max
    monitor->event_buffer = calloc(monitor->event_buffer_size, sizeof(rgtp_event_t));
    if (!monitor->event_buffer) {
        free(monitor);
        return NULL;
    }
    monitor->event_count = 0;
    monitor->event_index = 0;
    
    // Initialize mutex
#ifdef _WIN32
    InitializeCriticalSection(&monitor->mutex);
#else
    pthread_mutex_init(&monitor->mutex, NULL);
#endif
    
    // Open log file if specified
    if (monitor->config.log_file_path) {
        monitor->log_file = fopen(monitor->config.log_file_path, "a");
    }
    
    return monitor;
}

int rgtp_perf_monitor_start(rgtp_perf_monitor_t* monitor) {
    if (!monitor) return -1;
    
    // Lock mutex
#ifdef _WIN32
    EnterCriticalSection(&monitor->mutex);
#else
    pthread_mutex_lock(&monitor->mutex);
#endif
    
    if (monitor->running) {
#ifdef _WIN32
        LeaveCriticalSection(&monitor->mutex);
#else
        pthread_mutex_unlock(&monitor->mutex);
#endif
        return -1; // Already running
    }
    
    monitor->running = 1;
    
    // Reset metrics start time
    clock_gettime(CLOCK_MONOTONIC, &monitor->current_metrics.start_time);
    
    // Unlock mutex
#ifdef _WIN32
    LeaveCriticalSection(&monitor->mutex);
#else
    pthread_mutex_unlock(&monitor->mutex);
#endif
    
    return 0;
}

int rgtp_perf_monitor_stop(rgtp_perf_monitor_t* monitor) {
    if (!monitor) return -1;
    
    // Lock mutex
#ifdef _WIN32
    EnterCriticalSection(&monitor->mutex);
#else
    pthread_mutex_lock(&monitor->mutex);
#endif
    
    if (!monitor->running) {
#ifdef _WIN32
        LeaveCriticalSection(&monitor->mutex);
#else
        pthread_mutex_unlock(&monitor->mutex);
#endif
        return -1; // Not running
    }
    
    monitor->running = 0;
    
    // Record end time
    clock_gettime(CLOCK_MONOTONIC, &monitor->current_metrics.end_time);
    monitor->current_metrics.total_duration_ms = 
        rgtp_perf_timestamp_diff_ms(&monitor->current_metrics.start_time, 
                                   &monitor->current_metrics.end_time);
    
    // Unlock mutex
#ifdef _WIN32
    LeaveCriticalSection(&monitor->mutex);
#else
    pthread_mutex_unlock(&monitor->mutex);
#endif
    
    return 0;
}

void rgtp_perf_monitor_destroy(rgtp_perf_monitor_t* monitor) {
    if (!monitor) return;
    
    // Stop the monitor if running
    rgtp_perf_monitor_stop(monitor);
    
    // Close log file
    if (monitor->log_file) {
        fclose(monitor->log_file);
    }
    
    // Free event buffer
    if (monitor->event_buffer) {
        free(monitor->event_buffer);
    }
    
    // Free config log path
    if (monitor->config.log_file_path) {
        free(monitor->config.log_file_path);
    }
    
    // Clean up mutex
#ifdef _WIN32
    DeleteCriticalSection(&monitor->mutex);
#else
    pthread_mutex_destroy(&monitor->mutex);
#endif
    
    free(monitor);
}

/* -------------------------------------------------------------------------- */
/* Event Recording Functions                                                  */
/* -------------------------------------------------------------------------- */

int rgtp_perf_record_event(rgtp_perf_monitor_t* monitor, const rgtp_event_t* event) {
    if (!monitor || !event) return -1;
    
    // Lock mutex
#ifdef _WIN32
    EnterCriticalSection(&monitor->mutex);
#else
    pthread_mutex_lock(&monitor->mutex);
#endif
    
    // Add event to circular buffer
    if (monitor->event_buffer) {
        monitor->event_buffer[monitor->event_index] = *event;
        monitor->event_index = (monitor->event_index + 1) % monitor->event_buffer_size;
        if (monitor->event_count < monitor->event_buffer_size) {
            monitor->event_count++;
        }
    }
    
    // Log to file if configured
    if (monitor->log_file && monitor->config.log_level >= 3) {
        fprintf(monitor->log_file, "[%ld.%09ld] EVENT_%d: transfer=%llu, session=%llu, chunk=%u, bytes=%u, value=%.3f\n",
                event->timestamp.tv_sec, event->timestamp.tv_nsec,
                event->type, 
                (unsigned long long)event->transfer_id,
                (unsigned long long)event->session_id,
                event->chunk_id, event->bytes, event->value);
        fflush(monitor->log_file);
    }
    
    // Unlock mutex
#ifdef _WIN32
    LeaveCriticalSection(&monitor->mutex);
#else
    pthread_mutex_unlock(&monitor->mutex);
#endif
    
    return 0;
}

int rgtp_perf_record_transfer_start(rgtp_perf_monitor_t* monitor, uint64_t transfer_id, uint64_t session_id, uint64_t total_size) {
    if (!monitor) return -1;
    
    rgtp_event_t event = {0};
    event.type = RGTP_EVENT_TRANSFER_START;
    clock_gettime(CLOCK_MONOTONIC, &event.timestamp);
    event.transfer_id = transfer_id;
    event.session_id = session_id;
    event.value = (float)total_size;
    snprintf(event.details, sizeof(event.details), "Starting transfer of %llu bytes", (unsigned long long)total_size);
    
    return rgtp_perf_record_event(monitor, &event);
}

int rgtp_perf_record_transfer_end(rgtp_perf_monitor_t* monitor, uint64_t transfer_id, uint64_t session_id, int success) {
    if (!monitor) return -1;
    
    rgtp_event_t event = {0};
    event.type = RGTP_EVENT_TRANSFER_END;
    clock_gettime(CLOCK_MONOTONIC, &event.timestamp);
    event.transfer_id = transfer_id;
    event.session_id = session_id;
    event.value = success ? 1.0f : 0.0f;
    snprintf(event.details, sizeof(event.details), "Transfer ended: %s", success ? "SUCCESS" : "FAILED");
    
    return rgtp_perf_record_event(monitor, &event);
}

int rgtp_perf_record_packet_sent(rgtp_perf_monitor_t* monitor, uint64_t transfer_id, uint32_t chunk_id, uint32_t size) {
    if (!monitor) return -1;
    
    rgtp_event_t event = {0};
    event.type = RGTP_EVENT_PACKET_SENT;
    clock_gettime(CLOCK_MONOTONIC, &event.timestamp);
    event.transfer_id = transfer_id;
    event.chunk_id = chunk_id;
    event.bytes = size;
    event.value = (float)size;
    snprintf(event.details, sizeof(event.details), "Packet sent: chunk %u, %u bytes", chunk_id, size);
    
    // Update metrics
#ifdef _WIN32
    EnterCriticalSection(&monitor->mutex);
#else
    pthread_mutex_lock(&monitor->mutex);
#endif
    monitor->current_metrics.total_bytes_sent += size;
    monitor->current_metrics.total_packets_sent++;
#ifdef _WIN32
    LeaveCriticalSection(&monitor->mutex);
#else
    pthread_mutex_unlock(&monitor->mutex);
#endif
    
    return rgtp_perf_record_event(monitor, &event);
}

int rgtp_perf_record_packet_received(rgtp_perf_monitor_t* monitor, uint64_t transfer_id, uint32_t chunk_id, uint32_t size) {
    if (!monitor) return -1;
    
    rgtp_event_t event = {0};
    event.type = RGTP_EVENT_PACKET_RECEIVED;
    clock_gettime(CLOCK_MONOTONIC, &event.timestamp);
    event.transfer_id = transfer_id;
    event.chunk_id = chunk_id;
    event.bytes = size;
    event.value = (float)size;
    snprintf(event.details, sizeof(event.details), "Packet received: chunk %u, %u bytes", chunk_id, size);
    
    // Update metrics
#ifdef _WIN32
    EnterCriticalSection(&monitor->mutex);
#else
    pthread_mutex_lock(&monitor->mutex);
#endif
    monitor->current_metrics.total_bytes_received += size;
    monitor->current_metrics.total_packets_received++;
#ifdef _WIN32
    LeaveCriticalSection(&monitor->mutex);
#else
    pthread_mutex_unlock(&monitor->mutex);
#endif
    
    return rgtp_perf_record_event(monitor, &event);
}

int rgtp_perf_record_retransmit(rgtp_perf_monitor_t* monitor, uint64_t transfer_id, uint32_t chunk_id) {
    if (!monitor) return -1;
    
    rgtp_event_t event = {0};
    event.type = RGTP_EVENT_RETRANSMIT;
    clock_gettime(CLOCK_MONOTONIC, &event.timestamp);
    event.transfer_id = transfer_id;
    event.chunk_id = chunk_id;
    event.value = 1.0f;
    snprintf(event.details, sizeof(event.details), "Retransmission: chunk %u", chunk_id);
    
    // Update metrics
#ifdef _WIN32
    EnterCriticalSection(&monitor->mutex);
#else
    pthread_mutex_lock(&monitor->mutex);
#endif
    monitor->current_metrics.total_retransmissions++;
#ifdef _WIN32
    LeaveCriticalSection(&monitor->mutex);
#else
    pthread_mutex_unlock(&monitor->mutex);
#endif
    
    return rgtp_perf_record_event(monitor, &event);
}

int rgtp_perf_record_congestion_adjustment(rgtp_perf_monitor_t* monitor, uint64_t transfer_id, float old_rate, float new_rate) {
    if (!monitor) return -1;
    
    rgtp_event_t event = {0};
    event.type = RGTP_EVENT_CONGESTION_ADJUSTED;
    clock_gettime(CLOCK_MONOTONIC, &event.timestamp);
    event.transfer_id = transfer_id;
    event.value = new_rate;
    snprintf(event.details, sizeof(event.details), "Rate adjusted: %.2f -> %.2f", old_rate, new_rate);
    
    // Update metrics
#ifdef _WIN32
    EnterCriticalSection(&monitor->mutex);
#else
    pthread_mutex_lock(&monitor->mutex);
#endif
    monitor->current_metrics.avg_rate_adjustments++;
    if (new_rate < old_rate) {
        monitor->current_metrics.total_rate_decreases++;
    } else if (new_rate > old_rate) {
        monitor->current_metrics.total_rate_increases++;
    }
#ifdef _WIN32
    LeaveCriticalSection(&monitor->mutex);
#else
    pthread_mutex_unlock(&monitor->mutex);
#endif
    
    return rgtp_perf_record_event(monitor, &event);
}

int rgtp_perf_record_rtt_measurement(rgtp_perf_monitor_t* monitor, uint64_t transfer_id, float rtt_ms) {
    if (!monitor) return -1;
    
    rgtp_event_t event = {0};
    event.type = RGTP_EVENT_RTT_MEASURED;
    clock_gettime(CLOCK_MONOTONIC, &event.timestamp);
    event.transfer_id = transfer_id;
    event.value = rtt_ms;
    snprintf(event.details, sizeof(event.details), "RTT measured: %.2f ms", rtt_ms);
    
    // Update metrics
#ifdef _WIN32
    EnterCriticalSection(&monitor->mutex);
#else
    pthread_mutex_lock(&monitor->mutex);
#endif
    if (monitor->current_metrics.avg_rtt_ms == 0) {
        monitor->current_metrics.avg_rtt_ms = rtt_ms;
        monitor->current_metrics.min_rtt_ms = rtt_ms;
        monitor->current_metrics.max_rtt_ms = rtt_ms;
    } else {
        monitor->current_metrics.avg_rtt_ms = 
            rgtp_perf_calculate_moving_avg(monitor->current_metrics.avg_rtt_ms, rtt_ms, 
                                          (int)monitor->current_metrics.total_packets_sent);
        if (rtt_ms < monitor->current_metrics.min_rtt_ms) {
            monitor->current_metrics.min_rtt_ms = rtt_ms;
        }
        if (rtt_ms > monitor->current_metrics.max_rtt_ms) {
            monitor->current_metrics.max_rtt_ms = rtt_ms;
        }
    }
#ifdef _WIN32
    LeaveCriticalSection(&monitor->mutex);
#else
    pthread_mutex_unlock(&monitor->mutex);
#endif
    
    return rgtp_perf_record_event(monitor, &event);
}

int rgtp_perf_record_error(rgtp_perf_monitor_t* monitor, uint64_t transfer_id, int error_code, const char* error_msg) {
    if (!monitor || !error_msg) return -1;
    
    rgtp_event_t event = {0};
    event.type = RGTP_EVENT_ERROR_OCCURRED;
    clock_gettime(CLOCK_MONOTONIC, &event.timestamp);
    event.transfer_id = transfer_id;
    event.value = (float)error_code;
    snprintf(event.details, sizeof(event.details), "Error %d: %s", error_code, error_msg);
    
    // Update metrics
#ifdef _WIN32
    EnterCriticalSection(&monitor->mutex);
#else
    pthread_mutex_lock(&monitor->mutex);
#endif
    monitor->current_metrics.total_errors++;
#ifdef _WIN32
    LeaveCriticalSection(&monitor->mutex);
#else
    pthread_mutex_unlock(&monitor->mutex);
#endif
    
    return rgtp_perf_record_event(monitor, &event);
}

/* -------------------------------------------------------------------------- */
/* Metrics Retrieval Functions                                                */
/* -------------------------------------------------------------------------- */

int rgtp_perf_get_current_metrics(rgtp_perf_monitor_t* monitor, rgtp_performance_metrics_t* metrics) {
    if (!monitor || !metrics) return -1;
    
    // Lock mutex to get consistent metrics
#ifdef _WIN32
    EnterCriticalSection(&monitor->mutex);
#else
    pthread_mutex_lock(&monitor->mutex);
#endif
    
    *metrics = monitor->current_metrics;
    
    // Calculate derived metrics
    if (metrics->total_duration_ms > 0) {
        metrics->avg_throughput_mbps = (metrics->total_bytes_sent * 8.0) / (metrics->total_duration_ms * 1000.0);
    }
    
    if (metrics->total_packets_sent > 0) {
        metrics->packet_loss_rate = (float)metrics->total_retransmissions / (float)metrics->total_packets_sent;
    }
    
    // Unlock mutex
#ifdef _WIN32
    LeaveCriticalSection(&monitor->mutex);
#else
    pthread_mutex_unlock(&monitor->mutex);
#endif
    
    return 0;
}

int rgtp_perf_get_transfer_metrics(rgtp_perf_monitor_t* monitor, uint64_t transfer_id, rgtp_performance_metrics_t* metrics) {
    // For now, return current metrics
    // In a complete implementation, this would aggregate metrics for specific transfer
    return rgtp_perf_get_current_metrics(monitor, metrics);
}

int rgtp_perf_get_session_metrics(rgtp_perf_monitor_t* monitor, uint64_t session_id, rgtp_performance_metrics_t* metrics) {
    // For now, return current metrics
    // In a complete implementation, this would aggregate metrics for specific session
    return rgtp_perf_get_current_metrics(monitor, metrics);
}

/* -------------------------------------------------------------------------- */
/* Analytics and Reporting Functions                                          */
/* -------------------------------------------------------------------------- */

int rgtp_perf_generate_report(rgtp_perf_monitor_t* monitor, const char* output_path) {
    if (!monitor || !output_path) return -1;
    
    FILE* file = fopen(output_path, "w");
    if (!file) return -1;
    
    rgtp_performance_metrics_t metrics;
    if (rgtp_perf_get_current_metrics(monitor, &metrics) != 0) {
        fclose(file);
        return -1;
    }
    
    fprintf(file, "RGTP Performance Report\n");
    fprintf(file, "=======================\n\n");
    
    fprintf(file, "Transfer Metrics:\n");
    fprintf(file, "  Total Bytes Sent: %llu\n", (unsigned long long)metrics.total_bytes_sent);
    fprintf(file, "  Total Bytes Received: %llu\n", (unsigned long long)metrics.total_bytes_received);
    fprintf(file, "  Total Packets Sent: %llu\n", (unsigned long long)metrics.total_packets_sent);
    fprintf(file, "  Total Packets Received: %llu\n", (unsigned long long)metrics.total_packets_received);
    fprintf(file, "  Retransmissions: %llu\n", (unsigned long long)metrics.total_retransmissions);
    fprintf(file, "  Errors: %llu\n", (unsigned long long)metrics.total_errors);
    fprintf(file, "\n");
    
    fprintf(file, "Performance Metrics:\n");
    fprintf(file, "  Total Duration: %.2f ms\n", metrics.total_duration_ms);
    fprintf(file, "  Average Throughput: %.2f Mbps\n", metrics.avg_throughput_mbps);
    fprintf(file, "  Peak Throughput: %.2f Mbps\n", metrics.peak_throughput_mbps);
    fprintf(file, "  Average RTT: %.2f ms\n", metrics.avg_rtt_ms);
    fprintf(file, "  Min RTT: %.2f ms\n", metrics.min_rtt_ms);
    fprintf(file, "  Max RTT: %.2f ms\n", metrics.max_rtt_ms);
    fprintf(file, "  Packet Loss Rate: %.4f\n", metrics.packet_loss_rate);
    fprintf(file, "  Jitter: %.2f ms\n", metrics.jitter_ms);
    fprintf(file, "\n");
    
    fprintf(file, "Congestion Control Metrics:\n");
    fprintf(file, "  Rate Adjustments: %u\n", metrics.avg_rate_adjustments);
    fprintf(file, "  Rate Decreases: %u\n", metrics.total_rate_decreases);
    fprintf(file, "  Rate Increases: %u\n", metrics.total_rate_increases);
    fprintf(file, "\n");
    
    fprintf(file, "Resource Utilization:\n");
    fprintf(file, "  Peak Memory Usage: %u KB\n", metrics.peak_memory_usage_kb);
    fprintf(file, "  Average Memory Usage: %u KB\n", metrics.avg_memory_usage_kb);
    fprintf(file, "  CPU Utilization: %.2f%%\n", metrics.cpu_utilization_percent);
    
    fclose(file);
    return 0;
}

char* rgtp_perf_format_json_report(rgtp_perf_monitor_t* monitor) {
    if (!monitor) return NULL;
    
    rgtp_performance_metrics_t metrics;
    if (rgtp_perf_get_current_metrics(monitor, &metrics) != 0) {
        return NULL;
    }
    
    // Allocate buffer for JSON (approximate size)
    char* json_buffer = malloc(2048);
    if (!json_buffer) return NULL;
    
    snprintf(json_buffer, 2048,
        "{\n"
        "  \"report_type\": \"rgtp_performance\",\n"
        "  \"timestamp\": \"%ld\",\n"
        "  \"transfer_metrics\": {\n"
        "    \"total_bytes_sent\": %llu,\n"
        "    \"total_bytes_received\": %llu,\n"
        "    \"total_packets_sent\": %llu,\n"
        "    \"total_packets_received\": %llu,\n"
        "    \"total_retransmissions\": %llu,\n"
        "    \"total_errors\": %llu\n"
        "  },\n"
        "  \"performance_metrics\": {\n"
        "    \"total_duration_ms\": %.2f,\n"
        "    \"avg_throughput_mbps\": %.2f,\n"
        "    \"peak_throughput_mbps\": %.2f,\n"
        "    \"avg_rtt_ms\": %.2f,\n"
        "    \"min_rtt_ms\": %.2f,\n"
        "    \"max_rtt_ms\": %.2f,\n"
        "    \"packet_loss_rate\": %.4f,\n"
        "    \"jitter_ms\": %.2f\n"
        "  },\n"
        "  \"congestion_control\": {\n"
        "    \"rate_adjustments\": %u,\n"
        "    \"rate_decreases\": %u,\n"
        "    \"rate_increases\": %u\n"
        "  },\n"
        "  \"resource_utilization\": {\n"
        "    \"peak_memory_kb\": %u,\n"
        "    \"avg_memory_kb\": %u,\n"
        "    \"cpu_utilization_percent\": %.2f\n"
        "  }\n"
        "}\n",
        time(NULL),
        (unsigned long long)metrics.total_bytes_sent,
        (unsigned long long)metrics.total_bytes_received,
        (unsigned long long)metrics.total_packets_sent,
        (unsigned long long)metrics.total_packets_received,
        (unsigned long long)metrics.total_retransmissions,
        (unsigned long long)metrics.total_errors,
        metrics.total_duration_ms,
        metrics.avg_throughput_mbps,
        metrics.peak_throughput_mbps,
        metrics.avg_rtt_ms,
        metrics.min_rtt_ms,
        metrics.max_rtt_ms,
        metrics.packet_loss_rate,
        metrics.jitter_ms,
        metrics.avg_rate_adjustments,
        metrics.total_rate_decreases,
        metrics.total_rate_increases,
        metrics.peak_memory_usage_kb,
        metrics.avg_memory_usage_kb,
        metrics.cpu_utilization_percent
    );
    
    return json_buffer;
}

char* rgtp_perf_format_csv_report(rgtp_perf_monitor_t* monitor) {
    if (!monitor) return NULL;
    
    rgtp_performance_metrics_t metrics;
    if (rgtp_perf_get_current_metrics(monitor, &metrics) != 0) {
        return NULL;
    }
    
    // Allocate buffer for CSV (approximate size)
    char* csv_buffer = malloc(1024);
    if (!csv_buffer) return NULL;
    
    snprintf(csv_buffer, 1024,
        "metric,value\n"
        "total_bytes_sent,%llu\n"
        "total_bytes_received,%llu\n"
        "total_packets_sent,%llu\n"
        "total_packets_received,%llu\n"
        "total_retransmissions,%llu\n"
        "total_errors,%llu\n"
        "total_duration_ms,%.2f\n"
        "avg_throughput_mbps,%.2f\n"
        "peak_throughput_mbps,%.2f\n"
        "avg_rtt_ms,%.2f\n"
        "min_rtt_ms,%.2f\n"
        "max_rtt_ms,%.2f\n"
        "packet_loss_rate,%.4f\n"
        "jitter_ms,%.2f\n"
        "rate_adjustments,%u\n"
        "rate_decreases,%u\n"
        "rate_increases,%u\n"
        "peak_memory_kb,%u\n"
        "avg_memory_kb,%u\n"
        "cpu_utilization_percent,%.2f\n",
        (unsigned long long)metrics.total_bytes_sent,
        (unsigned long long)metrics.total_bytes_received,
        (unsigned long long)metrics.total_packets_sent,
        (unsigned long long)metrics.total_packets_received,
        (unsigned long long)metrics.total_retransmissions,
        (unsigned long long)metrics.total_errors,
        metrics.total_duration_ms,
        metrics.avg_throughput_mbps,
        metrics.peak_throughput_mbps,
        metrics.avg_rtt_ms,
        metrics.min_rtt_ms,
        metrics.max_rtt_ms,
        metrics.packet_loss_rate,
        metrics.jitter_ms,
        metrics.avg_rate_adjustments,
        metrics.total_rate_decreases,
        metrics.total_rate_increases,
        metrics.peak_memory_usage_kb,
        metrics.avg_memory_usage_kb,
        metrics.cpu_utilization_percent
    );
    
    return csv_buffer;
}

int rgtp_perf_export_events(rgtp_perf_monitor_t* monitor, const char* output_path) {
    if (!monitor || !output_path) return -1;
    
    FILE* file = fopen(output_path, "w");
    if (!file) return -1;
    
    // Lock mutex to access event buffer consistently
#ifdef _WIN32
    EnterCriticalSection(&monitor->mutex);
#else
    pthread_mutex_lock(&monitor->mutex);
#endif
    
    fprintf(file, "timestamp,event_type,transfer_id,session_id,chunk_id,bytes,value,details\n");
    
    size_t start_idx = 0;
    if (monitor->event_count == monitor->event_buffer_size) {
        // Buffer is full, start from oldest position
        start_idx = monitor->event_index;
    }
    
    for (size_t i = 0; i < monitor->event_count; i++) {
        size_t idx = (start_idx + i) % monitor->event_buffer_size;
        rgtp_event_t* event = &monitor->event_buffer[idx];
        
        fprintf(file, "%ld.%09ld,%d,%llu,%llu,%u,%u,%.3f,\"%s\"\n",
                event->timestamp.tv_sec, event->timestamp.tv_nsec,
                event->type,
                (unsigned long long)event->transfer_id,
                (unsigned long long)event->session_id,
                event->chunk_id, event->bytes, event->value,
                event->details);
    }
    
    // Unlock mutex
#ifdef _WIN32
    LeaveCriticalSection(&monitor->mutex);
#else
    pthread_mutex_unlock(&monitor->mutex);
#endif
    
    fclose(file);
    return 0;
}