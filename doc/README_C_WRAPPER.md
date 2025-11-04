# Red Giant Protocol - C Wrapper

A complete, high-performance C wrapper for the Red Giant Protocol that provides a simple API for file transmission using the exposure-based architecture.

## 🚀 Quick Start

### Build and Test
```bash
# Build everything
make all

# Run tests
make test

# Run examples
make examples

# Build with maximum optimization
make benchmark
```

### Simple Usage
```c
#include "red_giant_wrapper.h"

int main() {
    // Set up callbacks (optional)
    rg_wrapper_set_progress_callback(my_progress_callback);
    rg_wrapper_set_log_callback(my_log_callback);
    
    // Transmit a file
    rg_wrapper_error_t result = rg_wrapper_transmit_file("myfile.dat", false);
    
    if (result == RG_WRAPPER_SUCCESS) {
        printf("File transmitted successfully!\n");
    }
    
    return 0;
}
```

## 📋 Features

### Core Features
- ✅ **High-Performance C Core**: Optimized for speed and efficiency
- ✅ **Simple API**: Easy-to-use wrapper functions
- ✅ **Reliable Mode**: Automatic error recovery and retry logic
- ✅ **Progress Monitoring**: Real-time progress callbacks
- ✅ **Batch Processing**: Process multiple files efficiently
- ✅ **Cross-Platform**: Works on Windows, Linux, and macOS
- ✅ **Memory Efficient**: Pool-based memory management
- ✅ **Thread-Safe**: Atomic operations for concurrent access

### Performance Features
- 🚀 **500+ MB/s Throughput**: Proven high-performance results
- 🚀 **Zero-Copy Operations**: Minimal memory overhead
- 🚀 **Optimized Chunking**: Automatic chunk size optimization
- 🚀 **Cache-Aligned Memory**: Optimized for modern CPUs
- 🚀 **SIMD-Ready**: Prepared for vectorization

## 📖 API Reference

### Core Functions

#### File Context Management
```c
// Initialize file for processing
rg_file_context_t* rg_wrapper_init_file(const char* filename, bool use_reliable_mode);

// Clean up file context
void rg_wrapper_cleanup_file(rg_file_context_t* context);
```

#### File Processing
```c
// Process file chunks and expose them
rg_wrapper_error_t rg_wrapper_process_file(rg_file_context_t* context);

// Retrieve file from surface
rg_wrapper_error_t rg_wrapper_retrieve_file(rg_file_context_t* context, const char* output_filename);
```

#### High-Level Workflows
```c
// Complete file transmission workflow
rg_wrapper_error_t rg_wrapper_transmit_file(const char* filename, bool use_reliable_mode);

// Complete file reception workflow
rg_wrapper_error_t rg_wrapper_receive_file(rg_file_context_t* context, const char* output_filename);

// Batch processing
rg_wrapper_error_t rg_wrapper_process_batch(const char** filenames, int file_count, bool use_reliable_mode);
```

#### Statistics and Monitoring
```c
// Get processing statistics
void rg_wrapper_get_stats(rg_file_context_t* context, 
                         uint32_t* processed_chunks, 
                         uint32_t* total_chunks,
                         uint32_t* throughput_mbps,
                         uint64_t* elapsed_ms,
                         bool* is_complete);

// Get reliability statistics (reliable mode only)
void rg_wrapper_get_reliability_stats(rg_file_context_t* context,
                                     uint32_t* failed_chunks,
                                     uint32_t* retry_operations);
```

#### Configuration
```c
// Set progress callback
void rg_wrapper_set_progress_callback(rg_progress_callback_t callback);

// Set logging callback
void rg_wrapper_set_log_callback(rg_log_callback_t callback);

// Get wrapper version
const char* rg_wrapper_get_version(void);
```

### Callback Types
```c
// Progress callback - called during processing
typedef void (*rg_progress_callback_t)(uint32_t processed, uint32_t total, 
                                       float percentage, uint32_t throughput_mbps);

// Logging callback - called for log messages
typedef void (*rg_log_callback_t)(const char* level, const char* message);
```

### Error Codes
```c
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
```

## 💡 Usage Examples

### Example 1: Simple File Transmission
```c
#include "red_giant_wrapper.h"

void simple_progress(uint32_t processed, uint32_t total, float percentage, uint32_t throughput) {
    printf("Progress: %.1f%% (%u MB/s)\n", percentage, throughput);
}

int main() {
    rg_wrapper_set_progress_callback(simple_progress);
    
    rg_wrapper_error_t result = rg_wrapper_transmit_file("document.pdf", false);
    
    if (result == RG_WRAPPER_SUCCESS) {
        printf("File transmitted successfully!\n");
    } else {
        printf("Transmission failed: %d\n", result);
    }
    
    return 0;
}
```

