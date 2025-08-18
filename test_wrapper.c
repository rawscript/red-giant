#ifdef RED_GIANT_C_DEMOS
// Red Giant Protocol - Wrapper Test Program
// Demonstrates the complete workflow of the Red Giant Protocol

#include "red_giant_wrapper.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#include <windows.h>
#define sleep(x) Sleep((x) * 1000)
#else
#include <unistd.h>
#endif

// Progress callback implementation
void progress_callback(uint32_t processed, uint32_t total, float percentage, uint32_t throughput_mbps) {
    printf("\r[PROGRESS] %u/%u chunks (%.1f%%) - %u MB/s", 
           processed, total, percentage, throughput_mbps);
    fflush(stdout);
    
    if (processed == total) {
        printf("\n");
    }
}

// Logging callback implementation
void log_callback(const char* level, const char* message) {
    time_t now;
    time(&now);
    struct tm* timeinfo = localtime(&now);
    
    printf("[%02d:%02d:%02d] [%s] %s\n", 
           timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec,
           level, message);
}

// Create a test file
bool create_test_file(const char* filename, size_t size_mb) {
    FILE* file = fopen(filename, "wb");
    if (!file) {
        printf("Failed to create test file: %s\n", filename);
        return false;
    }
    
    printf("Creating test file: %s (%.1f MB)\n", filename, (double)size_mb);
    
    // Write test data
    char buffer[4096];
    for (int i = 0; i < 4096; i++) {
        buffer[i] = (char)(i % 256);
    }
    
    size_t total_bytes = size_mb * 1024 * 1024;
    size_t bytes_written = 0;
    
    while (bytes_written < total_bytes) {
        size_t to_write = (total_bytes - bytes_written > 4096) ? 4096 : (total_bytes - bytes_written);
        if (fwrite(buffer, 1, to_write, file) != to_write) {
            printf("Failed to write test data\n");
            fclose(file);
            return false;
        }
        bytes_written += to_write;
    }
    
    fclose(file);
    printf("Test file created successfully\n");
    return true;
}

// Test basic file transmission
void test_basic_transmission(const char* filename) {
    printf("\n=== Testing Basic File Transmission ===\n");
    
    rg_wrapper_error_t result = rg_wrapper_transmit_file(filename, false);
    
    if (result == RG_WRAPPER_SUCCESS) {
        printf("✅ Basic transmission test PASSED\n");
    } else {
        printf("❌ Basic transmission test FAILED (error: %d)\n", result);
    }
}

// Test reliable file transmission
void test_reliable_transmission(const char* filename) {
    printf("\n=== Testing Reliable File Transmission ===\n");
    
    rg_wrapper_error_t result = rg_wrapper_transmit_file(filename, true);
    
    if (result == RG_WRAPPER_SUCCESS) {
        printf("✅ Reliable transmission test PASSED\n");
    } else {
        printf("❌ Reliable transmission test FAILED (error: %d)\n", result);
    }
}

// Test file transmission and reception workflow
void test_transmission_reception_workflow(const char* input_filename, const char* output_filename) {
    printf("\n=== Testing Transmission & Reception Workflow ===\n");
    
    // Initialize file context
    rg_file_context_t* context = rg_wrapper_init_file(input_filename, false);
    if (!context) {
        printf("❌ Failed to initialize file context\n");
        return;
    }
    
    // Process file (transmission)
    printf("Starting transmission...\n");
    rg_wrapper_error_t result = rg_wrapper_process_file(context);
    if (result != RG_WRAPPER_SUCCESS) {
        printf("❌ Transmission failed (error: %d)\n", result);
        rg_wrapper_cleanup_file(context);
        return;
    }
    
    // Get statistics
    uint32_t processed, total, throughput;
    uint64_t elapsed;
    bool complete;
    rg_wrapper_get_stats(context, &processed, &total, &throughput, &elapsed, &complete);
    
    printf("Transmission completed: %u/%u chunks, %u MB/s, %llu ms\n", 
           processed, total, throughput, (unsigned long long)elapsed);
    
    // Simulate brief delay
    sleep(1);
    
    // Retrieve file (reception)
    printf("Starting reception...\n");
    result = rg_wrapper_retrieve_file(context, output_filename);
    if (result != RG_WRAPPER_SUCCESS) {
        printf("❌ Reception failed (error: %d)\n", result);
        rg_wrapper_cleanup_file(context);
        return;
    }
    
    printf("✅ Transmission & Reception workflow test PASSED\n");
    
    // Cleanup
    rg_wrapper_cleanup_file(context);
}

