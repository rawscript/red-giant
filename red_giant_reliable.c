// Red Giant Protocol - Reliable Exposure System
// Maintains exposure-based architecture with robust error handling
#define _GNU_SOURCE
#include <openssl/sha.h>
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
    uint8_t integrity_hash[SHA256_DIGEST_LENGTH];
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
rg_reliable_surface_t* rg_create_reliable_surface(const rg_manifest_t* manifest, uint64_t retry_interval_ns) {
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
    reliable->retry_interval_ns = retry_interval_ns; // was 1mss = 1000000; // 1ms
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

    // Calculate SHA-256 hash for verification
    unsigned char hash[SHA256_DIGEST_LENGTH];
    unsigned char new_hash[SHA256_DIGEST_LENGTH];
    SHA256((const unsigned char*)data, size, hash);
    memcpy(rel_data->integrity_hash, hash, SHA256_DIGEST_LENGTH);

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
            struct timespec remaining, request;
            request.tv_sec = 0;
            request.tv_nsec = reliable->retry_interval_ns * (1 << attempt);

            while (nanosleep(&request, &remaining) == -1) {
                if (errno == EINTR) {
                    request = remaining; // Continue with remaining time
                } else {
                    perror("nanosleep"); // Handle other errors
                    break; // Exit loop on error
                }
            }
        }
    }

    // Mark as failed for later recovery
    atomic_store(&rel_data->needs_retry, true);
    atomic_fetch_add(&reliable->failed_chunks, 1);
    return false;
}

bool get_chunk_data(rg_exposure_surface_t* surface, uint32_t chunk_id, 
                   void** data, uint32_t* size) {
    if (!surface || !data || !size) {
        errno = EINVAL;
        perror("Invalid arguments to get_chunk_data");
        return false;
    }

    // Get chunk information
    rg_chunk_info_t chunk_info;
    if (!rg_get_chunk_info(surface, chunk_id, &chunk_info)) {
        perror("Failed to get chunk info");
        return false;
    }

    // Validate chunk size
    if (chunk_info.size == 0 || chunk_info.size > MAX_CHUNK_SIZE) {
        errno = EINVAL;
        perror("Invalid chunk size");
        return false;
    }

    // Allocate memory for chunk data
    *size = chunk_info.size;
    *data = malloc(*size);
    if (!*data) {
        perror("Failed to allocate memory for chunk data");
        return false;
    }

    // Attempt to retrieve chunk from storage
    if (!retrieve_chunk_from_storage(chunk_id, *data, *size)) {
        perror("Failed to retrieve chunk from storage");
        free(*data);
        *data = NULL;
        return false;
    }

    // Validate retrieved data
    if (!validate_chunk_data(*data, *size)) {
        perror("Retrieved chunk data validation failed");
        free(*data);
        *data = NULL;
        return false;
    }

    return true;
}




//Recover failed chunks with integrity check and retry logic
void rg_recover_failed_chunks(rg_reliable_surface_t* reliable) {
    if (!reliable) return;

    uint32_t recovered = 0;
    uint64_t current_time = get_timestamp_ns();

    for (uint32_t i = 0; i < reliable->surface->manifest.total_chunks; i++) {
        rg_chunk_reliability_t* rel_data = &reliable->reliability_data[i];

        // Check if chunk needs recovery and enough time has passed since last attempt
        if (atomic_load(&rel_data->needs_retry) &&
            (current_time - rel_data->last_attempt) > reliable->retry_interval_ns) {

             // Get the original data from your storage system
            void* chunk_data = NULL;
            uint32_t chunk_size = 0;
            bool data_retrieved = get_chunk_data(reliable->surface, i, &chunk_data, &chunk_size);
            
             if (!data_retrieved || !chunk_data) {
                printf("Failed to retrieve original data for chunk %u\n", i);
                continue;
            }
            // Verify integrity of the original data
            unsigned char hash[SHA256_DIGEST_LENGTH];
            SHA256(chunk_data, chunk_size, new_hash);

            //Compare the calculated hash with the stored hash
            if (memcmp(new_hash, rel_data->integrity_hash, SHA256_DIGEST_LENGTH) != 0) {
                printf("Integrity check failed for chunk %u\n", i);
                free(chunk_data);
                continue;
            }
            // Attempt to re-expose the chunk
            bool retry_success = false;
            for (uint32_t attempt = 0; attempt < reliable->max_retries; attempt++) {
                if (rg_expose_chunk_fast(reliable->surface, i, chunk_data, chunk_size)) {
                    retry_success = true;
                    break;
                }
                
                // Wait before next retry
                struct timespec delay = {
                    .tv_sec = 0,
                    .tv_nsec = reliable->retry_interval_ns * (1 << attempt)
                };
                struct timespec remaining;
                while (nanosleep(&delay, &remaining) == -1) {
                    if (errno == EINTR) {
                        delay = remaining;  // Continue with remaining time
                    } else {
                        perror("nanosleep");
                        break;
                    }
            }
             if (retry_success) {
                atomic_store(&rel_data->needs_retry, false);
                atomic_fetch_sub(&reliable->failed_chunks, 1);
                recovered++;
                printf("Successfully recovered chunk %u\n", i);
            } else {
                printf("Failed to recover chunk %u after %u attempts\n", i, reliable->max_retries);
            }

            free(chunk_data);
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

