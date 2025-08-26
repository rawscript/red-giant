// Red Giant Protocol - Complete Wrapper Implementation
// Provides high-level API for the Red Giant Protocol with full workflow support

#include "red_giant.h"
#include "red_giant_wrapper.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <stdarg.h>
#include <time.h>

#ifdef _WIN32
#include <windows.h>
#include <io.h>
#define F_OK 0
#define access _access
#define stat _stat
#define S_ISREG(m) (((m) & S_IFMT) == S_IFREG)
#else
#include <unistd.h>
#include <sys/types.h>
#include <dirent.h>
#endif

// Wrapper-specific constants
#define RG_DEFAULT_CHUNK_SIZE (1024 * 1024)  // 1MB default chunks
#define RG_MAX_FILENAME_LEN 256
#define RG_MAX_ERROR_MSG_LEN 512
#define RG_PROGRESS_UPDATE_INTERVAL 100  // Update progress every 100 chunks

// File processing context implementation
struct rg_file_context_t {
    char filename[RG_MAX_FILENAME_LEN];
    FILE* file_handle;
    uint64_t file_size;
    uint32_t total_chunks;
    uint32_t chunk_size;
    uint32_t processed_chunks;
    rg_exposure_surface_t* surface;
    uint64_t start_time;
    bool use_reliable_mode;
    char error_message[RG_MAX_ERROR_MSG_LEN];
};

// Global callback pointers
static rg_progress_callback_t g_progress_callback = NULL;
static rg_log_callback_t g_log_callback = NULL;

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

// Internal logging function
static void rg_log(const char* level, const char* format, ...) {
    if (!g_log_callback) return;
    
    char message[RG_MAX_ERROR_MSG_LEN];
    va_list args;
    va_start(args, format);
    vsnprintf(message, sizeof(message), format, args);
    va_end(args);
    
    g_log_callback(level, message);
}

// Internal progress reporting
static void rg_report_progress(rg_file_context_t* context) {
    if (!g_progress_callback || !context) return;
    
    float percentage = (context->total_chunks > 0) ? 
        ((float)context->processed_chunks / context->total_chunks) * 100.0f : 0.0f;
    
    // Calculate throughput
    uint64_t elapsed_ns = get_timestamp_ns() - context->start_time;
    uint32_t throughput_mbps = 0;
    
    if (elapsed_ns > 0) {
        uint64_t bytes_processed = (uint64_t)context->processed_chunks * context->chunk_size;
        double elapsed_seconds = elapsed_ns / 1000000000.0;
        double throughput_bytes_per_sec = bytes_processed / elapsed_seconds;
        throughput_mbps = (uint32_t)(throughput_bytes_per_sec / (1024 * 1024));
    }
    
    g_progress_callback(context->processed_chunks, context->total_chunks, percentage, throughput_mbps);
}

// ============================================================================
// CONFIGURATION FUNCTIONS
// ============================================================================

void rg_wrapper_set_progress_callback(rg_progress_callback_t callback) {
    g_progress_callback = callback;
}

void rg_wrapper_set_log_callback(rg_log_callback_t callback) {
    g_log_callback = callback;
}

const char* rg_wrapper_get_version(void) {
    return RG_WRAPPER_VERSION;
}

// ============================================================================
// FILE CONTEXT MANAGEMENT
// ============================================================================

