/**
 * @file simple_puller.c
 * @brief Simple RGTP puller example
 * 
 * This example demonstrates how to pull data using RGTP.
 * It connects to an exposer and downloads the exposed data.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "rgtp/rgtp_sdk.h"

void progress_callback(size_t bytes_transferred, size_t total_bytes, void* user_data) {
    (void)user_data;
    double percent = (double)bytes_transferred / total_bytes * 100.0;
    
    // Simple progress bar
    int bar_width = 50;
    int filled = (int)(percent / 100.0 * bar_width);
    
    printf("\r[");
    for (int i = 0; i < bar_width; i++) {
        if (i < filled) {
            printf("=");
        } else if (i == filled) {
            printf(">");
        } else {
            printf(" ");
        }
    }
    printf("] %.1f%% (%zu/%zu bytes)", percent, bytes_transferred, total_bytes);
    fflush(stdout);
}

void error_callback(int error_code, const char* error_message, void* user_data) {
    (void)user_data;
    fprintf(stderr, "\nError %d: %s\n", error_code, error_message);
}

int main(int argc, char* argv[]) {
    if (argc != 3 && argc != 4) {
        printf("Usage: %s <host> <port> [output_file]\n", argv[0]);
        printf("Examples:\n");
        printf("  %s 192.168.1.100 9999\n", argv[0]);
        printf("  %s localhost 9999 downloaded_file.bin\n", argv[0]);
        return 1;
    }

    const char* host = argv[1];
    uint16_t port = (uint16_t)atoi(argv[2]);
    const char* output_file = (argc == 4) ? argv[3] : "rgtp_download.bin";
    
    printf("RGTP Simple Puller\n");
    printf("==================\n");
    printf("Connecting to: %s:%d\n", host, port);
    printf("Output file: %s\n", output_file);
    
    // Initialize RGTP
    if (rgtp_init() != 0) {
        fprintf(stderr, "Failed to initialize RGTP\n");
        return 1;
    }
    
    // Create configuration
    rgtp_config_t config;
    rgtp_config_default(&config);
    config.progress_cb = progress_callback;
    config.error_cb = error_callback;
    config.adaptive_mode = true;
    config.timeout_ms = 30000; // 30 second timeout
    
    // Auto-detect network type
    printf("Auto-configuring for network conditions...\n");
    rgtp_config_for_wan(&config); // Default to WAN settings for pulling
    
    // Create client
    rgtp_client_t* client = rgtp_client_create_with_config(&config);
    if (!client) {
        fprintf(stderr, "Failed to create RGTP client\n");
        rgtp_cleanup();
        return 1;
    }
    
    printf("Starting pull operation...\n");
    
    // Record start time
    clock_t start_time = clock();
    
    // Pull the file
    int result = rgtp_client_pull_to_file(client, host, port, output_file);
    
    // Calculate elapsed time
    clock_t end_time = clock();
    double elapsed_seconds = ((double)(end_time - start_time)) / CLOCKS_PER_SEC;
    
    if (result == 0) {
        printf("\n✅ Pull completed successfully!\n");
        
        // Get final statistics
        rgtp_stats_t stats;
        if (rgtp_client_get_stats(client, &stats) == 0) {
            char throughput_str[64];
            char avg_throughput_str[64];
            char size_str[64];
            char duration_str[64];
            
            rgtp_format_throughput(stats.throughput_mbps, throughput_str, sizeof(throughput_str));
            rgtp_format_throughput(stats.avg_throughput_mbps, avg_throughput_str, sizeof(avg_throughput_str));
            rgtp_format_size(stats.bytes_transferred, size_str, sizeof(size_str));
            rgtp_format_duration(stats.elapsed_ms, duration_str, sizeof(duration_str));
            
            printf("\n📊 Transfer Statistics:\n");
            printf("   • File size: %s\n", size_str);
            printf("   • Duration: %s\n", duration_str);
            printf("   • Average throughput: %s\n", avg_throughput_str);
            printf("   • Peak throughput: %s\n", throughput_str);
            printf("   • Chunks transferred: %u/%u\n", stats.chunks_transferred, stats.total_chunks);
            printf("   • Retransmissions: %u\n", stats.retransmissions);
            printf("   • Efficiency: %.1f%%\n", 
                   (double)(stats.total_chunks - stats.retransmissions) / stats.total_chunks * 100.0);
        }
        
        printf("   • File saved as: %s\n", output_file);
        
    } else {
        printf("\n❌ Pull failed!\n");
        
        // Try to get partial statistics
        rgtp_stats_t stats;
        if (rgtp_client_get_stats(client, &stats) == 0 && stats.bytes_transferred > 0) {
            char size_str[64];
            rgtp_format_size(stats.bytes_transferred, size_str, sizeof(size_str));
            printf("   • Partial data received: %s (%.1f%%)\n", 
                   size_str, stats.completion_percent);
        }
    }
    
    // Cleanup
    rgtp_client_destroy(client);
    rgtp_cleanup();
    
    return (result == 0) ? 0 : 1;
}