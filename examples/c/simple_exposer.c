/**
 * @file simple_exposer.c
 * @brief Simple RGTP exposer example
 * 
 * This example demonstrates how to expose data using RGTP.
 * It exposes a file and waits for clients to pull it.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include "rgtp/rgtp_sdk.h"

static volatile bool running = true;

void signal_handler(int sig) {
    (void)sig;
    running = false;
    printf("\nShutting down...\n");
}

void progress_callback(size_t bytes_transferred, size_t total_bytes, void* user_data) {
    (void)user_data;
    double percent = (double)bytes_transferred / total_bytes * 100.0;
    printf("\rProgress: %.1f%% (%zu/%zu bytes)", percent, bytes_transferred, total_bytes);
    fflush(stdout);
}

void error_callback(int error_code, const char* error_message, void* user_data) {
    (void)user_data;
    fprintf(stderr, "Error %d: %s\n", error_code, error_message);
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        printf("Usage: %s <file_to_expose>\n", argv[0]);
        printf("Example: %s document.pdf\n", argv[0]);
        return 1;
    }

    const char* filename = argv[1];
    
    // Set up signal handling
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    printf("RGTP Simple Exposer\n");
    printf("==================\n");
    printf("Exposing file: %s\n", filename);
    
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
    
    // Auto-detect network type and optimize
    printf("Auto-configuring for network conditions...\n");
    rgtp_config_for_lan(&config); // Default to LAN settings
    
    // Create session
    rgtp_session_t* session = rgtp_session_create_with_config(&config);
    if (!session) {
        fprintf(stderr, "Failed to create RGTP session\n");
        rgtp_cleanup();
        return 1;
    }
    
    printf("Starting exposure on port %d...\n", config.port);
    
    // Expose the file
    if (rgtp_session_expose_file(session, filename) != 0) {
        fprintf(stderr, "Failed to expose file: %s\n", filename);
        rgtp_session_destroy(session);
        rgtp_cleanup();
        return 1;
    }
    
    printf("File exposed successfully!\n");
    printf("Clients can now pull from: <this_host>:%d\n", config.port);
    printf("Press Ctrl+C to stop...\n\n");
    
    // Monitor progress
    while (running) {
        rgtp_stats_t stats;
        if (rgtp_session_get_stats(session, &stats) == 0) {
            if (stats.chunks_transferred > 0) {
                char throughput_str[64];
                char size_str[64];
                
                rgtp_format_throughput(stats.throughput_mbps, throughput_str, sizeof(throughput_str));
                rgtp_format_size(stats.bytes_transferred, size_str, sizeof(size_str));
                
                printf("\rStats: %s transferred, %s, %.1f%% complete", 
                       size_str, throughput_str, stats.completion_percent);
                fflush(stdout);
            }
        }
        
        sleep(1);
    }
    
    printf("\nStopping exposure...\n");
    
    // Cleanup
    rgtp_session_destroy(session);
    rgtp_cleanup();
    
    printf("Exposure stopped.\n");
    return 0;
}