// examples/c/udp_pull_file.c � FINAL, BIT-PERFECT PULLER (DEC 2025)

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "rgtp/rgtp.h"

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <conio.h>
#else
#include <arpa/inet.h>
#include <unistd.h>
#endif

int main(int argc, char** argv) {
    if (argc != 4) {
        printf("Usage: %s <server-ip> <exposure-id-32-hex> <output-file>\n", argv[0]);
        return 1;
    }

#ifdef _WIN32
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);
#endif

    rgtp_init();

    int sock = rgtp_socket();
    struct sockaddr_in server = { 0 };
    server.sin_family = AF_INET;
    server.sin_port = htons(443);
    inet_pton(AF_INET, argv[1], &server.sin_addr);

    uint64_t exposure_id[2];
    sscanf(argv[2], "%16llx%16llx", &exposure_id[0], &exposure_id[1]);

    rgtp_surface_t* surface = NULL;
    if (rgtp_pull_start(sock, &server, exposure_id, &surface) != 0 || !surface) {
        fprintf(stderr, "Failed to start pull\n");
        return 1;
    }

    FILE* out = fopen(argv[3], "wb");
    if (!out) { perror("fopen"); return 1; }

    uint8_t* buffer = malloc(10 * 1024 * 1024);
    size_t total_written = 0;

    printf("Connected! Waiting for data...\n");

    while (rgtp_progress(surface) < 1.0f) {
        size_t received = 0;
        // Process all available packets in this iteration
        int result;
        while ((result = rgtp_pull_next(surface, buffer, 10 * 1024 * 1024, &received)) == 0 && received > 0) {
            fwrite(buffer, 1, received, out);
            total_written += received;
            received = 0; // Reset for next iteration
        }
        
        // Check if we've reached end of transfer
        if (result == -1 && all_chunks_written(surface)) {
            // End of transfer reached - all chunks have been processed
            break;
        }

        printf("\rProgress: %.3f / %.3f GB (%.1f%%)  speed: ~%.1f GB/s   ",
            total_written / 1e9,
            surface->total_size / 1e9,
            rgtp_progress(surface) * 100.0f,
            (received / 1e9) / 0.2);  // rough estimate
        fflush(stdout);

#ifdef _WIN32
        if (_kbhit() && _getch() == 27) break;
        Sleep(50);
#else
        usleep(50000);
#endif
    }
    
    // Final flush to ensure all buffered chunks are written
    size_t received = 0;
    while (rgtp_pull_next(surface, buffer, 10 * 1024 * 1024, &received) == 0 && received > 0) {
        fwrite(buffer, 1, received, out);
        total_written += received;
        received = 0;
    }

    printf("\n\nDONE! Saved as %s (%.3f GB)\n", argv[3], total_written / 1e9);
    fclose(out);
    free(buffer);
    rgtp_destroy_surface(surface);
    close(sock);

#ifdef _WIN32
    WSACleanup();
#endif
    return 0;
}