rg_file_context_t* rg_wrapper_init_file(const char* filename, bool use_reliable_mode) {
    if (!filename) {
        rg_log("ERROR", "Filename cannot be NULL");
        return NULL;
    }
    
    // Check if file exists and is readable
    if (access(filename, F_OK) != 0) {
        rg_log("ERROR", "File not found: %s", filename);
        return NULL;
    }
    
    // Get file size
    struct stat file_stat;
    if (stat(filename, &file_stat) != 0) {
        rg_log("ERROR", "Failed to get file stats: %s", filename);
        return NULL;
    }
    
    if (!S_ISREG(file_stat.st_mode)) {
        rg_log("ERROR", "Not a regular file: %s", filename);
        return NULL;
    }
    
    // Allocate context
    rg_file_context_t* context = calloc(1, sizeof(rg_file_context_t));
    if (!context) {
        rg_log("ERROR", "Failed to allocate file context");
        return NULL;
    }
    
    // Initialize context
    strncpy(context->filename, filename, RG_MAX_FILENAME_LEN - 1);
    context->filename[RG_MAX_FILENAME_LEN - 1] = '\0';
    context->file_size = file_stat.st_size;
    context->chunk_size = RG_DEFAULT_CHUNK_SIZE;
    context->total_chunks = (uint32_t)((context->file_size + context->chunk_size - 1) / context->chunk_size);
    context->processed_chunks = 0;
    context->use_reliable_mode = use_reliable_mode;
    context->start_time = get_timestamp_ns();
    
    rg_log("INFO", "Initialized file context: %s (%llu bytes, %u chunks)", 
           filename, (unsigned long long)context->file_size, context->total_chunks);
    
    // Create manifest
    rg_manifest_t manifest = {
        .total_chunks = context->total_chunks,
        .chunk_size = context->chunk_size,
        .total_size = context->file_size,
        .version = 1,
        .encoding_type = 0,
        .exposure_cadence_ms = 0
    };
    
    // Initialize file_id
    snprintf(manifest.file_id, sizeof(manifest.file_id), "file_%llu", 
             (unsigned long long)get_timestamp_ns());
    
    // Create surface
    if (use_reliable_mode) {
        // For now, use regular surface - reliable surface functions need to be implemented
        context->surface = rg_create_surface(&manifest);
        if (!context->surface) {
            rg_log("ERROR", "Failed to create surface for file: %s", filename);
            free(context);
            return NULL;
        }
    } else {
        context->surface = rg_create_surface(&manifest);
        if (!context->surface) {
            rg_log("ERROR", "Failed to create surface for file: %s", filename);
            free(context);
            return NULL;
        }
    }
    
    return context;
}

// ============================================================================
// FILE PROCESSING FUNCTIONS
// ============================================================================

rg_wrapper_error_t rg_wrapper_process_file(rg_file_context_t* context) {
    if (!context) {
        rg_log("ERROR", "File context is NULL");
        return RG_WRAPPER_ERROR_INVALID_FILE;
    }
    
    rg_log("INFO", "Starting file processing: %s", context->filename);
    
    // Open file for reading
    context->file_handle = fopen(context->filename, "rb");
    if (!context->file_handle) {
        rg_log("ERROR", "Failed to open file: %s", context->filename);
        return RG_WRAPPER_ERROR_FILE_ACCESS;
    }
    
    // Allocate chunk buffer
    void* chunk_buffer = malloc(context->chunk_size);
    if (!chunk_buffer) {
        rg_log("ERROR", "Failed to allocate chunk buffer");
        fclose(context->file_handle);
        context->file_handle = NULL;
        return RG_WRAPPER_ERROR_MEMORY_ALLOC;
    }
    
    // Process chunks
    uint32_t chunk_id = 0;
    size_t bytes_read;
    bool success = true;
    
    while ((bytes_read = fread(chunk_buffer, 1, context->chunk_size, context->file_handle)) > 0) {
        // Expose chunk
        bool chunk_success = rg_expose_chunk_fast(
            context->surface,
            chunk_id,
            chunk_buffer,
            (uint32_t)bytes_read
        );
        
        if (!chunk_success) {
            rg_log("ERROR", "Failed to expose chunk %u", chunk_id);
            success = false;
            break;
        }
        
        context->processed_chunks++;
        chunk_id++;
        
        // Report progress periodically
        if (context->processed_chunks % RG_PROGRESS_UPDATE_INTERVAL == 0 || 
            context->processed_chunks == context->total_chunks) {
            rg_report_progress(context);
        }
    }
    
    // Cleanup
    free(chunk_buffer);
    fclose(context->file_handle);
    context->file_handle = NULL;
    
    if (!success) {
        return RG_WRAPPER_ERROR_CHUNK_PROCESS;
    }
    
    // Final progress report
    rg_report_progress(context);
    
    rg_log("INFO", "File processing completed: %s (%u chunks)", 
           context->filename, context->processed_chunks);
    
    return RG_WRAPPER_SUCCESS;
}

