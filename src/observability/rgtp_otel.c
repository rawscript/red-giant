/**
 * @file rgtp_otel.c
 * @brief OpenTelemetry span emission (guarded by RGTP_ENABLE_OTEL).
 *
 * When RGTP_ENABLE_OTEL is defined, emits trace spans for key operations:
 *   - exposure creation
 *   - chunk encryption
 *   - Merkle tree construction
 *   - pull request handling
 *   - chunk transmission
 *   - FEC encoding/decoding
 *   - surface destruction
 *
 * When RGTP_ENABLE_OTEL is not defined, all functions are no-ops.
 *
 * Requirements: 13.2
 */

#include "rgtp_otel_internal.h"

#ifdef RGTP_ENABLE_OTEL

#include <string.h>
#include <stdio.h>

/* ── Span context ───────────────────────────────────────────────────────── */

typedef struct {
    char     name[64];
    uint64_t start_ns;
    uint64_t trace_id[2];
    uint64_t span_id;
} rgtp_span_t;

static uint64_t otel_now_ns(void)
{
#ifdef _WIN32
    FILETIME ft;
    GetSystemTimeAsFileTime(&ft);
    uint64_t t = ((uint64_t)ft.dwHighDateTime << 32) | ft.dwLowDateTime;
    return t * 100u;
#else
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return (uint64_t)ts.tv_sec * 1000000000u + (uint64_t)ts.tv_nsec;
#endif
}

/* ── Span lifecycle ─────────────────────────────────────────────────────── */

rgtp_span_handle_t rgtp_otel_span_start(const char* name,
                                         const uint8_t trace_id[16])
{
    rgtp_span_t* span = (rgtp_span_t*)malloc(sizeof(rgtp_span_t));
    if (!span) return NULL;

    memset(span, 0, sizeof(*span));
    snprintf(span->name, sizeof(span->name), "%s", name ? name : "unknown");
    span->start_ns = otel_now_ns();
    if (trace_id) {
        memcpy(span->trace_id, trace_id, 16);
    }
    /* In a real implementation, span_id would be a CSPRNG 64-bit value */
    span->span_id = span->start_ns ^ (uint64_t)(uintptr_t)span;

    return (rgtp_span_handle_t)span;
}

void rgtp_otel_span_end(rgtp_span_handle_t handle)
{
    if (!handle) return;
    rgtp_span_t* span = (rgtp_span_t*)handle;
    uint64_t duration_ns = otel_now_ns() - span->start_ns;

    /* In a real implementation, this would export to an OTLP collector.
     * For now, emit to stderr when RGTP_OTEL_DEBUG is set. */
#ifdef RGTP_OTEL_DEBUG
    fprintf(stderr, "[OTEL] span=%s trace=%016llx%016llx span_id=%016llx duration_ns=%llu\n",
            span->name,
            (unsigned long long)span->trace_id[0],
            (unsigned long long)span->trace_id[1],
            (unsigned long long)span->span_id,
            (unsigned long long)duration_ns);
#else
    (void)duration_ns;
#endif

    free(span);
}

void rgtp_otel_span_set_attr(rgtp_span_handle_t handle,
                              const char* key, const char* value)
{
    /* In a real implementation, attach key-value attributes to the span. */
    (void)handle; (void)key; (void)value;
}

#else /* RGTP_ENABLE_OTEL not defined — all no-ops */

rgtp_span_handle_t rgtp_otel_span_start(const char* name,
                                         const uint8_t trace_id[16])
{
    (void)name; (void)trace_id;
    return NULL;
}

void rgtp_otel_span_end(rgtp_span_handle_t handle)
{
    (void)handle;
}

void rgtp_otel_span_set_attr(rgtp_span_handle_t handle,
                              const char* key, const char* value)
{
    (void)handle; (void)key; (void)value;
}

#endif /* RGTP_ENABLE_OTEL */
