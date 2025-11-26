/**
 * @file direct_memory_access.c
 * @brief Example demonstrating RGTP with direct memory access
 * 
 * This example shows how RGTP can work with direct memory access
 * instead of packet-based communication.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#endif

#include "rgtp/rgtp.h"

int main() {
    // Initialize RGTP
    if (rgtp_init() != 0) {
        fprintf(stderr, "Failed to initialize RGTP\n");
        return 1;
    }

    // Create socket
    int sockfd = rgtp_socket();
    if (sockfd < 0) {
        fprintf(stderr, "Failed to create RGTP socket\n");
        rgtp_cleanup();
        return 1;
    }

    // Bind to port
    if (rgtp_bind(sockfd, 9999) != 0) {
        fprintf(stderr, "Failed to bind RGTP socket\n");
        rgtp_cleanup();
        return 1;
    }

    // Sample data to expose
    const char* message = "Hello, RGTP with Direct Memory Access!";
    size_t message_len = strlen(message);

    // Create destination address (localhost:9999)
    struct sockaddr_in dest = {0};
    dest.sin_family = AF_INET;
    dest.sin_port = htons(9999);
    dest.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    // Expose data using direct memory access
    printf("Exposing data: %s\n", message);
    rgtp_surface_t* surface = NULL;
    if (rgtp_expose_data(sockfd, message, message_len, &dest, &surface) != 0) {
        fprintf(stderr, "Failed to expose data\n");
        rgtp_cleanup();
        return 1;
    }

    printf("Data exposed successfully using direct memory access!\n");
    printf("Shared memory size: %zu bytes\n", surface->shared_memory_size);

    // Clean up
    if (surface) {
        free(surface->chunk_bitmap);
        free(surface);
    }
    
    rgtp_cleanup();
    return 0;
}