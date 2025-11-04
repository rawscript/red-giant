/**
 * @file rgtp_sdk.h
 * @brief Red Giant Transport Protocol - High-Level SDK
 * @version 1.0.0
 * 
 * This header provides a high-level, easy-to-use SDK for RGTP.
 * It abstracts away the low-level details and provides simple
 * APIs for common use cases.
 * 
 * Features:
 * - Simple file transfer APIs
 * - Automatic configuration
 * - Built-in error handling
 * - Progress callbacks
 * - Multi-threading support
 * 
 * @copyright MIT License
 */

#ifndef RGTP_SDK_H
#define RGTP_SDK_H

#include "rgtp.h"
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// Forward declarations
typedef struct rgtp_session rgtp_session_t;
typedef struct rgtp_client rgtp_client_t;

/**
 * @brief Progress callback function type
 * @param bytes_transferred Number of bytes transferred so far
 * @param total_bytes Total number of bytes to transfer
 * @param user_data User-provided data pointer
 */
typedef void (*rgtp_progress_callback_t)(size_t bytes_transferred, 
                                        size_t total_bytes, 
                                        void* user_data);

/**
 * @brief Error callback function type
 * @param error_code RGTP error code
 * @param error_message Human-readable error message
 * @param user_data User-provided data pointer
 */
typedef void (*rgtp_error_callback_t)(int error_code, 
                                     const char* error_message, 
                                     void* user_data);

/**
 * @brief Session configuration
 */
typedef struct {
    uint32_t chunk_size;              ///< Chunk size in bytes (0 = auto)
    uint32_t exposure_rate;           ///< Initial exposure rate (chunks/sec)
    bool adaptive_mode;               ///< Enable adaptive rate control
    bool enable_compression;          ///< Enable data compression
    bool enable_encryption;           ///< Enable encryption
    uint16_t port;                   ///< Port number (0 = auto)
    int timeout_ms;                  ///< Operation timeout in milliseconds
    rgtp_progress_callback_t progress_cb; ///< Progress callback
    rgtp_error_callback_t error_cb;   ///< Error callback
    void* user_data;                 ///< User data for callbacks
} rgtp_config_t;

/**
 * @brief Transfer statistics
 */
typedef struct {
    size_t bytes_transferred;        ///< Bytes successfully transferred
    size_t total_bytes;             ///< Total bytes in transfer
    double throughput_mbps;         ///< Current throughput in MB/s
    double avg_throughput_mbps;     ///< Average throughput in MB/s
    uint32_t chunks_transferred;    ///< Number of chunks transferred
    uint32_t total_chunks;          ///< Total number of chunks
    uint32_t retransmissions;       ///< Number of retransmissions
    double completion_percent;      ///< Completion percentage (0-100)
    int64_t elapsed_ms;            ///< Elapsed time in milliseconds
    int64_t estimated_remaining_ms; ///< Estimated remaining time
} rgtp_stats_t;

// =============================================================================
// High-Level Session API
// =============================================================================

/**
 * @brief Create a new RGTP session with default configuration
 * @return New session handle, or NULL on error
 */
rgtp_session_t* rgtp_session_create(void);

/**
 * @brief Create a new RGTP session with custom configuration
 * @param config Configuration parameters
 * @return New session handle, or NULL on error
 */
rgtp_session_t* rgtp_session_create_with_config(const rgtp_config_t* config);

/**
 * @brief Destroy an RGTP session
 * @param session Session to destroy
 */
void rgtp_session_destroy(rgtp_session_t* session);

/**
 * @brief Expose data for pulling
 * @param session RGTP session
 * @param data Data to expose
 * @param size Size of data in bytes
 * @return 0 on success, -1 on error
 */
int rgtp_session_expose_data(rgtp_session_t* session, const void* data, size_t size);

/**
 * @brief Expose a file for pulling
 * @param session RGTP session
 * @param filename Path to file to expose
 * @return 0 on success, -1 on error
 */
int rgtp_session_expose_file(rgtp_session_t* session, const char* filename);

/**
 * @brief Wait for exposure to complete
 * @param session RGTP session
 * @return 0 on success, -1 on error or timeout
 */
int rgtp_session_wait_complete(rgtp_session_t* session);

/**
 * @brief Get session statistics
 * @param session RGTP session
 * @param stats Output statistics structure
 * @return 0 on success, -1 on error
 */
int rgtp_session_get_stats(rgtp_session_t* session, rgtp_stats_t* stats);

/**
 * @brief Cancel ongoing exposure
 * @param session RGTP session
 * @return 0 on success, -1 on error
 */
int rgtp_session_cancel(rgtp_session_t* session);

// =============================================================================
// High-Level Client API
// =============================================================================

/**
 * @brief Create a new RGTP client
 * @return New client handle, or NULL on error
 */
rgtp_client_t* rgtp_client_create(void);

/**
 * @brief Create a new RGTP client with custom configuration
 * @param config Configuration parameters
 * @return New client handle, or NULL on error
 */
rgtp_client_t* rgtp_client_create_with_config(const rgtp_config_t* config);

