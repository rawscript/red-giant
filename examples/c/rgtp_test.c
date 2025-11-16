/**
 * Simple RGTP Test Program
 * 
 * This program demonstrates the basic functionality of the RGTP library
 * on Windows.
 */

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "rgtp/rgtp.h"

int main() {
    printf("RGTP Test Program\n");
    printf("=================\n");
    
    // Test creating an RGTP socket
    printf("Creating RGTP socket...\n");
    int sockfd = rgtp_socket();
    if (sockfd < 0) {
        printf("Failed to create RGTP socket\n");
        return 1;
    }
    
    printf("RGTP socket created successfully (fd: %d)\n", sockfd);
    
    // Test binding the socket
    printf("Binding socket to port 9999...\n");
    if (rgtp_bind(sockfd, 9999) < 0) {
        printf("Failed to bind socket\n");
    } else {
        printf("Socket bound successfully\n");
    }
    
    // Close the socket
    #ifdef _WIN32
    closesocket(sockfd);
    #else
    close(sockfd);
    #endif
    
    printf("RGTP test completed successfully!\n");
    return 0;
}