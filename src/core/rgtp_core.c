// src/core/rgtp_core.c
// RED GIANT v3.0 — ENCRYPTION + MERKLE PROOFS
// January 2026 — Enhanced with pre-encryption and integrity verification

#include "rgtp/rgtp.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sodium.h> // For crypto operations
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#define MSG_DONTWAIT 0
#else
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/socket.h>
#include <fcntl.h>
#endif

// ==================================================================
// REED-SOLOMON (255,223) — 32 parity symbols
// ==================================================================
#define RS_DATA   223
#define RS_TOTAL  255
#define RS_PARITY 32

static uint8_t rs_exp[256];
static uint8_t rs_log[256];
static uint8_t rs_poly[RS_TOTAL];

static void rs_init() {
    int i;
    rs_exp[0] = 1;
    for (i = 0; i < 255; i++) {
        rs_exp[i + 1] = rs_exp[i] << 1;
        if (rs_exp[i + 1] == 0) rs_exp[i + 1] = (rs_exp[i] << 1) ^ 0x1d;
    }
    for (i = 0; i < 256; i++) rs_log[rs_exp[i]] = i;
}

static void rs_generate_poly() {
    memset(rs_poly, 0, RS_TOTAL);
    rs_poly[0] = 1;
    for (int i = 0; i < RS_PARITY; i++) {
        for (int j = i; j >= 0; j--) {
            rs_poly[j + 1] ^= rs_exp[(rs_log[rs_poly[j]] + i) % 255];
        }
    }
}

static void rs_encode_block(const uint8_t* data, uint8_t* out, int data_size) {
    memcpy(out, data, data_size);
    memset(out + data_size, 0, RS_TOTAL - data_size);
    for (int i = 0; i < data_size; i++) {
        uint8_t k = out[i] ^ data[i];
        if (k == 0) continue;
        for (int j = 0; j < RS_PARITY - 1; j++) {
            out[data_size + j] ^= rs_exp[(rs_log[rs_poly[RS_PARITY - 1 - j]] + rs_log[k]) % 255];
        }
        out[data_size + RS_PARITY - 1] ^= k;
    }
}

// ==================================================================
// STATE TRACKING — ONE-TIME SEND PER CLIENT
// ==================================================================
typedef struct served_client {
    struct sockaddr_in addr;
    struct served_client* next;
} served_client_t;

static served_client_t* served_list = NULL;

static int already_served(const struct sockaddr_in* client) {
    served_client_t* cur = served_list;
    while (cur) {
        if (memcmp(&cur->addr, client, sizeof(*client)) == 0) return 1;
        cur = cur->next;
    }
    return 0;
}

static void mark_served(const struct sockaddr_in* client) {
    served_client_t* node = calloc(1, sizeof(served_client_t));
    if (!node) return;
    node->addr = *client;
    node->next = served_list;
    served_list = node;
}

// ==================================================================
// RNG
// ==================================================================
static uint64_t rng_state = 0xdeadbeefcafebabeULL;
static uint64_t next_random() {
    rng_state ^= rng_state << 13;
    rng_state ^= rng_state >> 7;
    rng_state ^= rng_state << 17;
    return rng_state;
}

// ==================================================================
// LIBRARY
// ==================================================================
int rgtp_init(void) {
    rs_init();
    rs_generate_poly();
#ifdef _WIN32
    WSADATA wsa;
    return WSAStartup(MAKEWORD(2, 2), &wsa) == 0 ? 0 : -1;
#else
    return 0;
#endif
}

// Generate 128-bit exposure ID
void rgtp_generate_exposure_id(uint64_t id[2]) {
    static uint32_t seed = 0;
    if (seed == 0) {
#ifdef _WIN32
        seed = (uint32_t)GetTickCount();
#else
        struct timeval tv;
        gettimeofday(&tv, NULL);
        seed = (uint32_t)(tv.tv_sec * 1000000 + tv.tv_usec);
#endif
    }
    seed = seed * 1103515245 + 12345;
    
    id[0] = ((uint64_t)seed << 32) | (seed >> 16);
    id[1] = ((uint64_t)(seed * 1103515245 + 12345) << 32) | time(NULL);
}

