/**
 * @file rgtp_core.c
 * @brief Red Giant Transport Protocol - Core Implementation
 * @version 1.0.0
 * 
 * This file implements the core RGTP functionality including:
 * - Socket management
 * - Exposure surface creation and management
 * - Chunk exposure and adaptive rate control
 * - Network packet handling
 * 
 * @copyright MIT License
 */

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <io.h>
#include <windows.h>
#define close closesocket
#define getpid() GetCurrentProcessId()
#define usleep(x) Sleep((x) / 1000)
static int winsock_initialized = 0;
#else
#include <sys/socket.h>
#include <netinet/ip.h>
#include <unistd.h>
#include <sys/types.h>
#include <unistd.h>
#endif

#include "rgtp/rgtp.h"
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>

/**
 * @brief Initialize RGTP library (call once per process)
 * @return 0 on success, -1 on error
 */
int rgtp_init(void) {
    #ifdef _WIN32
    // Initialize Winsock
    if (!winsock_initialized) {
        WSADATA wsaData;
        if (WSAStartup(MAKEWORD(2,2), &wsaData) != 0) {
            return -1;
        }
        winsock_initialized = 1;
    }
    #endif
    return 0;
}

/**
 * @brief Cleanup RGTP library (call before exit)
 */
void rgtp_cleanup(void) {
    #ifdef _WIN32
    if (winsock_initialized) {
        WSACleanup();
        winsock_initialized = 0;
    }
    #endif
}

// Create RGTP socket (raw socket for custom protocol)
int rgtp_socket(void) {
    #ifdef _WIN32
    // Initialize Winsock if not already done
    if (!winsock_initialized) {
        WSADATA wsaData;
        if (WSAStartup(MAKEWORD(2,2), &wsaData) != 0) {
            return -1;
        }
        winsock_initialized = 1;
    }
    #endif
    
    // Try to create raw IP socket for our custom protocol
    // Note: On Windows, raw sockets require admin privileges
    int sockfd = socket(AF_INET, SOCK_RAW, IPPROTO_RGTP);
    if (sockfd < 0) {
        #ifdef _WIN32
        // If raw socket fails, fall back to UDP socket for testing
        // This allows the library to function without admin privileges
        sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (sockfd < 0) {
            return -1;
        }
        #endif
    }
    
    // Set socket options for optimal performance
    int opt = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));
    
    // Large buffers for high throughput
    int buffer_size = 2 * 1024 * 1024; // 2MB
    setsockopt(sockfd, SOL_SOCKET, SO_RCVBUF, (const char*)&buffer_size, sizeof(buffer_size));
    setsockopt(sockfd, SOL_SOCKET, SO_SNDBUF, (const char*)&buffer_size, sizeof(buffer_size));
    
    return sockfd;
}

// Bind RGTP socket
int rgtp_bind(int sockfd, uint16_t port) {
    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);
    
    return bind(sockfd, (struct sockaddr*)&addr, sizeof(addr));
}

// Calculate optimal chunk size based on network conditions
static uint32_t calculate_optimal_chunk_size(size_t total_size) {
    // Start with MTU-based sizing (1500 - IP header - RGTP header)
    uint32_t base_chunk = 1500 - 20 - sizeof(rgtp_header_t);
    
    // Scale based on total size
    if (total_size < 64 * 1024) {
        return base_chunk; // Small files: single MTU chunks
    } else if (total_size < 1024 * 1024) {
        return base_chunk * 4; // Medium files: 4x MTU
    } else {
        return base_chunk * 16; // Large files: 16x MTU (jumbo frames)
    }
}

