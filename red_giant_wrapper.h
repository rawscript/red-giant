// Red Giant Protocol - Wrapper Header
// High-level API for the Red Giant Protocol with complete workflow support

#ifndef RED_GIANT_WRAPPER_H
#define RED_GIANT_WRAPPER_H

#include "red_giant.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Wrapper version
#define RG_WRAPPER_VERSION "1.0.0"

// Wrapper error codes
typedef enum {
    RG_WRAPPER_SUCCESS = 0,
    RG_WRAPPER_ERROR_FILE_NOT_FOUND = -100,
    RG_WRAPPER_ERROR_FILE_ACCESS = -101,
    RG_WRAPPER_ERROR_INVALID_FILE = -102,
    RG_WRAPPER_ERROR_MEMORY_ALLOC = -103,
    RG_WRAPPER_ERROR_SURFACE_CREATE = -104,
    RG_WRAPPER_ERROR_CHUNK_PROCESS = -105,
    RG_WRAPPER_ERROR_TRANSMISSION = -106
} rg_wrapper_error_t;

// Forward declaration of file context
typedef struct rg_file_context_t rg_file_context_t;

// Callback function types
typedef void (*rg_progress_callback_t)(uint32_t processed, uint32_t total, float percentage, uint32_t throughput_mbps);
typedef void (*rg_log_callback_t)(const char* level, const char* message);

// ============================================================================
// WRAPPER API FUNCTIONS
// ============================================================================

// Configuration functions
void rg_wrapper_set_progress_callback(rg_progress_callback_t callback);
void rg_wrapper_set_log_callback(rg_log_callback_t callback);
const char* rg_wrapper_get_version(void);

// File context management
rg_file_context_t* rg_wrapper_init_file(const char* filename, bool use_reliable_mode);
void rg_wrapper_cleanup_file(rg_file_context_t* context);

// File processing functions
rg_wrapper_error_t rg_wrapper_process_file(rg_file_context_t* context);
rg_wrapper_error_t rg_wrapper_retrieve_file(rg_file_context_t* context, const char* output_filename);

// Statistics and monitoring
void rg_wrapper_get_stats(rg_file_context_t* context, 
                         uint32_t* processed_chunks, 
                         uint32_t* total_chunks,
                         uint32_t* throughput_mbps,
                         uint64_t* elapsed_ms,
                         bool* is_complete);

void rg_wrapper_get_reliability_stats(rg_file_context_t* context,
                                     uint32_t* failed_chunks,
                                     uint32_t* retry_operations);

// Recovery functions (reliable mode only)
void rg_wrapper_recover_failed_chunks(rg_file_context_t* context);

// High-level workflow functions
rg_wrapper_error_t rg_wrapper_transmit_file(const char* filename, bool use_reliable_mode);
rg_wrapper_error_t rg_wrapper_receive_file(rg_file_context_t* context, const char* output_filename);
rg_wrapper_error_t rg_wrapper_process_batch(const char** filenames, int file_count, bool use_reliable_mode);

#ifdef __cplusplus
}
#endif

#endif // RED_GIANT_WRAPPER_H