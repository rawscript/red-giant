/**
 * @file rgtp_alloc.c
 * @brief Custom allocator hook and internal malloc/free wrappers.
 *
 * All heap allocations inside librgtp go through rgtp_malloc / rgtp_calloc /
 * rgtp_free.  A caller may replace the underlying allocator at startup via
 * rgtp_set_allocator().  The global hook is protected by a once-init guard so
 * it is safe to call from multiple threads.
 *
 * Requirements: 2.1, 16.2, 16.5
 */

#include "rgtp/rgtp.h"
#include "rgtp_alloc_internal.h"

#include <stdlib.h>
#include <string.h>   /* memset */
#include <stdint.h>

/* ── Platform once-init ─────────────────────────────────────────────────── */
#ifdef _WIN32
#  include <windows.h>
   typedef INIT_ONCE  rgtp_once_t;
#  define RGTP_ONCE_INIT INIT_ONCE_STATIC_INIT
   static BOOL CALLBACK _once_cb(PINIT_ONCE o, PVOID p, PVOID* c)
   {
       (void)o; (void)p; (void)c;
       return TRUE;
   }
#  define RGTP_ONCE_DO(once, fn) InitOnceExecuteOnce(&(once), _once_cb, NULL, NULL)
#else
#  include <pthread.h>
   typedef pthread_once_t rgtp_once_t;
#  define RGTP_ONCE_INIT   PTHREAD_ONCE_INIT
#  define RGTP_ONCE_DO(once, fn) pthread_once(&(once), (fn))
#endif

/* ── Global allocator state ─────────────────────────────────────────────── */

static rgtp_allocator_t g_alloc = { NULL, NULL, NULL };

/* ── Public API ─────────────────────────────────────────────────────────── */

rgtp_error_t rgtp_set_allocator(const rgtp_allocator_t* alloc)
{
    if (alloc == NULL) {
        /* Restore defaults */
        g_alloc.alloc = NULL;
        g_alloc.free  = NULL;
        g_alloc.ctx   = NULL;
        return RGTP_OK;
    }

    /* Both pointers must be set together, or both NULL */
    if ((alloc->alloc == NULL) != (alloc->free == NULL)) {
        return RGTP_ERR_INVALID_ARG;
    }

    g_alloc = *alloc;
    return RGTP_OK;
}

/* ── Internal wrappers ──────────────────────────────────────────────────── */

void* rgtp_malloc(size_t size)
{
    if (size == 0) {
        return NULL;
    }

    if (g_alloc.alloc != NULL) {
        return g_alloc.alloc(size, g_alloc.ctx);
    }

#if defined(RGTP_MEMORY_PROFILE_EMBEDDED)
    /*
     * In EMBEDDED profile without a custom allocator we still fall through to
     * system malloc, but callers are expected to supply an arena allocator via
     * rgtp_set_allocator().  The size guard below enforces the embedded limit.
     */
    if (size > (16u * 1024u * 1024u)) {
        return NULL;   /* Enforce 16 MB max allocation in EMBEDDED profile */
    }
#endif

    return malloc(size);
}

void* rgtp_calloc(size_t nmemb, size_t size)
{
    if (nmemb == 0 || size == 0) {
        return NULL;
    }

    /* Overflow check: nmemb * size must not wrap */
    if (size > SIZE_MAX / nmemb) {
        return NULL;
    }

    size_t total = nmemb * size;

    if (g_alloc.alloc != NULL) {
        void* p = g_alloc.alloc(total, g_alloc.ctx);
        if (p != NULL) {
            memset(p, 0, total);
        }
        return p;
    }

#if defined(RGTP_MEMORY_PROFILE_EMBEDDED)
    if (total > (16u * 1024u * 1024u)) {
        return NULL;
    }
#endif

    return calloc(nmemb, size);
}

void rgtp_free(void* ptr)
{
    if (ptr == NULL) {
        return;
    }

    if (g_alloc.free != NULL) {
        g_alloc.free(ptr, g_alloc.ctx);
        return;
    }

    free(ptr);
}
