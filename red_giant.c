
// Red Giant Protocol - High-Performance C Core Implementation
#include "red_giant.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <stdatomic.h>
#include <errno.h>

#ifdef _WIN32
#include <windows.h>
#include <malloc.h>
#define aligned_alloc(alignment, size) _aligned_malloc(size, alignment)
#define aligned_free(ptr) _aligned_free(ptr)
#else
#include <sys/time.h>
#include <unistd.h>
#define aligned_free(ptr) free(ptr)
#endif

// High-resolution timer with error handling
static uint64_t get_timestamp_ns() {
#ifdef _WIN32
    LARGE_INTEGER freq, counter;
    if (!QueryPerformanceFrequency(&freq) || !QueryPerformanceCounter(&counter)) {
        return 0;
    }
    return (counter.QuadPart * 1000000000ULL) / freq.QuadPart;
#else
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
        return 0;
    }
    return ts.tv_sec * 1000000000ULL + ts.tv_nsec;
#endif
}

// Safe aligned allocation with fallback
static void* safe_aligned_alloc(size_t alignment, size_t size) {
    if (size == 0) return NULL;
    
    void* ptr = aligned_alloc(alignment, size);
    if (!ptr && alignment > sizeof(void*)) {
        // Fallback to regular malloc if aligned_alloc fails
        ptr = malloc(size);
    }
    return ptr;
}

// Validate manifest parameters
static bool validate_manifest(const rg_manifest_t* manifest) {
    return manifest && 
           manifest->total_size > 0 && 
           manifest->chunk_size > 0 && 
           manifest->chunk_size <= RG_MAX_CHUNK_SIZE &&
           manifest->total_chunks > 0 &&
           manifest->total_chunks <= RG_MAX_CONCURRENT_CHUNKS;
}

rg_exposure_surface_t* rg_create_surface(const rg_manifest_t* manifest) {
    if (!validate_manifest(manifest)) {
        fprintf(stderr, "[ERROR] Invalid manifest parameters\n");
        return NULL;
    }
    
    rg_exposure_surface_t* surface = safe_aligned_alloc(RG_CACHE_LINE_SIZE, sizeof(rg_exposure_surface_t));
    if (!surface) {
        fprintf(stderr, "[ERROR] Failed to allocate surface memory\n");
        return NULL;
    }
    
    memset(surface, 0, sizeof(rg_exposure_surface_t));
    memcpy(&surface->manifest, manifest, sizeof(rg_manifest_t));
    
    // Allocate chunks array
    size_t chunks_size = manifest->total_chunks * sizeof(rg_chunk_t);
    surface->chunks = safe_aligned_alloc(RG_CACHE_LINE_SIZE, chunks_size);
    if (!surface->chunks) {
        fprintf(stderr, "[ERROR] Failed to allocate chunks memory\n");
        aligned_free(surface);
        return NULL;
    }
    memset(surface->chunks, 0, chunks_size);
    
    // Initialize chunks
    for (uint32_t i = 0; i < manifest->total_chunks; i++) {
        surface->chunks[i].sequence_id = i;
        surface->chunks[i].offset = (uint64_t)i * manifest->chunk_size;
        surface->chunks[i].data_size = (i == manifest->total_chunks - 1) ? 
            (uint32_t)(manifest->total_size % manifest->chunk_size == 0 ? 
                      manifest->chunk_size : manifest->total_size % manifest->chunk_size) : 
            manifest->chunk_size;
        atomic_store(&surface->chunks[i].is_exposed, false);
        surface->chunks[i].pull_count = 0;
    }
    
    // Allocate memory pool
    surface->pool_size = manifest->total_chunks * manifest->chunk_size;
    surface->memory_pool = safe_aligned_alloc(RG_CACHE_LINE_SIZE, surface->pool_size);
    if (!surface->memory_pool) {
        fprintf(stderr, "[ERROR] Failed to allocate memory pool\n");
        aligned_free(surface->chunks);
        aligned_free(surface);
        return NULL;
    }
    
    // Allocate shared buffer
    surface->buffer_size = manifest->chunk_size * 8;
    surface->shared_buffer = safe_aligned_alloc(RG_CACHE_LINE_SIZE, surface->buffer_size);
    if (!surface->shared_buffer) {
        fprintf(stderr, "[ERROR] Failed to allocate shared buffer\n");
        aligned_free(surface->memory_pool);
        aligned_free(surface->chunks);
        aligned_free(surface);
        return NULL;
    }
    
    // Initialize free slot tracking
    surface->free_slots = malloc(manifest->total_chunks * sizeof(uint32_t));
    if (!surface->free_slots) {
        fprintf(stderr, "[ERROR] Failed to allocate free slots tracking\n");
        aligned_free(surface->shared_buffer);
        aligned_free(surface->memory_pool);
        aligned_free(surface->chunks);
        aligned_free(surface);
        return NULL;
    }
    
    for (uint32_t i = 0; i < manifest->total_chunks; i++) {
        surface->free_slots[i] = i;
    }
    surface->free_slot_count = manifest->total_chunks;
    
    atomic_store(&surface->exposed_count, 0);
    atomic_store(&surface->red_flag_raised, false);
    surface->start_time = get_timestamp_ns();
    
    printf("[SUCCESS] Red Giant surface created: %u chunks, %zu MB pool\n", 
           manifest->total_chunks, surface->pool_size / (1024*1024));
    
    return surface;
}

