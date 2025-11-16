/**
 * @file test_basic.c
 * @brief Basic RGTP functionality test
 */

#ifdef _WIN32
#include <winsock2.h>
#endif

#include "rgtp/rgtp.h"
#include <stdio.h>
#include <stdlib.h>

int main() {
    printf("Testing RGTP basic functionality...\n");
    
    // Initialize RGTP library
    if (rgtp_init() < 0) {
        printf("FAIL: rgtp_init() failed\n");
        return 1;
    }
    printf("PASS: rgtp_init() succeeded\n");
    
    // Test socket creation
    int sockfd = rgtp_socket();
    if (sockfd < 0) {
        printf("FAIL: rgtp_socket() returned %d\n", sockfd);
        rgtp_cleanup();
        return 1;
    }
    printf("PASS: rgtp_socket() returned %d\n", sockfd);
    
    // Test binding
    if (rgtp_bind(sockfd, 9999) < 0) {
        printf("FAIL: rgtp_bind() failed\n");
    } else {
        printf("PASS: rgtp_bind() succeeded\n");
    }
    
    // Clean up
#ifdef _WIN32
    closesocket(sockfd);
#else
    close(sockfd);
#endif
    
    rgtp_cleanup();
    printf("Basic RGTP test completed.\n");
    return 0;
}