/**
 * @file rgtp_init_threadsafe.c
 * @brief Thread-safe library initialization and cleanup.
 *
 * This file implements thread-safe initialization patterns for RGTP.
 * All global state access is now protected by mutexes.
 */

#include "rgtp/rgtp.h"
#include <stdatomic.h>
#include <string.h>

/* ── Platform-specific includes ─────────────────────────────────────────── */
#ifdef _WIN32
#  include <windows.h>
#else
#  include <pthread.h>
#endif

/* ── Forward declarations ──────────────────────────────────────────────── */
#if defined(RGTP_ENABLE_FEC)
extern void gf256_init(void);
#endif

/* ── Once-init guard ───────────────────────────────────────────────────── */
#ifdef _WIN32
static INIT_ONCE s_init_once = INIT_ONCE_STATIC_INIT;
#else
static pthread_once_t s_init_once = PTHREAD_ONCE_INIT;
#endif

/* ── Initialization state ──────────────────────────────────────────────── */
static _Atomic int s_init_result = RGTP_OK;
static _Atomic int s_initialised = 0;

/* ── Cleanup registration ──────────────────────────────────────────────── */
#ifdef _WIN32
static_atomics.h
#endif