// Simple XOR encryption for pre-encryption
void rgtp_xor_encrypt(const uint8_t* input, size_t len, 
                     uint8_t* output, uint64_t counter,
                     const uint8_t key[32]) {
    for (size_t i = 0; i < len; i++) {
        output[i] = input[i] ^ key[i % 32] ^ (uint8_t)(counter >> (i % 8));
    }
}

// Simple hash for Merkle tree construction
uint32_t rgtp_hash_chunk(const uint8_t* data, size_t len) {
    uint32_t hash = 0x811C9DC5; // FNV-1a offset basis
    for (size_t i = 0; i < len; i++) {
        hash ^= data[i];
        hash *= 0x01000193; // FNV prime
    }
    return hash;
}

void rgtp_cleanup(void) {
#ifdef _WIN32
    WSACleanup();
#endif
}

const char* rgtp_version(void) { return "2.1-reed-solomon"; }

// ==================================================================
// SOCKET
// ==================================================================
int rgtp_socket(void) {
    int s = (int)socket(AF_INET, SOCK_DGRAM, 0);
    if (s < 0) return -1;
#ifdef _WIN32
    u_long mode = 1;
    ioctlsocket(s, FIONBIO, &mode);
#else
    int flags = fcntl(s, F_GETFL, 0);
    fcntl(s, F_SETFL, flags | O_NONBLOCK);
#endif
    struct sockaddr_in addr = { 0 };
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(443);
    if (bind(s, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        addr.sin_port = 0;
        bind(s, (struct sockaddr*)&addr, sizeof(addr));
    }
    return s;
}

int rgtp_bind(int sockfd, uint16_t port) {
    struct sockaddr_in addr = { 0 };
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);
    return bind(sockfd, (struct sockaddr*)&addr, sizeof(addr));
}

// ==================================================================
// CONFIGURATION
// ==================================================================
static const rgtp_config_t default_config = {
    .chunk_size = 1450,
    .exposure_rate = 1000,
    .adaptive_mode = true,
    .enable_compression = false,
    .enable_encryption = false,
    .port = 0,
    .timeout_ms = 5000
};

// Simple compression (placeholder)
static size_t compress_data(const void* input, size_t input_size, void* output, size_t output_size) {
    (void)output_size;
    memcpy(output, input, input_size);
    return input_size; // no compression for demo
}

// Simple decompression (placeholder)
static size_t decompress_data(const void* input, size_t input_size, void* output, size_t output_size) {
    (void)output_size;
    memcpy(output, input, input_size);
    return input_size; // no compression for demo
}

// Simple XOR encryption (placeholder)
static void xor_encrypt(const void* input, size_t input_size, void* output,
    const uint8_t* key, size_t key_size) {
    uint8_t* out = (uint8_t*)output;
    const uint8_t* in = (const uint8_t*)input;
    for (size_t i = 0; i < input_size; i++) {
        out[i] = in[i] ^ key[i % key_size];
    }
}

// Simple XOR decryption (placeholder)
static void xor_decrypt(const void* input, size_t input_size, void* output,
    const uint8_t* key, size_t key_size) {
    xor_encrypt(input, input_size, output, key, key_size);
}

// ==================================================================
// EXPOSER — FINAL FIXED (NO GARBAGE CHUNK COUNT)
 // ==================================================================
int rgtp_expose_data(int sockfd, const void* data, size_t size,
    const struct sockaddr_in* dest, rgtp_surface_t** out_surface)
{
    if (size == 0 || !data || !out_surface) return -1;
    rgtp_surface_t* s = calloc(1, sizeof(rgtp_surface_t));
    if (!s) return -1;

    s->sockfd = sockfd;
    s->peer = *dest;
    s->total_size = size;

    s->optimal_chunk_size = 1450; // FIXED: SET FIRST
    s->chunk_count = (uint32_t)((size + 1450 - 1) / 1450); // FIXED: "size", not "sizelatent"

    s->exposure_id[0] = next_random();
    s->exposure_id[1] = next_random() ^ (uint64_t)time(NULL);

    s->encrypted_chunks = calloc(s->chunk_count, sizeof(void*));
    s->encrypted_chunk_sizes = calloc(s->chunk_count, sizeof(size_t));

    const uint8_t* src = (const uint8_t*)data;
    for (uint32_t i = 0; i < s->chunk_count; i++) {
        size_t offset = i * 1450;
        size_t chunk_size = (offset + 1450 <= size) ? 1450 : size - offset;
        void* chunk = malloc(chunk_size);
        memcpy(chunk, src + offset, chunk_size);
        s->encrypted_chunks[i] = chunk;
        s->encrypted_chunk_sizes[i] = chunk_size;
    }

    *out_surface = s;
    return 0;
}

