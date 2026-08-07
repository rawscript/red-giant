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
 *
 * NOTE: Public API functions (rgtp_init, rgtp_version, rgtp_cleanup, etc.)
 *       are defined in rgtp_init.c
 *       Stubs for unimplemented functions are in rgtp_stubs.c
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
/* Use static allocation with runtime initialization for thread safety */
static CRITICAL_SECTION s_served_mutex_instance;
static int s_served_mutex_initialized = 0;
static CRITICAL_SECTION* s_served_mutex = NULL;
#else
static pthread_mutex_t s_served_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t s_rs_init_mutex = PTHREAD_MUTEX_INITIALIZER;
#endif

/* Helper to get mutex pointer safely (thread-safe lazy initialization) */
static CRITICAL_SECTION* get_served_mutex(void)
{
#ifdef _WIN32
    if (s_served_mutex == NULL) {
        /* Use InterlockedCompareExchange for thread-safe lazy init */
        CRITICAL_SECTION* expected = NULL;
        if (InterlockedCompareExchangePointer((void**)&s_served_mutex, 
                                               &s_served_mutex_instance, 
                                               expected) == expected) {
            /* We won the race - initialize */
            InitializeCriticalSection(&s_served_mutex_instance);
        }
    }
    return s_served_mutex;
#else
    return &s_served_mutex;
#endif
}

typedef struct served_client {
    struct sockaddr_in addr;
    struct served_client* next;
} served_client_t;

static served_client_t* served_list = NULL;

/* ── Initialize Reed-Solomon tables (thread-safe) ───────────────────────── */

static void rs_init_tables(void)
{
    /* Check if already initialized without lock first (fast path) */
    /* States: 0=not started, 2=tables initialized, 3=poly started, 4=fully initialized */
    if (atomic_load(&rs_initialized) >= 2) {
        return;
    }

    /* Double-checked locking pattern */
#ifdef _WIN32
    while (atomic_load(&rs_initialized) < 2) {
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
    if (atomic_load(&rs_initialized) < 2) {
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
    
    CRITICAL_SECTION* mtx = get_served_mutex();
    if (!mtx) return 0;
    
#ifdef _WIN32
    EnterCriticalSection(mtx);
#else
    pthread_mutex_lock(&s_served_mutex);
#endif
    
    served_client_t* cur = served_list;
    while (cur) {
        if (memcmp(&cur->addr, client, sizeof(*client)) == 0) {
#ifdef _WIN32
            LeaveCriticalSection(mtx);
#else
            pthread_mutex_unlock(&s_served_mutex);
#endif
            return 1;
        }
        cur = cur->next;
    }
    
#ifdef _WIN32
    LeaveCriticalSection(mtx);
#else
    pthread_mutex_unlock(&s_served_mutex);
#endif
    return 0;
}

void rgtp_mark_served(const struct sockaddr_in* client)
{
    if (!client) return;
    
    CRITICAL_SECTION* mtx = get_served_mutex();
    if (!mtx) return;
    
#ifdef _WIN32
    EnterCriticalSection(mtx);
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
    LeaveCriticalSection(mtx);
#else
    pthread_mutex_unlock(&s_served_mutex);
#endif
}

void rgtp_served_list_cleanup(void)
{
    CRITICAL_SECTION* mtx = get_served_mutex();
    if (!mtx) return;
    
#ifdef _WIN32
    EnterCriticalSection(mtx);
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
    LeaveCriticalSection(mtx);
#else
    pthread_mutex_unlock(&s_served_mutex);
#endif
}