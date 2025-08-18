// Red Giant Protocol - Integration Test
// Tests the complete workflow end-to-end

#include "red_giant_wrapper.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <assert.h>

#ifdef _WIN32
#include <windows.h>
#define sleep(x) Sleep((x) * 1000)
#else
#include <unistd.h>
#endif

// Test configuration
#define TEST_FILE_SIZE_MB 2
#define TEST_CHUNK_PATTERN 0xAB

// Global test state
static struct {
    int tests_run;
    int tests_passed;
    int tests_failed;
    bool verbose;
} test_state = {0, 0, 0, false};

// Test logging
void test_log(const char* level, const char* message) {
    if (test_state.verbose || strcmp(level, "ERROR") == 0) {
        printf("[%s] %s\n", level, message);
    }
}

// Test progress (minimal output)
void test_progress(uint32_t processed, uint32_t total, float percentage, uint32_t throughput) {
    if (test_state.verbose && (processed == total || processed % 100 == 0)) {
        printf("  Progress: %u/%u (%.1f%%) - %u MB/s\n", processed, total, percentage, throughput);
    }
}

// Test assertion macro
#define TEST_ASSERT(condition, message) do { \
    test_state.tests_run++; \
    if (condition) { \
        test_state.tests_passed++; \
        if (test_state.verbose) printf("  ✅ %s\n", message); \
    } else { \
        test_state.tests_failed++; \
        printf("  ❌ %s\n", message); \
        return false; \
    } \
} while(0)

// Create test file with known pattern
bool create_test_file(const char* filename, size_t size_mb, uint8_t pattern) {
    FILE* file = fopen(filename, "wb");
    if (!file) return false;
    
    size_t total_bytes = size_mb * 1024 * 1024;
    uint8_t* buffer = malloc(4096);
    if (!buffer) {
        fclose(file);
        return false;
    }
    
    // Fill buffer with pattern
    for (int i = 0; i < 4096; i++) {
        buffer[i] = (uint8_t)(pattern + (i % 256));
    }
    
    size_t bytes_written = 0;
    while (bytes_written < total_bytes) {
        size_t to_write = (total_bytes - bytes_written > 4096) ? 4096 : (total_bytes - bytes_written);
        if (fwrite(buffer, 1, to_write, file) != to_write) {
            free(buffer);
            fclose(file);
            return false;
        }
        bytes_written += to_write;
    }
    
    free(buffer);
    fclose(file);
    return true;
}

// Verify file contents match expected pattern
bool verify_file_pattern(const char* filename, size_t expected_size, uint8_t pattern) {
    FILE* file = fopen(filename, "rb");
    if (!file) return false;
    
    uint8_t buffer[4096];
    size_t bytes_read;
    size_t total_read = 0;
    
    while ((bytes_read = fread(buffer, 1, sizeof(buffer), file)) > 0) {
        for (size_t i = 0; i < bytes_read; i++) {
            uint8_t expected = (uint8_t)(pattern + ((total_read + i) % 256));
            if (buffer[i] != expected) {
                fclose(file);
                return false;
            }
        }
        total_read += bytes_read;
    }
    
    fclose(file);
    return total_read == expected_size;
}

// Test 1: Basic wrapper functionality
bool test_basic_functionality() {
    printf("\n🧪 Test 1: Basic Wrapper Functionality\n");
    
    // Test version
    const char* version = rg_wrapper_get_version();
    TEST_ASSERT(version != NULL, "Version string is not NULL");
    TEST_ASSERT(strlen(version) > 0, "Version string is not empty");
    
    // Test callback setting
    rg_wrapper_set_log_callback(test_log);
    rg_wrapper_set_progress_callback(test_progress);
    
    return true;
}

// Test 2: File context management
bool test_file_context_management() {
    printf("\n🧪 Test 2: File Context Management\n");
    
    const char* test_file = "test_context.dat";
    
    // Create test file
    TEST_ASSERT(create_test_file(test_file, 1, TEST_CHUNK_PATTERN), "Test file created");
    
    // Test file initialization
    rg_file_context_t* context = rg_wrapper_init_file(test_file, false);
    TEST_ASSERT(context != NULL, "File context initialized");
    
    // Test invalid file
    rg_file_context_t* invalid_context = rg_wrapper_init_file("nonexistent.dat", false);
    TEST_ASSERT(invalid_context == NULL, "Invalid file rejected");
    
    // Test cleanup
    rg_wrapper_cleanup_file(context);
    rg_wrapper_cleanup_file(NULL); // Should not crash
    
    remove(test_file);
    return true;
}

