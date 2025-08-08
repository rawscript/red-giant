// Red Giant Protocol - High-Performance C Core Header
#ifndef RED_GIANT_H
#define RED_GIANT_H

#include <stdint.h>
#include <stdbool.h>
#include <stdatomic.h>
#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

// Optimized constants for high performance
#define RG_MAX_CHUNK_SIZE (1024* 1024 * 64)  // 2MB max chunks
#define RG_MAX_MANIFEST_SIZE 8192
#define RG_RED_FLAG_MARKER 0xDEADBEEF
#define RG_MAX_CONCURRENT_CHUNKS 1024

// Cache line size detection
#ifndef RG_CACHE_LINE_SIZE
#if defined(__x86_64__) || defined(__i386__)
#define RG_CACHE_LINE_SIZE 64
#elif defined(__aarch64__) || defined(__arm__)
#define RG_CACHE_LINE_SIZE 64
#else
#define RG_CACHE_LINE_SIZE 64  // Safe default
#endif
#endif

// Protocol message types
typedef enum {
    RG_MSG_HANDSHAKE = 0x01,
    RG_MSG_MANIFEST = 0x02,
    RG_MSG_CHUNK_READY = 0x03,
    RG_MSG_CHUNK_DATA = 0x04,
    RG_MSG_RED_FLAG = 0x05,
    RG_MSG_ERROR = 0xFF
} rg_msg_type_t;

// Error codes
typedef enum {
    RG_SUCCESS = 0,
    RG_ERROR_INVALID_PARAM = -1,
    RG_ERROR_MEMORY_ALLOC = -2,
    RG_ERROR_CHUNK_NOT_FOUND = -3,
    RG_ERROR_CHUNK_TOO_LARGE = -4,
    RG_ERROR_SURFACE_FULL = -5
} rg_error_t;

// Manifest structure - the orchestration blueprint
typedef struct {
    char file_id[64];
    uint64_t total_size;
    uint32_t chunk_size;
    uint16_t encoding_type;
    uint32_t exposure_cadence_ms;
    uint32_t total_chunks;
    uint8_t hash[32]; // SHA-256 of complete file
    uint32_t version;
    uint32_t reserved[3]; // For future use
} rg_manifest_t;

// High-performance chunk structure with proper alignment
typedef struct {
    uint32_t sequence_id;
    uint32_t data_size;
    uint64_t offset;
    uint8_t chunk_hash[16]; // MD5 for quick validation
    atomic_bool is_exposed;  // Atomic for thread safety
    void* data_ptr;
    uint64_t exposure_timestamp;
    atomic_uint_fast32_t pull_count; // Atomic pull counter
    uint32_t padding[2];          // Ensure proper alignment
} __attribute__((aligned(RG_CACHE_LINE_SIZE))) rg_chunk_t;

// High-performance exposure surface with memory pools
typedef struct {
    rg_manifest_t manifest;
    rg_chunk_t* chunks;
    atomic_uint_fast32_t exposed_count;  // Atomic operations
    atomic_bool red_flag_raised;
    void* shared_buffer;
    size_t buffer_size;
    void* memory_pool;                // Pre-allocated memory pool
    size_t pool_size;
    uint32_t* free_slots;            // Free slot tracking
    uint32_t free_slot_count;
    atomic_uint_fast64_t total_bytes_exposed;
    uint64_t start_time;
    uint32_t reserved[4];            // For future use
} rg_exposure_surface_t;

// Core API functions
rg_exposure_surface_t* rg_create_surface(const rg_manifest_t* manifest);
void rg_destroy_surface(rg_exposure_surface_t* surface);

// Fast chunk operations with memory optimization
bool rg_expose_chunk_fast(rg_exposure_surface_t* surface, uint32_t chunk_id, const void* data, uint32_t size);
const rg_chunk_t* rg_peek_chunk_fast(rg_exposure_surface_t* surface, uint32_t chunk_id);
bool rg_pull_chunk_fast(rg_exposure_surface_t* surface, uint32_t chunk_id, void* dest_buffer, uint32_t* size);

// Batch operations for better performance
uint32_t rg_expose_batch(rg_exposure_surface_t* surface, uint32_t start_chunk, uint32_t count, const void** data_ptrs, uint32_t* sizes);
uint32_t rg_pull_batch(rg_exposure_surface_t* surface, uint32_t start_chunk, uint32_t count, void** dest_buffers, uint32_t* sizes);

// Status and control
void rg_raise_red_flag(rg_exposure_surface_t* surface);
bool rg_is_complete(const rg_exposure_surface_t* surface);
uint64_t rg_get_performance_stats(const rg_exposure_surface_t* surface, uint32_t* throughput_mbps);

// Memory management helpers
void* rg_alloc_aligned(size_t size, size_t alignment);
void rg_free_aligned(void* ptr);

#ifdef __cplusplus
}
#endif

#endif // RED_GIANT_H
#endif // RED_GIANT_H
