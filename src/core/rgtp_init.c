/**
 * @file rgtp_init.c
 * @brief Library lifecycle: rgtp_init(), rgtp_cleanup(), rgtp_version().
 *
 * rgtp_init() must be called exactly once before any other RGTP function.
 * It is idempotent — subsequent calls return RGTP_OK without re-initialising.
 *
 * Responsibilities:
 *  1. Initialise the crypto backend (sodium_init / RAND_poll).
 *  2. Initialise Winsock on Windows (WSAStartup).
 *  3. Initialise GF(2^8) FEC tables (gf256_init) when FEC is enabled.
 *  4. Set runtime SIMD dispatch function pointers when SIMD is enabled.
 *
 * Requirements: 3.13, 8.4, 21.4
 */

#include "rgtp/rgtp.h"

#include <stdatomic.h>
#include <string.h>

/* ── Crypto backend headers ─────────────────────────────────────────────── */
#if defined(RGTP_CRYPTO_BACKEND_LIBSODIUM)
#  include <sodium.h>
#elif defined(RGTP_CRYPTO_BACKEND_OPENSSL)
#  include <openssl/rand.h>
#  include <openssl/err.h>
#endif

/* ── FEC forward declaration ────────────────────────────────────────────── */
#if defined(RGTP_ENABLE_FEC)
extern void gf256_init(void);
#endif

/* ── Platform once-init ─────────────────────────────────────────────────── */
#ifdef _WIN32
#  include <windows.h>
   static INIT_ONCE  s_once = INIT_ONCE_STATIC_INIT;
#else
#  include <pthread.h>
   static pthread_once_t s_once = PTHREAD_ONCE_INIT;
#endif

/* ── Initialisation state ───────────────────────────────────────────────── */
static _Atomic int s_init_result = RGTP_OK;   /* set during once-init */
static _Atomic int s_initialised = 0;

/* ── Internal once-init worker ──────────────────────────────────────────── */

static void do_init(void)
{
    int result = RGTP_OK;

#ifdef _WIN32
    /* Winsock initialisation */
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        result = RGTP_ERR_SOCKET;
        goto done;
    }
#endif

    /* Crypto backend initialisation */
#if defined(RGTP_CRYPTO_BACKEND_LIBSODIUM)
    if (sodium_init() == -1) {
        result = RGTP_ERR_CRYPTO_INIT;
        goto done;
    }
#elif defined(RGTP_CRYPTO_BACKEND_OPENSSL)
    /* RAND_poll seeds the OpenSSL PRNG; returns 1 on success */
    if (RAND_poll() != 1) {
        result = RGTP_ERR_CRYPTO_INIT;
        goto done;
    }
#endif

    /* FEC table initialisation */
#if defined(RGTP_ENABLE_FEC)
    gf256_init();
#endif

    /* SIMD dispatch — function pointers are set in rgtp_rs_simd.c at
     * module load time via constructor attributes; nothing to do here
     * unless a runtime CPUID check is needed on platforms without
     * __attribute__((constructor)) support. */

done:
    atomic_store(&s_init_result, result);
    atomic_store(&s_initialised, 1);
}

/* ── Platform once-init wrappers ────────────────────────────────────────── */

#ifdef _WIN32
static BOOL CALLBACK _once_cb(PINIT_ONCE o, PVOID p, PVOID* c)
{
    (void)o; (void)p; (void)c;
    do_init();
    return TRUE;
}
#endif

/* ── Public API ─────────────────────────────────────────────────────────── */

rgtp_error_t rgtp_init(void)
{
#ifdef _WIN32
    InitOnceExecuteOnce(&s_once, _once_cb, NULL, NULL);
#else
    pthread_once(&s_once, do_init);
#endif

    return (rgtp_error_t)atomic_load(&s_init_result);
}

void rgtp_cleanup(void)
{
    if (!atomic_load(&s_initialised)) {
        return;
    }

#ifdef _WIN32
    WSACleanup();
#endif

    /* Reset state so rgtp_init() can be called again if needed */
    atomic_store(&s_initialised, 0);
    atomic_store(&s_init_result, RGTP_OK);

    /* Re-arm the once guard — platform-specific */
#ifdef _WIN32
    {
        INIT_ONCE fresh = INIT_ONCE_STATIC_INIT;
        s_once = fresh;
    }
#else
    {
        pthread_once_t fresh = PTHREAD_ONCE_INIT;
        s_once = fresh;
    }
#endif
}

const char* rgtp_version(void)
{
    return RGTP_VERSION_STRING;
}
