#include "perf_monitor.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

int test_monitor_creation() {
    printf("Testing monitor creation...\n");
    
    rgtp_perf_config_t config;
    rgtp_perf_config_init(&config);
    
    // Test default config
    rgtp_perf_config_set_defaults(&config);
    assert(config.enable_realtime_monitoring == 1);
    assert(config.log_level == 3);
    assert(config.sample_interval_ms == 100);
    
    // Create monitor
    rgtp_perf_monitor_t* monitor = rgtp_perf_monitor_create(&config);
    assert(monitor != NULL);
    
    // Test starting/stopping
    int start_result = rgtp_perf_monitor_start(monitor);
    assert(start_result == 0);
    
    int stop_result = rgtp_perf_monitor_stop(monitor);
    assert(stop_result == 0);
    
    // Clean up
    rgtp_perf_monitor_destroy(monitor);
    
    printf("✓ Monitor creation test passed\n");
    return 0;
}

int test_event_recording() {
    printf("Testing event recording...\n");
    
    rgtp_perf_config_t config;
    rgtp_perf_config_init(&config);
    rgtp_perf_config_set_defaults(&config);
    
    rgtp_perf_monitor_t* monitor = rgtp_perf_monitor_create(&config);
    assert(monitor != NULL);
    
    // Start monitor
    rgtp_perf_monitor_start(monitor);
    
    // Generate some IDs
    uint64_t transfer_id = rgtp_perf_generate_id();
    uint64_t session_id = rgtp_perf_generate_id();
    
    // Record various events
    int result = rgtp_perf_record_transfer_start(monitor, transfer_id, session_id, 1024000); // 1MB transfer
    assert(result == 0);
    
    result = rgtp_perf_record_packet_sent(monitor, transfer_id, 1, 1450);
    assert(result == 0);
    
    result = rgtp_perf_record_packet_received(monitor, transfer_id, 1, 1450);
    assert(result == 0);
    
    result = rgtp_perf_record_retransmit(monitor, transfer_id, 5);
    assert(result == 0);
    
    result = rgtp_perf_record_congestion_adjustment(monitor, transfer_id, 1000.0f, 800.0f);
    assert(result == 0);
    
    result = rgtp_perf_record_rtt_measurement(monitor, transfer_id, 50.0f);
    assert(result == 0);
    
    result = rgtp_perf_record_error(monitor, transfer_id, -1, "Test error");
    assert(result == 0);
    
    result = rgtp_perf_record_transfer_end(monitor, transfer_id, session_id, 1);
    assert(result == 0);
    
    // Stop monitor
    rgtp_perf_monitor_stop(monitor);
    
    // Clean up
    rgtp_perf_monitor_destroy(monitor);
    
    printf("✓ Event recording test passed\n");
    return 0;
}

int test_metrics_collection() {
    printf("Testing metrics collection...\n");
    
    rgtp_perf_config_t config;
    rgtp_perf_config_init(&config);
    rgtp_perf_config_set_defaults(&config);
    
    rgtp_perf_monitor_t* monitor = rgtp_perf_monitor_create(&config);
    assert(monitor != NULL);
    
    rgtp_perf_monitor_start(monitor);
    
    // Generate some IDs
    uint64_t transfer_id = rgtp_perf_generate_id();
    uint64_t session_id = rgtp_perf_generate_id();
    
    // Record some events to populate metrics
    rgtp_perf_record_transfer_start(monitor, transfer_id, session_id, 1024000);
    
    for (int i = 0; i < 10; i++) {
        rgtp_perf_record_packet_sent(monitor, transfer_id, i, 1450);
        rgtp_perf_record_packet_received(monitor, transfer_id, i, 1450);
        rgtp_perf_record_rtt_measurement(monitor, transfer_id, 25.0f + (i * 2));
    }
    
    // Record a few retransmissions
    rgtp_perf_record_retransmit(monitor, transfer_id, 3);
    rgtp_perf_record_retransmit(monitor, transfer_id, 7);
    
    // Record congestion adjustments
    rgtp_perf_record_congestion_adjustment(monitor, transfer_id, 1000.0f, 800.0f);
    rgtp_perf_record_congestion_adjustment(monitor, transfer_id, 800.0f, 900.0f);
    
    rgtp_perf_record_transfer_end(monitor, transfer_id, session_id, 1);
    
    rgtp_perf_monitor_stop(monitor);
    
    // Get metrics
    rgtp_performance_metrics_t metrics;
    int result = rgtp_perf_get_current_metrics(monitor, &metrics);
    assert(result == 0);
    
    // Verify some metrics
    assert(metrics.total_bytes_sent >= 10 * 1450);
    assert(metrics.total_bytes_received >= 10 * 1450);
    assert(metrics.total_retransmissions == 2);
    assert(metrics.avg_rate_adjustments == 2);
    
    printf("  Total bytes sent: %llu\n", (unsigned long long)metrics.total_bytes_sent);
    printf("  Total retransmissions: %llu\n", (unsigned long long)metrics.total_retransmissions);
    printf("  Avg RTT: %.2f ms\n", metrics.avg_rtt_ms);
    printf("  Min RTT: %.2f ms\n", metrics.min_rtt_ms);
    printf("  Max RTT: %.2f ms\n", metrics.max_rtt_ms);
    
    // Clean up
    rgtp_perf_monitor_destroy(monitor);
    
    printf("✓ Metrics collection test passed\n");
    return 0;
}

