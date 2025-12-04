#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include "rgtp/rgtp.h"

int main(int argc, char** argv) {
    if (argc != 3 && argc != 4) {
        printf("Usage: %s <host> <output-file> [exposure-id]\n", argv[0]);
        printf("Example: %s 203.0.113.37 myfile.iso\n", argv[0]);
        return 1;
    }

    rgtp_init();

    int sock = rgtp_socket();
    if (sock < 0) { perror("socket"); return 1; }

    struct sockaddr_in server = {0};
    server.sin_family = AF_INET;
    server.sin_port = htons(443);
    inet_pton(AF_INET, argv[1], &server.sin_addr);

    // For now: auto-discovery by receiving first manifest packet
    printf("Waiting for manifest from %s:443...\n", argv[1]);

    // Simple blocking receive to get manifest
    uint8_t buf[1500];
    socklen_t slen = sizeof(server);
    ssize_t n = recvfrom(sock, buf, sizeof(buf), 0, (struct sockaddr*)&server, &slen);
    if (n < 48) { perror("recv"); return 1; }

    rgtp_header_t* h = (rgtp_header_t*)buf;
    if (h->type != RGTP_EXPOSE_MANIFEST) {
        fprintf(stderr, "Not a manifest packet\n");
        return 1;
    }

    uint64_t total = be64toh(h->total_size);
    uint32_t chunks = ntohl(h->chunk_count);
    uint32_t chunk_sz = ntohl(h->chunk_size);

    printf("Manifest received!\n");
    printf("Size: %.1f MB | Chunks: %u | Chunk size: %u KB\n",
           total/1e6, chunks, chunk_sz/1024);

    // Create surface for pulling
    rgtp_surface_t* s = rgtp_create_surface(sock, &server);
    memcpy(s->exposure_id, h->exposure_id, 16);
    s->total_size = total;
    s->chunk_count = chunks;
    s->optimal_chunk_size = chunk_sz;

    // Allocate bitmap + storage
    s->chunk_bitmap = calloc(1, (chunks + 7)/8);
    s->encrypted_chunks = calloc(chunks, sizeof(void*));

    FILE* out = fopen(argv[2], "wb");
    if (!out) { perror("fopen"); return 1; }

    size_t received = 0;
    while (received < total && running) {
        rgtp_poll(s, 100);

        // Simple progress
        printf("\r[%-50s] %.1f%% (%.1f/%.1f MB)",
               "==================================================" + (50 - 50*received/total),
               100.0 * received/total,
               received/1e6, total/1e6);
        fflush(stdout);
        usleep(100000);
    }

    printf("\nDownload complete: %s\n", argv[2]);
    fclose(out);
    rgtp_destroy_surface(s);
    close(sock);
    rgtp_cleanup();
    return 0;
}