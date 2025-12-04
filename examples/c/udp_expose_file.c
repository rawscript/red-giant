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
        return 1;
    }

    rgtp_init();
    signal(SIGINT, sigint);

    FILE* f = fopen(argv[1], "rb");
    if (!f) { perror("fopen"); return 1; }
    fseek(f, 0, SEEK_END);
    size_t size = ftell(f);
    fseek(f, 0, SEEK_SET);
    void* data = malloc(size);
    if (fread(data, 1, size, f) != size) { perror("fread"); fclose(f); free(data); return 1; }
    fclose(f);

    int sock = rgtp_socket();
    struct sockaddr_in any = { .sin_family = AF_INET, .sin_addr.s_addr = INADDR_ANY };

    rgtp_surface_t* surface = NULL;
    if (rgtp_expose_data(sock, data, size, &any, &surface) != 0 || !surface) {
        fprintf(stderr, "Failed to expose file\n");
        return 1;
    }

    printf("\nRED GIANT UDP EXPOSER v2.0\n");
    printf("File: %s (%.2f GB)\n", argv[1], size / 1e9);
    printf("Exposure ID: %016llx%016llx\n",
           (unsigned long long)surface->exposure_id[0],
           (unsigned long long)surface->exposure_id[1]);
    printf("Serving on UDP 443 — unlimited clients, zero state\n");
    printf("Pull with: ./udp_pull_file <your-ip> %s\n\n", argv[1]);

    while (running) {
        rgtp_poll(surface, 100);
        printf("\rSent: %.2f GB | Pull pressure: %u    ",
               surface->bytes_sent / 1e9, surface->pull_pressure);
        fflush(stdout);
        usleep(200000);
    }

    printf("\nGoodbye.\n");
    rgtp_destroy_surface(surface);
    free(data);
    close(sock);
    rgtp_cleanup();
    return 0;
}