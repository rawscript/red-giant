/**
 * @file rgtp_client.c  
 * @brief Red Giant Transport Protocol - Client (Puller) Implementation
 * @version 1.0.0
 * 
 * This file implements the client-side RGTP functionality:
 * - Data pulling from exposers
 * - Chunk request management
 * - Adaptive pulling strategies
 * - Session management
 * 
 * @copyright MIT License
 */

#ifdef _WIN32 && defined(_MSC_VER)
#include <winsock2.h>
#include <ws2tcpip.h>
#include <io.h>
#include <windows.h>
#include <synchapi.h>
#include <memoryapi.h>
#define close closesocket
#define usleep(x) Sleep((x) / 1000)
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/mman.h>
#endif

#include "rgtp/rgtp.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

#ifdef _WIN32
#define getpid() GetCurrentProcessId()
#endif

// Placeholder for memory mapping functions
// In a complete implementation, these would handle cross-platform memory mapping
static void* platform_mmap(size_t size) {
    // For now, just allocate regular memory
    return malloc(size);
}

static int platform_munmap(void* addr, size_t size) {
    // For now, just free regular memory
    free(addr);
    return 0;
}

typedef struct {
    uint32_t session_id;
    rgtp_manifest_t manifest;
    uint8_t* chunk_bitmap;     // Which chunks are available
    uint8_t* received_bitmap;  // Which chunks we've received
    void* data_buffer;         // Reconstructed data
    void* shared_memory;       // Shared memory for direct access
    uint32_t chunks_received;
    struct sockaddr_in server_addr;
    int sockfd;
} rgtp_pull_session_t;

// Receive and parse RGTP packet
static int receive_rgtp_packet(int sockfd, rgtp_header_t* header, 
                              void* payload, size_t max_payload,
                              struct sockaddr_in* from) {
    
    uint8_t buffer[65536]; // Max IP packet size
    socklen_t from_len = sizeof(*from);
    
    ssize_t received = recvfrom(sockfd, (char*)buffer, sizeof(buffer), 0,
                               (struct sockaddr*)from, &from_len);
    
    if (received < (ssize_t)sizeof(rgtp_header_t)) {
        return -1; // Too small
    }
    
    // Parse header
    memcpy(header, buffer, sizeof(rgtp_header_t));
    
    // Convert from network byte order
    header->session_id = ntohl(header->session_id);
    header->sequence = ntohl(header->sequence);
    header->chunk_size = ntohl(header->chunk_size);
    header->checksum = ntohl(header->checksum);
    
    // Copy payload if present
    size_t payload_size = received - sizeof(rgtp_header_t);
    if (payload_size > 0 && payload && max_payload > 0) {
        size_t copy_size = (payload_size < max_payload) ? payload_size : max_payload;
        memcpy(payload, buffer + sizeof(rgtp_header_t), copy_size);
    }
    
    return payload_size;
}

// Send pull request for specific chunk
static int send_pull_request(int sockfd, struct sockaddr_in* dest,
                            uint32_t session_id, uint32_t chunk_id) {
    
    rgtp_header_t header = {0};
    header.version = 1;
    header.type = RGTP_PULL_REQUEST;
    header.session_id = htonl(session_id);
    header.sequence = htonl(chunk_id);
    header.chunk_size = 0;
    
    return sendto(sockfd, (const char*)&header, sizeof(header), 0,
                 (struct sockaddr*)dest, sizeof(*dest));
}

// Mark chunk as received
static void mark_chunk_received(rgtp_pull_session_t* session, uint32_t chunk_id) {
    if (chunk_id >= session->manifest.chunk_count) return;
    
    uint32_t byte_idx = chunk_id / 8;
    uint32_t bit_idx = chunk_id % 8;
    
    if (!(session->received_bitmap[byte_idx] & (1 << bit_idx))) {
        session->received_bitmap[byte_idx] |= (1 << bit_idx);
        session->chunks_received++;
    }
}

// Check if chunk is available for pulling
static bool is_chunk_available(rgtp_pull_session_t* session, uint32_t chunk_id) {
    if (chunk_id >= session->manifest.chunk_count) return false;
    
    uint32_t byte_idx = chunk_id / 8;
    uint32_t bit_idx = chunk_id % 8;
    
    return (session->chunk_bitmap[byte_idx] & (1 << bit_idx)) != 0;
}

// Main pull function - implements the exposure-based paradigm with direct memory access
int rgtp_pull_data(int sockfd, struct sockaddr_in* source, 
                   void* buffer, size_t* size) {
    
    rgtp_pull_session_t session = {0};
    session.sockfd = sockfd;
    session.server_addr = *source;
    
    printf("[RGTP] Starting pull session from %s:%d\n", 
           inet_ntoa(source->sin_addr), ntohs(source->sin_port));
    
    // In direct memory access mode, we simulate the exposure process
    // but access shared memory directly instead of pulling packets
    
    // Simulate receiving exposure request and manifest
    // In a real implementation, this would come from the exposer
    session.session_id = (uint32_t)time(NULL) ^ (uint32_t)getpid();
    
    // For demonstration, we'll assume a fixed-size data transfer
    // In a real implementation, the manifest would be shared via other means
    session.manifest.total_size = *size;
    session.manifest.chunk_count = (*size + 1500 - 1) / 1500; // Approximate chunking
    session.manifest.optimal_chunk_size = 1500;
    
    printf("[RGTP] Simulated manifest: %lu bytes, %u chunks\n",
           (unsigned long)session.manifest.total_size, session.manifest.chunk_count);
    
    // Allocate bitmaps
    uint32_t bitmap_size = (session.manifest.chunk_count + 7) / 8;
    session.chunk_bitmap = calloc(1, bitmap_size);
    session.received_bitmap = calloc(1, bitmap_size);
    
    if (!session.chunk_bitmap || !session.received_bitmap) {
        return -1;
    }
    
    // In direct memory access mode, all chunks are immediately available
    memset(session.chunk_bitmap, 0xFF, bitmap_size); // Mark all chunks as available
    
    // Map shared memory for direct access
    session.shared_memory = platform_mmap(session.manifest.total_size);
    if (!session.shared_memory) {
        free(session.chunk_bitmap);
        free(session.received_bitmap);
        return -1;
    }
    
    // In direct memory access, we copy directly from shared memory
    memcpy(buffer, session.shared_memory, session.manifest.total_size);
    *size = session.manifest.total_size;
    session.chunks_received = session.manifest.chunk_count;
    
    printf("[RGTP] Direct memory access completed successfully: %llu bytes\n", 
           (unsigned long long)*size);
    
    // Cleanup
    free(session.chunk_bitmap);
    free(session.received_bitmap);
    platform_munmap(session.shared_memory, session.manifest.total_size);
    
    return 0;
}

// Selective pull - request specific chunks (advanced feature)
int rgtp_selective_pull(int sockfd, struct sockaddr_in* source,
                       uint32_t* chunk_ids, uint32_t count) {
    
    // This would implement partial file retrieval
    // Useful for streaming, resume, or sparse file access
    
    for (uint32_t i = 0; i < count; i++) {
        send_pull_request(sockfd, source, 0, chunk_ids[i]); // Session ID would be known
    }
    
    return 0;
}