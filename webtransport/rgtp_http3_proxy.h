#ifndef RGTP_HTTP3_PROXY_H
#define RGTP_HTTP3_PROXY_H

#include "../include/rgtp/rgtp.h"
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* -------------------------------------------------------------------------- */
/* HTTP/3 Proxy Configuration Structure                                      */
/* -------------------------------------------------------------------------- */
typedef struct {
    uint16_t http3_port;              // HTTP/3 listening port
    uint16_t rgtp_port;               // RGTP backend port
    char* backend_host;               // RGTP backend host
    int max_concurrent_streams;       // Maximum concurrent HTTP/3 streams
    int idle_timeout_ms;              // Connection idle timeout
    int keepalive_interval_ms;        // Keep-alive ping interval
    int buffer_size;                  // Internal buffer size
    void* user_data;                  // User data for callbacks
} rgtp_http3_proxy_config_t;

/* -------------------------------------------------------------------------- */
/* HTTP/3 Proxy Handle Structure                                             */
/* -------------------------------------------------------------------------- */
typedef struct rgtp_http3_proxy rgtp_http3_proxy_t;

/* -------------------------------------------------------------------------- */
/* HTTP/3 Proxy Callback Types                                               */
/* -------------------------------------------------------------------------- */
typedef void (*rgtp_http3_request_callback_t)(
    rgtp_http3_proxy_t* proxy,
    const char* path,
    const char* method,
    const char* headers,
    void* user_data
);

typedef void (*rgtp_http3_response_callback_t)(
    rgtp_http3_proxy_t* proxy,
    int stream_id,
    int status_code,
    const char* headers,
    const void* body,
    size_t body_len,
    void* user_data
);

typedef void (*rgtp_http3_error_callback_t)(
    rgtp_http3_proxy_t* proxy,
    int error_code,
    const char* error_message,
    void* user_data
);

/* -------------------------------------------------------------------------- */
/* HTTP/3 Proxy Configuration Functions                                      */
/* -------------------------------------------------------------------------- */
rgtp_http3_proxy_config_t* rgtp_http3_config_create();
void rgtp_http3_config_destroy(rgtp_http3_proxy_config_t* config);
void rgtp_http3_config_set_port(rgtp_http3_proxy_config_t* config, uint16_t port);
void rgtp_http3_config_set_backend(rgtp_http3_proxy_config_t* config, 
                                   const char* host, uint16_t port);
void rgtp_http3_config_set_max_streams(rgtp_http3_proxy_config_t* config, int max_streams);
void rgtp_http3_config_set_idle_timeout(rgtp_http3_proxy_config_t* config, int timeout_ms);
void rgtp_http3_config_set_keepalive(rgtp_http3_proxy_config_t* config, int interval_ms);

/* -------------------------------------------------------------------------- */
/* HTTP/3 Proxy Lifecycle Functions                                          */
/* -------------------------------------------------------------------------- */
rgtp_http3_proxy_t* rgtp_http3_proxy_create(const rgtp_http3_proxy_config_t* config);
int rgtp_http3_proxy_start(rgtp_http3_proxy_t* proxy);
int rgtp_http3_proxy_stop(rgtp_http3_proxy_t* proxy);
void rgtp_http3_proxy_destroy(rgtp_http3_proxy_t* proxy);

/* -------------------------------------------------------------------------- */
/* HTTP/3 Proxy Callback Registration Functions                              */
/* -------------------------------------------------------------------------- */
void rgtp_http3_proxy_set_request_callback(rgtp_http3_proxy_t* proxy,
                                           rgtp_http3_request_callback_t callback);
void rgtp_http3_proxy_set_response_callback(rgtp_http3_proxy_t* proxy,
                                            rgtp_http3_response_callback_t callback);
void rgtp_http3_proxy_set_error_callback(rgtp_http3_proxy_t* proxy,
                                         rgtp_http3_error_callback_t callback);

/* -------------------------------------------------------------------------- */
/* HTTP/3 Proxy Management Functions                                         */
/* -------------------------------------------------------------------------- */
int rgtp_http3_proxy_add_route(rgtp_http3_proxy_t* proxy, 
                               const char* path_pattern,
                               rgtp_http3_request_callback_t handler);
int rgtp_http3_proxy_remove_route(rgtp_http3_proxy_t* proxy, const char* path_pattern);
int rgtp_http3_proxy_forward_request(rgtp_http3_proxy_t* proxy,
                                     int stream_id,
                                     const char* path,
                                     const char* method,
                                     const char* headers,
                                     const void* body,
                                     size_t body_len);
int rgtp_http3_proxy_send_response(rgtp_http3_proxy_t* proxy,
                                   int stream_id,
                                   int status_code,
                                   const char* headers,
                                   const void* body,
                                   size_t body_len);

/* -------------------------------------------------------------------------- */
/* HTTP/3 Proxy Statistics Functions                                         */
/* -------------------------------------------------------------------------- */
typedef struct {
    uint64_t total_requests;
    uint64_t total_responses;
    uint64_t total_errors;
    uint64_t active_streams;
    uint64_t bytes_sent;
    uint64_t bytes_received;
    float avg_response_time_ms;
    int current_connections;
} rgtp_http3_stats_t;

int rgtp_http3_proxy_get_stats(rgtp_http3_proxy_t* proxy, rgtp_http3_stats_t* stats);

/* -------------------------------------------------------------------------- */
/* Utility Functions                                                         */
/* -------------------------------------------------------------------------- */
const char* rgtp_http3_error_string(int error_code);
const char* rgtp_http3_status_reason(int status_code);

#ifdef __cplusplus
}
#endif

#endif // RGTP_HTTP3_PROXY_H