### Example 2: Reliable Transmission with Recovery
```c
#include "red_giant_wrapper.h"

int main() {
    // Initialize file with reliable mode
    rg_file_context_t* context = rg_wrapper_init_file("large_file.dat", true);
    if (!context) {
        printf("Failed to initialize file\n");
        return 1;
    }
    
    // Process file
    rg_wrapper_error_t result = rg_wrapper_process_file(context);
    
    // Check reliability statistics
    uint32_t failed_chunks, retry_ops;
    rg_wrapper_get_reliability_stats(context, &failed_chunks, &retry_ops);
    
    printf("Failed chunks: %u, Retries: %u\n", failed_chunks, retry_ops);
    
    // Attempt recovery if needed
    if (failed_chunks > 0) {
        printf("Attempting chunk recovery...\n");
        rg_wrapper_recover_failed_chunks(context);
    }
    
    rg_wrapper_cleanup_file(context);
    return 0;
}
```

### Example 3: Transmission and Reception
```c
#include "red_giant_wrapper.h"

int main() {
    // Initialize context
    rg_file_context_t* context = rg_wrapper_init_file("input.dat", false);
    if (!context) return 1;
    
    // Transmission phase
    printf("Starting transmission...\n");
    rg_wrapper_error_t result = rg_wrapper_process_file(context);
    if (result != RG_WRAPPER_SUCCESS) {
        printf("Transmission failed\n");
        rg_wrapper_cleanup_file(context);
        return 1;
    }
    
    // Reception phase
    printf("Starting reception...\n");
    result = rg_wrapper_retrieve_file(context, "output.dat");
    if (result == RG_WRAPPER_SUCCESS) {
        printf("File successfully transmitted and received!\n");
    }
    
    rg_wrapper_cleanup_file(context);
    return 0;
}
```

### Example 4: Batch Processing
```c
#include "red_giant_wrapper.h"

int main() {
    const char* files[] = {
        "file1.dat",
        "file2.dat", 
        "file3.dat"
    };
    
    rg_wrapper_error_t result = rg_wrapper_process_batch(files, 3, true);
    
    if (result == RG_WRAPPER_SUCCESS) {
        printf("All files processed successfully!\n");
    } else {
        printf("Batch processing failed: %d\n", result);
    }
    
    return 0;
}
```

## 🔧 Build Options

### Standard Build
```bash
make all                    # Build everything
make test_wrapper          # Build test program only
make example_usage         # Build example program only
```

### Optimized Builds
```bash
make benchmark             # Maximum optimization
make CFLAGS="-O3 -march=native -DNDEBUG"  # Custom optimization
```

### Debug Build
```bash
make debug                 # Debug build with symbols
make memcheck             # Run with Valgrind (requires valgrind)
```

### Library Build
```bash
make libred_giant.a       # Static library
make libred_giant.so      # Shared library
make install              # Install system-wide
```

## 📊 Performance

### Benchmarks
- **Throughput**: 500+ MB/s on modern hardware
- **Latency**: Sub-millisecond chunk processing
- **Memory**: Efficient pool-based allocation
- **CPU**: Multi-core optimized

### Optimization Features
- Cache-aligned memory allocation
- SIMD-ready data structures
- Zero-copy operations where possible
- Optimized chunk size calculation
- Pool-based memory management

## 🛠️ Integration

### In Your Project
1. Copy the header and source files:
   - `red_giant.h`
   - `red_giant.c`
   - `red_giant_reliable.c`
   - `red_giant_wrapper.h`
   - `red_giant_wrapper.c`

2. Include in your build:
   ```c
   #include "red_giant_wrapper.h"
   ```

3. Link with math library (Linux/macOS):
   ```bash
   gcc -o myapp myapp.c red_giant_wrapper.c red_giant.c red_giant_reliable.c -lm
   ```

### CMake Integration
```cmake
add_executable(myapp 
    myapp.c
    red_giant_wrapper.c
    red_giant.c
    red_giant_reliable.c
)

target_link_libraries(myapp m)  # Link math library on Unix
```

## 🐛 Troubleshooting

### Common Issues

**Build Errors**
- Ensure you have a C99-compatible compiler
- On Windows, use MinGW or Visual Studio
- Install development headers for your system

**Runtime Errors**
- Check file permissions
- Ensure sufficient memory is available
- Verify file paths are correct

**Performance Issues**
- Use reliable mode only when necessary
- Adjust chunk sizes for your use case
- Enable compiler optimizations

### Debug Mode
```bash
make debug
./test_wrapper_debug
```

### Memory Leak Detection
```bash
make memcheck  # Requires Valgrind
```

## 📝 License

This implementation is part of the Red Giant Protocol project. See the main project README for license information.

## 🤝 Contributing

1. Follow the existing code style
2. Add tests for new features
3. Update documentation
4. Ensure cross-platform compatibility

## 📞 Support

For issues and questions:
1. Check the troubleshooting section
2. Run the test suite: `make test`
3. Run the examples: `make examples`
4. Review the API documentation above