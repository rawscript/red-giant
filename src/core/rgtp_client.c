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

#include "rgtp/rgtp.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

typedef struct {
    uint32_t session_id;
    rgtp_manifest_t manifest;
    uint8_t* chunk_bitmap;     // Which chunks are available
    uint8_t* received_bitmap;  // Which chunks we've received
    void* data_buffer;         // Reconstructed data
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
    
    ssize_t received = recvfrom(sockfd, buffer, sizeof(buffer), 0,
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
    
    return sendto(sockfd, &header, sizeof(header), 0,
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

// Main pull function - implements the exposure-based paradigm
int rgtp_pull_data(int sockfd, struct sockaddr_in* source, 
                   void* buffer, size_t* size) {
    
    rgtp_pull_session_t session = {0};
    session.sockfd = sockfd;
    session.server_addr = *source;
    
    printf("[RGTP] Starting pull session from %s:%d\n", 
           inet_ntoa(source->sin_addr), ntohs(source->sin_port));
    
    // Phase 1: Wait for exposure request and manifest
    rgtp_header_t header;
    uint8_t payload[4096];
    struct sockaddr_in from;
    
    while (1) {
        int payload_size = receive_rgtp_packet(sockfd, &header, payload, 
                                              sizeof(payload), &from);
        
        if (payload_size < 0) continue;
        
        if (header.type == RGTP_EXPOSE_REQUEST) {
            session.session_id = header.session_id;
            printf("[RGTP] Received exposure request, session %u\n", session.session_id);
        }
        else if (header.type == RGTP_EXPOSE_MANIFEST && 
                 header.session_id == session.session_id) {
            
            if (payload_size >= (int)sizeof(rgtp_manifest_t)) {
                memcpy(&session.manifest, payload, sizeof(rgtp_manifest_t));
                
                printf("[RGTP] Manifest received: %lu bytes, %u chunks\n",
                       session.manifest.total_size, session.manifest.chunk_count);
                
                // Allocate bitmaps and data buffer
                uint32_t bitmap_size = (session.manifest.chunk_count + 7) / 8;
                session.chunk_bitmap = calloc(1, bitmap_size);
                session.received_bitmap = calloc(1, bitmap_size);
                session.data_buffer = malloc(session.manifest.total_size);
                
                if (!session.chunk_bitmap || !session.received_bitmap || 
                    !session.data_buffer) {
                    return -1;
                }
                
                break; // Ready to start pulling
            }
        }
    }
    
    // Phase 2: Listen for chunk availability and pull aggressively
    time_t start_time = time(NULL);
    uint32_t next_chunk_to_request = 0;
    
    while (session.chunks_received < session.manifest.chunk_count) {
        
        // Listen for chunk availability announcements
        int payload_size = receive_rgtp_packet(sockfd, &header, payload, 
                                              sizeof(payload), &from);
        
        if (payload_size >= 0 && header.session_id == session.session_id) {
            
            if (header.type == RGTP_CHUNK_AVAILABLE) {
                uint32_t chunk_id = header.sequence;
                
                // Mark chunk as available
                if (chunk_id < session.manifest.chunk_count) {
                    uint32_t byte_idx = chunk_id / 8;
                    uint32_t bit_idx = chunk_id % 8;
                    session.chunk_bitmap[byte_idx] |= (1 << bit_idx);
                    
                    printf("[RGTP] Chunk %u available\n", chunk_id);
                }
            }
            else if (header.type == RGTP_CHUNK_DATA) {
                uint32_t chunk_id = header.sequence;
                
                if (chunk_id < session.manifest.chunk_count && payload_size > 0) {
                    // Store chunk data
                    size_t chunk_offset = chunk_id * session.manifest.optimal_chunk_size;
                    if (chunk_offset + payload_size <= session.manifest.total_size) {
                        memcpy((uint8_t*)session.data_buffer + chunk_offset, 
                               payload, payload_size);
                        mark_chunk_received(&session, chunk_id);
                        
                        printf("[RGTP] Received chunk %u (%d bytes) - %u/%u complete\n",
                               chunk_id, payload_size, session.chunks_received,
                               session.manifest.chunk_count);
                    }
                }
            }
            else if (header.type == RGTP_EXPOSURE_COMPLETE) {
                printf("[RGTP] Server completed exposure\n");
            }
        }
        
        // Aggressive pulling strategy - request available chunks immediately
        for (uint32_t i = next_chunk_to_request; i < session.manifest.chunk_count; i++) {
            if (is_chunk_available(&session, i)) {
                // Check if we already have this chunk
                uint32_t byte_idx = i / 8;
                uint32_t bit_idx = i % 8;
                bool already_received = (session.received_bitmap[byte_idx] & (1 << bit_idx)) != 0;
                
                if (!already_received) {
                    send_pull_request(sockfd, source, session.session_id, i);
                    printf("[RGTP] Requested chunk %u\n", i);
                }
                
                if (i == next_chunk_to_request) {
                    next_chunk_to_request++;
                }
            } else {
                break; // Wait for more chunks to become available
            }
        }
        
        // Timeout check
        if (time(NULL) - start_time > 30) {
            printf("[RGTP] Pull timeout\n");
            break;
        }
        
        usleep(1000); // Small delay to prevent busy waiting
    }
    
    // Phase 3: Return completed data
    if (session.chunks_received == session.manifest.chunk_count) {
        if (*size >= session.manifest.total_size) {
            memcpy(buffer, session.data_buffer, session.manifest.total_size);
            *size = session.manifest.total_size;
            
            printf("[RGTP] Pull completed successfully: %lu bytes\n", *size);
            
            // Cleanup
            free(session.chunk_bitmap);
            free(session.received_bitmap);
            free(session.data_buffer);
            
            return 0;
        } else {
            printf("[RGTP] Buffer too small\n");
            return -1;
        }
    }
    
    printf("[RGTP] Pull incomplete: %u/%u chunks received\n",
           session.chunks_received, session.manifest.chunk_count);
    
    // Cleanup
    free(session.chunk_bitmap);
    free(session.received_bitmap);
    free(session.data_buffer);
    
    return -1;
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