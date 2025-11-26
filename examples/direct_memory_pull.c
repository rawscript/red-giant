/**
 * @file direct_memory_pull.c
 * @brief Example demonstrating RGTP pull with direct memory access
 * 
 * This example shows how RGTP can pull data using direct memory access
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

    // Create source address (localhost:9999)
    struct sockaddr_in source = {0};
    source.sin_family = AF_INET;
    source.sin_port = htons(9999);
    source.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    // Buffer to receive data
    char buffer[1024] = {0};
    size_t buffer_size = sizeof(buffer);

    // Pull data using direct memory access
    printf("Pulling data from %s:%d\n", 
           inet_ntoa(source.sin_addr), ntohs(source.sin_port));
    
    if (rgtp_pull_data(sockfd, &source, buffer, &buffer_size) != 0) {
        fprintf(stderr, "Failed to pull data\n");
        rgtp_cleanup();
        return 1;
    }

    printf("Data pulled successfully using direct memory access!\n");
    printf("Received: %s\n", buffer);
    printf("Size: %zu bytes\n", buffer_size);

    // Clean up
    rgtp_cleanup();
    return 0;
}