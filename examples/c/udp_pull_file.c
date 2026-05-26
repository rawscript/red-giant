/**
 * @file udp_pull_file.c
 * @brief Pull a file from an RGTP exposer over UDP.
 *
 * Usage: udp_pull_file <host:port> <exposure-id-hex> <output-file>
 *
 * The puller drives all flow control by issuing pull requests. Each received
 * chunk is verified with AEAD authentication and an optional Merkle proof
 * before being written to the output file.
 *
 * Build:
 *   cmake -B build -DRGTP_BUILD_EXAMPLES=ON
 *   cmake --build build
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <time.h>

#include "rgtp/rgtp.h"

#ifdef _WIN32
#  include <winsock2.h>
#  include <ws2tcpip.h>
#  include <windows.h>
#  define SLEEP_MS(ms) Sleep(ms)
#else
#  include <arpa/inet.h>
#  include <unistd.h>
#  define SLEEP_MS(ms) usleep((ms) * 1000u)
#endif

static volatile int g_running = 1;

static void on_sigint(int sig)
{
    (void)sig;
    g_running = 0;
}

static void print_usage(const char *prog)
{
    fprintf(stderr,
            "Usage: %s <host:port> <exposure-id-hex> <output-file>\n"
            "  host:port        — Exposer address, e.g. 192.168.1.10:9000\n"
            "  exposure-id-hex  — 32 hex characters (128-bit Exposure_ID)\n"
            "  output-file      — Path to write the received file\n",
            prog);
}

static int parse_host_port(const char *arg, char *host_out, size_t host_sz,
                            uint16_t *port_out)
{
    /* Find last colon to support IPv6 addresses */
    const char *colon = strrchr(arg, ':');
    if (!colon) return -1;

    size_t host_len = (size_t)(colon - arg);
    if (host_len == 0 || host_len >= host_sz) return -1;
    memcpy(host_out, arg, host_len);
    host_out[host_len] = '\0';

    int port = atoi(colon + 1);
    if (port <= 0 || port > 65535) return -1;
    *port_out = (uint16_t)port;
    return 0;
}

static int parse_exposure_id(const char *hex, uint8_t out[16])
{
    if (strlen(hex) != 32) return -1;
    for (int i = 0; i < 16; i++) {
        unsigned int byte = 0;
        if (sscanf(hex + i * 2, "%2x", &byte) != 1) return -1;
        out[i] = (uint8_t)byte;
    }
    return 0;
}

static const char *fmt_bytes(uint64_t n, char *buf, size_t bufsz)
{
    static const char *units[] = { "B", "KB", "MB", "GB", "TB" };
    double v = (double)n;
    int u = 0;
    while (v >= 1024.0 && u < 4) { v /= 1024.0; u++; }
    snprintf(buf, bufsz, "%.2f %s", v, units[u]);
    return buf;
}

