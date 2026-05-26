/**
 * @file rgtp_otel_internal.h
 * @brief Internal OpenTelemetry span API.
 */

#ifndef RGTP_OTEL_INTERNAL_H
#define RGTP_OTEL_INTERNAL_H

#include <stdint.h>
#include <stddef.h>

#ifdef RGTP_ENABLE_OTEL
#  include <stdlib.h>   /* malloc/free for span allocation */
#  ifdef _WIN32
#    include <windows.h>
#  else
#    include <time.h>
#  endif
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef void* rgtp_span_handle_t;

rgtp_span_handle_t rgtp_otel_span_start(const char*    name,
                                         const uint8_t  trace_id[16]);
void               rgtp_otel_span_end(rgtp_span_handle_t handle);
void               rgtp_otel_span_set_attr(rgtp_span_handle_t handle,
                                            const char* key,
                                            const char* value);

/* Convenience macros */
#define RGTP_SPAN_START(name, tid) rgtp_otel_span_start((name), (tid))
#define RGTP_SPAN_END(h)           rgtp_otel_span_end((h))
#define RGTP_SPAN_ATTR(h, k, v)    rgtp_otel_span_set_attr((h), (k), (v))

#ifdef __cplusplus
}
#endif

#endif /* RGTP_OTEL_INTERNAL_H */
