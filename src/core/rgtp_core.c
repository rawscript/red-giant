/**
 * @file rgtp_core.c
 * @brief Red Giant Transport Protocol — UDP Edition (December 2025)
 * @version 2.0.0-udp
 * 
 * The transport that kills TCP with kindness.
 * Expose once. Serve a billion. Zero state. Instant resume.
 */

#include "rgtp/rgtp.h"
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <inttypes.h>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#define close closesocket
#define getpid() GetCurrentProcessId()
static int winsock_initialized = 0;
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>
#endif

/* -------------------------------------------------------------------------- */
/* Platform abstraction                                                       */
/* -------------------------------------------------------------------------- */
#ifdef _WIN32
static void* platform_mmap(size_t size) {
    HANDLE h = CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, (DWORD)size, NULL);
    if (!h) return NULL;
    void* ptr = MapViewOfFile(h, FILE_MAP_ALL_ACCESS, 0, 0, size);
    CloseHandle(h);
    return ptr;
}
static int platform_munmap(void* addr, size_t size) {
    (void)size;
    return UnmapViewOfFile(addr) ? 0 : -1;
}
#else
#include <sys/mman.h>
static void* platform_mmap(size_t size) {
    void* p = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    return (p == MAP_FAILED) ? NULL : p;
}
static int platform_munmap(void* addr, size_t size) {
    return munmap(addr, size);
}
#endif

/* -------------------------------------------------------------------------- */
/* Crypto primitives (ChaCha20-Poly1305 — replace with libsodium later)      */
/* -------------------------------------------------------------------------- */
static void random_bytes(uint8_t* buf, size_t len) {
    for (size_t i = 0; i < len; i++) buf[i] = (uint8_t)rand();
}

static void chacha20_poly1305_encrypt(const uint8_t* key, const uint8_t* nonce,
                                      const uint8_t* plaintext, size_t pt_len,
                                      uint8_t* ciphertext) {
    // Stub — in real life: libsodium crypto_aead_chacha20poly1305_ietf_encrypt()
    memcpy(ciphertext, plaintext, pt_len);
    for (size_t i = 0; i < pt_len; i++) ciphertext[i] ^= key[i % 32] ^ nonce[i % 12];
}

/* -------------------------------------------------------------------------- */
/* Library init / cleanup                                                     */
/* -------------------------------------------------------------------------- */
int rgtp_init(void) {
#ifdef _WIN32
    if (!winsock_initialized) {
        WSADATA wsa;
        if (WSAStartup(MAKEWORD(2,2), &wsa) != 0) return -1;
        winsock_initialized = 1;
    }
#endif
    srand((unsigned)time(NULL));
    return 0;
}

void rgtp_cleanup(void) {
#ifdef _WIN32
    if (winsock_initialized) WSACleanup();
#endif
}

/* -------------------------------------------------------------------------- */
/* UDP socket creation — the moment Red Giant became real                     */
/* -------------------------------------------------------------------------- */
int rgtp_socket(void) {
#ifdef _WIN32
    if (!winsock_initialized) rgtp_init();
#endif

    int fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (fd < 0) return -1;

    int opt = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (void*)&opt, sizeof(opt));
#ifdef SO_REUSEPORT
    setsockopt(fd, SOL_SOCKET, SO_REUSEPORT, (void*)&opt, sizeof(opt));
#endif

    int buf = 8 * 1024 * 1024; // 8 MB
    setsockopt(fd, SOL_SOCKET, SO_RCVBUF, (void*)&buf, sizeof(buf));
    setsockopt(fd, SOL_SOCKET, SO_SNDBUF, (void*)&buf, sizeof(buf));

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(443);
    if (bind(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        addr.sin_port = 0;
        bind(fd, (struct sockaddr*)&addr, sizeof(addr));
    }

    return fd;
}

int rgtp_bind(int sockfd, uint16_t port) {
    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);
    return bind(sockfd, (struct sockaddr*)&addr, sizeof(addr));
}

/* -------------------------------------------------------------------------- */
/* Exposure surface — now with 128-bit ID and pre-encrypted chunks            */
/* -------------------------------------------------------------------------- */
rgtp_surface_t* rgtp_create_surface(int sockfd, const struct sockaddr_in* peer) {
    rgtp_surface_t* s = calloc(1, sizeof(rgtp_surface_t));
    if (!s) return NULL;

    s->sockfd = sockfd;
    if (peer) s->peer = *peer;

    // 128-bit exposure ID (stateless!)
    random_bytes((uint8_t*)s->exposure_id, 16);

    // Crypto keys
    random_bytes(s->send_key, 32);
    random_bytes(s->recv_key, 32);

    return s;
}

