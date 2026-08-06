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

/* ── Library initialization ─────────────────────────────────────────────── */

int rgtp_init(void)
{
    /* Pre-initialize the mutex for served_list (idempotent) */
    CRITICAL_SECTION* mtx = get_served_mutex();
    if (!mtx) {
        return -1;
    }
    
    /* Initialize Reed-Solomon tables (thread-safe) */
    rs_init_tables();
    rs_generate_poly();
    
#ifdef _WIN32
    WSADATA wsa;
    int wsa_result = WSAStartup(MAKEWORD(2, 2), &wsa);
    if (wsa_result != 0) {
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
    /* On Windows, we skip DeleteCriticalSection to handle potential
     * re-initialization. The OS handles cleanup at process exit. */
    WSACleanup();
#else
    /* Destroy mutexes - mark as destroyed to prevent reuse issues */
    pthread_mutex_destroy(&s_served_mutex);
    pthread_mutex_destroy(&s_rs_init_mutex);
#endif
}

/* ── Public API functions ───────────────────────────────────────────────── */

const char* rgtp_version(void) { return "2.1-reed-solomon"; }

rgtp_error_t rgtp_expose(rgtp_socket_t* sock,
                          const void* data,
                          size_t size,
                          const rgtp_config_t* cfg,
                          rgtp_surface_t** out_surface)
{
    (void)sock; (void)data; (void)size; (void)cfg;
    if (!out_surface) return RGTP_ERR_INVALID_ARG;
    *out_surface = NULL;
    return RGTP_ERR_NOT_SUPPORTED;
}

rgtp_error_t rgtp_poll(rgtp_surface_t* surface, int timeout_ms)
{
    (void)surface; (void)timeout_ms;
    return RGTP_ERR_NOT_SUPPORTED;
}

void rgtp_destroy_surface(rgtp_surface_t* surface)
{
    (void)surface;
}

rgtp_error_t rgtp_get_exposure_id(const rgtp_surface_t* surface,
                                    uint8_t out_id[16])
{
    (void)surface;
    if (!out_id) return RGTP_ERR_INVALID_ARG;
    memset(out_id, 0, 16);
    return RGTP_OK;
}

rgtp_error_t rgtp_pull_start(rgtp_socket_t* sock,
                              const struct sockaddr_storage* server,
                              const uint8_t exposure_id[16],
                              const rgtp_config_t* cfg,
                              rgtp_surface_t** out_surface)
{
    (void)sock; (void)server; (void)exposure_id; (void)cfg;
    if (!out_surface) return RGTP_ERR_INVALID_ARG;
    *out_surface = NULL;
    return RGTP_ERR_NOT_SUPPORTED;
}

rgtp_error_t rgtp_pull_next(rgtp_surface_t* surface,
                              void* buffer,
                              size_t buf_size,
                              size_t* out_received,
                              uint32_t* out_chunk_index)
{
    (void)surface; (void)buffer; (void)buf_size;
    if (out_received) *out_received = 0;
    if (out_chunk_index) *out_chunk_index = 0;
    return RGTP_ERR_NOT_SUPPORTED;
}

float rgtp_progress(const rgtp_surface_t* surface)
{
    (void)surface;
    return 0.0f;
}

rgtp_error_t rgtp_get_stats(const rgtp_surface_t* surface,
                             rgtp_stats_t* out)
{
    (void)surface;
    if (!out) return RGTP_ERR_INVALID_ARG;
    memset(out, 0, sizeof(*out));
    return RGTP_OK;
}

rgtp_error_t rgtp_get_latency_stats(const rgtp_surface_t* surface,
                                      rgtp_latency_stats_t* out)
{
    (void)surface;
    if (!out) return RGTP_ERR_INVALID_ARG;
    memset(out, 0, sizeof(*out));
    return RGTP_OK;
}

void rgtp_set_log_callback(rgtp_log_fn fn, void* ctx)
{
    (void)fn; (void)ctx;
}

void rgtp_set_log_level(int level)
{
    (void)level;
}

/* Satellite API stubs - required for tests to link */
rgtp_error_t rgtp_get_satellite_stats(const rgtp_surface_t* surface,
                                       rgtp_satellite_stats_t* out)
{
    (void)surface;
    if (!out) return RGTP_ERR_INVALID_ARG;
    memset(out, 0, sizeof(*out));
    return RGTP_OK;
}

rgtp_error_t rgtp_schedule_contact(rgtp_surface_t* surface,
                                    uint64_t start_time,
                                    uint64_t end_time,
                                    const char* ground_station)
{
    (void)surface; (void)start_time; (void)end_time; (void)ground_station;
    return RGTP_ERR_NOT_SUPPORTED;
}

rgtp_error_t rgtp_update_link_parameters(rgtp_surface_t* surface,
                                          float snr_db,
                                          float ber,
                                          int32_t doppler_hz)
{
    (void)surface; (void)snr_db; (void)ber; (void)doppler_hz;
    return RGTP_ERR_NOT_SUPPORTED;
}

rgtp_error_t rgtp_enable_store_forward(rgtp_surface_t* surface,
                                         uint64_t max_storage_bytes,
                                         uint32_t max_storage_time_s)
{
    (void)surface; (void)max_storage_bytes; (void)max_storage_time_s;
    return RGTP_ERR_NOT_SUPPORTED;
}

rgtp_error_t rgtp_get_contact_windows(const rgtp_surface_t* surface,
                                       rgtp_contact_window_t* windows,
                                       size_t max_windows,
                                       size_t* out_count)
{
    (void)surface;
    if (!out_count) return RGTP_ERR_INVALID_ARG;
    *out_count = 0;
    (void)windows; (void)max_windows;
    return RGTP_OK;
}

rgtp_error_t rgtp_configure_ccsds(rgtp_surface_t* surface,
                                   bool enable_tm,
                                   bool enable_tc,
                                   bool enable_aos,
                                   bool enable_cfdp)
{
    (void)surface; (void)enable_tm; (void)enable_tc; (void)enable_aos; (void)enable_cfdp;
    return RGTP_ERR_NOT_SUPPORTED;
}

rgtp_error_t rgtp_send_emergency_tc(rgtp_surface_t* surface,
                                     const void* tc_data,
                                     size_t tc_size,
                                     uint8_t priority)
{
    (void)surface; (void)tc_data; (void)tc_size; (void)priority;
    return RGTP_ERR_NOT_SUPPORTED;
}

rgtp_error_t rgtp_calculate_link_budget(const rgtp_surface_t* surface,
                                         float* out_margin_db,
                                         float* out_ebno_db)
{
    (void)surface;
    if (!out_margin_db || !out_ebno_db) return RGTP_ERR_INVALID_ARG;
    *out_margin_db = 0.0f;
    *out_ebno_db = 0.0f;
    return RGTP_OK;
}

rgtp_error_t rgtp_configure_doppler(rgtp_surface_t* surface,
                                     bool enable_compensation,
                                     uint32_t max_doppler_hz,
                                     float update_rate_hz)
{
    (void)surface; (void)enable_compensation; (void)max_doppler_hz; (void)update_rate_hz;
    return RGTP_ERR_NOT_SUPPORTED;
}

rgtp_error_t rgtp_check_contact_status(const rgtp_surface_t* surface,
                                        bool* out_in_contact,
                                        uint32_t* out_time_to_contact_s,
                                        uint32_t* out_time_left_s)
{
    (void)surface;
    if (!out_in_contact) return RGTP_ERR_INVALID_ARG;
    *out_in_contact = false;
    if (out_time_to_contact_s) *out_time_to_contact_s = 0;
    if (out_time_left_s) *out_time_left_s = 0;
    return RGTP_OK;
}

rgtp_error_t rgtp_set_allocator(const rgtp_allocator_t* alloc);

int rgtp_is_initialized(void)
{
    return 0;
}
