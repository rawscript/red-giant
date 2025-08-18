// Red Giant Protocol - Complete Wrapper Implementation
// Provides high-level API for the Red Giant Protocol with full workflow support

#include "red_giant.c"
#include "red_giant_reliable.c"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <stdarg.h>

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
#define RG_WRAPPER_VERSION "1.0.0"
#define RG_DEFAULT_CHUNK_SIZE (1024 * 1024)  // 1MB default chunks
#define RG_MAX_FILENAME_LEN 256
#define RG_MAX_ERROR_MSG_LEN 512
#define RG_PROGRESS_UPDATE_INTERVAL 100  // Update progress every 100 chunks

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

// File processing context
typedef struct {
    char filename[RG_MAX_FILENAME_LEN];
    FILE* file_handle;
    uint64_t file_size;
    uint32_t total_chunks;
    uint32_t chunk_size;
    uint32_t processed_chunks;
    rg_exposure_surface_t* surface;
    rg_reliable_surface_t* reliable_surface;
    uint64_t start_time;
    bool use_reliable_mode;
    char error_message[RG_MAX_ERROR_MSG_LEN];
} rg_file_context_t;

// Progress callback function type
typedef void (*rg_progress_callback_t)(uint32_t processed, uint32_t total, float percentage, uint32_t throughput_mbps);

// Logging callback function type
typedef void (*rg_log_callback_t)(const char* level, const char* message);

// Global callbacks
static rg_progress_callback_t g_progress_callback = NULL;
static rg_log_callback_t g_log_callback = NULL;

// ============================================================================
// UTILITY FUNCTIONS
// ============================================================================

// Default logging function
static void default_log_callback(const char* level, const char* message) {
    printf("[%s] %s\n", level, message);
    fflush(stdout);
}

// Log wrapper function
static void rg_log(const char* level, const char* format, ...) {
    char buffer[1024];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    
    if (g_log_callback) {
        g_log_callback(level, buffer);
    } else {
        default_log_callback(level, buffer);
    }
}

// Get file size
static uint64_t get_file_size(const char* filename) {
    struct stat st;
    if (stat(filename, &st) == 0 && S_ISREG(st.st_mode)) {
        return (uint64_t)st.st_size;
    }
    return 0;
}

// Calculate optimal chunk size based on file size
static uint32_t calculate_optimal_chunk_size(uint64_t file_size) {
    if (file_size < 1024 * 1024) {  // < 1MB
        return 64 * 1024;  // 64KB chunks
    } else if (file_size < 100 * 1024 * 1024) {  // < 100MB
        return 1024 * 1024;  // 1MB chunks
    } else if (file_size < 1024 * 1024 * 1024) {  // < 1GB
        return 4 * 1024 * 1024;  // 4MB chunks
    } else {
        return 8 * 1024 * 1024;  // 8MB chunks for large files
    }
}

// Generate file hash for manifest
static void generate_file_hash(const char* filename, uint8_t* hash) {
    // Simple hash implementation - in production, use proper crypto library
    memset(hash, 0, 32);
    
    FILE* file = fopen(filename, "rb");
    if (!file) return;
    
    uint8_t buffer[4096];
    size_t bytes_read;
    uint32_t hash_accumulator = 0;
    
    while ((bytes_read = fread(buffer, 1, sizeof(buffer), file)) > 0) {
        for (size_t i = 0; i < bytes_read; i++) {
            hash_accumulator = hash_accumulator * 31 + buffer[i];
        }
    }
    
    // Store hash in first 4 bytes, repeat pattern for 32 bytes
    for (int i = 0; i < 32; i += 4) {
        memcpy(hash + i, &hash_accumulator, sizeof(uint32_t));
    }
    
    fclose(file);
}

// ============================================================================
// WRAPPER API FUNCTIONS
// ============================================================================

// Set progress callback
void rg_wrapper_set_progress_callback(rg_progress_callback_t callback) {
    g_progress_callback = callback;
}

// Set logging callback
void rg_wrapper_set_log_callback(rg_log_callback_t callback) {
    g_log_callback = callback;
}

// Get wrapper version
const char* rg_wrapper_get_version(void) {
    return RG_WRAPPER_VERSION;
}

