// Red Giant Protocol - High-Performance C Core Implementation
#include "red_giant.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <stdatomic.h>

#ifdef _WIN32
#include <windows.h>
#include <malloc.h>
#define aligned_alloc(alignment, size) _aligned_malloc(size, alignment)
#define aligned_free(ptr) _aligned_free(ptr)
#else
#include <sys/time.h>
#define aligned_free(ptr) free(ptr)
#endif

// High-resolution timer
static uint64_t get_timestamp_ns() {
#ifdef _WIN32
    LARGE_INTEGER freq, counter;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&counter);
    return (counter.QuadPart * 1000000000) / freq.QuadPart;
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000000000ULL + ts.tv_nsec;
#endif
}

rg_exposure_surface_t* rg_create_surface(const rg_manifest_t* manifest) {
    rg_exposure_surface_t* surface = aligned_alloc(RG_CACHE_LINE_SIZE, sizeof(rg_exposure_surface_t));
    if (!surface) return NULL;
    
    memset(surface, 0, sizeof(rg_exposure_surface_t));
    
    // Copy manifest
    memcpy(&surface->manifest, manifest, sizeof(rg_manifest_t));
    
    // Allocate cache-aligned chunk array
    size_t chunks_size = manifest->total_chunks * sizeof(rg_chunk_t);
    surface->chunks = aligned_alloc(RG_CACHE_LINE_SIZE, chunks_size);
    if (!surface->chunks) {
        aligned_free(surface);
        return NULL;
    }
    memset(surface->chunks, 0, chunks_size);
    
    // Initialize chunks with optimized layout
    for (uint32_t i = 0; i < manifest->total_chunks; i++) {
        surface->chunks[i].sequence_id = i;
        surface->chunks[i].offset = (uint64_t)i * manifest->chunk_size;
        surface->chunks[i].data_size = (i == manifest->total_chunks - 1) ? 
            (uint32_t)(manifest->total_size % manifest->chunk_size) : manifest->chunk_size;
        surface->chunks[i].is_exposed = false;
        surface->chunks[i].pull_count = 0;
    }
    
    // Allocate large memory pool for high-performance chunk storage
    surface->pool_size = manifest->total_chunks * manifest->chunk_size;
    surface->memory_pool = aligned_alloc(RG_CACHE_LINE_SIZE, surface->pool_size);
    if (!surface->memory_pool) {
        aligned_free(surface->chunks);
        aligned_free(surface);
        return NULL;
    }
    
    // Allocate shared buffer (larger for better performance)
    surface->buffer_size = manifest->chunk_size * 8; // 8x buffer for concurrent access
    surface->shared_buffer = aligned_alloc(RG_CACHE_LINE_SIZE, surface->buffer_size);
    if (!surface->shared_buffer) {
        aligned_free(surface->memory_pool);
        aligned_free(surface->chunks);
        aligned_free(surface);
        return NULL;
    }
    
    // Initialize free slot tracking
    surface->free_slots = malloc(manifest->total_chunks * sizeof(uint32_t));
    for (uint32_t i = 0; i < manifest->total_chunks; i++) {
        surface->free_slots[i] = i;
    }
    surface->free_slot_count = manifest->total_chunks;
    
    surface->start_time = get_timestamp_ns();
    
    printf("[PERF] High-performance surface created: %u chunks, %zu MB pool\n", 
           manifest->total_chunks, surface->pool_size / (1024*1024));
    
    return surface;
}

void rg_destroy_surface(rg_exposure_surface_t* surface) {
    if (!surface) return;
    
    aligned_free(surface->chunks);
    aligned_free(surface->shared_buffer);
    aligned_free(surface->memory_pool);
    free(surface->free_slots);
    aligned_free(surface);
}

