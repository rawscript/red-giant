/**
 * @file rgtp_log_internal.h
 * @brief Internal logging helpers used by all RGTP modules.
 */

#ifndef RGTP_LOG_INTERNAL_H
#define RGTP_LOG_INTERNAL_H

#include "rgtp/rgtp.h"
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Key-value pair for structured log events. */
typedef struct {
    const char* key;
    const char* value;
} rgtp_log_kv_t;

/**
 * @brief Emit a structured log event.
 *
 * @param level      RGTP_LOG_ERROR / WARN / INFO / DEBUG
 * @param component  Module name string, e.g. "rgtp.crypto"
 * @param message    Human-readable summary
 * @param kvs        Array of key-value pairs (may be NULL)
 * @param kv_count   Number of entries in @p kvs (max 8)
 */
void rgtp_log_emit(int level, const char* component, const char* message,
                   const rgtp_log_kv_t* kvs, int kv_count);

/* ── Convenience macros ─────────────────────────────────────────────────── */

#define RGTP_LOG0(level, comp, msg) \
    rgtp_log_emit((level), (comp), (msg), NULL, 0)

#define RGTP_LOG1(level, comp, msg, k1, v1) \
    do { \
        rgtp_log_kv_t _kv[] = {{(k1),(v1)}}; \
        rgtp_log_emit((level), (comp), (msg), _kv, 1); \
    } while(0)

#define RGTP_LOG2(level, comp, msg, k1, v1, k2, v2) \
    do { \
        rgtp_log_kv_t _kv[] = {{(k1),(v1)},{(k2),(v2)}}; \
        rgtp_log_emit((level), (comp), (msg), _kv, 2); \
    } while(0)

#define RGTP_LOG3(level, comp, msg, k1, v1, k2, v2, k3, v3) \
    do { \
        rgtp_log_kv_t _kv[] = {{(k1),(v1)},{(k2),(v2)},{(k3),(v3)}}; \
        rgtp_log_emit((level), (comp), (msg), _kv, 3); \
    } while(0)

#ifdef __cplusplus
}
#endif

#endif /* RGTP_LOG_INTERNAL_H */
