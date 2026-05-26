/**
 * @file rgtp_alloc_internal.h
 * @brief Internal allocator wrappers — used by all RGTP modules.
 *
 * All heap allocations inside librgtp go through these wrappers so that a
 * custom allocator registered via rgtp_set_allocator() is honoured globally.
 */

#ifndef RGTP_ALLOC_INTERNAL_H
#define RGTP_ALLOC_INTERNAL_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Allocate @p size bytes.  Returns NULL on failure.
 * Equivalent to malloc(size) when no custom allocator is set.
 */
void* rgtp_malloc(size_t size);

/**
 * @brief Allocate @p nmemb * @p size zero-initialised bytes.
 * Returns NULL on failure or on overflow of nmemb*size.
 */
void* rgtp_calloc(size_t nmemb, size_t size);

/**
 * @brief Free a block previously returned by rgtp_malloc / rgtp_calloc.
 * Safe to call with NULL.
 */
void  rgtp_free(void* ptr);

#ifdef __cplusplus
}
#endif

#endif /* RGTP_ALLOC_INTERNAL_H */