// High-performance chunk exposure with memory pool
bool rg_expose_chunk_fast(rg_exposure_surface_t* surface, uint32_t chunk_id, const void* data, uint32_t size) {
    if (!surface || chunk_id >= surface->manifest.total_chunks) return false;
    
    rg_chunk_t* chunk = &surface->chunks[chunk_id];
    
    // Use memory pool for optimal cache performance
    chunk->data_ptr = (uint8_t*)surface->memory_pool + (chunk_id * surface->manifest.chunk_size);
    
    // Fast memory copy (compiler will optimize to SIMD if possible)
    memcpy(chunk->data_ptr, data, size);
    
    // Update chunk metadata
    chunk->data_size = size;
    chunk->exposure_timestamp = get_timestamp_ns();
    
    // Atomic update for thread safety
    __atomic_store_n(&chunk->is_exposed, true, __ATOMIC_RELEASE);
    __atomic_fetch_add(&surface->exposed_count, 1, __ATOMIC_ACQ_REL);
    __atomic_fetch_add(&surface->total_bytes_exposed, size, __ATOMIC_RELAXED);
    
    return true;
}

// Fast chunk peek with atomic read
const rg_chunk_t* rg_peek_chunk_fast(rg_exposure_surface_t* surface, uint32_t chunk_id) {
    if (!surface || chunk_id >= surface->manifest.total_chunks) return NULL;
    
    rg_chunk_t* chunk = &surface->chunks[chunk_id];
    return __atomic_load_n(&chunk->is_exposed, __ATOMIC_ACQUIRE) ? chunk : NULL;
}

// High-performance chunk pull with statistics
bool rg_pull_chunk_fast(rg_exposure_surface_t* surface, uint32_t chunk_id, void* dest_buffer, uint32_t* size) {
    const rg_chunk_t* chunk = rg_peek_chunk_fast(surface, chunk_id);
    if (!chunk) return false;
    
    // Fast memory copy
    memcpy(dest_buffer, chunk->data_ptr, chunk->data_size);
    *size = chunk->data_size;
    
    // Update pull statistics atomically
    __atomic_fetch_add((uint32_t*)&chunk->pull_count, 1, __ATOMIC_RELAXED);
    
    return true;
}

void rg_raise_red_flag(rg_exposure_surface_t* surface) {
    if (!surface) return;
    surface->red_flag_raised = true;
    printf("[RED FLAG] 🚩 All chunks exposed - transmission complete!\n");
}

bool rg_is_complete(const rg_exposure_surface_t* surface) {
    return surface && surface->red_flag_raised && 
           surface->exposed_count == surface->manifest.total_chunks;
}
// Batch exposure for maximum throughput
uint32_t rg_expose_batch(rg_exposure_surface_t* surface, uint32_t start_chunk, uint32_t count, const void** data_ptrs, uint32_t* sizes) {
    if (!surface || start_chunk + count > surface->manifest.total_chunks) return 0;
    
    uint32_t exposed = 0;
    for (uint32_t i = 0; i < count; i++) {
        if (rg_expose_chunk_fast(surface, start_chunk + i, data_ptrs[i], sizes[i])) {
            exposed++;
        }
    }
    
    printf("[BATCH_EXPOSE] %u chunks exposed in batch\n", exposed);
    return exposed;
}

// Batch pull for maximum throughput
uint32_t rg_pull_batch(rg_exposure_surface_t* surface, uint32_t start_chunk, uint32_t count, void** dest_buffers, uint32_t* sizes) {
    if (!surface || start_chunk + count > surface->manifest.total_chunks) return 0;
    
    uint32_t pulled = 0;
    for (uint32_t i = 0; i < count; i++) {
        if (rg_pull_chunk_fast(surface, start_chunk + i, dest_buffers[i], &sizes[i])) {
            pulled++;
        }
    }
    
    return pulled;
}

// Performance statistics
uint64_t rg_get_performance_stats(const rg_exposure_surface_t* surface, uint32_t* throughput_mbps) {
    if (!surface) return 0;
    
    uint64_t elapsed_ns = get_timestamp_ns() - surface->start_time;
    uint64_t elapsed_ms = elapsed_ns / 1000000;
    
    if (elapsed_ms > 0) {
        uint64_t bytes_per_sec = (surface->total_bytes_exposed * 1000) / elapsed_ms;
        *throughput_mbps = (uint32_t)(bytes_per_sec / (1024 * 1024));
    } else {
        *throughput_mbps = 0;
    }
    
    return elapsed_ms;
}

// Memory alignment helpers
void* rg_alloc_aligned(size_t size, size_t alignment) {
    return aligned_alloc(alignment, size);
}

void rg_free_aligned(void* ptr) {
    aligned_free(ptr);
}
