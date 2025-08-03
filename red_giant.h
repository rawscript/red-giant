// Red Giant Protocol - High-Performance C Core Header
#ifndef RED_GIANT_H
#define RED_GIANT_H

#include <stdint.h>
#include <stdbool.h>
#include <sys/types.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

// Optimized constants for high performance
#define RG_MAX_CHUNK_SIZE (1024 * 1024)  // 1MB max chunks
#define RG_MAX_MANIFEST_SIZE 8192
#define RG_RED_FLAG_MARKER 0xDEADBEEF
#define RG_CACHE_LINE_SIZE 64
#define RG_MAX_CONCURRENT_CHUNKS 256

// Protocol message types
typedef enum {
    RG_MSG_HANDSHAKE = 0x01,
    RG_MSG_MANIFEST = 0x02,
    RG_MSG_CHUNK_READY = 0x03,
    RG_MSG_CHUNK_DATA = 0x04,
    RG_MSG_RED_FLAG = 0x05,
    RG_MSG_ERROR = 0xFF
} rg_msg_type_t;

// Manifest structure - the orchestration blueprint
typedef struct {
    char file_id[64];
    uint64_t total_size;
    uint32_t chunk_size;
    uint16_t encoding_type;
    uint32_t exposure_cadence_ms;
    uint32_t total_chunks;
    uint8_t hash[32]; // SHA-256 of complete file
} rg_manifest_t;

// High-performance chunk structure with cache alignment
typedef struct __attribute__((aligned(RG_CACHE_LINE_SIZE))) {
    uint32_t sequence_id;
    uint32_t data_size;
    uint64_t offset;
    uint8_t chunk_hash[16]; // MD5 for quick validation
    volatile bool is_exposed;  // Volatile for thread safety
    void* data_ptr;
    uint64_t exposure_timestamp;
    uint32_t pull_count;       // Track how many times pulled
    uint32_t padding;          // Ensure cache line alignment
} rg_chunk_t;

// High-performance exposure surface with memory pools
typedef struct {
    rg_manifest_t manifest;
    rg_chunk_t* chunks;
    volatile uint32_t exposed_count;  // Atomic operations
    volatile bool red_flag_raised;
    void* shared_buffer;
    size_t buffer_size;
    void* memory_pool;                // Pre-allocated memory pool
    size_t pool_size;
    uint32_t* free_slots;            // Free slot tracking
    uint32_t free_slot_count;
    uint64_t total_bytes_exposed;
    uint64_t start_time;
} rg_exposure_surface_t;

// Core C functions for low-level operations
rg_exposure_surface_t* rg_create_surface(const rg_manifest_t* manifest);
void rg_destroy_surface(rg_exposure_surface_t* surface);
bool rg_expose_chunk(rg_exposure_surface_t* surface, uint32_t chunk_id, const void* data);
const rg_chunk_t* rg_peek_chunk(rg_exposure_surface_t* surface, uint32_t chunk_id);
bool rg_pull_chunk(rg_exposure_surface_t* surface, uint32_t chunk_id, void* dest_buffer);
void rg_raise_red_flag(rg_exposure_surface_t* surface);
bool rg_is_complete(const rg_exposure_surface_t* surface);

#endif // RED_GIANT_H