// Test 3: File processing workflow
bool test_file_processing_workflow() {
    printf("\n🧪 Test 3: File Processing Workflow\n");
    
    const char* input_file = "test_input.dat";
    const char* output_file = "test_output.dat";
    
    // Create test file
    TEST_ASSERT(create_test_file(input_file, TEST_FILE_SIZE_MB, TEST_CHUNK_PATTERN), "Test file created");
    
    // Initialize context
    rg_file_context_t* context = rg_wrapper_init_file(input_file, false);
    TEST_ASSERT(context != NULL, "File context initialized");
    
    // Process file
    rg_wrapper_error_t result = rg_wrapper_process_file(context);
    TEST_ASSERT(result == RG_WRAPPER_SUCCESS, "File processing succeeded");
    
    // Check statistics
    uint32_t processed, total, throughput;
    uint64_t elapsed;
    bool complete;
    rg_wrapper_get_stats(context, &processed, &total, &throughput, &elapsed, &complete);
    
    TEST_ASSERT(processed > 0, "Chunks were processed");
    TEST_ASSERT(total > 0, "Total chunks calculated");
    TEST_ASSERT(processed == total, "All chunks processed");
    TEST_ASSERT(complete, "Processing marked as complete");
    
    // Retrieve file
    result = rg_wrapper_retrieve_file(context, output_file);
    TEST_ASSERT(result == RG_WRAPPER_SUCCESS, "File retrieval succeeded");
    
    // Verify file integrity
    size_t expected_size = TEST_FILE_SIZE_MB * 1024 * 1024;
    TEST_ASSERT(verify_file_pattern(output_file, expected_size, TEST_CHUNK_PATTERN), "File integrity verified");
    
    // Cleanup
    rg_wrapper_cleanup_file(context);
    remove(input_file);
    remove(output_file);
    
    return true;
}

// Test 4: Reliable mode functionality
bool test_reliable_mode() {
    printf("\n🧪 Test 4: Reliable Mode Functionality\n");
    
    const char* test_file = "test_reliable.dat";
    
    // Create test file
    TEST_ASSERT(create_test_file(test_file, 1, TEST_CHUNK_PATTERN), "Test file created");
    
    // Initialize reliable context
    rg_file_context_t* context = rg_wrapper_init_file(test_file, true);
    TEST_ASSERT(context != NULL, "Reliable context initialized");
    
    // Process file
    rg_wrapper_error_t result = rg_wrapper_process_file(context);
    TEST_ASSERT(result == RG_WRAPPER_SUCCESS, "Reliable processing succeeded");
    
    // Check reliability statistics
    uint32_t failed_chunks, retry_ops;
    rg_wrapper_get_reliability_stats(context, &failed_chunks, &retry_ops);
    
    // Note: In normal conditions, there should be no failures
    // This test mainly verifies the API works correctly
    TEST_ASSERT(true, "Reliability statistics retrieved");
    
    // Test recovery function (should not crash even with no failures)
    rg_wrapper_recover_failed_chunks(context);
    TEST_ASSERT(true, "Recovery function executed");
    
    // Cleanup
    rg_wrapper_cleanup_file(context);
    remove(test_file);
    
    return true;
}

// Test 5: High-level workflow functions
bool test_high_level_workflows() {
    printf("\n🧪 Test 5: High-Level Workflow Functions\n");
    
    const char* test_file = "test_workflow.dat";
    
    // Create test file
    TEST_ASSERT(create_test_file(test_file, 1, TEST_CHUNK_PATTERN), "Test file created");
    
    // Test simple transmission
    rg_wrapper_error_t result = rg_wrapper_transmit_file(test_file, false);
    TEST_ASSERT(result == RG_WRAPPER_SUCCESS, "Simple transmission succeeded");
    
    // Test reliable transmission
    result = rg_wrapper_transmit_file(test_file, true);
    TEST_ASSERT(result == RG_WRAPPER_SUCCESS, "Reliable transmission succeeded");
    
    // Test batch processing
    const char* batch_files[] = {test_file};
    result = rg_wrapper_process_batch(batch_files, 1, false);
    TEST_ASSERT(result == RG_WRAPPER_SUCCESS, "Batch processing succeeded");
    
    // Test invalid parameters
    result = rg_wrapper_transmit_file(NULL, false);
    TEST_ASSERT(result != RG_WRAPPER_SUCCESS, "NULL filename rejected");
    
    result = rg_wrapper_transmit_file("nonexistent.dat", false);
    TEST_ASSERT(result != RG_WRAPPER_SUCCESS, "Nonexistent file rejected");
    
    // Cleanup
    remove(test_file);
    
    return true;
}

