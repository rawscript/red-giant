#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include "rgtp/rgtp.h"

int main(int argc, char** argv) {
    if (argc != 3) {
        printf("Usage: %s <server-ip> <output-file>\n", argv[0]);
        return 1;
    }

    rgtp_init();
    int sock = rgtp_socket();

    struct sockaddr_in server = {0};
    server.sin_family = AF_INET;
    server.sin_port = htons(443);
    inet_pton(AF_INET, argv[1], &server.sin_addr);

    // Wait for first manifest packet
    uint8_t buf[2048];
    socklen_t len = sizeof(server);
    ssize_t n = recvfrom(sock, buf, sizeof(buf), 0, (struct sockaddr*)&server, &len);
    if (n < 48) { perror("recv manifest"); return 1; }

    rgtp_header_t* h = (rgtp_header_t*)buf;
    if (h->type != RGTP_EXPOSE_MANIFEST) {
        fprintf(stderr, "Not a manifest\n");
        return 1;
    }

    uint64_t total = be64toh(h->total_size);
    uint32_t chunks = ntohl(h->chunk_count);
    uint32_t chunk_sz = ntohl(h->chunk_size);

    printf("Manifest received!\n");
    printf("Size: %.2f GB | Chunks: %u | Chunk size: %u KB\n",
           total/1e9, chunks, chunk_sz/1024);

    rgtp_surface_t* s = rgtp_create_surface(sock, &server);
    memcpy(s->exposure_id, h->exposure_id, 16);
    s->total_size = total;
    s->chunk_count = chunks;
    s->optimal_chunk_size = chunk_sz;
    s->chunk_bitmap = calloc(1, (chunks + 7)/8);

    FILE* out = fopen(argv[2], "wb");
    size_t received = 0;

    printf("Downloading...\n");
    while (received < total) {
        rgtp_poll(s, 100);
        if (s->bytes_sent > received) {
            received = s->bytes_sent;
            int bar = 50 * received / total;
            printf("\r[%.*s%*s] %.1f%% (%.2f/%.2f GB)",
                   bar, "**************************************************",
                   50-bar, "", 100.0*received/total, received/1e9, total/1e9);
            fflush(stdout);
        }
        usleep(100000);
    }

    // In real version: write decrypted chunks to file
    printf("\nDownload complete: %s\n", argv[2]);
    rgtp_destroy_surface(s);
    close(sock);
    rgtp_cleanup();
    return 0;
}