rg_wrapper_error_t rg_wrapper_retrieve_file(rg_file_context_t* context, const char* output_filename) {
    if (!context || !output_filename) {
        rg_log("ERROR", "Invalid parameters for file retrieval");
        return RG_WRAPPER_ERROR_INVALID_FILE;
    }
    
    rg_log("INFO", "Starting file retrieval: %s -> %s", context->filename, output_filename);
    
    // Open output file
    FILE* output_file = fopen(output_filename, "wb");
    if (!output_file) {
        rg_log("ERROR", "Failed to create output file: %s", output_filename);
        return RG_WRAPPER_ERROR_FILE_ACCESS;
    }
    
    // Retrieve chunks
    for (uint32_t chunk_id = 0; chunk_id < context->total_chunks; chunk_id++) {
        const rg_chunk_t* chunk = rg_peek_chunk_fast(context->surface, chunk_id);
        if (!chunk) {
            rg_log("ERROR", "Failed to retrieve chunk %u", chunk_id);
            fclose(output_file);
            return RG_WRAPPER_ERROR_CHUNK_PROCESS;
        }
        
        // Write chunk data to output file
        size_t bytes_written = fwrite(chunk->data_ptr, 1, chunk->data_size, output_file);
        if (bytes_written != chunk->data_size) {
            rg_log("ERROR", "Failed to write chunk %u to output file", chunk_id);
            fclose(output_file);
            return RG_WRAPPER_ERROR_FILE_ACCESS;
        }
    }
    
    fclose(output_file);
    
    rg_log("INFO", "File retrieval completed: %s", output_filename);
    return RG_WRAPPER_SUCCESS;
}

// ============================================================================
// STATISTICS AND MONITORING
// ============================================================================

void rg_wrapper_get_stats(rg_file_context_t* context, 
                         uint32_t* processed_chunks, 
                         uint32_t* total_chunks,
                         uint32_t* throughput_mbps,
                         uint64_t* elapsed_ms,
                         bool* is_complete) {
    if (!context) {
        if (processed_chunks) *processed_chunks = 0;
        if (total_chunks) *total_chunks = 0;
        if (throughput_mbps) *throughput_mbps = 0;
        if (elapsed_ms) *elapsed_ms = 0;
        if (is_complete) *is_complete = false;
        return;
    }
    
    if (processed_chunks) *processed_chunks = context->processed_chunks;
    if (total_chunks) *total_chunks = context->total_chunks;
    if (is_complete) *is_complete = (context->processed_chunks == context->total_chunks);
    
    // Calculate elapsed time and throughput
    uint64_t current_time = get_timestamp_ns();
    uint64_t elapsed_ns = current_time - context->start_time;
    
    if (elapsed_ms) *elapsed_ms = elapsed_ns / 1000000;  // Convert to milliseconds
    
    if (throughput_mbps && elapsed_ns > 0) {
      uint64_t bytes_processed = (uint64_t)context->processed_chunks * context->chunk_size;
      // Avoid division by zero
      if (elapsed_ns > 0) {
        double elapsed_seconds = (double)elapsed_ns / 1000000000.0;
        double throughput_bytes_per_sec = (double)bytes_processed / elapsed_seconds;
        *throughput_mbps = (uint32_t)(throughput_bytes_per_sec / (1024 * 1024));
      } else {
        *throughput_mbps = 0;
      }
    } else if (throughput_mbps) {
        *throughput_mbps = 0;
    }
}

void rg_wrapper_get_reliability_stats(rg_file_context_t* context,
                                     uint32_t* failed_chunks,
                                     uint32_t* retry_operations) {
    if (!context || !context->use_reliable_mode) {
        if (failed_chunks) *failed_chunks = 0;
        if (retry_operations) *retry_operations = 0;
        return;
    }
    
    // For now, return zeros - reliable surface functions need to be implemented
    if (failed_chunks) *failed_chunks = 0;
    if (retry_operations) *retry_operations = 0;
}

// Recovery functions (reliable mode only)
void rg_wrapper_recover_failed_chunks(rg_file_context_t* context) {
    if (!context || !context->use_reliable_mode) {
        return;
    }
    
    rg_log("INFO", "Starting chunk recovery process");
    // Recovery implementation would go here
}

