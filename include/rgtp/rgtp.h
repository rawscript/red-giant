#ifndef RGTP_H
#define RGTP_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

// Forward declaration
struct sockaddr_in;

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
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
/* Configuration Structure                                                    */
/* -------------------------------------------------------------------------- */
typedef struct {
    uint32_t chunk_size;              // Chunk size in bytes (0 = auto)
    uint32_t exposure_rate;           // Initial exposure rate (chunks/sec)
    bool adaptive_mode;               // Enable adaptive rate control
    bool enable_compression;          // Enable data compression
    bool enable_encryption;           // Enable encryption
    uint16_t port;                   // Port number (0 = auto)
    int timeout_ms;                  // Operation timeout in milliseconds
    void* user_data;                 // User data for callbacks
} rgtp_config_t;

/* -------------------------------------------------------------------------- */
/* Statistics Structure                                                       */
/* -------------------------------------------------------------------------- */
typedef struct {
    uint64_t bytes_sent;
    uint64_t bytes_received;
    uint32_t chunks_sent;
    uint32_t chunks_received;
    float packet_loss_rate;
    int rtt_ms;                      // Round-trip time in milliseconds
    uint32_t packets_lost;
    uint32_t retransmissions;
    float avg_throughput_mbps;
    float completion_percent;
    uint32_t active_connections;
} rgtp_stats_t;

/* -------------------------------------------------------------------------- */
/* Session Management Structures                                              */
/* -------------------------------------------------------------------------- */
typedef struct rgtp_session {
    int sockfd;
    rgtp_config_t config;
    rgtp_surface_t* active_surface;
    bool is_exposing;
    bool is_running;
    void* user_data;
    // Callbacks
    void (*on_progress)(size_t transferred, size_t total, void* user_data);
    void (*on_complete)(void* user_data);
    void (*on_error)(int error_code, const char* message, void* user_data);
} rgtp_session_t;

typedef struct rgtp_client {
    int sockfd;
    rgtp_config_t config;
    rgtp_surface_t* active_surface;
    bool is_connected;
    bool is_running;
    void* user_data;
    // Callbacks
    void (*on_progress)(size_t received, size_t total, void* user_data);
    void (*on_complete)(const char* filename, void* user_data);
    void (*on_error)(int error_code, const char* message, void* user_data);
} rgtp_client_t;

/* -------------------------------------------------------------------------- */
/* Exposure surface — the core data structure                                */
/* -------------------------------------------------------------------------- */
typedef struct rgtp_surface_s {
    uint64_t  exposure_id[2];
    uint64_t  total_size;
    uint32_t  chunk_count;
    uint32_t  optimal_chunk_size;
    
    // Configuration
    rgtp_config_t config;

    uint8_t   send_key[32];
    uint8_t   recv_key[32];

    void** encrypted_chunks;
    size_t* encrypted_chunk_sizes;
    uint8_t* chunk_bitmap;

    // For puller: buffer to store received chunks in order
    void** received_chunks;
    size_t* received_chunk_sizes;
    uint8_t* received_chunk_bitmap;
    uint32_t next_expected_chunk;
    
    // For puller: track received bytes for accurate progress
    uint64_t  bytes_received;

    void* shared_memory;
    size_t    shared_memory_size;

    int       sockfd;
    struct sockaddr_in peer;

    uint64_t  bytes_sent;
    uint32_t  pull_pressure;
    
    // Statistics for adaptive rate control
    uint64_t  bytes_received_stats;
    uint32_t  chunks_sent;
    uint32_t  chunks_received;
    uint32_t  acks_received;
    uint32_t  packets_lost;
    int       rtt_ms;
    uint64_t  last_packet_time_ms;  // Last packet time in milliseconds
    
    // NAT traversal
    bool      nat_traversal_enabled;
    struct sockaddr_in public_addr;
} rgtp_surface_t;

/* -------------------------------------------------------------------------- */
/* Memory Pool Structures                                                     */
/* -------------------------------------------------------------------------- */

#define RGTP_MEMORY_POOL_SIZE 1024
#define RGTP_DEFAULT_CHUNK_SIZE 1450

typedef struct rgtp_memory_pool {
    void* blocks[RGTP_MEMORY_POOL_SIZE];
    size_t block_size;
    int free_count;
    int total_blocks;
    pthread_mutex_t mutex;  // For thread safety
} rgtp_memory_pool_t;

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

    // New function for exposing data with configuration
    int         rgtp_expose_data_with_config(int sockfd,
        const void* data, size_t size,
        const struct sockaddr_in* dest,
        const rgtp_config_t* config,
        rgtp_surface_t** out_surface);

    // New functions for enhanced features
    void        rgtp_generate_exposure_id(uint64_t id[2]);
    void        rgtp_xor_encrypt(const uint8_t* input, size_t len, 
                     uint8_t* output, uint64_t counter,
                     const uint8_t key[32]);
    uint32_t    rgtp_hash_chunk(const uint8_t* data, size_t len);

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
    
    // New function for active puller polling
    int         rgtp_puller_poll(rgtp_surface_t* surface, const struct sockaddr_in* server);
    
    // Statistics functions
    int         rgtp_get_stats(const rgtp_surface_t* surface, rgtp_stats_t* stats);
    
    // NAT traversal functions
    int         rgtp_enable_nat_traversal(rgtp_surface_t* surface);
    int         rgtp_perform_hole_punching(rgtp_surface_t* surface, 
        const struct sockaddr_in* peer_addr);
    
    // Adaptive rate control functions
    int         rgtp_set_exposure_rate(rgtp_surface_t* surface, uint32_t chunks_per_sec);
    int         rgtp_adaptive_exposure(rgtp_surface_t* surface);
    int         rgtp_get_exposure_status(rgtp_surface_t* surface, float* completion_pct);
    
    // Memory pool functions
    rgtp_memory_pool_t* rgtp_memory_pool_create(size_t block_size, int num_blocks);
    void        rgtp_memory_pool_destroy(rgtp_memory_pool_t* pool);
    void*       rgtp_memory_pool_alloc(rgtp_memory_pool_t* pool);
    void        rgtp_memory_pool_free(rgtp_memory_pool_t* pool, void* ptr);
    int         rgtp_memory_pool_init_global();
    void        rgtp_memory_pool_cleanup_global();

    // Session management functions
    rgtp_session_t* rgtp_session_create(const rgtp_config_t* config);
    int         rgtp_session_expose_file(rgtp_session_t* session, const char* filename);
    int         rgtp_session_wait_complete(rgtp_session_t* session);
    int         rgtp_session_get_stats(rgtp_session_t* session, rgtp_stats_t* stats);
    void        rgtp_session_destroy(rgtp_session_t* session);
    
    // Client management functions
    rgtp_client_t* rgtp_client_create(const rgtp_config_t* config);
    int         rgtp_client_pull_to_file(rgtp_client_t* client, const char* host, 
                    uint16_t port, const char* filename);
    int         rgtp_client_get_stats(rgtp_client_t* client, rgtp_stats_t* stats);
    void        rgtp_client_destroy(rgtp_client_t* client);

    // Helper function to check if all chunks have been written
    static inline int all_chunks_written(const rgtp_surface_t* surface) {
        if (!surface || surface->chunk_count == 0) return 0;
        return surface->next_expected_chunk >= surface->chunk_count;
    }

#ifdef __cplusplus
}
#endif

#endif // RGTP_H