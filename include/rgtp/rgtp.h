#ifndef RGTP_H
#define RGTP_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#define close closesocket
#define sleep(x) Sleep((x)*1000)
#define usleep(x) Sleep((x)/1000)
static inline uint64_t htobe64(uint64_t v) {
    return ((uint64_t)htonl((uint32_t)v) << 32) | htonl((uint32_t)(v >> 32));
}
static inline uint64_t be64toh(uint64_t v) { return htobe64(v); }
#else
#include <arpa/inet.h>
#include <unistd.h>
#endif

/* -------------------------------------------------------------------------- */
/* Exposure surface — the core data structure                                */
/* -------------------------------------------------------------------------- */
typedef struct rgtp_surface_s {
    uint64_t  exposure_id[2];
    uint64_t  total_size;
    uint32_t  chunk_count;
    uint32_t  optimal_chunk_size;

    uint8_t   send_key[32];
    uint8_t   recv_key[32];

    void** encrypted_chunks;
    size_t* encrypted_chunk_sizes;
    uint8_t* chunk_bitmap;

    void* shared_memory;
    size_t    shared_memory_size;

    int       sockfd;
    struct sockaddr_in peer;

    uint64_t  bytes_sent;
    uint32_t  pull_pressure;
} rgtp_surface_t;

/* -------------------------------------------------------------------------- */
/* Public API — clean, minimal, future-proof                                  */
/* -------------------------------------------------------------------------- */
#ifdef __cplusplus
extern "C" {
#endif

    int         rgtp_init(void);
    void        rgtp_cleanup(void);
    const char* rgtp_version(void);

    int         rgtp_socket(void);
    int         rgtp_bind(int sockfd, uint16_t port);

    int         rgtp_expose_data(int sockfd,
        const void* data, size_t size,
        const struct sockaddr_in* dest,
        rgtp_surface_t** out_surface);

    int         rgtp_poll(rgtp_surface_t* surface, int timeout_ms);
    void        rgtp_destroy_surface(rgtp_surface_t* surface);

    int         rgtp_pull_start(int sockfd,
        const struct sockaddr_in* server,
        uint64_t exposure_id[2],
        rgtp_surface_t** out_surface);

    int         rgtp_pull_next(rgtp_surface_t* surface,
        void* buffer, size_t buffer_size,
        size_t* out_received);

    float       rgtp_progress(const rgtp_surface_t* surface);

#ifdef __cplusplus
}
#endif

#endif // RGTP_H