// Create exposure surface - the core innovation
static rgtp_surface_t* create_exposure_surface(uint32_t session_id, 
                                               const void* data, size_t size,
                                               struct sockaddr_in* dest) {
    (void)data; // Mark parameter as used to avoid warning
    rgtp_surface_t* surface = calloc(1, sizeof(rgtp_surface_t));
    if (!surface) return NULL;
    
    surface->session_id = session_id;
    surface->peer_addr = *dest;
    
    // Calculate chunking
    uint32_t chunk_size = calculate_optimal_chunk_size(size);
    uint32_t chunk_count = (size + chunk_size - 1) / chunk_size;
    
    // Initialize manifest
    surface->manifest.total_size = size;
    surface->manifest.chunk_count = chunk_count;
    surface->manifest.optimal_chunk_size = chunk_size;
    surface->manifest.exposure_mode = 1; // Adaptive mode
    surface->manifest.priority = 100;    // Normal priority
    
    // Create chunk bitmap - this is the key innovation
    surface->bitmap_size = (chunk_count + 7) / 8; // Bits to bytes
    surface->chunk_bitmap = calloc(1, surface->bitmap_size);
    if (!surface->chunk_bitmap) {
        free(surface);
        return NULL;
    }
    
    // Initialize adaptive parameters
    surface->exposure_rate = 100;        // Start with 100 chunks/sec
    surface->congestion_window = 10;     // Conservative start
    surface->pull_pressure = 0;
    
    return surface;
}

// Set chunk as exposed in bitmap
static void mark_chunk_exposed(rgtp_surface_t* surface, uint32_t chunk_id) {
    if (chunk_id >= surface->manifest.chunk_count) return;
    
    uint32_t byte_idx = chunk_id / 8;
    uint32_t bit_idx = chunk_id % 8;
    
    surface->chunk_bitmap[byte_idx] |= (1 << bit_idx);
}

// Check if chunk is exposed
static bool is_chunk_exposed(rgtp_surface_t* surface, uint32_t chunk_id) {
    if (chunk_id >= surface->manifest.chunk_count) return false;
    
    uint32_t byte_idx = chunk_id / 8;
    uint32_t bit_idx = chunk_id % 8;
    
    return (surface->chunk_bitmap[byte_idx] & (1 << bit_idx)) != 0;
}

// Send RGTP packet
static int send_rgtp_packet(int sockfd, struct sockaddr_in* dest,
                           rgtp_packet_type_t type, uint32_t session_id,
                           uint32_t sequence, const void* payload, 
                           uint32_t payload_size) {
    
    // Prepare packet
    size_t packet_size = sizeof(rgtp_header_t) + payload_size;
    uint8_t* packet = malloc(packet_size);
    if (!packet) return -1;
    
    // Fill header
    rgtp_header_t* header = (rgtp_header_t*)packet;
    header->version = 1;
    header->type = type;
    header->flags = 0;
    header->session_id = htonl(session_id);
    header->sequence = htonl(sequence);
    header->chunk_size = htonl(payload_size);
    
    // Copy payload
    if (payload && payload_size > 0) {
        memcpy(packet + sizeof(rgtp_header_t), payload, payload_size);
    }
    
    // Calculate checksum (simple for now)
    header->checksum = 0;
    uint32_t checksum = 0;
    for (size_t i = 0; i < packet_size; i++) {
        checksum += packet[i];
    }
    header->checksum = htonl(checksum);
    
    // Send packet
    ssize_t sent = sendto(sockfd, (const char*)packet, (int)packet_size, 0,
                         (struct sockaddr*)dest, sizeof(*dest));
    
    free(packet);
    return (sent == (ssize_t)packet_size) ? 0 : -1;
}

