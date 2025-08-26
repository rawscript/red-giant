// Red Giant Protocol - Reliable Exposure System
// Maintains exposure-based architecture with robust error handling
#ifndef _WIN32
#define _GNU_SOURCE
#endif

#include "red_giant.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <stdint.h>
#include <stdatomic.h>
#include <errno.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

// Use built-in hash instead of OpenSSL for portability
#define SHA256_DIGEST_LENGTH 32
#define MAX_CHUNK_SIZE (1024 * 1024 * 64)

// Simple hash function as OpenSSL replacement
static void simple_hash(const unsigned char* data, size_t len, unsigned char* hash) {
    // Simple hash implementation - replace with proper crypto library if needed
    memset(hash, 0, SHA256_DIGEST_LENGTH);
    for (size_t i = 0; i < len; i++) {
        hash[i % SHA256_DIGEST_LENGTH] ^= data[i];
    }
}

// Get current timestamp in nanoseconds
static uint64_t get_timestamp_ns(void) {
#ifdef _WIN32
    LARGE_INTEGER frequency, counter;
    QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&counter);
    return (uint64_t)((counter.QuadPart * 1000000000LL) / frequency.QuadPart);
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000LL + ts.tv_nsec;
#endif
}

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

// Missing type definitions
typedef struct {
    uint32_t size;
    uint64_t offset;
    bool is_exposed;
} rg_chunk_info_t;

// Missing function implementations
static bool rg_get_chunk_info(rg_exposure_surface_t* surface, uint32_t chunk_id, rg_chunk_info_t* info) {
    if (!surface || !info || chunk_id >= surface->manifest.total_chunks) {
        return false;
    }
    
    rg_chunk_t* chunk = &surface->chunks[chunk_id];
    info->size = chunk->data_size;
    info->offset = chunk->offset;
    info->is_exposed = atomic_load(&chunk->is_exposed);
    return true;
}

static bool retrieve_chunk_from_storage(uint32_t chunk_id, void* data, uint32_t size) {
    // Placeholder implementation - in real system this would retrieve from storage
    if (!data || size == 0) return false;
    
    // For now, just fill with pattern data
    memset(data, (int)(chunk_id & 0xFF), size);
    return true;
}

static bool validate_chunk_data(const void* data, uint32_t size) {
    // Basic validation - check if data is not null and size is reasonable
    return data != NULL && size > 0 && size <= MAX_CHUNK_SIZE;
}

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

    // Calculate hash for verification
    unsigned char hash[SHA256_DIGEST_LENGTH];
    simple_hash((const unsigned char*)data, size, hash);
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
            
            // Check for overflow in exponential backoff calculation
            uint64_t delay_ns = reliable->retry_interval_ns;
            if (attempt < 32 && delay_ns <= UINT64_MAX >> attempt) {
              delay_ns *= (1 << attempt);
            } else {
              delay_ns = reliable->retry_interval_ns * 10; // Cap the delay
            }
            
            request.tv_nsec = delay_ns;

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

  // Validate chunk_id
  if (chunk_id >= surface->manifest.total_chunks) {
    errno = EINVAL;
    perror("Invalid chunk ID");
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
      unsigned char new_hash[SHA256_DIGEST_LENGTH];
      simple_hash(chunk_data, chunk_size, new_hash);

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
            .tv_nsec = (long)reliable->retry_interval_ns  // Cast to long to match field type
        };
        
        // Check for overflow in exponential backoff calculation
        if (attempt < 32 && delay.tv_nsec <= (long long)(UINT64_MAX >> attempt)) {
          delay.tv_nsec *= (1 << attempt);
        } else {
          delay.tv_nsec = (long)(reliable->retry_interval_ns * 10); // Cap the delay
        }
        
        struct timespec remaining;
        while (nanosleep(&delay, &remaining) == -1) {
            if (errno == EINTR) {
                delay = remaining;  // Continue with remaining time
            } else {
                perror("nanosleep");
                break;
            }
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

      // Always free the chunk data
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