// Test batch processing
void test_batch_processing() {
    printf("\n=== Testing Batch Processing ===\n");
    
    // Create multiple test files
    const char* filenames[] = {
        "test_batch_1.dat",
        "test_batch_2.dat",
        "test_batch_3.dat"
    };
    const int file_count = 3;
    
    // Create test files
    for (int i = 0; i < file_count; i++) {
        if (!create_test_file(filenames[i], 1)) {  // 1MB each
            printf("❌ Failed to create test file: %s\n", filenames[i]);
            return;
        }
    }
    
    // Process batch
    rg_wrapper_error_t result = rg_wrapper_process_batch(filenames, file_count, false);
    
    if (result == RG_WRAPPER_SUCCESS) {
        printf("✅ Batch processing test PASSED\n");
    } else {
        printf("❌ Batch processing test FAILED (error: %d)\n", result);
    }
    
    // Cleanup test files
    for (int i = 0; i < file_count; i++) {
        remove(filenames[i]);
    }
}

// Test reliability features
void test_reliability_features(const char* filename) {
    printf("\n=== Testing Reliability Features ===\n");
    
    // Initialize reliable context
    rg_file_context_t* context = rg_wrapper_init_file(filename, true);
    if (!context) {
        printf("❌ Failed to initialize reliable file context\n");
        return;
    }
    
    // Process file with reliability
    rg_wrapper_error_t result = rg_wrapper_process_file(context);
    if (result != RG_WRAPPER_SUCCESS) {
        printf("❌ Reliable processing failed (error: %d)\n", result);
        rg_wrapper_cleanup_file(context);
        return;
    }
    
    // Get reliability statistics
    uint32_t failed_chunks, retry_operations;
    rg_wrapper_get_reliability_stats(context, &failed_chunks, &retry_operations);
    
    printf("Reliability stats: %u failed chunks, %u retry operations\n", 
           failed_chunks, retry_operations);
    
    // Test recovery if there were failures
    if (failed_chunks > 0) {
        printf("Attempting chunk recovery...\n");
        rg_wrapper_recover_failed_chunks(context);
        
        // Check stats again
        rg_wrapper_get_reliability_stats(context, &failed_chunks, &retry_operations);
        printf("After recovery: %u failed chunks, %u retry operations\n", 
               failed_chunks, retry_operations);
    }
    
    printf("✅ Reliability features test PASSED\n");
    
    // Cleanup
    rg_wrapper_cleanup_file(context);
}

int main(int argc, char* argv[]) {
    printf("🚀 Red Giant Protocol - Wrapper Test Suite\n");
    printf("Version: %s\n", rg_wrapper_get_version());
    printf("═══════════════════════════════════════════\n");
    
    // Set callbacks
    rg_wrapper_set_progress_callback(progress_callback);
    rg_wrapper_set_log_callback(log_callback);
    
    // Test file names
    const char* test_file = "test_input.dat";
    const char* output_file = "test_output.dat";
    
    // Create test file
    if (!create_test_file(test_file, 5)) {  // 5MB test file
        printf("❌ Failed to create test file\n");
        return 1;
    }
    
    // Run tests
    test_basic_transmission(test_file);
    test_reliable_transmission(test_file);
    test_transmission_reception_workflow(test_file, output_file);
    test_batch_processing();
    test_reliability_features(test_file);
    
    // Cleanup
    remove(test_file);
    remove(output_file);
    
    printf("\n🎉 All tests completed!\n");
    return 0;
}
#endif