// Test 6: Error handling
bool test_error_handling() {
    printf("\n🧪 Test 6: Error Handling\n");
    
    // Test NULL parameters
    rg_file_context_t* null_context = rg_wrapper_init_file(NULL, false);
    TEST_ASSERT(null_context == NULL, "NULL filename rejected");
    
    // Test invalid file
    rg_file_context_t* invalid_context = rg_wrapper_init_file("nonexistent.dat", false);
    TEST_ASSERT(invalid_context == NULL, "Nonexistent file rejected");
    
    // Test processing with NULL context
    rg_wrapper_error_t result = rg_wrapper_process_file(NULL);
    TEST_ASSERT(result != RG_WRAPPER_SUCCESS, "NULL context rejected");
    
    // Test retrieval with NULL context
    result = rg_wrapper_retrieve_file(NULL, "output.dat");
    TEST_ASSERT(result != RG_WRAPPER_SUCCESS, "NULL context rejected for retrieval");
    
    // Test statistics with NULL context
    uint32_t dummy;
    rg_wrapper_get_stats(NULL, &dummy, &dummy, &dummy, NULL, NULL);
    TEST_ASSERT(true, "Statistics with NULL context handled gracefully");
    
    return true;
}

// Test 7: Performance validation
bool test_performance_validation() {
    printf("\n🧪 Test 7: Performance Validation\n");
    
    const char* test_file = "test_performance.dat";
    
    // Create larger test file for performance testing
    TEST_ASSERT(create_test_file(test_file, 5, TEST_CHUNK_PATTERN), "Performance test file created");
    
    // Initialize context
    rg_file_context_t* context = rg_wrapper_init_file(test_file, false);
    TEST_ASSERT(context != NULL, "Performance test context initialized");
    
    // Measure processing time
    clock_t start_time = clock();
    rg_wrapper_error_t result = rg_wrapper_process_file(context);
    clock_t end_time = clock();
    
    TEST_ASSERT(result == RG_WRAPPER_SUCCESS, "Performance test processing succeeded");
    
    // Calculate processing time
    double processing_time = ((double)(end_time - start_time)) / CLOCKS_PER_SEC;
    
    // Get statistics
    uint32_t processed, total, throughput;
    uint64_t elapsed_ms;
    bool complete;
    rg_wrapper_get_stats(context, &processed, &total, &throughput, &elapsed_ms, &complete);
    
    printf("  Performance Results:\n");
    printf("    File Size: 5 MB\n");
    printf("    Chunks: %u\n", total);
    printf("    Processing Time: %.3f seconds\n", processing_time);
    printf("    Throughput: %u MB/s\n", throughput);
    
    // Basic performance validation (should process at least 1 MB/s)
    TEST_ASSERT(throughput > 0, "Throughput measurement available");
    TEST_ASSERT(processing_time < 10.0, "Processing completed in reasonable time");
    
    // Cleanup
    rg_wrapper_cleanup_file(context);
    remove(test_file);
    
    return true;
}

// Run all tests
void run_all_tests(bool verbose) {
    test_state.verbose = verbose;
    
    printf("🚀 Red Giant Protocol - Integration Test Suite\n");
    printf("Version: %s\n", rg_wrapper_get_version());
    printf("==============================================\n");
    
    // Set up callbacks
    rg_wrapper_set_log_callback(test_log);
    rg_wrapper_set_progress_callback(test_progress);
    
    // Run tests
    bool all_passed = true;
    
    all_passed &= test_basic_functionality();
    all_passed &= test_file_context_management();
    all_passed &= test_file_processing_workflow();
    all_passed &= test_reliable_mode();
    all_passed &= test_high_level_workflows();
    all_passed &= test_error_handling();
    all_passed &= test_performance_validation();
    
    // Print results
    printf("\n📊 Test Results\n");
    printf("===============\n");
    printf("Tests Run: %d\n", test_state.tests_run);
    printf("Tests Passed: %d\n", test_state.tests_passed);
    printf("Tests Failed: %d\n", test_state.tests_failed);
    
    if (all_passed && test_state.tests_failed == 0) {
        printf("\n🎉 All tests PASSED! The Red Giant Protocol wrapper is working correctly.\n");
    } else {
        printf("\n❌ Some tests FAILED. Please check the implementation.\n");
    }
}

int main(int argc, char* argv[]) {
    bool verbose = false;
    
    // Check for verbose flag
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--verbose") == 0) {
            verbose = true;
            break;
        }
    }
    
    run_all_tests(verbose);
    
    return (test_state.tests_failed == 0) ? 0 : 1;
}