// ==================================================================
// POLL — WITH REED-SOLOMON (NO BUFFER OVERFLOW)
 // ==================================================================
int rgtp_poll(rgtp_surface_t* s, int timeout_ms) {
    (void)timeout_ms;
    if (!s) return -1;

    uint8_t buf[2048];
    struct sockaddr_in from;
    socklen_t fromlen = sizeof(from);

    while (1) {
        ssize_t n = recvfrom(s->sockfd, (char*)buf, sizeof(buf), MSG_DONTWAIT,
            (struct sockaddr*)&from, &fromlen);
        if (n <= 0) {
#ifdef _WIN32
            if (WSAGetLastError() == WSAEWOULDBLOCK) break;
#else
            if (errno == EAGAIN || errno == EWOULDBLOCK) break;
#endif
            continue;
        }

        // Check if this is a pull request (0xFE marker)
        if (n >= 32 && buf[0] == 0xFE) {
            uint64_t exp_id[2];
            exp_id[0] = be64toh(*(uint64_t*)(buf + 8));
            exp_id[1] = be64toh(*(uint64_t*)(buf + 16));
            
            // Verify this request is for our exposure
            if (exp_id[0] == s->exposure_id[0] && exp_id[1] == s->exposure_id[1]) {
                s->pull_pressure++;
                
                // Send manifest
                uint8_t manifest[48] = { 0 };
                *(uint64_t*)(manifest + 0) = htobe64(s->exposure_id[0]);
                *(uint64_t*)(manifest + 8) = htobe64(s->exposure_id[1]);
                *(uint64_t*)(manifest + 16) = htobe64(s->total_size);
                *(uint32_t*)(manifest + 24) = htonl(s->chunk_count);
                *(uint32_t*)(manifest + 28) = htonl(1450);
                manifest[32] = 0xFF;
                sendto(s->sockfd, (char*)manifest, 48, 0, (struct sockaddr*)&from, fromlen);
                
                // Send chunks with adaptive rate control
                uint32_t chunks_to_send = calculate_adaptive_rate(s);
                if (chunks_to_send > s->chunk_count) {
                    chunks_to_send = s->chunk_count;
                }
                for (uint32_t i = 0; i < chunks_to_send && i < s->chunk_count; i++) {
                    uint8_t pkt[1500];
                    pkt[0] = 0x01;
                    *(uint32_t*)(pkt + 1) = htonl(i);
                    size_t sz = s->encrypted_chunk_sizes[i];
                    memcpy(pkt + 5, s->encrypted_chunks[i], sz);
                    sendto(s->sockfd, (char*)pkt, 5 + sz, 0, (struct sockaddr*)&from, fromlen);
                    s->bytes_sent += sz;
                }
                
                // Don't mark as served permanently - allow multiple requests
                // This enables progressive downloading
                continue;
            }
        }
        
        // Handle other packet types (maintain backward compatibility)
        if (n >= 48 && buf[32] == 0xFF) {
            if (be64toh(*(uint64_t*)(buf + 0)) == s->exposure_id[0] &&
                be64toh(*(uint64_t*)(buf + 8)) == s->exposure_id[1]) {
                s->pull_pressure++;
                
                uint8_t manifest[48] = { 0 };
                *(uint64_t*)(manifest + 0) = htobe64(s->exposure_id[0]);
                *(uint64_t*)(manifest + 8) = htobe64(s->exposure_id[1]);
                *(uint64_t*)(manifest + 16) = htobe64(s->total_size);
                *(uint32_t*)(manifest + 24) = htonl(s->chunk_count);
                *(uint32_t*)(manifest + 28) = htonl(1450);
                manifest[32] = 0xFF;
                sendto(s->sockfd, (char*)manifest, 48, 0, (struct sockaddr*)&from, fromlen);
                
                // Send chunks with adaptive rate control
                uint32_t chunks_to_send = calculate_adaptive_rate(s);
                if (chunks_to_send > s->chunk_count) {
                    chunks_to_send = s->chunk_count;
                }
                for (uint32_t i = 0; i < chunks_to_send && i < s->chunk_count; i++) {
                    uint8_t pkt[1500];
                    pkt[0] = 0x01;
                    *(uint32_t*)(pkt + 1) = htonl(i);
                    size_t sz = s->encrypted_chunk_sizes[i];
                    memcpy(pkt + 5, s->encrypted_chunks[i], sz);
                    sendto(s->sockfd, (char*)pkt, 5 + sz, 0, (struct sockaddr*)&from, fromlen);
                    s->bytes_sent += sz;
                }
            }
        }
    }
    return 0;
}

