#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <arpa/inet.h>
#include "rgtp/rgtp.h"

static volatile int running = 1;

void sigint(int s) { (void)s; running = 0; }

int main(int argc, char** argv) {
    if (argc != 2) {
        printf("Usage: %s <file-to-expose>\n", argv[0]);
        printf("Example: %s ubuntu.iso\n", argv[0]);
        return 1;
    }

    rgtp_init();
    signal(SIGINT, sigint);

    int sock = rgtp_socket();
    if (sock < 0) { perror("socket"); return 1; }

    // Read entire file
    FILE* f = fopen(argv[1], "rb");
    if (!f) { perror("fopen"); return 1; }
    fseek(f, 0, SEEK_END);
    size_t size = ftell(f);
    fseek(f, 0, SEEK_SET);
    void* data = malloc(size);
    fread(data, 1, size, f);
    fclose(f);

    struct sockaddr_in anywhere = {0};
    anywhere.sin_family = AF_INET;
    anywhere.sin_addr.s_addr = INADDR_ANY;
    anywhere.sin_port = htons(443);

    rgtp_surface_t* surface = NULL;
    int r = rgtp_expose_data(sock, data, size, &anywhere, &surface);
    if (r != 0 || !surface) {
        fprintf(stderr, "Failed to expose file\n");
        return 1;
    }

    printf("\nRED GIANT UDP EXPOSER\n");
    printf("File: %s (%.1f MB)\n", argv[1], size/1e6);
    printf("Exposure ID: %016llx%016llx\n", 
           (unsigned long long)surface->exposure_id[0],
           (unsigned long long)surface->exposure_id[1]);
    printf("Listening on UDP 443 — serving unlimited clients\n");
    printf("Run puller: ./udp_pull_file <this-ip> %s\n\n", argv[1]);

    while (running) {
        rgtp_poll(surface, 100);
        printf("\rSent: %.2f MB | Pull pressure: %u     ",
               surface->bytes_sent/1e6, surface->pull_pressure);
        fflush(stdout);
        usleep(200000);
    }

    printf("\nShutting down...\n");
    rgtp_destroy_surface(surface);
    free(data);
    close(sock);
    rgtp_cleanup();
    return 0;
}