/**
 * @file rgtp.h
 * @brief Red Giant Transport Protocol (RGTP) - Layer 4 Implementation
 * @version 1.0.0
 * @date 2024
 * 
 * RGTP is a revolutionary Layer 4 transport protocol that implements
 * exposure-based data transmission, solving fundamental TCP limitations
 * while providing better performance than UDP.
 * 
 * Key Features:
 * - Stateless chunk-based transfers
 * - Natural multicast support  
 * - Adaptive flow control
 * - Built-in resume capability
 * - No head-of-line blocking
 * 
 * @copyright MIT License
 */

#ifndef RGTP_H
#define RGTP_H

#include <stdint.h>
#include <stdbool.h>
#include <sys/socket.h>
#include <netinet/in.h>

// RGTP operates directly over IP (protocol number 253 - experimental)
#define IPPROTO_RGTP 253
#define RGTP_DEFAULT_PORT 9999

// Exposure-based packet types
typedef enum {
    RGTP_EXPOSE_REQUEST = 0x01,    // "I have data to expose"
    RGTP_EXPOSE_MANIFEST = 0x02,   // "Here's what I'm exposing"
    RGTP_CHUNK_AVAILABLE = 0x03,   // "Chunk X is ready for pull"
    RGTP_PULL_REQUEST = 0x04,      // "Give me chunk X"
    RGTP_CHUNK_DATA = 0x05,        // "Here's chunk X data"
    RGTP_EXPOSURE_COMPLETE = 0x06, // "All chunks exposed"
    RGTP_PULL_COMPLETE = 0x07,     // "All chunks received"
    RGTP_ERROR = 0xFF
} rgtp_packet_type_t;

// RGTP Header (20 bytes - efficient)
typedef struct __attribute__((packed)) {
    uint8_t version;           // Protocol version
    uint8_t type;             // Packet type
    uint16_t flags;           // Control flags
    uint32_t session_id;      // Exposure session
    uint32_t sequence;        // Chunk sequence number
    uint32_t chunk_size;      // Size of this chunk
    uint32_t checksum;        // Header + data checksum
} rgtp_header_t;

// Exposure Manifest - describes what's being exposed
typedef struct __attribute__((packed)) {
    uint64_t total_size;      // Total data size
    uint32_t chunk_count;     // Number of chunks
    uint32_t optimal_chunk_size; // Recommended chunk size
    uint16_t exposure_mode;   // Sequential, parallel, adaptive
    uint16_t priority;        // Exposure priority
    uint8_t content_hash[32]; // SHA-256 of complete data
} rgtp_manifest_t;

// Exposure Surface - the core innovation
typedef struct {
    uint32_t session_id;
    rgtp_manifest_t manifest;
    
    // Chunk availability bitmap - this is the key innovation
    uint8_t* chunk_bitmap;    // 1 bit per chunk (exposed/not exposed)
    uint32_t bitmap_size;
    
    // Adaptive exposure control
    uint32_t exposure_rate;   // Chunks per second
    uint32_t congestion_window; // Like TCP cwnd but for exposure
    uint32_t pull_pressure;   // How many pulls are pending
    
    // Performance metrics
    uint64_t bytes_exposed;
    uint64_t bytes_pulled;
    uint32_t retransmissions;
    
    // Socket info
    int sockfd;
    struct sockaddr_in peer_addr;
} rgtp_surface_t;

// Core RGTP functions
int rgtp_socket(void);
int rgtp_bind(int sockfd, uint16_t port);
int rgtp_expose_data(int sockfd, const void* data, size_t size, 
                     struct sockaddr_in* dest, rgtp_surface_t** surface);
int rgtp_pull_data(int sockfd, struct sockaddr_in* source, 
                   void* buffer, size_t* size);

// Exposure control
int rgtp_set_exposure_rate(rgtp_surface_t* surface, uint32_t chunks_per_sec);
int rgtp_get_exposure_status(rgtp_surface_t* surface, float* completion_pct);
int rgtp_adaptive_exposure(rgtp_surface_t* surface); // Auto-adjust based on network

// Advanced features
int rgtp_selective_pull(int sockfd, struct sockaddr_in* source, 
                        uint32_t* chunk_ids, uint32_t count);
int rgtp_parallel_exposure(rgtp_surface_t* surface, uint32_t thread_count);

/**
 * @brief Get RGTP library version
 * @return Version string (e.g., "1.0.0")
 */
const char* rgtp_version(void);

/**
 * @brief Get RGTP build information
 * @return Build info string
 */
const char* rgtp_build_info(void);

/**
 * @brief Initialize RGTP library (call once per process)
 * @return 0 on success, -1 on error
 */
int rgtp_init(void);

/**
 * @brief Cleanup RGTP library (call before exit)
 */
void rgtp_cleanup(void);

#ifdef __cplusplus
}
#endif

#endif // RGTP_H