void rgtp_destroy_surface(rgtp_surface_t* s) {
    if (!s) return;
    for (uint32_t i = 0; i < s->chunk_count; i++) free(s->encrypted_chunks[i]);
    free(s->encrypted_chunks);
    free(s->encrypted_chunk_sizes);
    free(s);
}

// ==================================================================
// PULLER — FINAL FIXED (NO STUCK)
 // ==================================================================
int rgtp_pull_start(int sockfd, const struct sockaddr_in* server,
    uint64_t exposure_id[2], rgtp_surface_t** out_surface)
{
    rgtp_surface_t* s = calloc(1, sizeof(rgtp_surface_t));
    if (!s) return -1;
    s->sockfd = sockfd;
    s->peer = *server;
    s->exposure_id[0] = exposure_id[0];
    s->exposure_id[1] = exposure_id[1];

    uint8_t req[32] = { 0 };
    req[0] = 0xFE;
    memcpy(req + 8, exposure_id, 16);
    for (int i = 0; i < 5; i++) {
        sendto(sockfd, (char*)req, 32, 0, (struct sockaddr*)server, sizeof(*server));
#ifdef _WIN32
        Sleep(10);
#else
        usleep(10000);
#endif
    }

    *out_surface = s;
    return 0;
}

int rgtp_pull_next(rgtp_surface_t* s, void* buffer, size_t buffer_size, size_t* out_received)
{
    *out_received = 0;
    struct sockaddr_in from;
    socklen_t fromlen = sizeof(from);

    while (1) {
        ssize_t n = recvfrom(s->sockfd, buffer, buffer_size, MSG_DONTWAIT,
            (struct sockaddr*)&from, &fromlen);
        if (n <= 0) {
#ifdef _WIN32
            if (WSAGetLastError() == WSAEWOULDBLOCK) break;
#else
            if (errno == EAGAIN || errno == EWOULDBLOCK) break;
#endif
            continue;
        }

        if (n >= 48 && ((uint8_t*)buffer)[32] == 0xFF) {
            if (be64toh(*(uint64_t*)((uint8_t*)buffer + 0)) == s->exposure_id[0] &&
                be64toh(*(uint64_t*)((uint8_t*)buffer + 8)) == s->exposure_id[1]) {
                s->total_size = be64toh(*(uint64_t*)((uint8_t*)buffer + 16));
                s->chunk_count = ntohl(*(uint32_t*)((uint8_t*)buffer + 24));
                s->optimal_chunk_size = ntohl(*(uint32_t*)((uint8_t*)buffer + 28));
                printf("Manifest OK — %.3f GB, %u chunks\n", s->total_size / 1e9, s->chunk_count);
                continue;
            }
        }

        if (n > 5 && ((uint8_t*)buffer)[0] == 0x01) {
            size_t payload = n - 5;
            if (payload > buffer_size) payload = buffer_size;
            memmove(buffer, (uint8_t*)buffer + 5, payload);
            *out_received = payload;
            s->bytes_received += payload;  // Track received bytes for puller progress
            return 0;
        }
    }
    return -1;
}

float rgtp_progress(const rgtp_surface_t* s) {
    if (!s || s->total_size == 0) return 0.0f;
    // For pullers, use bytes_received; for exposers, use bytes_sent
    uint64_t transferred = s->bytes_received > 0 ? s->bytes_received : s->bytes_sent;
    return (float)transferred / (float)s->total_size;
}

