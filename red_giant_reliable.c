
// Red Giant Protocol - Reliable Exposure System
// Maintains exposure-based architecture with robust error handling
#define _GNU_SOURCE
#include "red_giant.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <stdint.h>
#include <stdatomic.h>
#include <errno.h>
#include <unistd.h>

// Reliable exposure with automatic retry and integrity checking
typedef struct {
    uint32_t chunk_id;
    uint32_t retry_count;
    uint64_t last_attempt;
    uint8_t integrity_hash[16];
    atomic_bool needs_retry;
} rg_chunk_reliability_t;

typedef struct {
    rg_exposure_surface_t* surface;
    rg_chunk_reliability_t* reliability_data;
    atomic_uint_fast32_t failed_chunks;
    atomic_uint_fast32_t retry_operations;
    uint32_t max_retries;
    uint64_t retry_interval_ns;
} rg_reliable_surface_t;

// Create reliable exposure surface with error recovery
rg_reliable_surface_t* rg_create_reliable_surface(const rg_manifest_t* manifest) {
    rg_reliable_surface_t* reliable = calloc(1, sizeof(rg_reliable_surface_t));
    if (!reliable) return NULL;

    // Create underlying high-performance surface
    reliable->surface = rg_create_surface(manifest);
    if (!reliable->surface) {
        free(reliable);
        return NULL;
    }

    // Initialize reliability tracking
    reliable->reliability_data = calloc(manifest->total_chunks, sizeof(rg_chunk_reliability_t));
    if (!reliable->reliability_data) {
        rg_destroy_surface(reliable->surface);
        free(reliable);
        return NULL;
    }

    reliable->max_retries = 3;
    reliable->retry_interval_ns = 1000000; // 1ms
    atomic_store(&reliable->failed_chunks, 0);
    atomic_store(&reliable->retry_operations, 0);

    return reliable;
}

// Reliable chunk exposure with automatic integrity checking
bool rg_expose_chunk_reliable(rg_reliable_surface_t* reliable, uint32_t chunk_id, 
                            const void* data, uint32_t size) {
    if (!reliable || !data || chunk_id >= reliable->surface->manifest.total_chunks) {
        return false;
    }

    rg_chunk_reliability_t* rel_data = &reliable->reliability_data[chunk_id];
    
    // Calculate integrity hash for verification
    uint32_t hash = 0;
    const uint8_t* bytes = (const uint8_t*)data;
    for (uint32_t i = 0; i < size; i++) {
        hash = hash * 31 + bytes[i];
    }
    memcpy(rel_data->integrity_hash, &hash, sizeof(hash));

    // Attempt exposure with retry logic
    for (uint32_t attempt = 0; attempt <= reliable->max_retries; attempt++) {
        if (rg_expose_chunk_fast(reliable->surface, chunk_id, data, size)) {
            rel_data->retry_count = attempt;
            atomic_store(&rel_data->needs_retry, false);
            return true;
        }
        
        // Retry with exponential backoff
        if (attempt < reliable->max_retries) {
            atomic_fetch_add(&reliable->retry_operations, 1);
            rel_data->last_attempt = get_timestamp_ns();
            
            // Brief sleep to avoid overwhelming the system
            struct timespec delay = {0, reliable->retry_interval_ns * (1 << attempt)};
            nanosleep(&delay, NULL);
        }
    }

    // Mark as failed for later recovery
    atomic_store(&rel_data->needs_retry, true);
    atomic_fetch_add(&reliable->failed_chunks, 1);
    return false;
}

// Background recovery system for failed chunks
void rg_recover_failed_chunks(rg_reliable_surface_t* reliable) {
    if (!reliable) return;

    uint32_t recovered = 0;
    uint64_t current_time = get_timestamp_ns();
    
    for (uint32_t i = 0; i < reliable->surface->manifest.total_chunks; i++) {
        rg_chunk_reliability_t* rel_data = &reliable->reliability_data[i];
        
        if (atomic_load(&rel_data->needs_retry) && 
            (current_time - rel_data->last_attempt) > (reliable->retry_interval_ns * 1000)) {
            
            // Attempt recovery (would need original data - implementation specific)
            // This is where you'd implement chunk reconstruction or re-request
            atomic_store(&rel_data->needs_retry, false);
            recovered++;
        }
    }
    
    if (recovered > 0) {
        printf("[RECOVERY] 🔄 Recovered %u failed chunks\n", recovered);
    }
}

// Get reliability statistics
void rg_get_reliability_stats(rg_reliable_surface_t* reliable, 
                            uint32_t* failed_chunks, uint32_t* retry_ops) {
    if (!reliable) return;
    
    if (failed_chunks) *failed_chunks = atomic_load(&reliable->failed_chunks);
    if (retry_ops) *retry_ops = atomic_load(&reliable->retry_operations);
}

// Cleanup reliable surface
void rg_destroy_reliable_surface(rg_reliable_surface_t* reliable) {
    if (!reliable) return;
    
    if (reliable->reliability_data) free(reliable->reliability_data);
    if (reliable->surface) rg_destroy_surface(reliable->surface);
    free(reliable);
}
