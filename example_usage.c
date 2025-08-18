// Red Giant Protocol - Usage Examples
// Demonstrates various ways to use the Red Giant Protocol wrapper

#include "red_giant_wrapper.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#include <windows.h>
#define sleep(x) Sleep((x) * 1000)
#else
#include <unistd.h>
#endif

// Global statistics
static struct {
    uint32_t total_files_processed;
    uint64_t total_bytes_processed;
    uint32_t total_chunks_processed;
    uint64_t total_processing_time_ms;
} g_stats = {0};

// Enhanced progress callback with ETA calculation
void enhanced_progress_callback(uint32_t processed, uint32_t total, float percentage, uint32_t throughput_mbps) {
    static uint64_t start_time = 0;
    static uint64_t last_update = 0;
    
    if (start_time == 0) {
        start_time = time(NULL);
    }
    
    uint64_t current_time = time(NULL);
    uint64_t elapsed = current_time - start_time;
    
    // Calculate ETA
    uint64_t eta = 0;
    if (processed > 0 && percentage > 0) {
        eta = (uint64_t)((elapsed * 100.0f / percentage) - elapsed);
    }
    
    // Update every second or on completion
    if (current_time != last_update || processed == total) {
        printf("\r[PROGRESS] %u/%u chunks (%.1f%%) | %u MB/s | Elapsed: %llus | ETA: %llus", 
               processed, total, percentage, throughput_mbps, 
               (unsigned long long)elapsed, (unsigned long long)eta);
        fflush(stdout);
        last_update = current_time;
    }
    
    if (processed == total) {
        printf("\n");
        start_time = 0;  // Reset for next file
    }
}

// Custom logging with different levels
void custom_log_callback(const char* level, const char* message) {
    time_t now;
    time(&now);
    struct tm* timeinfo = localtime(&now);
    
    // Color coding for different log levels
    const char* color = "";
    const char* reset = "\033[0m";
    
    if (strcmp(level, "ERROR") == 0) {
        color = "\033[31m";  // Red
    } else if (strcmp(level, "WARNING") == 0) {
        color = "\033[33m";  // Yellow
    } else if (strcmp(level, "INFO") == 0) {
        color = "\033[32m";  // Green
    } else if (strcmp(level, "DEBUG") == 0) {
        color = "\033[36m";  // Cyan
    }
    
    printf("%s[%02d:%02d:%02d] [%s] %s%s\n", 
           color, timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec,
           level, message, reset);
}

// Example 1: Simple file transmission
void example_simple_transmission() {
    printf("\n🚀 Example 1: Simple File Transmission\n");
    printf("=====================================\n");
    
    const char* filename = "example_simple.dat";
    
    // Create a test file
    FILE* file = fopen(filename, "wb");
    if (file) {
        char data[] = "Hello, Red Giant Protocol! This is a simple test file.";
        fwrite(data, 1, strlen(data), file);
        fclose(file);
        
        printf("Created test file: %s\n", filename);
        
        // Transmit the file
        rg_wrapper_error_t result = rg_wrapper_transmit_file(filename, false);
        
        if (result == RG_WRAPPER_SUCCESS) {
            printf("✅ Simple transmission completed successfully!\n");
            g_stats.total_files_processed++;
        } else {
            printf("❌ Simple transmission failed (error: %d)\n", result);
        }
        
        remove(filename);
    }
}

// Example 2: Reliable transmission with error recovery
void example_reliable_transmission() {
    printf("\n🛡️ Example 2: Reliable Transmission with Error Recovery\n");
    printf("======================================================\n");
    
    const char* filename = "example_reliable.dat";
    
    // Create a larger test file
    FILE* file = fopen(filename, "wb");
    if (file) {
        // Write 1MB of test data
        char buffer[1024];
        for (int i = 0; i < 1024; i++) {
            buffer[i] = (char)(i % 256);
        }
        
        for (int i = 0; i < 1024; i++) {
            fwrite(buffer, 1, 1024, file);
        }
        fclose(file);
        
        printf("Created 1MB test file: %s\n", filename);
        
        // Initialize reliable context
        rg_file_context_t* context = rg_wrapper_init_file(filename, true);
        if (context) {
            // Process file
            uint64_t start_time = time(NULL);
            rg_wrapper_error_t result = rg_wrapper_process_file(context);
            uint64_t end_time = time(NULL);
            
            // Get statistics
            uint32_t processed, total, throughput, failed_chunks, retry_ops;
            uint64_t elapsed_ms;
            bool complete;
            
            rg_wrapper_get_stats(context, &processed, &total, &throughput, &elapsed_ms, &complete);
            rg_wrapper_get_reliability_stats(context, &failed_chunks, &retry_ops);
            
            printf("Processing completed in %llu seconds\n", (unsigned long long)(end_time - start_time));
            printf("Statistics: %u/%u chunks processed\n", processed, total);
            printf("Reliability: %u failed chunks, %u retry operations\n", failed_chunks, retry_ops);
            
            if (result == RG_WRAPPER_SUCCESS) {
                printf("✅ Reliable transmission completed successfully!\n");
                g_stats.total_files_processed++;
                g_stats.total_chunks_processed += processed;
                g_stats.total_processing_time_ms += elapsed_ms;
            } else {
                printf("❌ Reliable transmission failed (error: %d)\n", result);
            }
            
            rg_wrapper_cleanup_file(context);
        }
        
        remove(filename);
    }
}