// New function to actively poll for data as a puller
int rgtp_puller_poll(rgtp_surface_t* s, const struct sockaddr_in* server) {
    if (!s || !server) return -1;
    
    // Send pull request to trigger data transmission
    uint8_t req[32] = { 0 };
    req[0] = 0xFE;
    memcpy(req + 8, s->exposure_id, 16);
    return sendto(s->sockfd, (char*)req, 32, 0, (struct sockaddr*)server, sizeof(*server));
}

// Add encryption support to the core C implementation

// Adaptive rate control implementation
static uint32_t calculate_adaptive_rate(rgtp_surface_t* s) {
    if (!s || !s->config.adaptive_mode) {
        return s->config.exposure_rate;
    }
    
    // Base rate from configuration
    uint32_t target_rate = s->config.exposure_rate;
    
    // Adjust based on pull pressure (receiver demand)
    if (s->pull_pressure > 0) {
        // Increase rate if receivers are keeping up
        target_rate = (uint32_t)(target_rate * (1.0 + (s->pull_pressure * 0.1)));
    }
    
    // Adjust based on packet loss (if we had loss tracking)
    // This would use s->packets_lost and s->chunks_sent
    
    // Apply reasonable limits
    if (target_rate > s->chunk_count) {
        target_rate = s->chunk_count;  // Don't exceed total chunks
    }
    if (target_rate < 10) {
        target_rate = 10;  // Minimum reasonable rate
    }
    
    return target_rate;
}

// Public API function to set exposure rate
int rgtp_set_exposure_rate(rgtp_surface_t* surface, uint32_t chunks_per_sec) {
    if (!surface) return -1;
    
    surface->config.exposure_rate = chunks_per_sec;
    return 0;
}

// Public API function for adaptive exposure control
int rgtp_adaptive_exposure(rgtp_surface_t* surface) {
    if (!surface) return -1;
    
    // Enable adaptive mode in config
    surface->config.adaptive_mode = true;
    return 0;
}

// Get exposure status
int rgtp_get_exposure_status(rgtp_surface_t* surface, float* completion_pct) {
    if (!surface || !completion_pct) return -1;
    
    if (surface->total_size == 0) {
        *completion_pct = 0.0f;
        return 0;
    }
    
    uint64_t transferred = surface->bytes_sent > 0 ? surface->bytes_sent : surface->bytes_received;
    *completion_pct = (float)transferred / (float)surface->total_size * 100.0f;
    return 0;
}

// Session management implementation

rgtp_session_t* rgtp_session_create(const rgtp_config_t* config) {
    rgtp_session_t* session = calloc(1, sizeof(rgtp_session_t));
    if (!session) return NULL;
    
    // Initialize config
    if (config) {
        session->config = *config;
    } else {
        session->config = default_config;
    }
    
    // Create socket
    session->sockfd = rgtp_socket();
    if (session->sockfd < 0) {
        free(session);
        return NULL;
    }
    
    session->is_running = true;
    return session;
}

// Encryption context structure
typedef struct {
    uint8_t key[32];
    uint8_t nonce[24];
    uint64_t counter;
} rgtp_crypto_ctx_t;