int main(int argc, char **argv)
{
    if (argc != 4) {
        print_usage(argv[0]);
        return 1;
    }

    /* ── Parse arguments ────────────────────────────────────────────── */
    char     host[256];
    uint16_t port = 0;
    if (parse_host_port(argv[1], host, sizeof(host), &port) != 0) {
        fprintf(stderr, "Error: invalid address '%s' (expected host:port)\n", argv[1]);
        return 1;
    }

    uint8_t exposure_id[16];
    if (parse_exposure_id(argv[2], exposure_id) != 0) {
        fprintf(stderr, "Error: invalid exposure ID '%s' (expected 32 hex chars)\n",
                argv[2]);
        return 1;
    }

    const char *output_path = argv[3];

    /* ── Initialise library ─────────────────────────────────────────── */
    rgtp_error_t err = rgtp_init();
    if (err != RGTP_OK) {
        fprintf(stderr, "rgtp_init: %s\n", rgtp_strerror(err));
        return 1;
    }

    signal(SIGINT, on_sigint);

    /* ── Create socket ──────────────────────────────────────────────── */
    rgtp_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.window_size = 64;
    cfg.timeout_ms  = 30000;

    rgtp_socket_t *sock = NULL;
    err = rgtp_socket_create(&cfg, &sock);
    if (err != RGTP_OK) {
        fprintf(stderr, "rgtp_socket_create: %s\n", rgtp_strerror(err));
        rgtp_cleanup(); return 1;
    }

    /* ── Resolve server address ─────────────────────────────────────── */
    struct sockaddr_storage server;
    memset(&server, 0, sizeof(server));

    struct sockaddr_in *sa4 = (struct sockaddr_in *)&server;
    sa4->sin_family = AF_INET;
    sa4->sin_port   = htons(port);
    if (inet_pton(AF_INET, host, &sa4->sin_addr) != 1) {
        fprintf(stderr, "Error: cannot resolve host '%s'\n", host);
        rgtp_socket_destroy(sock); rgtp_cleanup(); return 1;
    }

    /* ── Start pull ─────────────────────────────────────────────────── */
    printf("RGTP Puller  %s\n", rgtp_version());
    printf("  Server      : %s:%u\n", host, port);
    printf("  Exposure ID : %s\n", argv[2]);
    printf("  Output      : %s\n\n", output_path);

    rgtp_surface_t *surface = NULL;
    err = rgtp_pull_start(sock, &server, exposure_id, &cfg, &surface);
    if (err != RGTP_OK) {
        fprintf(stderr, "rgtp_pull_start: %s\n", rgtp_strerror(err));
        rgtp_socket_destroy(sock); rgtp_cleanup(); return 1;
    }

    /* ── Open output file ───────────────────────────────────────────── */
    FILE *out = fopen(output_path, "wb");
    if (!out) {
        perror("fopen");
        rgtp_destroy_surface(surface);
        rgtp_socket_destroy(sock);
        rgtp_cleanup();
        return 1;
    }

    /* ── Receive chunks ─────────────────────────────────────────────── */
    static uint8_t chunk_buf[65536 + 16];   /* max chunk + AEAD tag */
    uint64_t total_written = 0;
    time_t   start         = time(NULL);

    printf("Receiving chunks...\n\n");

    while (g_running && rgtp_progress(surface) < 1.0f) {
        size_t   received    = 0;
        uint32_t chunk_index = 0;

        err = rgtp_pull_next(surface, chunk_buf, sizeof(chunk_buf),
                             &received, &chunk_index);

        if (err == RGTP_OK && received > 0) {
            if (fwrite(chunk_buf, 1, received, out) != received) {
                perror("fwrite");
                break;
            }
            total_written += received;
        } else if (err == RGTP_ERR_TIMEOUT) {
            /* No chunk arrived yet — keep waiting */
        } else if (err == RGTP_ERR_AUTH_FAIL) {
            fprintf(stderr, "\nWarning: authentication failure on chunk %u — discarded\n",
                    chunk_index);
        } else if (err != RGTP_OK) {
            fprintf(stderr, "\nrgtp_pull_next: %s\n", rgtp_strerror(err));
            break;
        }

        /* Progress display */
        double elapsed = difftime(time(NULL), start);
        double mbps    = elapsed > 0
                         ? (double)total_written / elapsed / 1048576.0
                         : 0.0;
        float  pct     = rgtp_progress(surface) * 100.0f;
        char   recv_buf[32];
        fmt_bytes(total_written, recv_buf, sizeof(recv_buf));

        printf("\r  Progress: %5.1f%%  Received: %-10s  Speed: %6.1f MB/s  "
               "Elapsed: %.0fs   ",
               pct, recv_buf, mbps, elapsed);
        fflush(stdout);

        SLEEP_MS(1);
    }

    fclose(out);

    /* ── Summary ────────────────────────────────────────────────────── */
    double elapsed = difftime(time(NULL), start);
    char   total_buf[32];
    fmt_bytes(total_written, total_buf, sizeof(total_buf));

    printf("\n\nDone.\n");
    printf("  Received : %s\n", total_buf);
    printf("  Output   : %s\n", output_path);
    printf("  Time     : %.1f seconds\n", elapsed);
    if (elapsed > 0) {
        printf("  Speed    : %.1f MB/s\n",
               (double)total_written / elapsed / 1048576.0);
    }

    /* ── Teardown ───────────────────────────────────────────────────── */
    rgtp_destroy_surface(surface);
    rgtp_socket_destroy(sock);
    rgtp_cleanup();
    return 0;
}