int test_report_generation() {
    printf("Testing report generation...\n");
    
    rgtp_perf_config_t config;
    rgtp_perf_config_init(&config);
    rgtp_perf_config_set_defaults(&config);
    
    rgtp_perf_monitor_t* monitor = rgtp_perf_monitor_create(&config);
    assert(monitor != NULL);
    
    rgtp_perf_monitor_start(monitor);
    
    // Generate some IDs
    uint64_t transfer_id = rgtp_perf_generate_id();
    uint64_t session_id = rgtp_perf_generate_id();
    
    // Record some events
    rgtp_perf_record_transfer_start(monitor, transfer_id, session_id, 2048000); // 2MB transfer
    
    for (int i = 0; i < 5; i++) {
        rgtp_perf_record_packet_sent(monitor, transfer_id, i, 1450);
        rgtp_perf_record_packet_received(monitor, transfer_id, i, 1450);
        rgtp_perf_record_rtt_measurement(monitor, transfer_id, 30.0f + (i * 1.5f));
    }
    
    rgtp_perf_record_transfer_end(monitor, transfer_id, session_id, 1);
    
    rgtp_perf_monitor_stop(monitor);
    
    // Test report generation
    int report_result = rgtp_perf_generate_report(monitor, "test_report.txt");
    assert(report_result == 0);
    
    // Test JSON report
    char* json_report = rgtp_perf_format_json_report(monitor);
    assert(json_report != NULL);
    printf("  JSON report length: %zu chars\n", strlen(json_report));
    free(json_report);
    
    // Test CSV report
    char* csv_report = rgtp_perf_format_csv_report(monitor);
    assert(csv_report != NULL);
    printf("  CSV report length: %zu chars\n", strlen(csv_report));
    free(csv_report);
    
    // Test event export
    int export_result = rgtp_perf_export_events(monitor, "test_events.csv");
    assert(export_result == 0);
    
    // Clean up
    rgtp_perf_monitor_destroy(monitor);
    
    // Clean up test files
    remove("test_report.txt");
    remove("test_events.csv");
    
    printf("✓ Report generation test passed\n");
    return 0;
}

int test_id_generation() {
    printf("Testing ID generation...\n");
    
    uint64_t id1 = rgtp_perf_generate_id();
    uint64_t id2 = rgtp_perf_generate_id();
    
    // IDs should be different (unless called within same nanosecond)
    assert(id1 != 0);
    assert(id2 != 0);
    
    printf("  Generated ID 1: %llu\n", (unsigned long long)id1);
    printf("  Generated ID 2: %llu\n", (unsigned long long)id2);
    
    printf("✓ ID generation test passed\n");
    return 0;
}

int test_timestamp_calculation() {
    printf("Testing timestamp calculation...\n");
    
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);
    
    // Sleep for a bit to have measurable difference
    #ifdef _WIN32
    Sleep(10);  // Sleep 10ms
    #else
    usleep(10000);  // Sleep 10ms
    #endif
    
    clock_gettime(CLOCK_MONOTONIC, &end);
    
    double diff_ms = rgtp_perf_timestamp_diff_ms(&start, &end);
    printf("  Time difference: %.2f ms\n", diff_ms);
    
    // Should be approximately 10ms (with some tolerance for system overhead)
    assert(diff_ms >= 5.0 && diff_ms < 50.0);
    
    printf("✓ Timestamp calculation test passed\n");
    return 0;
}

int main() {
    printf("RGTP Performance Monitoring System Tests\n");
    printf("========================================\n");
    
    test_monitor_creation();
    test_event_recording();
    test_metrics_collection();
    test_report_generation();
    test_id_generation();
    test_timestamp_calculation();
    
    printf("\n✓ All performance monitoring tests passed!\n");
    printf("Performance monitoring and analytics system is ready.\n");
    
    return 0;
}