void rg_destroy_surface(rg_exposure_surface_t* surface) {
    if (!surface) return;
    
    if (surface->chunks) aligned_free(surface->chunks);
    if (surface->shared_buffer) aligned_free(surface->shared_buffer);
    if (surface->memory_pool) aligned_free(surface->memory_pool);
    if (surface->free_slots) free(surface->free_slots);
    aligned_free(surface);
}

bool rg_expose_chunk_fast(rg_exposure_surface_t* surface, uint32_t chunk_id, const void* data, uint32_t size) {
    if (!surface || !data || chunk_id >= surface->manifest.total_chunks || size == 0) {
        return false;
    }
    
    rg_chunk_t* chunk = &surface->chunks[chunk_id];
    
    // Check if already exposed
    if (atomic_load(&chunk->is_exposed)) {
        return true; // Already exposed, not an error
    }
    
    // Validate size
    if (size > surface->manifest.chunk_size) {
        fprintf(stderr, "[ERROR] Chunk %u size %u exceeds maximum %u\n", 
                chunk_id, size, surface->manifest.chunk_size);
        return false;
    }
    
    // Use memory pool for optimal performance
    chunk->data_ptr = (uint8_t*)surface->memory_pool + ((uint64_t)chunk_id * surface->manifest.chunk_size);
    
    // Safe memory copy
    memcpy(chunk->data_ptr, data, size);
    chunk->data_size = size;
    chunk->exposure_timestamp = get_timestamp_ns();
    
    // Atomic update for thread safety
    atomic_store(&chunk->is_exposed, true);
    atomic_fetch_add(&surface->exposed_count, 1);
    atomic_fetch_add(&surface->total_bytes_exposed, size);
    
    return true;
}

const rg_chunk_t* rg_peek_chunk_fast(rg_exposure_surface_t* surface, uint32_t chunk_id) {
    if (!surface || chunk_id >= surface->manifest.total_chunks) {
        return NULL;
    }
    
    rg_chunk_t* chunk = &surface->chunks[chunk_id];
    return atomic_load(&chunk->is_exposed) ? chunk : NULL;
}

bool rg_pull_chunk_fast(rg_exposure_surface_t* surface, uint32_t chunk_id, void* dest_buffer, uint32_t* size) {
    if (!surface || !dest_buffer || !size) {
        return false;
    }
    
    const rg_chunk_t* chunk = rg_peek_chunk_fast(surface, chunk_id);
    if (!chunk) {
        return false;
    }
    
    // Safe memory copy
    memcpy(dest_buffer, chunk->data_ptr, chunk->data_size);
    *size = chunk->data_size;
    
    // Update pull statistics atomically
    atomic_fetch_add((volatile uint32_t*)&chunk->pull_count, 1);
    
    return true;
}

void rg_raise_red_flag(rg_exposure_surface_t* surface) {
    if (!surface) return;
    
    atomic_store(&surface->red_flag_raised, true);
    printf("[RED FLAG] 🚩 All chunks exposed - transmission complete!\n");
}

bool rg_is_complete(const rg_exposure_surface_t* surface) {
    if (!surface) return false;
    
    return atomic_load(&surface->red_flag_raised) && 
           atomic_load(&surface->exposed_count) == surface->manifest.total_chunks;
}

uint32_t rg_expose_batch(rg_exposure_surface_t* surface, uint32_t start_chunk, uint32_t count, const void** data_ptrs, uint32_t* sizes) {
    if (!surface || !data_ptrs || !sizes || 
        start_chunk + count > surface->manifest.total_chunks) {
        return 0;
    }
    
    uint32_t exposed = 0;
    for (uint32_t i = 0; i < count; i++) {
        if (data_ptrs[i] && rg_expose_chunk_fast(surface, start_chunk + i, data_ptrs[i], sizes[i])) {
            exposed++;
        }
    }
    
    return exposed;
}

uint32_t rg_pull_batch(rg_exposure_surface_t* surface, uint32_t start_chunk, uint32_t count, void** dest_buffers, uint32_t* sizes) {
    if (!surface || !dest_buffers || !sizes || 
        start_chunk + count > surface->manifest.total_chunks) {
        return 0;
    }
    
    uint32_t pulled = 0;
    for (uint32_t i = 0; i < count; i++) {
        if (dest_buffers[i] && rg_pull_chunk_fast(surface, start_chunk + i, dest_buffers[i], &sizes[i])) {
            pulled++;
        }
    }
    
    return pulled;
}

uint64_t rg_get_performance_stats(const rg_exposure_surface_t* surface, uint32_t* throughput_mbps) {
    if (!surface || !throughput_mbps) {
        if (throughput_mbps) *throughput_mbps = 0;
        return 0;
    }
    
    uint64_t current_time = get_timestamp_ns();
    if (current_time < surface->start_time) {
        *throughput_mbps = 0;
        return 0;
    }
    
    uint64_t elapsed_ns = current_time - surface->start_time;
    uint64_t elapsed_ms = elapsed_ns / 1000000;
    
    if (elapsed_ms > 0) {
        uint64_t bytes_per_sec = (atomic_load(&surface->total_bytes_exposed) * 1000) / elapsed_ms;
        *throughput_mbps = (uint32_t)(bytes_per_sec / (1024 * 1024));
    } else {
        *throughput_mbps = 0;
    }
    
    return elapsed_ms;
}

void* rg_alloc_aligned(size_t size, size_t alignment) {
    return safe_aligned_alloc(alignment, size);
}

void rg_free_aligned(void* ptr) {
    if (ptr) aligned_free(ptr);
}
