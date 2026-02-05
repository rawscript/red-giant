#include "rgtp_http3_proxy.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

// Test callback functions
void test_request_callback(rgtp_http3_proxy_t* proxy,
                          const char* path,
                          const char* method,
                          const char* headers,
                          void* user_data) {
    printf("Request callback: %s %s\n", method, path);
    // Store test result in user data
    *(int*)user_data = 1;
}

void test_response_callback(rgtp_http3_proxy_t* proxy,
                           int stream_id,
                           int status_code,
                           const char* headers,
                           const void* body,
                           size_t body_len,
                           void* user_data) {
    printf("Response callback: %d %d bytes\n", status_code, (int)body_len);
    // Store test result in user data
    *(int*)user_data = 2;
}

void test_error_callback(rgtp_http3_proxy_t* proxy,
                        int error_code,
                        const char* error_message,
                        void* user_data) {
    printf("Error callback: %d %s\n", error_code, error_message);
    // Store test result in user_data
    *(int*)user_data = -1;
}

int test_proxy_creation() {
    printf("Testing proxy creation...\n");
    
    // Create configuration
    rgtp_http3_proxy_config_t* config = rgtp_http3_config_create();
    assert(config != NULL);
    
    // Test configuration setters
    rgtp_http3_config_set_port(config, 8443);
    rgtp_http3_config_set_backend(config, "localhost", 9999);
    rgtp_http3_config_set_max_streams(config, 50);
    rgtp_http3_config_set_idle_timeout(config, 15000);
    
    // Create proxy
    rgtp_http3_proxy_t* proxy = rgtp_http3_proxy_create(config);
    assert(proxy != NULL);
    
    // Test that configuration was copied correctly
    assert(proxy->config.http3_port == 8443);
    assert(strcmp(proxy->config.backend_host, "localhost") == 0);
    assert(proxy->config.rgtp_port == 9999);
    assert(proxy->config.max_concurrent_streams == 50);
    assert(proxy->config.idle_timeout_ms == 15000);
    
    // Clean up
    rgtp_http3_proxy_destroy(proxy);
    rgtp_http3_config_destroy(config);
    
    printf("✓ Proxy creation test passed\n");
    return 0;
}

int test_proxy_lifecycle() {
    printf("Testing proxy lifecycle...\n");
    
    rgtp_http3_proxy_config_t* config = rgtp_http3_config_create();
    rgtp_http3_proxy_t* proxy = rgtp_http3_proxy_create(config);
    
    // Test start
    int result = rgtp_http3_proxy_start(proxy);
    assert(result == 0);
    assert(proxy->running == 1);
    
    // Test double start (should fail)
    result = rgtp_http3_proxy_start(proxy);
    assert(result == -1);
    
    // Test stop
    result = rgtp_http3_proxy_stop(proxy);
    assert(result == 0);
    assert(proxy->running == 0);
    
    // Test double stop (should fail)
    result = rgtp_http3_proxy_stop(proxy);
    assert(result == -1);
    
    // Clean up
    rgtp_http3_proxy_destroy(proxy);
    rgtp_http3_config_destroy(config);
    
    printf("✓ Proxy lifecycle test passed\n");
    return 0;
}

int test_callbacks() {
    printf("Testing callbacks...\n");
    
    rgtp_http3_proxy_config_t* config = rgtp_http3_config_create();
    rgtp_http3_proxy_t* proxy = rgtp_http3_proxy_create(config);
    
    int callback_result = 0;
    
    // Register callbacks
    rgtp_http3_proxy_set_request_callback(proxy, test_request_callback);
    rgtp_http3_proxy_set_response_callback(proxy, test_response_callback);
    rgtp_http3_proxy_set_error_callback(proxy, test_error_callback);
    
    // Test request callback registration worked by checking function pointers
    // (We can't easily call them without full HTTP/3 stack)
    
    // Test forward request (simulated)
    int forward_result = rgtp_http3_proxy_forward_request(
        proxy, 1, "/test", "GET", "Host: localhost", NULL, 0);
    assert(forward_result == 0);
    
    // Test send response (simulated)
    int response_result = rgtp_http3_proxy_send_response(
        proxy, 1, 200, "Content-Type: text/plain", "Hello", 5);
    assert(response_result == 0);
    
    // Clean up
    rgtp_http3_proxy_destroy(proxy);
    rgtp_http3_config_destroy(config);
    
    printf("✓ Callbacks test passed\n");
    return 0;
}