void rgtp_destroy_surface(rgtp_surface_t* s) {
    if (!s) return;
    if (s->encrypted_chunks) free(s->encrypted_chunks);
    if (s->chunk_bitmap) free(s->chunk_bitmap);
    if (s->shared_memory) platform_munmap(s->shared_memory, s->shared_memory_size);
    free(s);
}

/* -------------------------------------------------------------------------- */
/* Expose data — encrypt once, serve forever                                  */
/* -------------------------------------------------------------------------- */
int rgtp_expose_data(int sockfd, const void* data, size_t size,
                     const struct sockaddr_in* dest, rgtp_surface_t** out_surface) {
    rgtp_surface_t* s = rgtp_create_surface(sockfd, dest);
    if (!s) return -1;

    s->total_size = size;
    s->optimal_chunk_size = 64 * 1024; // 64 KB default
    if (size > 100 * 1024 * 1024) s->optimal_chunk_size = 256 * 1024; // 256 KB for huge files

    s->chunk_count = (size + s->optimal_chunk_size - 1) / s->optimal_chunk_size;
    s->chunk_bitmap = calloc(1, (s->chunk_count + 7) / 8);
    s->encrypted_chunks = calloc(s->chunk_count, sizeof(void*));
    if (!s->chunk_bitmap || !s->encrypted_chunks) {
        rgtp_destroy_surface(s);
        return -1;
    }

    // Pre-encrypt every chunk (the killer feature)
    for (uint32_t i = 0; i < s->chunk_count; i++) {
        size_t off = i * s->optimal_chunk_size;
        size_t chunk_len = (off + s->optimal_chunk_size > size) ? size - off : s->optimal_chunk_size;

        uint8_t nonce[12] = {0};
        *(uint32_t*)nonce = htonl(i);

        uint8_t* ct = malloc(chunk_len + 16); // +16 for Poly1305 tag
        chacha20_poly1305_encrypt(s->send_key, nonce, (const uint8_t*)data + off, chunk_len, ct);

        s->encrypted_chunks[i] = ct;
        s->encrypted_chunk_sizes[i] = chunk_len + 16;
        __builtin___clear_cache(ct, ct + chunk_len + 16);
    }

    // Send manifest (handshake)
    rgtp_send_manifest(s);

    *out_surface = s;
    return 0;
}

/* -------------------------------------------------------------------------- */
/* Pull data — receiver-driven                                                */
/* -------------------------------------------------------------------------- */
int rgtp_pull_data(int sockfd, const struct sockaddr_in* server,
                   void* buffer, size_t* buffer_size) {
    // Simplified for now — real version uses exposure ID discovery
    // This is the stub that will become the full pull loop
    return -1; // TODO: implement full puller logic
}

/* -------------------------------------------------------------------------- */
/* Internal packet sending helpers                                            */
/* -------------------------------------------------------------------------- */
static int send_udp_packet(int fd, const struct sockaddr_in* to,
                           const void* data, size_t len) {
    return sendto(fd, data, len, 0, (struct sockaddr*)to, sizeof(*to)) == (ssize_t)len ? 0 : -1;
}

int rgtp_send_manifest(rgtp_surface_t* s) {
    uint8_t pkt[256];
    rgtp_header_t* h = (rgtp_header_t*)pkt;
    h->version = 2;
    h->type = RGTP_EXPOSE_MANIFEST;
    memcpy(h->exposure_id, s->exposure_id, 16);
    h->total_size = htobe64(s->total_size);
    h->chunk_count = htonl(s->chunk_count);
    h->chunk_size = htonl(s->optimal_chunk_size);

    return send_udp_packet(s->sockfd, &s->peer, pkt, sizeof(rgtp_header_t) + 32);
}

/* -------------------------------------------------------------------------- */
/* Main receive loop (call from your app)                                     */
/* -------------------------------------------------------------------------- */
int rgtp_poll(rgtp_surface_t* s, int timeout_ms) {
    // TODO: recvmsg + dispatch to pull handler
    return 0;
}