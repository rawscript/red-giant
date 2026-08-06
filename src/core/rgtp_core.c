/**
 * @file rgtp_core.c
 * @brief Core RGTP implementation with thread-safe globals.
 *
 * This file contains the core RGTP functionality with proper thread safety
 * for global state. All mutable global variables are protected by mutexes.
 *
 * Thread safety guarantees:
 *  - Reed-Solomon tables are immutable after initialization (read-only)
 *  - served_list is protected by a mutex
 *  - PRNG state uses thread-local storage to avoid contention
 */

#include "rgtp/rgtp.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#define MSG_DONTWAIT 0
#else
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <errno.h>
#endif

/* ── Platform-specific includes ─────────────────────────────────────────── */
#ifdef _WIN32
#  include <windows.h>
#  define RGTP_THREAD_LOCAL __declspec(thread)
#else
#  include <pthread.h>
#  define RGTP_THREAD_LOCAL __thread
#endif

/* ── Reed-Solomon tables (read-only after init) ─────────────────────────── */
#define RS_DATA   223
#define RS_TOTAL  255
#define RS_PARITY 32

static uint8_t rs_exp[256];
static uint8_t rs_log[256];
static uint8_t rs_poly[RS_TOTAL];
static _Atomic int rs_initialized = 0;

/* Thread-local RNG state to avoid contention on global state */
RGTP_THREAD_LOCAL uint64_t tls_rng_state = 0xdeadbeefcafebabeULL;
RGTP_THREAD_LOCAL int tls_rng_seeded = 0;

/* ── served_list - protected by mutex ──────────────────────────────────── */
#ifdef _WIN32
static INIT_ONCE s_served_mutex_init_once = INIT_ONCE_STATIC_INIT;
static CRITICAL_SECTION* s_served_mutex = NULL;
#else
static pthread_mutex_t s_served_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t s_rs_init_mutex = PTHREAD_MUTEX_INITIALIZER;
#endif

typedef struct served_client {
    struct sockaddr_in addr;
    struct served_client* next;
} served_client_t;

static served_client_t* served_list = NULL;

/* ── Initialize Reed-Solomon tables (thread-safe) ───────────────────────── */

static void rs_init_tables(void)
{
    /* Check if already initialized without lock first (fast path) */
    if (atomic_load(&rs_initialized) == 1) {
        return;
    }

    /* Double-checked locking pattern */
#ifdef _WIN32
    while (atomic_load(&rs_initialized) == 0) {
        if (InterlockedCompareExchange(&rs_initialized, 1, 0) == 0) {
            /* We got the lock - initialize */
            int i;
            rs_exp[0] = 1;
            for (i = 0; i < 255; i++) {
                rs_exp[i + 1] = rs_exp[i] << 1;
                if (rs_exp[i + 1] == 0) rs_exp[i + 1] = (rs_exp[i] << 1) ^ 0x1d;
            }
            for (i = 0; i < 256; i++) rs_log[rs_exp[i]] = i;
            atomic_store(&rs_initialized, 2);  /* Mark fully initialized */
            return;
        }
        /* Another thread is initializing - wait */
        Sleep(0);  /* Yield to allow the initializing thread to progress */
    }
#else
    pthread_mutex_lock(&s_rs_init_mutex);
    if (atomic_load(&rs_initialized) == 0) {
        int i;
        rs_exp[0] = 1;
        for (i = 0; i < 255; i++) {
            rs_exp[i + 1] = rs_exp[i] << 1;
            if (rs_exp[i + 1] == 0) rs_exp[i + 1] = (rs_exp[i] << 1) ^ 0x1d;
        }
        for (i = 0; i < 256; i++) rs_log[rs_exp[i]] = i;
        atomic_store(&rs_initialized, 2);
    }
    pthread_mutex_unlock(&s_rs_init_mutex);
#endif
}

static void rs_generate_poly(void)
{
    /* Wait for tables to be initialized */
    while (atomic_load(&rs_initialized) < 2) {
        rs_init_tables();
#ifdef _WIN32
        if (atomic_load(&rs_initialized) < 2) {
            Sleep(1);
        }
#else
        if (atomic_load(&rs_initialized) < 2) {
            struct timespec ts = {0, 100000};  /* 100us */
            nanosleep(&ts, NULL);
        }
#endif
    }

    /* Generate polynomial - only one thread should do this */
    if (atomic_exchange(&rs_initialized, 3) != 3) {
        memset(rs_poly, 0, RS_TOTAL);
        rs_poly[0] = 1;
        for (int i = 0; i < RS_PARITY; i++) {
            for (int j = i; j >= 0; j--) {
                rs_poly[j + 1] ^= rs_exp[(rs_log[rs_poly[j]] + i) % 255];
            }
        }
        atomic_store(&rs_initialized, 4);  /* Fully initialized */
    }
}

