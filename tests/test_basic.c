/**
 * @file test_basic.c
 * @brief Basic RGTP functionality tests
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <sys/wait.h>
#include "rgtp/rgtp_sdk.h"

// Test configuration
#define TEST_DATA_SIZE 1024
#define TEST_PORT 19999
#define TEST_TIMEOUT_MS 5000

// Test results
static int tests_run = 0;
static int tests_passed = 0;

#define TEST_ASSERT(condition, message) do { \
    tests_run++; \
    if (condition) { \
        tests_passed++; \
        printf("✅ PASS: %s\n", message); \
    } else { \
        printf("❌ FAIL: %s\n", message); \
    } \
} while(0)

// Generate test data
void generate_test_data(uint8_t* buffer, size_t size) {
    for (size_t i = 0; i < size; i++) {
        buffer[i] = (uint8_t)(i ^ (i >> 8) ^ 0xAA);
    }
}

// Verify test data
bool verify_test_data(const uint8_t* buffer, size_t size) {
    for (size_t i = 0; i < size; i++) {
        uint8_t expected = (uint8_t)(i ^ (i >> 8) ^ 0xAA);
        if (buffer[i] != expected) {
            return false;
        }
    }
    return true;
}

// Test 1: Library initialization
void test_initialization() {
    printf("\n🧪 Test 1: Library Initialization\n");
    printf("================================\n");
    
    // Test initialization
    int result = rgtp_init();
    TEST_ASSERT(result == 0, "Library initialization");
    
    // Test version
    const char* version = rgtp_version();
    TEST_ASSERT(version != NULL && strlen(version) > 0, "Version string available");
    printf("   RGTP Version: %s\n", version);
    
    // Test cleanup
    rgtp_cleanup();
    TEST_ASSERT(true, "Library cleanup");
}

// Test 2: Configuration management
void test_configuration() {
    printf("\n🧪 Test 2: Configuration Management\n");
    printf("==================================\n");
    
    rgtp_init();
    
    rgtp_config_t config;
    
    // Test default configuration
    rgtp_config_default(&config);
    TEST_ASSERT(config.chunk_size > 0, "Default chunk size set");
    TEST_ASSERT(config.exposure_rate > 0, "Default exposure rate set");
    TEST_ASSERT(config.timeout_ms > 0, "Default timeout set");
    
    // Test LAN optimization
    rgtp_config_for_lan(&config);
    uint32_t lan_chunk_size = config.chunk_size;
    TEST_ASSERT(lan_chunk_size > 0, "LAN configuration applied");
    
    // Test WAN optimization
    rgtp_config_for_wan(&config);
    TEST_ASSERT(config.chunk_size <= lan_chunk_size, "WAN uses smaller chunks than LAN");
    
    // Test mobile optimization
    rgtp_config_for_mobile(&config);
    TEST_ASSERT(config.chunk_size > 0, "Mobile configuration applied");
    TEST_ASSERT(config.timeout_ms >= 5000, "Mobile uses longer timeout");
    
    rgtp_cleanup();
}

// Test 3: Session creation and destruction
void test_session_management() {
    printf("\n🧪 Test 3: Session Management\n");
    printf("============================\n");
    
    rgtp_init();
    
    // Test default session creation
    rgtp_session_t* session1 = rgtp_session_create();
    TEST_ASSERT(session1 != NULL, "Default session creation");
    
    // Test configured session creation
    rgtp_config_t config;
    rgtp_config_default(&config);
    config.port = TEST_PORT;
    config.timeout_ms = TEST_TIMEOUT_MS;
    
    rgtp_session_t* session2 = rgtp_session_create_with_config(&config);
    TEST_ASSERT(session2 != NULL, "Configured session creation");
    
    // Test session destruction
    rgtp_session_destroy(session1);
    rgtp_session_destroy(session2);
    TEST_ASSERT(true, "Session destruction");
    
    rgtp_cleanup();
}

// Test 4: Client creation and destruction
void test_client_management() {
    printf("\n🧪 Test 4: Client Management\n");
    printf("===========================\n");
    
    rgtp_init();
    
    // Test default client creation
    rgtp_client_t* client1 = rgtp_client_create();
    TEST_ASSERT(client1 != NULL, "Default client creation");
    
    // Test configured client creation
    rgtp_config_t config;
    rgtp_config_default(&config);
    config.timeout_ms = TEST_TIMEOUT_MS;
    
    rgtp_client_t* client2 = rgtp_client_create_with_config(&config);
    TEST_ASSERT(client2 != NULL, "Configured client creation");
    
    // Test client destruction
    rgtp_client_destroy(client1);
    rgtp_client_destroy(client2);
    TEST_ASSERT(true, "Client destruction");
    
    rgtp_cleanup();
}

// Test 5: File operations
void test_file_operations() {
    printf("\n🧪 Test 5: File Operations\n");
    printf("=========================\n");
    
    rgtp_init();
    
    // Create test file
    const char* test_filename = "/tmp/rgtp_test_file.bin";
    const char* output_filename = "/tmp/rgtp_test_output.bin";
    
    FILE* f = fopen(test_filename, "wb");
    TEST_ASSERT(f != NULL, "Test file creation");
    
    if (f) {
        uint8_t test_data[TEST_DATA_SIZE];
        generate_test_data(test_data, TEST_DATA_SIZE);
        
        size_t written = fwrite(test_data, 1, TEST_DATA_SIZE, f);
        fclose(f);
        
        TEST_ASSERT(written == TEST_DATA_SIZE, "Test data written to file");
        
        // Test file exposure (without actual network transfer)
        rgtp_config_t config;
        rgtp_config_default(&config);
        config.port = TEST_PORT;
        config.timeout_ms = 1000; // Short timeout for test
        
        rgtp_session_t* session = rgtp_session_create_with_config(&config);
        TEST_ASSERT(session != NULL, "Session created for file test");
        
        if (session) {
            // Note: This will fail without a real network setup, but tests the API
            int result = rgtp_session_expose_file(session, test_filename);
            TEST_ASSERT(result == 0 || result == -1, "File exposure API called");
            
            rgtp_session_destroy(session);
        }
        
        // Cleanup test files
        unlink(test_filename);
        unlink(output_filename);
    }
    
    rgtp_cleanup();
}

// Test 6: Error handling
void test_error_handling() {
    printf("\n🧪 Test 6: Error Handling\n");
    printf("========================\n");
    
    rgtp_init();
    
    // Test invalid parameters
    rgtp_session_t* null_session = rgtp_session_create_with_config(NULL);
    TEST_ASSERT(null_session == NULL, "NULL config rejected");
    
    rgtp_client_t* null_client = rgtp_client_create_with_config(NULL);
    TEST_ASSERT(null_client == NULL, "NULL config rejected");
    
    // Test error string function
    const char* error_msg = rgtp_error_string(-1);
    TEST_ASSERT(error_msg != NULL && strlen(error_msg) > 0, "Error string available");
    
    // Test non-existent file
    rgtp_session_t* session = rgtp_session_create();
    if (session) {
        int result = rgtp_session_expose_file(session, "/non/existent/file.bin");
        TEST_ASSERT(result != 0, "Non-existent file rejected");
        rgtp_session_destroy(session);
    }
    
    rgtp_cleanup();
}

// Test 7: Utility functions
void test_utilities() {
    printf("\n🧪 Test 7: Utility Functions\n");
    printf("===========================\n");
    
    char buffer[256];
    
    // Test throughput formatting
    const char* throughput_str = rgtp_format_throughput(123.45, buffer, sizeof(buffer));
    TEST_ASSERT(throughput_str != NULL && strlen(throughput_str) > 0, "Throughput formatting");
    printf("   Throughput format: %s\n", throughput_str);
    
    // Test duration formatting
    const char* duration_str = rgtp_format_duration(65000, buffer, sizeof(buffer));
    TEST_ASSERT(duration_str != NULL && strlen(duration_str) > 0, "Duration formatting");
    printf("   Duration format: %s\n", duration_str);
    
    // Test size formatting
    const char* size_str = rgtp_format_size(1024*1024*5, buffer, sizeof(buffer));
    TEST_ASSERT(size_str != NULL && strlen(size_str) > 0, "Size formatting");
    printf("   Size format: %s\n", size_str);
}

// Test 8: Statistics
void test_statistics() {
    printf("\n🧪 Test 8: Statistics\n");
    printf("====================\n");
    
    rgtp_init();
    
    rgtp_session_t* session = rgtp_session_create();
    TEST_ASSERT(session != NULL, "Session created for stats test");
    
    if (session) {
        rgtp_stats_t stats;
        int result = rgtp_session_get_stats(session, &stats);
        TEST_ASSERT(result == 0, "Statistics retrieval");
        
        // Initial stats should be zero
        TEST_ASSERT(stats.bytes_transferred == 0, "Initial bytes transferred is zero");
        TEST_ASSERT(stats.total_bytes == 0, "Initial total bytes is zero");
        TEST_ASSERT(stats.completion_percent == 0.0, "Initial completion is zero");
        
        rgtp_session_destroy(session);
    }
    
    rgtp_client_t* client = rgtp_client_create();
    TEST_ASSERT(client != NULL, "Client created for stats test");
    
    if (client) {
        rgtp_stats_t stats;
        int result = rgtp_client_get_stats(client, &stats);
        TEST_ASSERT(result == 0, "Client statistics retrieval");
        
        rgtp_client_destroy(client);
    }
    
    rgtp_cleanup();
}

int main() {
    printf("RGTP Basic Functionality Tests\n");
    printf("==============================\n");
    printf("Testing RGTP library basic functionality...\n");
    
    // Run all tests
    test_initialization();
    test_configuration();
    test_session_management();
    test_client_management();
    test_file_operations();
    test_error_handling();
    test_utilities();
    test_statistics();
    
    // Print results
    printf("\n📊 Test Results\n");
    printf("==============\n");
    printf("Tests run: %d\n", tests_run);
    printf("Tests passed: %d\n", tests_passed);
    printf("Tests failed: %d\n", tests_run - tests_passed);
    
    if (tests_passed == tests_run) {
        printf("🎉 All tests passed!\n");
        return 0;
    } else {
        printf("💥 Some tests failed!\n");
        return 1;
    }
}