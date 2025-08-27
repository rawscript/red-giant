#include "red_giant_wrapper.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// Progress callback
void progress_callback(uint32_t processed, uint32_t total, float percentage, uint32_t throughput_mbps) {
    if (processed % 100 == 0) { // Print every 100 chunks to reduce output
        printf("\rProgress: %.2f%% (%u/%u chunks) - Throughput: %u MB/s", 
               percentage, processed, total, throughput_mbps);
        fflush(stdout);
    }
}

int main() {
    printf("Red Giant Protocol Throughput Test\n");
    printf("==================================\n");
    
    // Create a large test file (200MB)
    const char* test_filename = "throughput_test.dat";
    const size_t test_file_size = 200 * 1024 * 1024; // 200 MB
    
    printf("Creating %zu MB test file...\n", test_file_size / (1024 * 1024));
    
    FILE* file = fopen(test_filename, "wb");
    if (!file) {
        printf("Error: Failed to create test file\n");
        return 1;
    }
    
    // Create buffer with pattern data
    char* buffer = malloc(1024 * 1024); // 1MB buffer
    if (!buffer) {
        printf("Error: Failed to allocate buffer\n");
        fclose(file);
        return 1;
    }
    
    // Fill buffer with pattern
    for (int i = 0; i < 1024 * 1024; i++) {
        buffer[i] = (char)(i % 256);
    }
    
    // Write data to file
    size_t written = 0;
    while (written < test_file_size) {
        size_t to_write = test_file_size - written;
        if (to_write > 1024 * 1024) {
            to_write = 1024 * 1024;
        }
        
        size_t result = fwrite(buffer, 1, to_write, file);
        if (result != to_write) {
            printf("Error: Failed to write data to file\n");
            free(buffer);
            fclose(file);
            return 1;
        }
        
        written += result;
    }
    
    free(buffer);
    fclose(file);
    printf("Test file created successfully: %zu bytes\n", test_file_size);
    
    // Set up progress callback
    rg_wrapper_set_progress_callback(progress_callback);
    
    // Initialize file context
    printf("Initializing file context...\n");
    rg_file_context_t* context = rg_wrapper_init_file(test_filename, false);
    if (!context) {
        printf("Error: Failed to initialize file context\n");
        return 1;
    }
    
    // Process file
    printf("Processing file for throughput test...\n");
    rg_wrapper_error_t result = rg_wrapper_process_file(context);
    
    if (result == RG_WRAPPER_SUCCESS) {
        printf("\n✅ File processed successfully!\n");
        
        // Get final stats
        uint32_t processed_chunks, total_chunks, throughput_mbps;
        uint64_t elapsed_ms;
        bool is_complete;
        rg_wrapper_get_stats(context, &processed_chunks, &total_chunks, &throughput_mbps, &elapsed_ms, &is_complete);
        
        printf("Final throughput: %u MB/s\n", throughput_mbps);
        
        if (throughput_mbps >= 500) {
            printf("✅ Throughput requirement met: %u MB/s (≥ 500 MB/s)\n", throughput_mbps);
        } else {
            printf("⚠️  Throughput requirement not met: %u MB/s (< 500 MB/s)\n", throughput_mbps);
        }
    } else {
        printf("\n❌ File processing failed!\n");
    }
    
    // Cleanup
    if (context) {
        rg_wrapper_cleanup_file(context);
    }
    
    // Remove test file
    remove(test_filename);
    
    return (result == RG_WRAPPER_SUCCESS) ? 0 : 1;
}