// Main exposure function - this is where the magic happens
int rgtp_expose_data(int sockfd, const void* data, size_t size,
                     struct sockaddr_in* dest, rgtp_surface_t** surface_out) {
    
    // Generate session ID
    uint32_t session_id = (uint32_t)time(NULL) ^ (uint32_t)getpid();
    
    // Create exposure surface
    rgtp_surface_t* surface = create_exposure_surface(session_id, data, size, dest);
    if (!surface) return -1;
    
    surface->sockfd = sockfd;
    
    // Step 1: Send exposure request
    if (send_rgtp_packet(sockfd, dest, RGTP_EXPOSE_REQUEST, session_id, 0, NULL, 0) < 0) {
        free(surface->chunk_bitmap);
        free(surface);
        return -1;
    }
    
    // Step 2: Send manifest
    if (send_rgtp_packet(sockfd, dest, RGTP_EXPOSE_MANIFEST, session_id, 0,
                        &surface->manifest, sizeof(surface->manifest)) < 0) {
        free(surface->chunk_bitmap);
        free(surface);
        return -1;
    }
    
    // Step 3: Begin adaptive exposure
    // This is the core innovation - expose chunks based on pull pressure
    uint32_t chunk_size = surface->manifest.optimal_chunk_size;
    
    for (uint32_t i = 0; i < surface->manifest.chunk_count; i++) {
        // Calculate chunk boundaries
        size_t chunk_start = i * chunk_size;
        size_t chunk_end = chunk_start + chunk_size;
        if (chunk_end > size) chunk_end = size;
        size_t actual_chunk_size = chunk_end - chunk_start;
        
        // Announce chunk availability
        if (send_rgtp_packet(sockfd, dest, RGTP_CHUNK_AVAILABLE, session_id, i, 
                            NULL, 0) < 0) {
            continue; // Don't fail entire exposure for one chunk
        }
        
        // Mark as exposed
        mark_chunk_exposed(surface, i);
        surface->bytes_exposed += actual_chunk_size;
        
        // Adaptive rate limiting based on congestion window
        if (i > 0 && (i % surface->congestion_window) == 0) {
            usleep(1000000 / surface->exposure_rate); // Rate limiting
        }
    }
    
    // Step 4: Signal exposure complete
    send_rgtp_packet(sockfd, dest, RGTP_EXPOSURE_COMPLETE, session_id, 0, NULL, 0);
    
    *surface_out = surface;
    return 0;
}

// Handle pull requests and send chunk data
int rgtp_handle_pull_request(rgtp_surface_t* surface, uint32_t chunk_id,
                            const void* original_data) {
    
    if (!is_chunk_exposed(surface, chunk_id)) {
        return -1; // Chunk not exposed yet
    }
    
    // Calculate chunk data
    const uint8_t* data_bytes = (const uint8_t*)original_data;
    uint32_t chunk_size = surface->manifest.optimal_chunk_size;
    size_t chunk_start = chunk_id * chunk_size;
    size_t chunk_end = chunk_start + chunk_size;
    if (chunk_end > surface->manifest.total_size) {
        chunk_end = surface->manifest.total_size;
    }
    size_t actual_chunk_size = chunk_end - chunk_start;
    
    // Send chunk data
    return send_rgtp_packet(surface->sockfd, &surface->peer_addr,
                           RGTP_CHUNK_DATA, surface->session_id, chunk_id,
                           data_bytes + chunk_start, actual_chunk_size);
}

// Adaptive exposure rate adjustment
int rgtp_adaptive_exposure(rgtp_surface_t* surface) {
    // Increase rate if pull pressure is high (receiver is keeping up)
    if (surface->pull_pressure > surface->congestion_window) {
        surface->exposure_rate = (surface->exposure_rate * 11) / 10; // +10%
        surface->congestion_window++;
    }
    // Decrease rate if no pull pressure (receiver is overwhelmed)
    else if (surface->pull_pressure == 0) {
        surface->exposure_rate = (surface->exposure_rate * 9) / 10; // -10%
        if (surface->congestion_window > 1) {
            surface->congestion_window--;
        }
    }
    
    // Bounds checking
    if (surface->exposure_rate < 10) surface->exposure_rate = 10;
    if (surface->exposure_rate > 10000) surface->exposure_rate = 10000;
    
    return 0;
}

// Get exposure completion percentage
int rgtp_get_exposure_status(rgtp_surface_t* surface, float* completion_pct) {
    if (!surface || !completion_pct) return -1;
    
    uint32_t exposed_chunks = 0;
    for (uint32_t i = 0; i < surface->manifest.chunk_count; i++) {
        if (is_chunk_exposed(surface, i)) {
            exposed_chunks++;
        }
    }
    
    *completion_pct = (float)exposed_chunks / surface->manifest.chunk_count * 100.0f;
    return 0;
}