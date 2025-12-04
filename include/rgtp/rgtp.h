/**
 * @file rgtp.h
 * @brief Red Giant Transport Protocol — UDP Edition (v2.0 — December 2025)
 * 
 * The transport that ends TCP’s 44-year reign.
 * Expose once. Serve a billion. Zero state. Instant resume.
 * 
 * Now 100 % UDP. Works on phones, laptops, servers, behind NAT, everywhere.
 */

#ifndef RGTP_H
#define RGTP_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
typedef int socklen_t;
#else
#include <sys/socket.h>
#include <netinet/in.h>
#endif

/* -------------------------------------------------------------------------- */
/* Constants                                                                  */
/* -------------------------------------------------------------------------- */
#define RGTP_DEFAULT_PORT       443
#define RGTP_MAX_CHUNK_SIZE     (1024 * 1024)   // 1 MB max
#define RGTP_DEFAULT_CHUNK_SIZE (64 * 1024)     // 64 KB default

/* -------------------------------------------------------------------------- */
/* Packet types                                                               */
/* -------------------------------------------------------------------------- */
typedef enum {
    RGTP_HANDSHAKE          = 0x01,  // Future Noise_XX handshake (reserved)
    RGTP_EXPOSE_MANIFEST    = 0x02,  // "I’m exposing this data"
    RGTP_PULL_REQUEST       = 0x03,  // Bitmap or range of wanted chunks
    RGTP_CHUNK_DATA         = 0x04,  // Encrypted chunk + tag
    RGTP_PULL_ACK           = 0x05,  // Optional receiver feedback
    RGTP_ERROR              = 0xFF
} rgtp_packet_type_t;

/* -------------------------------------------------------------------------- */
/* Header — 48 bytes fixed                                                    */
/* -------------------------------------------------------------------------- */
#pragma pack(push, 1)
typedef struct {
    uint8_t   version;           // 2
    uint8_t   type;
    uint16_t  flags;
    uint64_t  exposure_id[2];    // 128-bit stateless identifier (the revolution)
    uint64_t  total_size;        // Big-endian
    uint32_t  chunk_count;       // Big-endian
    uint32_t  chunk_size;        // Big-endian (optimal)
    uint32_t  sequence_start;    // For CHUNK_DATA batches
    uint32_t  sequence_count;    // Number of chunks in this packet
} rgtp_header_t;
#pragma pack(pop)

/* -------------------------------------------------------------------------- */
/* Exposure surface — the core data structure                                */
/* -------------------------------------------------------------------------- */
typedef struct rgtp_surface_s {
    // Public — read-only for apps
    uint64_t  exposure_id[2];
    uint64_t  total_size;
    uint32_t  chunk_count;
    uint32_t  optimal_chunk_size;

    // Private — internal crypto & state
    uint8_t   send_key[32];
    uint8_t   recv_key[32];

    void**    encrypted_chunks;         // Pre-encrypted chunks (encrypt once!)
    size_t*   encrypted_chunk_sizes;
    uint8_t*  chunk_bitmap;             // 1 bit per chunk (received/exposed)

    void*     shared_memory;            // For localhost DMA mode
    size_t    shared_memory_size;

    int       sockfd;
    struct sockaddr_in peer;

    // Stats
    uint64_t  bytes_sent;
    uint32_t  pull_pressure;
} rgtp_surface_t;

/* -------------------------------------------------------------------------- */
/* Public API — clean, minimal, future-proof                                  */
/* -------------------------------------------------------------------------- */
#ifdef __cplusplus
extern "C" {
#endif

// Library
int         rgtp_init(void);
void        rgtp_cleanup(void);
const char* rgtp_version(void);         // Returns "2.0-udp"

// Socket
int         rgtp_socket(void);          // UDP socket on port 443
int         rgtp_bind(int sockfd, uint16_t port);

// Exposer side
int         rgtp_expose_data(int sockfd,
                             const void* data, size_t size,
                             const struct sockaddr_in* dest,
                             rgtp_surface_t** out_surface);

int         rgtp_poll(rgtp_surface_t* surface, int timeout_ms);  // Call often
void        rgtp_destroy_surface(rgtp_surface_t* surface);

// Puller side
int         rgtp_pull_start(int sockfd,
                            const struct sockaddr_in* server,
                            uint64_t exposure_id[2],
                            rgtp_surface_t** out_surface);

int         rgtp_pull_next(rgtp_surface_t* surface,
                           void* buffer, size_t buffer_size,
                           size_t* out_received);

float       rgtp_progress(const rgtp_surface_t* surface);  // 0.0 → 1.0

#ifdef __cplusplus
}
#endif

#endif // RGTP_H