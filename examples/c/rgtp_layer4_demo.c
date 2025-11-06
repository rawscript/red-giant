/**
 * RGTP Layer 4 Protocol Demonstration
 * 
 * This example shows RGTP as a pure Layer 4 transport protocol that
 * completely replaces TCP/UDP. RGTP operates directly over IP packets.
 * 
 * Network Stack Comparison:
 * 
 * Traditional TCP Stack:        RGTP Stack:
 * ┌─────────────────┐          ┌─────────────────┐
 * │   Application   │          │   Application   │
 * ├─────────────────┤          ├─────────────────┤
 * │       TCP       │    →     │      RGTP       │
 * ├─────────────────┤          ├─────────────────┤
 * │       IP        │          │       IP        │
 * ├─────────────────┤          ├─────────────────┤
 * │    Ethernet     │          │    Ethernet     │
 * └─────────────────┘          └─────────────────┘
 * 
 * Key Differences:
 * - TCP: Connection-oriented, stream-based, sequential delivery
 * - RGTP: Exposure-based, chunk-oriented, pull-on-demand
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <arpa/inet.h>
#include <pthread.h>
#include "rgtp/rgtp.h"

#define RGTP_PROTOCOL_NUMBER 253  // Experimental protocol number
#define DATA_SIZE 1024 * 1024     // 1MB of test data
#define CHUNK_SIZE 64 * 1024      // 64KB chunks

// RGTP packet types
typedef enum {
    RGTP_EXPOSE_ANNOUNCE = 1,
    RGTP_CHUNK_MANIFEST = 2,
    RGTP_PULL_REQUEST = 3,
    RGTP_CHUNK_DATA = 4,
    RGTP_EXPOSURE_COMPLETE = 5
} rgtp_packet_type_t;

// RGTP header (sits directly after IP header)
typedef struct __attribute__((packed)) {
    uint8_t version;        // RGTP version (1)
    uint8_t type;          // Packet type
    uint16_t flags;        // Control flags
    uint32_t session_id;   // Exposure session ID
    uint32_t chunk_id;     // Chunk identifier
    uint32_t chunk_size;   // Size of this chunk
    uint32_t total_chunks; // Total chunks in exposure
    uint32_t checksum;     // Data integrity checksum
} rgtp_header_t;

// Exposer (server) structure
typedef struct {
    int raw_socket;           // Raw IP socket for RGTP
    struct sockaddr_in addr;  // Local address
    uint8_t* data;           // Data to expose
    size_t data_size;        // Total data size
    uint32_t session_id;     // Current session ID
    uint32_t total_chunks;   // Number of chunks
    pthread_t thread;        // Handler thread
    int running;             // Running flag
} rgtp_exposer_t;

// Puller (client) structure
typedef struct {
    int raw_socket;           // Raw IP socket for RGTP
    struct sockaddr_in target; // Target exposer address
    uint8_t* buffer;         // Received data buffer
    size_t buffer_size;      // Buffer size
    uint32_t session_id;     // Target session ID
    uint32_t chunks_received; // Chunks received so far
    uint32_t total_chunks;   // Total expected chunks
    uint8_t* chunk_map;      // Bitmap of received chunks
} rgtp_puller_t;

// Calculate simple checksum
uint32_t calculate_checksum(const uint8_t* data, size_t size) {
    uint32_t checksum = 0;
    for (size_t i = 0; i < size; i++) {
        checksum += data[i];
    }
    return checksum;
}

// Create raw RGTP socket
int create_rgtp_socket() {
    // Create raw IP socket for RGTP protocol
    int sockfd = socket(AF_INET, SOCK_RAW, RGTP_PROTOCOL_NUMBER);
    if (sockfd < 0) {
        perror("Failed to create raw socket (need root privileges)");
        return -1;
    }
    
    // Enable IP header inclusion
    int one = 1;
    if (setsockopt(sockfd, IPPROTO_IP, IP_HDRINCL, &one, sizeof(one)) < 0) {
        perror("Failed to set IP_HDRINCL");
        close(sockfd);
        return -1;
    }
    
    printf("Created raw RGTP socket (protocol %d)\n", RGTP_PROTOCOL_NUMBER);
    return sockfd;
}

// Send RGTP packet
int send_rgtp_packet(int sockfd, const struct sockaddr_in* dest, 
                     rgtp_packet_type_t type, uint32_t session_id,
                     uint32_t chunk_id, const uint8_t* data, size_t data_size) {
    
    // Prepare RGTP header with proper checksum handling
    uint32_t checksum = 0;
    if (data && data_size > 0) {
        checksum = calculate_checksum(data, data_size);
    }
    
    rgtp_header_t header = {
        .version = 1,
        .type = type,
        .flags = 0,
        .session_id = htonl(session_id),
        .chunk_id = htonl(chunk_id),
        .chunk_size = htonl(data_size),
        .total_chunks = 0, // Set by caller if needed
        .checksum = htonl(checksum)
    };
    
    // Create packet buffer
    size_t packet_size = sizeof(header) + data_size;
    uint8_t* packet = malloc(packet_size);
    if (!packet) return -1;
    
    // Copy header and data
    memcpy(packet, &header, sizeof(header));
    if (data && data_size > 0) {
        memcpy(packet + sizeof(header), data, data_size);
    }
    
    // Send via raw socket (RGTP over IP)
    ssize_t sent = sendto(sockfd, packet, packet_size, 0,
                         (struct sockaddr*)dest, sizeof(*dest));
    
    free(packet);
    
    if (sent < 0) {
        perror("Failed to send RGTP packet");
        return -1;
    }
    
    printf("Sent RGTP packet: type=%d, session=%u, chunk=%u, size=%zu\n",
           type, session_id, chunk_id, data_size);
    
    return 0;
}

// Receive RGTP packet
int receive_rgtp_packet(int sockfd, rgtp_header_t* header, 
                       uint8_t* data_buffer, size_t buffer_size,
                       struct sockaddr_in* src_addr) {
    
    uint8_t packet_buffer[65536]; // Max IP packet size
    socklen_t addr_len = sizeof(*src_addr);
    
    // Receive packet
    ssize_t received = recvfrom(sockfd, packet_buffer, sizeof(packet_buffer), 0,
                               (struct sockaddr*)src_addr, &addr_len);
    
    if (received < sizeof(rgtp_header_t)) {
        return -1; // Invalid packet
    }
    
    // Extract RGTP header
    memcpy(header, packet_buffer, sizeof(rgtp_header_t));
    
    // Convert from network byte order
    header->session_id = ntohl(header->session_id);
    header->chunk_id = ntohl(header->chunk_id);
    header->chunk_size = ntohl(header->chunk_size);
    header->total_chunks = ntohl(header->total_chunks);
    header->checksum = ntohl(header->checksum);
    
    // Extract data and verify checksum
    size_t data_size = header->chunk_size;
    if (data_size > 0) {
        if (data_size > buffer_size) {
            fprintf(stderr, "RGTP packet data size (%zu) exceeds buffer size (%zu)\n", data_size, buffer_size);
            return -1;
        }
        
        memcpy(data_buffer, packet_buffer + sizeof(rgtp_header_t), data_size);
        
        // Verify checksum for data integrity
        uint32_t computed_checksum = calculate_checksum(data_buffer, data_size);
        if (computed_checksum != header->checksum) {
            fprintf(stderr, "RGTP Checksum mismatch! Expected: %u, Computed: %u (chunk %u, %zu bytes)\n",
                    header->checksum, computed_checksum, header->chunk_id, data_size);
            fprintf(stderr, "Packet may be corrupted or tampered with. Discarding.\n");
            return -1; // Indicate checksum failure
        }
    } else {
        // For control packets with no data, checksum should be 0
        if (header->checksum != 0) {
            fprintf(stderr, "RGTP Control packet has non-zero checksum: %u (should be 0)\n", header->checksum);
            fprintf(stderr, "Packet may be corrupted. Discarding.\n");
            return -1;
        }
    }
    
    if (data_size > 0) {
        printf("Received RGTP packet: type=%d, session=%u, chunk=%u, size=%u, checksum=OK\n",
               header->type, header->session_id, header->chunk_id, header->chunk_size);
    } else {
        printf("Received RGTP control packet: type=%d, session=%u, chunk=%u\n",
               header->type, header->session_id, header->chunk_id);
    }
    
    return data_size;
}

// Exposer thread - handles pull requests and sends chunks
void* exposer_thread(void* arg) {
    rgtp_exposer_t* exposer = (rgtp_exposer_t*)arg;
    rgtp_header_t header;
    uint8_t buffer[1024];
    struct sockaddr_in client_addr;
    
    printf("RGTP Exposer started on %s:%d\n", 
           inet_ntoa(exposer->addr.sin_addr), ntohs(exposer->addr.sin_port));
    
    // Announce exposure availability
    struct sockaddr_in broadcast_addr = exposer->addr;
    broadcast_addr.sin_addr.s_addr = INADDR_BROADCAST;
    
    send_rgtp_packet(exposer->raw_socket, &broadcast_addr, RGTP_EXPOSE_ANNOUNCE,
                     exposer->session_id, 0, NULL, 0);
    
    while (exposer->running) {
        // Listen for pull requests
        int data_size = receive_rgtp_packet(exposer->raw_socket, &header, 
                                          buffer, sizeof(buffer), &client_addr);
        
        if (data_size >= 0 && header.type == RGTP_PULL_REQUEST && 
            header.session_id == exposer->session_id) {
            
            uint32_t requested_chunk = header.chunk_id;
            
            printf("Pull request for chunk %u from %s\n", 
                   requested_chunk, inet_ntoa(client_addr.sin_addr));
            
            // Send requested chunk
            if (requested_chunk < exposer->total_chunks) {
                size_t chunk_offset = requested_chunk * CHUNK_SIZE;
                size_t chunk_size = CHUNK_SIZE;
                
                // Adjust size for last chunk
                if (chunk_offset + chunk_size > exposer->data_size) {
                    chunk_size = exposer->data_size - chunk_offset;
                }
                
                send_rgtp_packet(exposer->raw_socket, &client_addr, RGTP_CHUNK_DATA,
                               exposer->session_id, requested_chunk,
                               exposer->data + chunk_offset, chunk_size);
            }
        }
        
        usleep(1000); // Small delay
    }
    
    return NULL;
}

// Create and start exposer
rgtp_exposer_t* create_exposer(const char* bind_addr, int port, 
                              const uint8_t* data, size_t data_size) {
    
    rgtp_exposer_t* exposer = malloc(sizeof(rgtp_exposer_t));
    if (!exposer) return NULL;
    
    // Create raw socket
    exposer->raw_socket = create_rgtp_socket();
    if (exposer->raw_socket < 0) {
        free(exposer);
        return NULL;
    }
    
    // Setup address
    exposer->addr.sin_family = AF_INET;
    exposer->addr.sin_port = htons(port);
    inet_pton(AF_INET, bind_addr, &exposer->addr.sin_addr);
    
    // Copy data to expose
    exposer->data = malloc(data_size);
    if (!exposer->data) {
        close(exposer->raw_socket);
        free(exposer);
        return NULL;
    }
    memcpy(exposer->data, data, data_size);
    exposer->data_size = data_size;
    exposer->session_id = rand(); // Random session ID
    exposer->total_chunks = (data_size + CHUNK_SIZE - 1) / CHUNK_SIZE;
    exposer->running = 1;
    
    // Start exposer thread
    pthread_create(&exposer->thread, NULL, exposer_thread, exposer);
    
    return exposer;
}

// Create puller
rgtp_puller_t* create_puller(const char* target_addr, int port) {
    rgtp_puller_t* puller = malloc(sizeof(rgtp_puller_t));
    if (!puller) return NULL;
    
    // Create raw socket
    puller->raw_socket = create_rgtp_socket();
    if (puller->raw_socket < 0) {
        free(puller);
        return NULL;
    }
    
    // Setup target address
    puller->target.sin_family = AF_INET;
    puller->target.sin_port = htons(port);
    inet_pton(AF_INET, target_addr, &puller->target.sin_addr);
    
    puller->buffer = NULL;
    puller->buffer_size = 0;
    puller->session_id = 0;
    puller->chunks_received = 0;
    puller->total_chunks = 0;
    puller->chunk_map = NULL;
    
    return puller;
}

// Pull data from exposer
int pull_data(rgtp_puller_t* puller) {
    rgtp_header_t header;
    uint8_t chunk_buffer[CHUNK_SIZE];
    struct sockaddr_in src_addr;
    
    printf("Pulling data from %s:%d\n", 
           inet_ntoa(puller->target.sin_addr), ntohs(puller->target.sin_port));
    
    // Wait for exposure announcement
    printf("Waiting for exposure announcement...\n");
    while (1) {
        int data_size = receive_rgtp_packet(puller->raw_socket, &header,
                                          chunk_buffer, sizeof(chunk_buffer), &src_addr);
        
        if (data_size >= 0 && header.type == RGTP_EXPOSE_ANNOUNCE) {
            puller->session_id = header.session_id;
            printf("Found exposure session %u\n", puller->session_id);
            break;
        }
    }
    
    // Request chunks sequentially (could be done out-of-order for better performance)
    uint32_t chunk_id = 0;
    puller->buffer = malloc(DATA_SIZE);
    if (!puller->buffer) {
        fprintf(stderr, "Failed to allocate buffer for data (%d bytes)\n", DATA_SIZE);
        return -1;
    }
    size_t total_received = 0;
    
    while (total_received < DATA_SIZE) {
        // Send pull request for next chunk
        send_rgtp_packet(puller->raw_socket, &puller->target, RGTP_PULL_REQUEST,
                        puller->session_id, chunk_id, NULL, 0);
        
        // Wait for chunk data with retry on checksum failure
        int retry_count = 0;
        const int max_retries = 3;
        
        while (retry_count <= max_retries) {
            int data_size = receive_rgtp_packet(puller->raw_socket, &header,
                                              chunk_buffer, sizeof(chunk_buffer), &src_addr);
            
            if (data_size < 0) {
                // Checksum failure or other error
                retry_count++;
                if (retry_count <= max_retries) {
                    printf("Chunk %u failed checksum, retrying (%d/%d)...\n", 
                           chunk_id, retry_count, max_retries);
                    // Re-request the chunk
                    send_rgtp_packet(puller->raw_socket, &puller->target, RGTP_PULL_REQUEST,
                                    puller->session_id, chunk_id, NULL, 0);
                    continue;
                } else {
                    fprintf(stderr, "Chunk %u failed after %d retries. Aborting.\n", 
                            chunk_id, max_retries);
                    return -1;
                }
            }
            
            if (data_size > 0 && header.type == RGTP_CHUNK_DATA && 
                header.session_id == puller->session_id && 
                header.chunk_id == chunk_id) {
                
                // Copy chunk to buffer (checksum already verified)
                size_t offset = chunk_id * CHUNK_SIZE;
                memcpy(puller->buffer + offset, chunk_buffer, data_size);
                total_received += data_size;
                
                printf("Received chunk %u (%d bytes), total: %zu/%d\n", 
                       chunk_id, data_size, total_received, DATA_SIZE);
                
                chunk_id++;
                break;
            }
            
            // Wrong packet type, session, or chunk - keep waiting
        }
    }
    
    printf("Data pull completed successfully!\n");
    return 0;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        printf("Usage: %s <exposer|puller> [target_ip]\n", argv[0]);
        printf("Note: Requires root privileges for raw sockets\n");
        return 1;
    }
    
    printf("RGTP Layer 4 Protocol Demo\n");
    printf("==========================\n");
    printf("RGTP Protocol Number: %d\n", RGTP_PROTOCOL_NUMBER);
    printf("Operating directly over IP (no TCP/UDP)\n\n");
    
    if (strcmp(argv[1], "exposer") == 0) {
        // Create test data
        uint8_t* test_data = malloc(DATA_SIZE);
        if (!test_data) {
            fprintf(stderr, "Failed to allocate test data (%d bytes)\n", DATA_SIZE);
            return 1;
        }
        for (int i = 0; i < DATA_SIZE; i++) {
            test_data[i] = i % 256;
        }
        
        printf("Starting RGTP Exposer...\n");
        printf("Data size: %d bytes (%d chunks)\n", DATA_SIZE, (DATA_SIZE + CHUNK_SIZE - 1) / CHUNK_SIZE);
        
        // Create exposer
        rgtp_exposer_t* exposer = create_exposer("0.0.0.0", 9999, test_data, DATA_SIZE);
        if (!exposer) {
            fprintf(stderr, "Failed to create exposer\n");
            free(test_data);
            return 1;
        }
        
        printf("Exposer running. Press Ctrl+C to stop.\n");
        
        // Keep running
        while (1) {
            sleep(1);
        }
        
    } else if (strcmp(argv[1], "puller") == 0) {
        const char* target_ip = argc > 2 ? argv[2] : "127.0.0.1";
        
        printf("Starting RGTP Puller...\n");
        printf("Target: %s:9999\n", target_ip);
        
        // Create puller
        rgtp_puller_t* puller = create_puller(target_ip, 9999);
        if (!puller) {
            fprintf(stderr, "Failed to create puller\n");
            return 1;
        }
        
        // Pull data
        if (pull_data(puller) == 0) {
            printf("Successfully pulled %d bytes via RGTP!\n", DATA_SIZE);
            
            // Verify data integrity
            int errors = 0;
            for (int i = 0; i < DATA_SIZE; i++) {
                if (puller->buffer[i] != (i % 256)) {
                    errors++;
                }
            }
            
            if (errors == 0) {
                printf("Data integrity check: PASSED\n");
            } else {
                printf("Data integrity check: FAILED (%d errors)\n", errors);
            }
        }
        
    } else {
        printf("Invalid mode. Use 'exposer' or 'puller'\n");
        return 1;
    }
    
    return 0;
}