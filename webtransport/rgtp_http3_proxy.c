#include "rgtp_http3_proxy.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <unistd.h>
#include <netinet/in.h>
#include <sys/socket.h>
#endif

// Internal structure for the proxy
struct rgtp_http3_proxy {
    rgtp_http3_proxy_config_t config;
    int running;
    int server_fd;
    
    // Callbacks
    rgtp_http3_request_callback_t request_callback;
    rgtp_http3_response_callback_t response_callback;
    rgtp_http3_error_callback_t error_callback;
    
    // Statistics
    rgtp_http3_stats_t stats;
    
    // Thread safety
#ifdef _WIN32
    CRITICAL_SECTION mutex;
#else
    pthread_mutex_t mutex;
#endif
};

/* -------------------------------------------------------------------------- */
/* HTTP/3 Proxy Configuration Functions                                       */
/* -------------------------------------------------------------------------- */

rgtp_http3_proxy_config_t* rgtp_http3_config_create() {
    rgtp_http3_proxy_config_t* config = calloc(1, sizeof(rgtp_http3_proxy_config_t));
    if (!config) return NULL;
    
    // Set default values
    config->http3_port = 443;  // Default HTTP/3 port
    config->rgtp_port = 9999;  // Default RGTP port
    config->backend_host = strdup("127.0.0.1");
    config->max_concurrent_streams = 100;
    config->idle_timeout_ms = 30000;  // 30 seconds
    config->keepalive_interval_ms = 10000;  // 10 seconds
    config->buffer_size = 64 * 1024;  // 64KB
    config->user_data = NULL;
    
    return config;
}

void rgtp_http3_config_destroy(rgtp_http3_proxy_config_t* config) {
    if (!config) return;
    
    if (config->backend_host) {
        free(config->backend_host);
    }
    
    free(config);
}

void rgtp_http3_config_set_port(rgtp_http3_proxy_config_t* config, uint16_t port) {
    if (config) {
        config->http3_port = port;
    }
}

void rgtp_http3_config_set_backend(rgtp_http3_proxy_config_t* config, 
                                   const char* host, uint16_t port) {
    if (!config || !host) return;
    
    if (config->backend_host) {
        free(config->backend_host);
    }
    
    config->backend_host = strdup(host);
    config->rgtp_port = port;
}

void rgtp_http3_config_set_max_streams(rgtp_http3_proxy_config_t* config, int max_streams) {
    if (config && max_streams > 0) {
        config->max_concurrent_streams = max_streams;
    }
}

void rgtp_http3_config_set_idle_timeout(rgtp_http3_proxy_config_t* config, int timeout_ms) {
    if (config && timeout_ms > 0) {
        config->idle_timeout_ms = timeout_ms;
    }
}

void rgtp_http3_config_set_keepalive(rgtp_http3_proxy_config_t* config, int interval_ms) {
    if (config && interval_ms > 0) {
        config->keepalive_interval_ms = interval_ms;
    }
}

/* -------------------------------------------------------------------------- */
/* HTTP/3 Proxy Lifecycle Functions                                           */
/* -------------------------------------------------------------------------- */

rgtp_http3_proxy_t* rgtp_http3_proxy_create(const rgtp_http3_proxy_config_t* config) {
    if (!config) return NULL;
    
    rgtp_http3_proxy_t* proxy = calloc(1, sizeof(rgtp_http3_proxy_t));
    if (!proxy) return NULL;
    
    // Copy configuration
    proxy->config = *config;
    if (config->backend_host) {
        proxy->config.backend_host = strdup(config->backend_host);
    }
    
    proxy->running = 0;
    proxy->server_fd = -1;
    
    // Initialize statistics
    memset(&proxy->stats, 0, sizeof(proxy->stats));
    
    // Initialize mutex
#ifdef _WIN32
    InitializeCriticalSection(&proxy->mutex);
#else
    pthread_mutex_init(&proxy->mutex, NULL);
#endif
    
    return proxy;
}

int rgtp_http3_proxy_start(rgtp_http3_proxy_t* proxy) {
    if (!proxy) return -1;
    
    // Lock mutex
#ifdef _WIN32
    EnterCriticalSection(&proxy->mutex);
#else
    pthread_mutex_lock(&proxy->mutex);
#endif
    
    if (proxy->running) {
#ifdef _WIN32
        LeaveCriticalSection(&proxy->mutex);
#else
        pthread_mutex_unlock(&proxy->mutex);
#endif
        return -1; // Already running
    }
    
    // Initialize network layer (simulated for now)
    // In a real implementation, this would set up HTTP/3 server
    
    proxy->running = 1;
    proxy->stats.current_connections = 0;
    
    // Unlock mutex
#ifdef _WIN32
    LeaveCriticalSection(&proxy->mutex);
#else
    pthread_mutex_unlock(&proxy->mutex);
#endif
    
    return 0;
}

