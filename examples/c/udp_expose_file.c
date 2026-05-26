/**
 * @file udp_expose_file.c
 * @brief Expose a file over RGTP UDP.
 *
 * Usage: udp_expose_file <file> [port]
 *
 * Pre-encrypts all chunks once, builds the Merkle tree, and serves pull
 * requests from the immutable chunk store. Memory footprint is O(file_size)
 * regardless of how many pullers connect simultaneously.
 *
 * Build:
 *   cmake -B build -DRGTP_BUILD_EXAMPLES=ON -DRGTP_ENABLE_FEC=ON
 *   cmake --build build
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <time.h>

#include "rgtp/rgtp.h"

#ifdef _WIN32
#  include <windows.h>
#  define SLEEP_MS(ms) Sleep(ms)
#else
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
    fprintf(stderr, "Usage: %s <file> [port]\n", prog);
    fprintf(stderr, "  file  — path to the file to expose\n");
    fprintf(stderr, "  port  — UDP port to listen on (default: 9000)\n");
}

static void *read_file(const char *path, size_t *out_size)
{
    FILE *f = fopen(path, "rb");
    if (!f) { perror("fopen"); return NULL; }

    if (fseek(f, 0, SEEK_END) != 0) { perror("fseek"); fclose(f); return NULL; }
    long sz = ftell(f);
    if (sz < 0) { perror("ftell"); fclose(f); return NULL; }
    rewind(f);

    void *buf = malloc((size_t)sz);
    if (!buf) { fprintf(stderr, "out of memory\n"); fclose(f); return NULL; }

    if (fread(buf, 1, (size_t)sz, f) != (size_t)sz) {
        perror("fread"); free(buf); fclose(f); return NULL;
    }
    fclose(f);
    *out_size = (size_t)sz;
    return buf;
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
    if (argc < 2 || argc > 3) {
        print_usage(argv[0]);
        return 1;
    }

    const char *file_path = argv[1];
    uint16_t    port      = (argc == 3) ? (uint16_t)atoi(argv[2]) : 9000;

    /* ── Initialise library ─────────────────────────────────────────── */
    rgtp_error_t err = rgtp_init();
    if (err != RGTP_OK) {
        fprintf(stderr, "rgtp_init: %s\n", rgtp_strerror(err));
        return 1;
    }

    signal(SIGINT, on_sigint);

    /* ── Read file ──────────────────────────────────────────────────── */
    size_t data_size = 0;
    void  *data      = read_file(file_path, &data_size);
    if (!data) { rgtp_cleanup(); return 1; }

    /* ── Create socket ──────────────────────────────────────────────── */
    rgtp_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.port          = port;
    cfg.fec_enabled   = true;
    cfg.fec_k         = 223;
    cfg.fec_n         = 255;
    cfg.merkle_proofs = true;
    cfg.timeout_ms    = 200;

    rgtp_socket_t *sock = NULL;
    err = rgtp_socket_create(&cfg, &sock);
    if (err != RGTP_OK) {
        fprintf(stderr, "rgtp_socket_create: %s\n", rgtp_strerror(err));
        free(data); rgtp_cleanup(); return 1;
    }

    /* ── Expose data ────────────────────────────────────────────────── */
    rgtp_surface_t *surface = NULL;
    err = rgtp_expose(sock, data, data_size, &cfg, &surface);
    if (err != RGTP_OK) {
        fprintf(stderr, "rgtp_expose: %s\n", rgtp_strerror(err));
        rgtp_socket_destroy(sock); free(data); rgtp_cleanup(); return 1;
    }

    /* ── Print info ─────────────────────────────────────────────────── */
    uint8_t eid[16];
    rgtp_get_exposure_id(surface, eid);

    char size_buf[32];
    fmt_bytes((uint64_t)data_size, size_buf, sizeof(size_buf));

    printf("RGTP Exposer  %s\n", rgtp_version());
    printf("  File        : %s  (%s)\n", file_path, size_buf);
    printf("  Exposure ID : ");
    for (int i = 0; i < 16; i++) printf("%02x", eid[i]);
    printf("\n");
    printf("  Port        : %u\n", port);
    printf("  FEC         : Reed-Solomon (n=255, k=223, ~14%% overhead)\n");
    printf("  Merkle      : proofs enabled\n");
    printf("\n");
    printf("  Pull command: udp_pull_file <server-ip>:%u ", port);
    for (int i = 0; i < 16; i++) printf("%02x", eid[i]);
    printf(" output.bin\n\n");
    printf("Serving pull requests. Press Ctrl+C to stop.\n\n");

    /* ── Serve loop ─────────────────────────────────────────────────── */
    time_t start = time(NULL);

    while (g_running) {
        err = rgtp_poll(surface, 200);
        if (err != RGTP_OK && err != RGTP_ERR_TIMEOUT) {
            fprintf(stderr, "\nrgtp_poll: %s\n", rgtp_strerror(err));
            break;
        }

        rgtp_stats_t stats;
        if (rgtp_get_stats(surface, &stats) == RGTP_OK) {
            char sent_buf[32];
            fmt_bytes(stats.bytes_sent, sent_buf, sizeof(sent_buf));
            double elapsed = difftime(time(NULL), start);
            double mbps = elapsed > 0
                          ? (double)stats.bytes_sent / elapsed / 1048576.0
                          : 0.0;
            printf("\r  Sent: %-10s  Pullers: %-4u  Throughput: %6.1f MB/s  "
                   "Uptime: %.0fs   ",
                   sent_buf, stats.pull_pressure, mbps, elapsed);
            fflush(stdout);
        }

        SLEEP_MS(50);
    }

    /* ── Teardown ───────────────────────────────────────────────────── */
    printf("\n\nShutting down...\n");
    rgtp_destroy_surface(surface);   /* zeroizes key before free */
    rgtp_socket_destroy(sock);
    free(data);
    rgtp_cleanup();
    return 0;
}