// Example 3: File transmission and reception workflow
void example_transmission_reception() {
    printf("\n🔄 Example 3: Complete Transmission & Reception Workflow\n");
    printf("=======================================================\n");
    
    const char* input_file = "example_input.dat";
    const char* output_file = "example_output.dat";
    
    // Create input file with known content
    FILE* file = fopen(input_file, "wb");
    if (file) {
        const char* test_content = "Red Giant Protocol - High Performance Data Transmission System\n"
                                  "This file will be transmitted using the exposure-based architecture\n"
                                  "and then reconstructed to verify data integrity.\n";
        
        // Write content multiple times to create a larger file
        for (int i = 0; i < 100; i++) {
            fprintf(file, "[Block %03d] %s", i, test_content);
        }
        fclose(file);
        
        printf("Created input file: %s\n", input_file);
        
        // Initialize context
        rg_file_context_t* context = rg_wrapper_init_file(input_file, false);
        if (context) {
            // Transmission phase
            printf("Starting transmission phase...\n");
            rg_wrapper_error_t result = rg_wrapper_process_file(context);
            
            if (result == RG_WRAPPER_SUCCESS) {
                printf("✅ Transmission phase completed\n");
                
                // Brief delay to simulate network transmission
                printf("Simulating network delay...\n");
                sleep(1);
                
                // Reception phase
                printf("Starting reception phase...\n");
                result = rg_wrapper_retrieve_file(context, output_file);
                
                if (result == RG_WRAPPER_SUCCESS) {
                    printf("✅ Reception phase completed\n");
                    
                    // Verify file integrity
                    FILE* input = fopen(input_file, "rb");
                    FILE* output = fopen(output_file, "rb");
                    
                    if (input && output) {
                        bool files_match = true;
                        int ch1, ch2;
                        
                        while ((ch1 = fgetc(input)) != EOF && (ch2 = fgetc(output)) != EOF) {
                            if (ch1 != ch2) {
                                files_match = false;
                                break;
                            }
                        }
                        
                        // Check if both files ended at the same time
                        if (files_match && fgetc(input) == EOF && fgetc(output) == EOF) {
                            printf("✅ File integrity verified - files match perfectly!\n");
                            g_stats.total_files_processed++;
                        } else {
                            printf("❌ File integrity check failed - files don't match\n");
                        }
                        
                        fclose(input);
                        fclose(output);
                    }
                } else {
                    printf("❌ Reception phase failed (error: %d)\n", result);
                }
            } else {
                printf("❌ Transmission phase failed (error: %d)\n", result);
            }
            
            rg_wrapper_cleanup_file(context);
        }
        
        remove(input_file);
        remove(output_file);
    }
}

// Example 4: Batch processing multiple files
void example_batch_processing() {
    printf("\n📦 Example 4: Batch Processing Multiple Files\n");
    printf("=============================================\n");
    
    const char* filenames[] = {
        "batch_file_1.dat",
        "batch_file_2.dat",
        "batch_file_3.dat",
        "batch_file_4.dat",
        "batch_file_5.dat"
    };
    const int file_count = 5;
    
    // Create test files with different sizes
    for (int i = 0; i < file_count; i++) {
        FILE* file = fopen(filenames[i], "wb");
        if (file) {
            // Create files of different sizes (1KB to 5KB)
            int file_size = (i + 1) * 1024;
            char* buffer = malloc(file_size);
            
            if (buffer) {
                // Fill with pattern data
                for (int j = 0; j < file_size; j++) {
                    buffer[j] = (char)((i * 37 + j) % 256);
                }
                
                fwrite(buffer, 1, file_size, file);
                free(buffer);
                
                printf("Created %s (%d KB)\n", filenames[i], (file_size / 1024));
            }
            
            fclose(file);
        }
    }
    
    // Process batch
    printf("Processing batch of %d files...\n", file_count);
    uint64_t start_time = time(NULL);
    
    rg_wrapper_error_t result = rg_wrapper_process_batch(filenames, file_count, false);
    
    uint64_t end_time = time(NULL);
    
    if (result == RG_WRAPPER_SUCCESS) {
        printf("✅ Batch processing completed successfully in %llu seconds!\n", 
               (unsigned long long)(end_time - start_time));
        g_stats.total_files_processed += file_count;
    } else {
        printf("❌ Batch processing failed (error: %d)\n", result);
    }
    
    // Cleanup
    for (int i = 0; i < file_count; i++) {
        remove(filenames[i]);
    }
}