int rgtp_http3_proxy_stop(rgtp_http3_proxy_t* proxy) {
    if (!proxy) return -1;
    
    // Lock mutex
#ifdef _WIN32
    EnterCriticalSection(&proxy->mutex);
#else
    pthread_mutex_lock(&proxy->mutex);
#endif
    
    if (!proxy->running) {
#ifdef _WIN32
        LeaveCriticalSection(&proxy->mutex);
#else
        pthread_mutex_unlock(&proxy->mutex);
#endif
        return -1; // Not running
    }
    
    proxy->running = 0;
    
    // In a real implementation, this would stop the HTTP/3 server
    
    // Unlock mutex
#ifdef _WIN32
    LeaveCriticalSection(&proxy->mutex);
#else
    pthread_mutex_unlock(&proxy->mutex);
#endif
    
    return 0;
}

void rgtp_http3_proxy_destroy(rgtp_http3_proxy_t* proxy) {
    if (!proxy) return;
    
    // Stop the proxy if running
    rgtp_http3_proxy_stop(proxy);
    
    // Clean up configuration
    if (proxy->config.backend_host) {
        free(proxy->config.backend_host);
    }
    
    // Clean up mutex
#ifdef _WIN32
    DeleteCriticalSection(&proxy->mutex);
#else
    pthread_mutex_destroy(&proxy->mutex);
#endif
    
    free(proxy);
}

/* -------------------------------------------------------------------------- */
/* HTTP/3 Proxy Callback Registration Functions                               */
/* -------------------------------------------------------------------------- */

void rgtp_http3_proxy_set_request_callback(rgtp_http3_proxy_t* proxy,
                                         rgtp_http3_request_callback_t callback) {
    if (proxy) {
        proxy->request_callback = callback;
    }
}

void rgtp_http3_proxy_set_response_callback(rgtp_http3_proxy_t* proxy,
                                          rgtp_http3_response_callback_t callback) {
    if (proxy) {
        proxy->response_callback = callback;
    }
}

void rgtp_http3_proxy_set_error_callback(rgtp_http3_proxy_t* proxy,
                                       rgtp_http3_error_callback_t callback) {
    if (proxy) {
        proxy->error_callback = callback;
    }
}

/* -------------------------------------------------------------------------- */
/* HTTP/3 Proxy Management Functions                                          */
/* -------------------------------------------------------------------------- */

int rgtp_http3_proxy_add_route(rgtp_http3_proxy_t* proxy, 
                               const char* path_pattern,
                               rgtp_http3_request_callback_t handler) {
    if (!proxy || !path_pattern || !handler) return -1;
    
    // In a real implementation, this would add routing logic
    // For now, we just return success to indicate the foundation is in place
    
    return 0;
}

int rgtp_http3_proxy_remove_route(rgtp_http3_proxy_t* proxy, const char* path_pattern) {
    if (!proxy || !path_pattern) return -1;
    
    // In a real implementation, this would remove routing logic
    // For now, we just return success
    
    return 0;
}

int rgtp_http3_proxy_forward_request(rgtp_http3_proxy_t* proxy,
                                    int stream_id,
                                    const char* path,
                                    const char* method,
                                    const char* headers,
                                    const void* body,
                                    size_t body_len) {
    if (!proxy || !path || !method) return -1;
    
    // Update statistics
#ifdef _WIN32
    EnterCriticalSection(&proxy->mutex);
#else
    pthread_mutex_lock(&proxy->mutex);
#endif
    proxy->stats.total_requests++;
    proxy->stats.bytes_received += body_len;
    proxy->stats.active_streams++;
#ifdef _WIN32
    LeaveCriticalSection(&proxy->mutex);
#else
    pthread_mutex_unlock(&proxy->mutex);
#endif
    
    // In a real implementation, this would forward the request to RGTP backend
    // For now, we simulate the forwarding process
    
    // Call the registered request callback if available
    if (proxy->request_callback) {
        proxy->request_callback(proxy, path, method, headers, proxy->config.user_data);
    }
    
    return 0;
}

