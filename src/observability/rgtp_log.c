/**
 * @file rgtp_log.c
 * @brief Structured logging subsystem.
 *
 * Provides a global log callback and level filter. All RGTP modules emit
 * structured rgtp_log_event_t events via RGTP_LOG() macro.
 *
 * Default output: stderr in a human-readable format.
 * Custom output: register via rgtp_set_log_callback().
 *
 * Requirements: 13.3, 13.4, 13.5, 13.6
 */

#include "rgtp/rgtp.h"
#include "rgtp_log_internal.h"

#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <stdatomic.h>

#ifdef _WIN32
#  include <windows.h>
#else
#  include <time.h>
#endif

/* ── Global state ───────────────────────────────────────────────────────── */

static rgtp_log_fn  g_log_fn  = NULL;
static void*        g_log_ctx = NULL;
static _Atomic int  g_log_level = RGTP_LOG_WARN;   /* default: WARN and above */

/* ── Default stderr formatter ───────────────────────────────────────────── */

static const char* level_str(int level)
{
    switch (level) {
    case RGTP_LOG_ERROR: return "ERROR";
    case RGTP_LOG_WARN:  return "WARN ";
    case RGTP_LOG_INFO:  return "INFO ";
    case RGTP_LOG_DEBUG: return "DEBUG";
    default:             return "?????";
    }
}

static void default_log_fn(const rgtp_log_event_t* event, void* ctx)
{
    (void)ctx;
    fprintf(stderr, "[RGTP %s] %s: %s",
            level_str(event->level),
            event->component ? event->component : "rgtp",
            event->message   ? event->message   : "");
    for (int i = 0; i < event->kv_count && i < 8; i++) {
        fprintf(stderr, " %s=%s",
                event->kv[i].key   ? event->kv[i].key   : "?",
                event->kv[i].value ? event->kv[i].value : "");
    }
    fprintf(stderr, "\n");
}

/* ── Timestamp helper ───────────────────────────────────────────────────── */

static uint64_t log_timestamp_ns(void)
{
#ifdef _WIN32
    FILETIME ft;
    GetSystemTimeAsFileTime(&ft);
    uint64_t t = ((uint64_t)ft.dwHighDateTime << 32) | ft.dwLowDateTime;
    return t * 100u;   /* 100ns → ns */
#else
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return (uint64_t)ts.tv_sec * 1000000000u + (uint64_t)ts.tv_nsec;
#endif
}

/* ── Public API ─────────────────────────────────────────────────────────── */

void rgtp_set_log_callback(rgtp_log_fn fn, void* ctx)
{
    g_log_fn  = fn;
    g_log_ctx = ctx;
}

void rgtp_set_log_level(int level)
{
    atomic_store(&g_log_level, level);
}

/* ── Internal emit function ─────────────────────────────────────────────── */

void rgtp_log_emit(int level, const char* component, const char* message,
                   const rgtp_log_kv_t* kvs, int kv_count)
{
    if (level > atomic_load(&g_log_level)) return;

    rgtp_log_event_t event;
    memset(&event, 0, sizeof(event));
    event.timestamp_ns = log_timestamp_ns();
    event.level        = level;
    event.component    = component;
    event.message      = message;

    int n = (kv_count < 8) ? kv_count : 8;
    for (int i = 0; i < n; i++) {
        event.kv[i].key   = kvs[i].key;
        event.kv[i].value = kvs[i].value;
    }
    event.kv_count = n;

    rgtp_log_fn fn = g_log_fn ? g_log_fn : default_log_fn;
    fn(&event, g_log_ctx);
}
