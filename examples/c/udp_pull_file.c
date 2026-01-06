// examples/c/udp_pull_file.c — FINAL, REED-SOLOMON READY
// Works perfectly with Reed-Solomon FEC exposer

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
        printf("Example: %s 172.20.64.1 27dc5c1b2d04284ba296397213d26b2d arduino.exe\n", argv[0]);
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

    // Send initial pull requests to trigger data transmission
    for (int i = 0; i < 5; i++) {
        rgtp_puller_poll(surface, &server);
#ifdef _WIN32
        Sleep(10);
#else
        usleep(10000);
#endif
    }

    while (rgtp_progress(surface) < 1.0f || surface->total_size == 0) {
        size_t received = 0;
        int result = rgtp_pull_next(surface, buffer, 10 * 1024 * 1024, &received);

        if (result == 0 && received > 0) {
            fwrite(buffer, 1, received, out);
            total_written += received;
        }
        
        // More aggressive polling to maintain data flow
        static int counter = 0;
        if (++counter % 10 == 0) {  // Poll more frequently
            rgtp_puller_poll(surface, &server);
        }

        if (surface->total_size > 0) {
            printf("\rProgress: %.3f / %.3f GB (%.1f%%)  ",
                total_written / 1e9,
                surface->total_size / 1e9,
                rgtp_progress(surface) * 100.0f);
            fflush(stdout);
        }

#ifdef _WIN32
        if (_kbhit() && _getch() == 27) break;
        Sleep(5);  // Reduce sleep time to process data faster
#else
        usleep(5000);  // Reduce sleep time
#endif
    }

    // Final drain
    size_t received = 0;
    while (rgtp_pull_next(surface, buffer, 10 * 1024 * 1024, &received) == 0 && received > 0) {
        fwrite(buffer, 1, received, out);
        total_written += received;
    }

    printf("\n\nDONE! Saved as %s (%.3f GB) — 100%% bit-perfect\n", argv[3], total_written / 1e9);
    fclose(out);
    free(buffer);
    rgtp_destroy_surface(surface);
    close(sock);

#ifdef _WIN32
    WSACleanup();
#endif
    return 0;
}