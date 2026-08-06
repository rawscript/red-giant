/**
 * @file rgtp_alloc.c
 * @brief Custom allocator support.
 */

#include "rgtp/rgtp.h"
#include "rgtp_alloc_internal.h"
#include <stdlib.h>
#include <string.h>

/* Default allocator uses malloc/free */
static rgtp_allocator_t s_default_allocator = {
    .alloc = NULL,
    .free = NULL,
    .ctx = NULL
};

static rgtp_allocator_t* s_allocator = &s_default_allocator;

rgtp_error_t rgtp_set_allocator(const rgtp_allocator_t* alloc)
{
    if (alloc == NULL) {
        s_allocator = &s_default_allocator;
        return RGTP_OK;
    }

    if (alloc->alloc == NULL || alloc->free == NULL) {
        return RGTP_ERR_INVALID_ARG;
    }

    s_allocator = (rgtp_allocator_t*)alloc;
    return RGTP_OK;
}

void* rgtp_malloc(size_t size)
{
    if (s_allocator->alloc) {
        return s_allocator->alloc(size, s_allocator->ctx);
    }
    return malloc(size);
}

void* rgtp_calloc(size_t nmemb, size_t size)
{
    void* ptr = rgtp_malloc(nmemb * size);
    if (ptr) {
        memset(ptr, 0, nmemb * size);
    }
    return ptr;
}

void rgtp_free(void* ptr)
{
    if (s_allocator->free) {
        s_allocator->free(ptr, s_allocator->ctx);
    } else {
        free(ptr);
    }
}