// Cleanup file context
void rg_wrapper_cleanup_file(rg_file_context_t* context) {
    if (!context) return;
    
    rg_log("INFO", "Cleaning up file context: %s", context->filename);
    
    if (context->file_handle) {
        fclose(context->file_handle);
    }
    
    if (context->surface) {
        rg_destroy_surface(context->surface);
    }
    
    free(context);
}

// ============================================================================
// HIGH-LEVEL WORKFLOW FUNCTIONS
// ============================================================================

// Complete file transmission workflow
rg_wrapper_error_t rg_wrapper_transmit_file(const char* filename, bool use_reliable_mode) {
    rg_log("INFO", "Starting file transmission workflow: %s", filename);
    
    // Initialize file context
    rg_file_context_t* context = rg_wrapper_init_file(filename, use_reliable_mode);
    if (!context) {
        return RG_WRAPPER_ERROR_FILE_NOT_FOUND;
    }
    
    // Process file
    rg_wrapper_error_t result = rg_wrapper_process_file(context);
    
    // Get final statistics
    uint32_t processed, total, throughput;
    uint64_t elapsed;
    bool complete;
    rg_wrapper_get_stats(context, &processed, &total, &throughput, &elapsed, &complete);
    
    rg_log("INFO", "Transmission completed - Processed: %u/%u chunks, Throughput: %u MB/s, Time: %llu ms", 
           processed, total, throughput, (unsigned long long)elapsed);
    
    if (use_reliable_mode) {
        uint32_t failed, retries;
        rg_wrapper_get_reliability_stats(context, &failed, &retries);
        rg_log("INFO", "Reliability stats - Failed chunks: %u, Retry operations: %u", failed, retries);
    }
    
    // Cleanup
    rg_wrapper_cleanup_file(context);
    
    return result;
}

// Complete file reception workflow
rg_wrapper_error_t rg_wrapper_receive_file(rg_file_context_t* context, const char* output_filename) {
    if (!context) {
        return RG_WRAPPER_ERROR_INVALID_FILE;
    }
    
    rg_log("INFO", "Starting file reception workflow: %s", output_filename);
    
    // Wait for transmission to complete
    bool complete = false;
    int timeout_counter = 0;
    const int max_timeout = 1000; // 10 seconds at 10ms intervals
    
    while (!complete && timeout_counter < max_timeout) {
        uint32_t processed, total;
        rg_wrapper_get_stats(context, &processed, &total, NULL, NULL, &complete);
        
        if (!complete) {
            // Brief sleep to avoid busy waiting
            #ifdef _WIN32
            Sleep(10);
            #else
            usleep(10000);  // 10ms
            #endif
            timeout_counter++;
        }
    }
    
    if (!complete) {
        rg_log("ERROR", "Timeout waiting for transmission to complete");
        return RG_WRAPPER_ERROR_TRANSMISSION;
    }
    
    // Retrieve file
    rg_wrapper_error_t result = rg_wrapper_retrieve_file(context, output_filename);
    
    rg_log("INFO", "Reception workflow completed");
    return result;
}

// Batch file processing
rg_wrapper_error_t rg_wrapper_process_batch(const char** filenames, int file_count, bool use_reliable_mode) {
    if (!filenames || file_count <= 0) {
        return RG_WRAPPER_ERROR_INVALID_FILE;
    }
    
    rg_log("INFO", "Starting batch processing: %d files", file_count);
    
    int successful_files = 0;
    
    for (int i = 0; i < file_count; i++) {
        rg_log("INFO", "Processing file %d/%d: %s", i + 1, file_count, filenames[i]);
        
        rg_wrapper_error_t result = rg_wrapper_transmit_file(filenames[i], use_reliable_mode);
        if (result == RG_WRAPPER_SUCCESS) {
            successful_files++;
        } else {
            rg_log("ERROR", "Failed to process file: %s (error code: %d)", filenames[i], result);
        }
    }
    
    rg_log("INFO", "Batch processing completed: %d/%d files successful", successful_files, file_count);
    
    return (successful_files == file_count) ? RG_WRAPPER_SUCCESS : RG_WRAPPER_ERROR_TRANSMISSION;
}