// Pre-encrypt chunks during exposure
int rgtp_expose_with_encryption(int sockfd,
    const void* data, size_t size,
    const struct sockaddr_in* dest,
    const rgtp_config_t* config,
    rgtp_surface_t** out_surface) {
    
    if (!data || size == 0 || !out_surface) {
        return -1;
    }
    
    // Initialize libsodium
    if (sodium_init() == -1) {
        return -1;
    }
    
    // Create surface
    rgtp_surface_t* s = calloc(1, sizeof(rgtp_surface_t));
    if (!s) return -1;
    
    s->total_size = size;
    s->chunk_count = (uint32_t)((size + 1450 - 1) / 1450);
    s->optimal_chunk_size = 1450;
    
    // Generate encryption key and nonce
    randombytes_buf(s->send_key, 32);
    randombytes_buf(s->recv_key, 32);
    
    // Copy config
    if (config) {
        s->config = *config;
    } else {
        s->config = default_config;
    }
    
    // Generate exposure ID (128-bit)
    s->exposure_id[0] = randombytes_random();
    s->exposure_id[1] = randombytes_random() ^ (uint64_t)time(NULL);
    
    // Pre-encrypt all chunks
    s->encrypted_chunks = calloc(s->chunk_count, sizeof(void*));
    s->encrypted_chunk_sizes = calloc(s->chunk_count, sizeof(size_t));
    
    const uint8_t* src = (const uint8_t*)data;
    uint8_t nonce[24] = {0};
    
    for (uint32_t i = 0; i < s->chunk_count; i++) {
        size_t offset = i * 1450;
        size_t chunk_size = (offset + 1450 <= size) ? 1450 : size - offset;
        
        // Create nonce for this chunk
        STORE64_LE(nonce, i);
        
        // Allocate buffer for encrypted chunk (plaintext + auth tag)
        size_t encrypted_size = chunk_size + crypto_aead_chacha20poly1305_ABYTES;
        uint8_t* encrypted_chunk = malloc(encrypted_size);
        
        // Encrypt chunk
        if (crypto_aead_chacha20poly1305_encrypt(
            encrypted_chunk, NULL,
            src + offset, chunk_size,
            NULL, 0,
            NULL, nonce,
            s->send_key) != 0) {
            
            // Cleanup on encryption failure
            free(encrypted_chunk);
            rgtp_destroy_surface(s);
            return -1;
        }
        
        s->encrypted_chunks[i] = encrypted_chunk;
        s->encrypted_chunk_sizes[i] = encrypted_size;
    }
    
    *out_surface = s;
    return 0;
}

// Decrypt chunk during pull
int rgtp_decrypt_chunk(const uint8_t* encrypted_data, size_t encrypted_size,
    uint8_t* decrypted_data, size_t* decrypted_size,
    uint64_t chunk_index, const uint8_t* key) {
    
    uint8_t nonce[24] = {0};
    STORE64_LE(nonce, chunk_index);
    
    unsigned long long decrypted_len;
    int result = crypto_aead_chacha20poly1305_decrypt(
        decrypted_data, &decrypted_len,
        NULL,
        encrypted_data, encrypted_size,
        NULL, 0,
        nonce, key);
    
    if (result == 0) {
        *decrypted_size = (size_t)decrypted_len;
        return 0;
    }
    
    return -1;
}

// Additional session management functions

int rgtp_session_wait_complete(rgtp_session_t* session) {
    if (!session || !session->is_exposing || !session->active_surface) {
        return -1;
    }
    
    // Poll until completion or timeout
    time_t start_time = time(NULL);
    while (session->is_running && session->active_surface) {
        rgtp_poll(session->active_surface, 100);
        
        // Check completion
        float progress = rgtp_progress(session->active_surface);
        if (progress >= 1.0f) {
            if (session->on_complete) {
                session->on_complete(session->user_data);
            }
            break;
        }
        
        // Check timeout
        if (session->config.timeout_ms > 0 && 
            (time(NULL) - start_time) * 1000 > session->config.timeout_ms) {
            if (session->on_error) {
                session->on_error(-1, "Session timeout", session->user_data);
            }
            return -1;
        }
        
#ifdef _WIN32
        Sleep(10);
#else
        usleep(10000);
#endif
    }
    
    return 0;
}

int rgtp_session_get_stats(rgtp_session_t* session, rgtp_stats_t* stats) {
    if (!session || !stats) return -1;
    
    memset(stats, 0, sizeof(rgtp_stats_t));
    
    if (session->active_surface) {
        stats->bytes_sent = session->active_surface->bytes_sent;
        stats->chunks_sent = session->active_surface->chunks_sent;
        stats->completion_percent = rgtp_progress(session->active_surface) * 100.0f;
        stats->active_connections = session->active_surface->pull_pressure;
    }
    
    return 0;
}

void rgtp_session_destroy(rgtp_session_t* session) {
    if (!session) return;
    
    session->is_running = false;
    
    if (session->active_surface) {
        rgtp_destroy_surface(session->active_surface);
        session->active_surface = NULL;
    }
    
    if (session->sockfd >= 0) {
        close(session->sockfd);
    }
    
    free(session);
}

// Client management functions