int rgtp_http3_proxy_send_response(rgtp_http3_proxy_t* proxy,
                                  int stream_id,
                                  int status_code,
                                  const char* headers,
                                  const void* body,
                                  size_t body_len) {
    if (!proxy) return -1;
    
    // Update statistics
#ifdef _WIN32
    EnterCriticalSection(&proxy->mutex);
#else
    pthread_mutex_lock(&proxy->mutex);
#endif
    proxy->stats.total_responses++;
    proxy->stats.bytes_sent += body_len;
    if (proxy->stats.active_streams > 0) {
        proxy->stats.active_streams--;
    }
#ifdef _WIN32
    LeaveCriticalSection(&proxy->mutex);
#else
    pthread_mutex_unlock(&proxy->mutex);
#endif
    
    // In a real implementation, this would send the response back via HTTP/3
    // For now, we simulate the response process
    
    // Call the registered response callback if available
    if (proxy->response_callback) {
        proxy->response_callback(proxy, stream_id, status_code, headers, body, body_len, proxy->config.user_data);
    }
    
    return 0;
}

/* -------------------------------------------------------------------------- */
/* HTTP/3 Proxy Statistics Functions                                          */
/* -------------------------------------------------------------------------- */

int rgtp_http3_proxy_get_stats(rgtp_http3_proxy_t* proxy, rgtp_http3_stats_t* stats) {
    if (!proxy || !stats) return -1;
    
    // Lock mutex to get consistent statistics
#ifdef _WIN32
    EnterCriticalSection(&proxy->mutex);
#else
    pthread_mutex_lock(&proxy->mutex);
#endif
    
    *stats = proxy->stats;
    
    // Unlock mutex
#ifdef _WIN32
    LeaveCriticalSection(&proxy->mutex);
#else
    pthread_mutex_unlock(&proxy->mutex);
#endif
    
    return 0;
}

/* -------------------------------------------------------------------------- */
/* Utility Functions                                                          */
/* -------------------------------------------------------------------------- */

const char* rgtp_http3_error_string(int error_code) {
    switch (error_code) {
        case 0: return "Success";
        case -1: return "General error";
        case -2: return "Invalid argument";
        case -3: return "Connection refused";
        case -4: return "Timeout";
        case -5: return "Not found";
        default: return "Unknown error";
    }
}

const char* rgtp_http3_status_reason(int status_code) {
    switch (status_code) {
        case 200: return "OK";
        case 201: return "Created";
        case 204: return "No Content";
        case 400: return "Bad Request";
        case 401: return "Unauthorized";
        case 403: return "Forbidden";
        case 404: return "Not Found";
        case 500: return "Internal Server Error";
        case 502: return "Bad Gateway";
        case 503: return "Service Unavailable";
        default: return "Unknown Status";
    }
}

/* -------------------------------------------------------------------------- */
/* RGTP Integration Functions                                                 */
/* -------------------------------------------------------------------------- */

/**
 * Function to integrate RGTP functionality with HTTP/3 proxy
 * This would handle exposing files via RGTP and serving them through HTTP/3
 */
int rgtp_http3_expose_via_proxy(rgtp_http3_proxy_t* proxy, const char* file_path, const char* url_path) {
    if (!proxy || !file_path || !url_path) return -1;
    
    // This function would integrate RGTP exposure with HTTP/3 serving
    // It would create an RGTP exposure and make it available via HTTP/3
    
    // Add route for the URL path
    int result = rgtp_http3_proxy_add_route(proxy, url_path, NULL);
    
    // In a complete implementation, this would:
    // 1. Create an RGTP exposure for the file
    // 2. Map the HTTP/3 URL to the RGTP exposure
    // 3. Handle the translation between HTTP/3 and RGTP protocols
    
    return result;
}

/**
 * Function to pull data from RGTP backend and serve via HTTP/3
 */
int rgtp_http3_pull_to_response(rgtp_http3_proxy_t* proxy, 
                               int stream_id,
                               const char* rgtp_exposure_id,
                               const char* rgtp_host,
                               uint16_t rgtp_port) {
    if (!proxy || !rgtp_exposure_id || !rgtp_host) return -1;
    
    // This function would pull data from RGTP backend and send as HTTP/3 response
    // It would handle the protocol translation between RGTP and HTTP/3
    
    // In a complete implementation, this would:
    // 1. Connect to RGTP backend
    // 2. Pull data using the exposure ID
    // 3. Format as HTTP/3 response
    // 4. Send back to client
    
    return rgtp_http3_proxy_send_response(proxy, stream_id, 200, 
                                         "content-type: application/octet-stream", 
                                         NULL, 0);
}