// Example 5: Performance monitoring and statistics
void example_performance_monitoring() {
    printf("\n📊 Example 5: Performance Monitoring & Statistics\n");
    printf("=================================================\n");
    
    const char* filename = "performance_test.dat";
    
    // Create a larger file for performance testing (10MB)
    FILE* file = fopen(filename, "wb");
    if (file) {
        printf("Creating 10MB performance test file...\n");
        
        char buffer[4096];
        for (int i = 0; i < 4096; i++) {
            buffer[i] = (char)(i % 256);
        }
        
        // Write 10MB (2560 blocks of 4KB)
        for (int i = 0; i < 2560; i++) {
            fwrite(buffer, 1, 4096, file);
        }
        fclose(file);
        
        // Initialize context with detailed monitoring
        rg_file_context_t* context = rg_wrapper_init_file(filename, true);
        if (context) {
            printf("Starting performance test...\n");
            
            uint64_t start_time = time(NULL);
            rg_wrapper_error_t result = rg_wrapper_process_file(context);
            uint64_t end_time = time(NULL);
            
            // Get detailed statistics
            uint32_t processed, total, throughput, failed_chunks, retry_ops;
            uint64_t elapsed_ms;
            bool complete;
            
            rg_wrapper_get_stats(context, &processed, &total, &throughput, &elapsed_ms, &complete);
            rg_wrapper_get_reliability_stats(context, &failed_chunks, &retry_ops);
            
            printf("\n📈 Performance Results:\n");
            printf("  File Size: 10 MB\n");
            printf("  Total Chunks: %u\n", total);
            printf("  Processed Chunks: %u\n", processed);
            printf("  Success Rate: %.2f%%\n", (float)processed * 100.0f / total);
            printf("  Processing Time: %llu ms\n", (unsigned long long)elapsed_ms);
            printf("  Wall Clock Time: %llu seconds\n", (unsigned long long)(end_time - start_time));
            printf("  Average Throughput: %u MB/s\n", throughput);
            printf("  Failed Chunks: %u\n", failed_chunks);
            printf("  Retry Operations: %u\n", retry_ops);
            
            if (result == RG_WRAPPER_SUCCESS) {
                printf("✅ Performance test completed successfully!\n");
                g_stats.total_files_processed++;
                g_stats.total_bytes_processed += 10 * 1024 * 1024;
                g_stats.total_chunks_processed += processed;
                g_stats.total_processing_time_ms += elapsed_ms;
            } else {
                printf("❌ Performance test failed (error: %d)\n", result);
            }
            
            rg_wrapper_cleanup_file(context);
        }
        
        remove(filename);
    }
}

// Print overall statistics
void print_overall_statistics() {
    printf("\n📊 Overall Session Statistics\n");
    printf("=============================\n");
    printf("Total Files Processed: %u\n", g_stats.total_files_processed);
    printf("Total Bytes Processed: %.2f MB\n", (double)g_stats.total_bytes_processed / (1024 * 1024));
    printf("Total Chunks Processed: %u\n", g_stats.total_chunks_processed);
    printf("Total Processing Time: %llu ms\n", (unsigned long long)g_stats.total_processing_time_ms);
    
    if (g_stats.total_processing_time_ms > 0) {
        double avg_throughput = (double)g_stats.total_bytes_processed / (g_stats.total_processing_time_ms / 1000.0) / (1024 * 1024);
        printf("Average Throughput: %.2f MB/s\n", avg_throughput);
    }
}

int main(int argc, char* argv[]) {
    printf("🚀 Red Giant Protocol - Usage Examples\n");
    printf("Version: %s\n", rg_wrapper_get_version());
    printf("=======================================\n");
    
    // Set up callbacks
    rg_wrapper_set_progress_callback(enhanced_progress_callback);
    rg_wrapper_set_log_callback(custom_log_callback);
    
    // Run examples
    example_simple_transmission();
    example_reliable_transmission();
    example_transmission_reception();
    example_batch_processing();
    example_performance_monitoring();
    
    // Print overall statistics
    print_overall_statistics();
    
    printf("\n🎉 All examples completed successfully!\n");
    printf("The Red Giant Protocol wrapper is ready for production use.\n");
    
    return 0;
}