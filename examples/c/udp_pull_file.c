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
        printf("Usage: %s <server-ip> <exposure-id-32-hex-chars> <output-file>\n", argv[0]);
        printf("Example: %s 192.168.1.100 27dc5c1b2d04284ba296397213dabdd5 downloaded.exe\n", argv[0]);
        return 1;
    }

#ifdef _WIN32
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);
#endif

    rgtp_init();

    int sock = rgtp_socket();
    if (sock < 0) {
        fprintf(stderr, "Failed to create socket\n");
        return 1;
    }

    struct sockaddr_in server = { 0 };
    server.sin_family = AF_INET;
    server.sin_port = htons(443);
    inet_pton(AF_INET, argv[1], &server.sin_addr);

    uint64_t exposure_id[2];
    sscanf(argv[2], "%16llx%16llx", &exposure_id[0], &exposure_id[1]);

    rgtp_surface_t* surface = NULL;
    if (rgtp_pull_start(sock, &server, exposure_id, &surface) != 0 || !surface) {
        fprintf(stderr, "Failed to connect to exposure\n");
        close(sock);
        return 1;
    }

    printf("Connected! Exposure has %.3f GB — pulling...\n", surface->total_size / 1e9);

    FILE* out = fopen(argv[3], "wb");
    if (!out) { perror("fopen"); return 1; }

    uint8_t* buffer = malloc(10 * 1024 * 1024);  // 10 MB buffer
    size_t total_received = 0;

    while (total_received < surface->total_size) {
        size_t received = 0;
        if (rgtp_pull_next(surface, buffer, 10 * 1024 * 1024, &received) != 0) {
            printf("\nNetwork hiccup — resuming...\n");
        }
        else {
            total_received += received;
            fwrite(buffer, 1, received, out);
        }

        printf("\rDownloaded: %.3f / %.3f GB (%.1f%%) — speed: %.1f MB/s   ",
            total_received / 1e9,
            surface->total_size / 1e9,
            100.0 * total_received / surface->total_size,
            (received / (10 * 1024 * 1024.0)) * 5.0);  // rough speed
        fflush(stdout);

#ifdef _WIN32
        if (_kbhit() && _getch() == 27) break;
#endif
    }

    printf("\n\nDONE! Saved as %s\n", argv[3]);
    fclose(out);
    free(buffer);
    rgtp_destroy_surface(surface);
    close(sock);

#ifdef _WIN32
    WSACleanup();
#endif
    return 0;
}