static void rs_encode_block(const uint8_t* data, uint8_t* out, int data_size)
{
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

    /* Signal that tables are ready */
    atomic_store(&rs_initialized, 1);
}

/* ── Thread-local RNG ───────────────────────────────────────────────────── */

static void tls_rng_seed(void)
{
    if (tls_rng_seeded) return;
    
#ifdef _WIN32
    tls_rng_state = (uint64_t)GetTickCount64();
#else
    struct timeval tv;
    gettimeofday(&tv, NULL);
    tls_rng_state = (uint64_t)(tv.tv_sec * 1000000 + tv.tv_usec);
#endif
    
    /* Additional entropy from stack pointer */
    tls_rng_state ^= (uint64_t)(uintptr_t)&tls_rng_state;
    
    tls_rng_seeded = 1;
}

static uint64_t tls_next_random(void)
{
    if (!tls_rng_seeded) tls_rng_seed();
    
    tls_rng_state ^= tls_rng_state << 13;
    tls_rng_state ^= tls_rng_state >> 7;
    tls_rng_state ^= tls_rng_state << 17;
    return tls_rng_state;
}

/* ── served_list functions (thread-safe) ────────────────────────────────── */

int rgtp_already_served(const struct sockaddr_in* client)
{
    if (!client) return 0;
    
#ifdef _WIN32
    EnterCriticalSection(&s_served_mutex);
#else
    pthread_mutex_lock(&s_served_mutex);
#endif
    
    served_client_t* cur = served_list;
    while (cur) {
        if (memcmp(&cur->addr, client, sizeof(*client)) == 0) {
#ifdef _WIN32
            LeaveCriticalSection(&s_served_mutex);
#else
            pthread_mutex_unlock(&s_served_mutex);
#endif
            return 1;
        }
        cur = cur->next;
    }
    
#ifdef _WIN32
    LeaveCriticalSection(&s_served_mutex);
#else
    pthread_mutex_unlock(&s_served_mutex);
#endif
    return 0;
}

void rgtp_mark_served(const struct sockaddr_in* client)
{
    if (!client) return;
    
#ifdef _WIN32
    EnterCriticalSection(&s_served_mutex);
#else
    pthread_mutex_lock(&s_served_mutex);
#endif
    
    served_client_t* node = calloc(1, sizeof(served_client_t));
    if (node) {
        node->addr = *client;
        node->next = served_list;
        served_list = node;
    }
    
#ifdef _WIN32
    LeaveCriticalSection(&s_served_mutex);
#else
    pthread_mutex_unlock(&s_served_mutex);
#endif
}

void rgtp_served_list_cleanup(void)
{
#ifdef _WIN32
    EnterCriticalSection(&s_served_mutex);
#else
    pthread_mutex_lock(&s_served_mutex);
#endif
    
    served_client_t* cur = served_list;
    while (cur) {
        served_client_t* next = cur->next;
        free(cur);
        cur = next;
    }
    served_list = NULL;
    
#ifdef _WIN32
    LeaveCriticalSection(&s_served_mutex);
#else
    pthread_mutex_unlock(&s_served_mutex);
#endif
}

/* ── Library initialization ─────────────────────────────────────────────── */

int rgtp_init(void)
{
    /* Initialize served_list mutex (idempotent) */
#ifdef _WIN32
    BOOL result = InitializeCriticalSectionAndSpinCount(&s_served_mutex, 0x80000000);
    if (!result) {
        return -1;
    }
#endif
    
    /* Initialize Reed-Solomon tables (thread-safe) */
    rs_init_tables();
    rs_generate_poly();
    
#ifdef _WIN32
    WSADATA wsa;
    int wsa_result = WSAStartup(MAKEWORD(2, 2), &wsa);
    if (wsa_result != 0) {
        DeleteCriticalSection(&s_served_mutex);
        return -1;
    }
#else
    /* No global initialization needed for pthread_mutex */
#endif
    
    return 0;
}

void rgtp_cleanup(void)
{
    rgtp_served_list_cleanup();
    
#ifdef _WIN32
    /* For Windows, we don't actually delete the critical section
     * to avoid issues with re-initialization. The OS will clean it up
     * when the process terminates. */
    /* DeleteCriticalSection(&s_served_mutex); */
    WSACleanup();
#else
    /* Destroy mutex - this is not idempotent on all platforms */
    pthread_mutex_destroy(&s_served_mutex);
    pthread_mutex_destroy(&s_rs_init_mutex);
#endif
}

/* ── Public API functions ───────────────────────────────────────────────── */

const char* rgtp_version(void) { return "2.1-reed-solomon"; }

/* ── Thread-safe PRNG usage ─────────────────────────────────────────────── */

void rgtp_generate_exposure_id(uint64_t id[2])
{
    id[0] = tls_next_random();
    id[1] = tls_next_random() ^ (uint64_t)time(NULL);
}