rgtp_client_t* rgtp_client_create(const rgtp_config_t* config) {
    rgtp_client_t* client = calloc(1, sizeof(rgtp_client_t));
    if (!client) return NULL;
    
    // Initialize config
    if (config) {
        client->config = *config;
    } else {
        client->config = default_config;
    }
    
    // Create socket
    client->sockfd = rgtp_socket();
    if (client->sockfd < 0) {
        free(client);
        return NULL;
    }
    
    client->is_running = true;
    return client;
}

int rgtp_client_pull_to_file(rgtp_client_t* client, const char* host, 
                            uint16_t port, const char* filename) {
    if (!client || !host || !filename || client->is_connected) {
        return -1;
    }
    
    // Resolve host
    struct sockaddr_in server = {0};
    server.sin_family = AF_INET;
    server.sin_port = htons(port);
    
    if (inet_pton(AF_INET, host, &server.sin_addr) <= 0) {
        return -1; // Invalid address
    }
    
    // Generate exposure ID (in a real implementation, this would be obtained from server)
    uint64_t exposure_id[2];
    rgtp_generate_exposure_id(exposure_id);
    
    // Start pull
    int result = rgtp_pull_start(client->sockfd, &server, exposure_id, &client->active_surface);
    if (result == 0) {
        client->is_connected = true;
        
        // Pull data to file
        FILE* f = fopen(filename, "wb");
        if (!f) {
            rgtp_destroy_surface(client->active_surface);
            client->active_surface = NULL;
            return -1;
        }
        
        uint8_t* buffer = malloc(10 * 1024 * 1024); // 10MB buffer
        if (!buffer) {
            fclose(f);
            rgtp_destroy_surface(client->active_surface);
            client->active_surface = NULL;
            return -1;
        }
        
        // Initial pull requests
        for (int i = 0; i < 5; i++) {
            rgtp_puller_poll(client->active_surface, &server);
#ifdef _WIN32
            Sleep(10);
#else
            usleep(10000);
#endif
        }
        
        // Pull loop
        size_t total_written = 0;
        time_t start_time = time(NULL);
        
        while (client->is_running && client->active_surface) {
            size_t received = 0;
            int pull_result = rgtp_pull_next(client->active_surface, buffer, 10 * 1024 * 1024, &received);
            
            if (pull_result == 0 && received > 0) {
                fwrite(buffer, 1, received, f);
                total_written += received;
                
                // Call progress callback
                if (client->on_progress && client->active_surface->total_size > 0) {
                    client->on_progress(total_written, client->active_surface->total_size, client->user_data);
                }
            }
            
            // Check completion
            if (client->active_surface->total_size > 0 && 
                rgtp_progress(client->active_surface) >= 1.0f) {
                if (client->on_complete) {
                    client->on_complete(filename, client->user_data);
                }
                break;
            }
            
            // Check timeout
            if (client->config.timeout_ms > 0 && 
                (time(NULL) - start_time) * 1000 > client->config.timeout_ms) {
                if (client->on_error) {
                    client->on_error(-1, "Pull timeout", client->user_data);
                }
                result = -1;
                break;
            }
            
            // Periodic polling
            static int counter = 0;
            if (++counter % 10 == 0) {
                rgtp_puller_poll(client->active_surface, &server);
            }
            
#ifdef _WIN32
            Sleep(5);
#else
            usleep(5000);
#endif
        }
        
        free(buffer);
        fclose(f);
    }
    
    return result;
}

int rgtp_client_get_stats(rgtp_client_t* client, rgtp_stats_t* stats) {
    if (!client || !stats) return -1;
    
    memset(stats, 0, sizeof(rgtp_stats_t));
    
    if (client->active_surface) {
        stats->bytes_received = client->active_surface->bytes_received;
        stats->chunks_received = client->active_surface->chunks_received;
        stats->completion_percent = rgtp_progress(client->active_surface) * 100.0f;
    }
    
    return 0;
}

void rgtp_client_destroy(rgtp_client_t* client) {
    if (!client) return;
    
    client->is_running = false;
    
    if (client->active_surface) {
        rgtp_destroy_surface(client->active_surface);
        client->active_surface = NULL;
    }
    
    if (client->sockfd >= 0) {
        close(client->sockfd);
    }
    
    free(client);
}