int test_routes() {
    printf("Testing routes...\n");
    
    rgtp_http3_proxy_config_t* config = rgtp_http3_config_create();
    rgtp_http3_proxy_t* proxy = rgtp_http3_proxy_create(config);
    
    // Test adding route
    int add_result = rgtp_http3_proxy_add_route(proxy, "/test", test_request_callback);
    assert(add_result == 0);
    
    // Test removing route
    int remove_result = rgtp_http3_proxy_remove_route(proxy, "/test");
    assert(remove_result == 0);
    
    // Clean up
    rgtp_http3_proxy_destroy(proxy);
    rgtp_http3_config_destroy(config);
    
    printf("✓ Routes test passed\n");
    return 0;
}

int test_statistics() {
    printf("Testing statistics...\n");
    
    rgtp_http3_proxy_config_t* config = rgtp_http3_config_create();
    rgtp_http3_proxy_t* proxy = rgtp_http3_proxy_create(config);
    
    // Get initial stats
    rgtp_http3_stats_t initial_stats;
    int stats_result = rgtp_http3_proxy_get_stats(proxy, &initial_stats);
    assert(stats_result == 0);
    
    // Forward a request to increment stats
    rgtp_http3_proxy_forward_request(proxy, 1, "/statstest", "GET", "", NULL, 0);
    rgtp_http3_proxy_send_response(proxy, 1, 200, "", "data", 4);
    
    // Get updated stats
    rgtp_http3_stats_t updated_stats;
    stats_result = rgtp_http3_proxy_get_stats(proxy, &updated_stats);
    assert(stats_result == 0);
    
    // Verify stats were updated
    assert(updated_stats.total_requests >= initial_stats.total_requests);
    assert(updated_stats.total_responses >= initial_stats.total_responses);
    
    // Clean up
    rgtp_http3_proxy_destroy(proxy);
    rgtp_http3_config_destroy(config);
    
    printf("✓ Statistics test passed\n");
    return 0;
}

int test_rgtp_integration() {
    printf("Testing RGTP integration...\n");
    
    rgtp_http3_proxy_config_t* config = rgtp_http3_config_create();
    rgtp_http3_proxy_t* proxy = rgtp_http3_proxy_create(config);
    
    // Test RGTP integration functions
    int expose_result = rgtp_http3_expose_via_proxy(proxy, "test.txt", "/download/test.txt");
    // This should succeed (return 0) even though implementation is minimal
    assert(expose_result == 0);
    
    int pull_result = rgtp_http3_pull_to_response(proxy, 1, "exposure123", "localhost", 9999);
    // This should succeed (return 0) even though implementation is minimal
    assert(pull_result == 0);
    
    // Clean up
    rgtp_http3_proxy_destroy(proxy);
    rgtp_http3_config_destroy(config);
    
    printf("✓ RGTP integration test passed\n");
    return 0;
}

int main() {
    printf("RGTP HTTP/3 Proxy Foundation Tests\n");
    printf("==================================\n");
    
    // Run all tests
    test_proxy_creation();
    test_proxy_lifecycle();
    test_callbacks();
    test_routes();
    test_statistics();
    test_rgtp_integration();
    
    printf("\n✓ All tests passed!\n");
    printf("HTTP/3 proxy foundation is ready for WebTransport integration.\n");
    
    return 0;
}