// Initialize file context
rg_file_context_t* rg_wrapper_init_file(const char* filename, bool use_reliable_mode) {
    if (!filename) {
        rg_log("ERROR", "Filename cannot be NULL");
        return NULL;
    }
    
    // Check if file exists and is accessible
    if (access(filename, F_OK) != 0) {
        rg_log("ERROR", "File not found: %s", filename);
        return NULL;
    }
    
    // Get file size
    uint64_t file_size = get_file_size(filename);
    if (file_size == 0) {
        rg_log("ERROR", "Invalid file or empty file: %s", filename);
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
    context->file_size = file_size;
    context->chunk_size = calculate_optimal_chunk_size(file_size);
    context->total_chunks = (uint32_t)((file_size + context->chunk_size - 1) / context->chunk_size);
    context->use_reliable_mode = use_reliable_mode;
    context->start_time = get_timestamp_ns();
    
    // Open file
    context->file_handle = fopen(filename, "rb");
    if (!context->file_handle) {
        rg_log("ERROR", "Failed to open file: %s", filename);
        free(context);
        return NULL;
    }
    
    // Create manifest
    rg_manifest_t manifest = {0};
    snprintf(manifest.file_id, sizeof(manifest.file_id), "file_%llu", (unsigned long long)get_timestamp_ns());
    manifest.total_size = file_size;
    manifest.chunk_size = context->chunk_size;
    manifest.encoding_type = 0;  // Raw binary
    manifest.exposure_cadence_ms = 10;  // 10ms between exposures
    manifest.total_chunks = context->total_chunks;
    manifest.version = 1;
    generate_file_hash(filename, manifest.hash);
    
    // Create exposure surface
    if (use_reliable_mode) {
        context->reliable_surface = rg_create_reliable_surface(&manifest, 1000000);  // 1ms retry interval
        if (!context->reliable_surface) {
            rg_log("ERROR", "Failed to create reliable surface");
            fclose(context->file_handle);
            free(context);
            return NULL;
        }
        context->surface = context->reliable_surface->surface;
    } else {
        context->surface = rg_create_surface(&manifest);
        if (!context->surface) {
            rg_log("ERROR", "Failed to create surface");
            fclose(context->file_handle);
            free(context);
            return NULL;
        }
    }
    
    rg_log("INFO", "Initialized file context: %s (%.2f MB, %u chunks)", 
           filename, (double)file_size / (1024 * 1024), context->total_chunks);
    
    return context;
}

// Process file chunks and expose them
rg_wrapper_error_t rg_wrapper_process_file(rg_file_context_t* context) {
    if (!context || !context->file_handle || !context->surface) {
        return RG_WRAPPER_ERROR_INVALID_FILE;
    }
    
    rg_log("INFO", "Starting file processing: %u chunks", context->total_chunks);
    
    // Allocate chunk buffer
    void* chunk_buffer = malloc(context->chunk_size);
    if (!chunk_buffer) {
        rg_log("ERROR", "Failed to allocate chunk buffer");
        return RG_WRAPPER_ERROR_MEMORY_ALLOC;
    }
    
    uint32_t successful_chunks = 0;
    uint64_t last_progress_time = get_timestamp_ns();
    
    // Process each chunk
    for (uint32_t chunk_id = 0; chunk_id < context->total_chunks; chunk_id++) {
        // Read chunk data
        size_t bytes_read = fread(chunk_buffer, 1, context->chunk_size, context->file_handle);
        if (bytes_read == 0 && ferror(context->file_handle)) {
            rg_log("ERROR", "Failed to read chunk %u", chunk_id);
            free(chunk_buffer);
            return RG_WRAPPER_ERROR_CHUNK_PROCESS;
        }
        
        // Expose chunk
        bool success;
        if (context->use_reliable_mode) {
            success = rg_expose_chunk_reliable(context->reliable_surface, chunk_id, chunk_buffer, (uint32_t)bytes_read);
        } else {
            success = rg_expose_chunk_fast(context->surface, chunk_id, chunk_buffer, (uint32_t)bytes_read);
        }
        
        if (success) {
            successful_chunks++;
            context->processed_chunks++;
        } else {
            rg_log("WARNING", "Failed to expose chunk %u", chunk_id);
        }
        
        // Update progress
        uint64_t current_time = get_timestamp_ns();
        if (g_progress_callback && 
            (chunk_id % RG_PROGRESS_UPDATE_INTERVAL == 0 || chunk_id == context->total_chunks - 1 ||
             (current_time - last_progress_time) > 1000000000ULL)) {  // Update at least every second
            
            float percentage = (float)(chunk_id + 1) * 100.0f / context->total_chunks;
            uint32_t throughput_mbps = 0;
            rg_get_performance_stats(context->surface, &throughput_mbps);
            
            g_progress_callback(chunk_id + 1, context->total_chunks, percentage, throughput_mbps);
            last_progress_time = current_time;
        }
    }
    
    free(chunk_buffer);
    
    // Raise red flag to indicate completion
    rg_raise_red_flag(context->surface);
    
    rg_log("INFO", "File processing completed: %u/%u chunks successful", 
           successful_chunks, context->total_chunks);
    
    return (successful_chunks == context->total_chunks) ? RG_WRAPPER_SUCCESS : RG_WRAPPER_ERROR_CHUNK_PROCESS;
}

// Retrieve file from surface
rg_wrapper_error_t rg_wrapper_retrieve_file(rg_file_context_t* context, const char* output_filename) {
    if (!context || !context->surface || !output_filename) {
        return RG_WRAPPER_ERROR_INVALID_FILE;
    }
    
    rg_log("INFO", "Starting file retrieval to: %s", output_filename);
    
    FILE* output_file = fopen(output_filename, "wb");
    if (!output_file) {
        rg_log("ERROR", "Failed to create output file: %s", output_filename);
        return RG_WRAPPER_ERROR_FILE_ACCESS;
    }
    
    // Allocate buffer for chunk data
    void* chunk_buffer = malloc(context->chunk_size);
    if (!chunk_buffer) {
        rg_log("ERROR", "Failed to allocate chunk buffer");
        fclose(output_file);
        return RG_WRAPPER_ERROR_MEMORY_ALLOC;
    }
    
    uint32_t retrieved_chunks = 0;
    uint64_t total_bytes_written = 0;
    
    // Retrieve each chunk
    for (uint32_t chunk_id = 0; chunk_id < context->total_chunks; chunk_id++) {
        uint32_t chunk_size = 0;
        
        if (rg_pull_chunk_fast(context->surface, chunk_id, chunk_buffer, &chunk_size)) {
            size_t bytes_written = fwrite(chunk_buffer, 1, chunk_size, output_file);
            if (bytes_written == chunk_size) {
                retrieved_chunks++;
                total_bytes_written += bytes_written;
            } else {
                rg_log("ERROR", "Failed to write chunk %u to file", chunk_id);
                break;
            }
        } else {
            rg_log("WARNING", "Failed to retrieve chunk %u", chunk_id);
        }
        
        // Update progress
        if (g_progress_callback && chunk_id % RG_PROGRESS_UPDATE_INTERVAL == 0) {
            float percentage = (float)(chunk_id + 1) * 100.0f / context->total_chunks;
            g_progress_callback(chunk_id + 1, context->total_chunks, percentage, 0);
        }
    }
    
    free(chunk_buffer);
    fclose(output_file);
    
    rg_log("INFO", "File retrieval completed: %u/%u chunks, %llu bytes written", 
           retrieved_chunks, context->total_chunks, (unsigned long long)total_bytes_written);
    
    return (retrieved_chunks == context->total_chunks) ? RG_WRAPPER_SUCCESS : RG_WRAPPER_ERROR_TRANSMISSION;
}

// Get processing statistics
void rg_wrapper_get_stats(rg_file_context_t* context, 
                         uint32_t* processed_chunks, 
                         uint32_t* total_chunks,
                         uint32_t* throughput_mbps,
                         uint64_t* elapsed_ms,
                         bool* is_complete) {
    if (!context) return;
    
    if (processed_chunks) *processed_chunks = context->processed_chunks;
    if (total_chunks) *total_chunks = context->total_chunks;
    if (is_complete) *is_complete = rg_is_complete(context->surface);
    
    if (throughput_mbps || elapsed_ms) {
        uint32_t throughput = 0;
        uint64_t elapsed = rg_get_performance_stats(context->surface, &throughput);
        
        if (throughput_mbps) *throughput_mbps = throughput;
        if (elapsed_ms) *elapsed_ms = elapsed;
    }
}

// Get reliability statistics (only for reliable mode)
void rg_wrapper_get_reliability_stats(rg_file_context_t* context,
                                     uint32_t* failed_chunks,
                                     uint32_t* retry_operations) {
    if (!context || !context->use_reliable_mode || !context->reliable_surface) {
        if (failed_chunks) *failed_chunks = 0;
        if (retry_operations) *retry_operations = 0;
        return;
    }
    
    rg_get_reliability_stats(context->reliable_surface, failed_chunks, retry_operations);
}

// Recover failed chunks (only for reliable mode)
void rg_wrapper_recover_failed_chunks(rg_file_context_t* context) {
    if (!context || !context->use_reliable_mode || !context->reliable_surface) {
        return;
    }
    
    rg_log("INFO", "Starting chunk recovery process");
    rg_recover_failed_chunks(context->reliable_surface);
}

// Cleanup file context
void rg_wrapper_cleanup_file(rg_file_context_t* context) {
    if (!context) return;
    
    rg_log("INFO", "Cleaning up file context: %s", context->filename);
    
    if (context->file_handle) {
        fclose(context->file_handle);
    }
    
    if (context->use_reliable_mode && context->reliable_surface) {
        rg_destroy_reliable_surface(context->reliable_surface);
    } else if (context->surface) {
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
    while (!rg_is_complete(context->surface)) {
        // Brief sleep to avoid busy waiting
        #ifdef _WIN32
        Sleep(10);
        #else
        usleep(10000);  // 10ms
        #endif
        
        // Optional: Add timeout logic here
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