/**
 * @brief Destroy an RGTP client
 * @param client Client to destroy
 */
void rgtp_client_destroy(rgtp_client_t* client);

/**
 * @brief Pull data from a remote exposer
 * @param client RGTP client
 * @param host Remote host address
 * @param port Remote port number
 * @param buffer Buffer to store received data
 * @param buffer_size Size of buffer
 * @param received_size Output: actual bytes received
 * @return 0 on success, -1 on error
 */
int rgtp_client_pull_data(rgtp_client_t* client, 
                          const char* host, uint16_t port,
                          void* buffer, size_t buffer_size, 
                          size_t* received_size);

/**
 * @brief Pull data and save to file
 * @param client RGTP client
 * @param host Remote host address
 * @param port Remote port number
 * @param filename Output filename
 * @return 0 on success, -1 on error
 */
int rgtp_client_pull_to_file(rgtp_client_t* client,
                             const char* host, uint16_t port,
                             const char* filename);

/**
 * @brief Get client statistics
 * @param client RGTP client
 * @param stats Output statistics structure
 * @return 0 on success, -1 on error
 */
int rgtp_client_get_stats(rgtp_client_t* client, rgtp_stats_t* stats);

/**
 * @brief Cancel ongoing pull operation
 * @param client RGTP client
 * @return 0 on success, -1 on error
 */
int rgtp_client_cancel(rgtp_client_t* client);

// =============================================================================
// Convenience Functions
// =============================================================================

/**
 * @brief Simple file transfer (blocking)
 * @param filename File to transfer
 * @param dest_host Destination host
 * @param dest_port Destination port
 * @return 0 on success, -1 on error
 */
int rgtp_send_file(const char* filename, const char* dest_host, uint16_t dest_port);

/**
 * @brief Simple file receive (blocking)
 * @param source_host Source host
 * @param source_port Source port
 * @param output_filename Output file
 * @return 0 on success, -1 on error
 */
int rgtp_receive_file(const char* source_host, uint16_t source_port, 
                      const char* output_filename);

/**
 * @brief Transfer data between memory buffers
 * @param data Data to send
 * @param size Size of data
 * @param dest_host Destination host
 * @param dest_port Destination port
 * @return 0 on success, -1 on error
 */
int rgtp_send_data(const void* data, size_t size, 
                   const char* dest_host, uint16_t dest_port);

/**
 * @brief Receive data into memory buffer
 * @param source_host Source host
 * @param source_port Source port
 * @param buffer Output buffer
 * @param buffer_size Buffer size
 * @param received_size Output: bytes received
 * @return 0 on success, -1 on error
 */
int rgtp_receive_data(const char* source_host, uint16_t source_port,
                      void* buffer, size_t buffer_size, size_t* received_size);

// =============================================================================
// Configuration Helpers
// =============================================================================

/**
 * @brief Get default configuration
 * @param config Output configuration structure
 */
void rgtp_config_default(rgtp_config_t* config);

/**
 * @brief Configure for LAN (high bandwidth, low latency)
 * @param config Configuration to modify
 */
void rgtp_config_for_lan(rgtp_config_t* config);

/**
 * @brief Configure for WAN (variable bandwidth, higher latency)
 * @param config Configuration to modify
 */
void rgtp_config_for_wan(rgtp_config_t* config);

/**
 * @brief Configure for mobile networks (limited bandwidth, high latency)
 * @param config Configuration to modify
 */
void rgtp_config_for_mobile(rgtp_config_t* config);

/**
 * @brief Configure for satellite links (very high latency)
 * @param config Configuration to modify
 */
void rgtp_config_for_satellite(rgtp_config_t* config);

// =============================================================================
// Utility Functions
// =============================================================================

/**
 * @brief Get human-readable error message
 * @param error_code RGTP error code
 * @return Error message string
 */
const char* rgtp_error_string(int error_code);

/**
 * @brief Format throughput for display
 * @param throughput_mbps Throughput in MB/s
 * @param buffer Output buffer
 * @param buffer_size Buffer size
 * @return Formatted string
 */
const char* rgtp_format_throughput(double throughput_mbps, char* buffer, size_t buffer_size);

/**
 * @brief Format time duration for display
 * @param milliseconds Time in milliseconds
 * @param buffer Output buffer
 * @param buffer_size Buffer size
 * @return Formatted string
 */
const char* rgtp_format_duration(int64_t milliseconds, char* buffer, size_t buffer_size);

/**
 * @brief Format data size for display
 * @param bytes Size in bytes
 * @param buffer Output buffer
 * @param buffer_size Buffer size
 * @return Formatted string
 */
const char* rgtp_format_size(size_t bytes, char* buffer, size_t buffer_size);

/**
 * @brief Enable debug logging
 * @param enable True to enable, false to disable
 */
void rgtp_set_debug_logging(bool enable);

/**
 * @brief Set log callback function
 * @param callback Log callback function (NULL to disable)
 */
void rgtp_set_log_callback(void (*callback)(const char* message));

#ifdef __cplusplus
}
#endif

#endif // RGTP_SDK_H