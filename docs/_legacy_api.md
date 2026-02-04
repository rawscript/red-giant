# RGTP API Reference

## Overview

The Red Giant Transport Protocol (RGTP) provides both low-level and high-level APIs for exposure-based data transmission. This document covers all available functions, structures, and usage patterns.

## Table of Contents

- [Core API](#core-api)
- [High-Level SDK](#high-level-sdk)
- [Configuration](#configuration)
- [Error Handling](#error-handling)
- [Examples](#examples)

## Core API

### Initialization

#### `int rgtp_init(void)`
Initialize the RGTP library. Must be called once per process before using any other RGTP functions.

**Returns:** 0 on success, -1 on error

#### `void rgtp_cleanup(void)`
Cleanup RGTP library resources. Should be called before process exit.

#### `const char* rgtp_version(void)`
Get the RGTP library version string.

**Returns:** Version string (e.g., "1.0.0")

### Socket Management

#### `int rgtp_socket(void)`
Create an RGTP socket for raw protocol communication.

**Returns:** Socket file descriptor on success, -1 on error

**Note:** Requires root privileges for raw socket creation.

#### `int rgtp_bind(int sockfd, uint16_t port)`
Bind an RGTP socket to a specific port.

**Parameters:**
- `sockfd`: RGTP socket file descriptor
- `port`: Port number to bind to

**Returns:** 0 on success, -1 on error

### Exposure Operations

#### `int rgtp_expose_data(int sockfd, const void* data, size_t size, struct sockaddr_in* dest, rgtp_surface_t** surface)`
Expose data for pulling by remote clients.

**Parameters:**
- `sockfd`: RGTP socket file descriptor
- `data`: Pointer to data to expose
- `size`: Size of data in bytes
- `dest`: Destination address (can be broadcast)
- `surface`: Output pointer to created exposure surface

**Returns:** 0 on success, -1 on error

#### `int rgtp_pull_data(int sockfd, struct sockaddr_in* source, void* buffer, size_t* size)`
Pull data from a remote exposer.

**Parameters:**
- `sockfd`: RGTP socket file descriptor
- `source`: Source address of exposer
- `buffer`: Buffer to store received data
- `size`: Input: buffer size, Output: actual bytes received

**Returns:** 0 on success, -1 on error

### Surface Management

#### `int rgtp_set_exposure_rate(rgtp_surface_t* surface, uint32_t chunks_per_sec)`
Set the exposure rate for adaptive flow control.

**Parameters:**
- `surface`: Exposure surface
- `chunks_per_sec`: Target chunks per second

**Returns:** 0 on success, -1 on error

#### `int rgtp_adaptive_exposure(rgtp_surface_t* surface)`
Enable adaptive exposure rate based on network conditions and receiver demand.

**Parameters:**
- `surface`: Exposure surface

**Returns:** 0 on success, -1 on error

#### `int rgtp_get_exposure_status(rgtp_surface_t* surface, float* completion_pct)`
Get exposure completion percentage.

**Parameters:**
- `surface`: Exposure surface
- `completion_pct`: Output completion percentage (0-100)

**Returns:** 0 on success, -1 on error

**Parameters:**
- `surface`: Exposure surface
- `chunks_per_sec`: Target chunks per second

**Returns:** 0 on success, -1 on error

#### `int rgtp_get_exposure_status(rgtp_surface_t* surface, float* completion_pct)`
Get exposure completion percentage.

**Parameters:**
- `surface`: Exposure surface
- `completion_pct`: Output completion percentage (0-100)

**Returns:** 0 on success, -1 on error

#### `int rgtp_adaptive_exposure(rgtp_surface_t* surface)`
Enable adaptive exposure rate based on network conditions.

**Parameters:**
- `surface`: Exposure surface

**Returns:** 0 on success, -1 on error

## High-Level SDK

### Session Management

#### `rgtp_session_t* rgtp_session_create(void)`
Create a new RGTP session with default configuration.

**Returns:** Session handle on success, NULL on error

#### `rgtp_session_t* rgtp_session_create_with_config(const rgtp_config_t* config)`
Create a new RGTP session with custom configuration.

**Parameters:**
- `config`: Configuration structure

**Returns:** Session handle on success, NULL on error

#### `void rgtp_session_destroy(rgtp_session_t* session)`
Destroy an RGTP session and free resources.

**Parameters:**
- `session`: Session to destroy

#### `int rgtp_session_expose_file(rgtp_session_t* session, const char* filename)`
Expose a file for pulling.

**Parameters:**
- `session`: RGTP session
- `filename`: Path to file to expose

**Returns:** 0 on success, -1 on error

#### `int rgtp_session_wait_complete(rgtp_session_t* session)`
Wait for exposure to complete.

**Parameters:**
- `session`: RGTP session

**Returns:** 0 on success, -1 on error or timeout

### Client Operations

#### `rgtp_client_t* rgtp_client_create(void)`
Create a new RGTP client with default configuration.

**Returns:** Client handle on success, NULL on error

#### `rgtp_client_t* rgtp_client_create_with_config(const rgtp_config_t* config)`
Create a new RGTP client with custom configuration.

**Parameters:**
- `config`: Configuration structure

**Returns:** Client handle on success, NULL on error

#### `void rgtp_client_destroy(rgtp_client_t* client)`
Destroy an RGTP client and free resources.

**Parameters:**
- `client`: Client to destroy

#### `int rgtp_client_pull_to_file(rgtp_client_t* client, const char* host, uint16_t port, const char* filename)`
Pull data from remote host and save to file.

**Parameters:**
- `client`: RGTP client
- `host`: Remote host address
- `port`: Remote port number
- `filename`: Output filename

**Returns:** 0 on success, -1 on error

### Statistics

#### `int rgtp_session_get_stats(rgtp_session_t* session, rgtp_stats_t* stats)`
Get session transfer statistics.

**Parameters:**
- `session`: RGTP session
- `stats`: Output statistics structure

**Returns:** 0 on success, -1 on error

#### `int rgtp_client_get_stats(rgtp_client_t* client, rgtp_stats_t* stats)`
Get client transfer statistics.

**Parameters:**
- `client`: RGTP client
- `stats`: Output statistics structure

**Returns:** 0 on success, -1 on error

## Configuration

### Configuration Structure

```c
typedef struct {
    uint32_t chunk_size;              // Chunk size in bytes (0 = auto)
    uint32_t exposure_rate;           // Initial exposure rate (chunks/sec)
    bool adaptive_mode;               // Enable adaptive rate control
    bool enable_compression;          // Enable data compression
    bool enable_encryption;           // Enable encryption
    uint16_t port;                   // Port number (0 = auto)
    int timeout_ms;                  // Operation timeout in milliseconds
    rgtp_progress_callback_t progress_cb; // Progress callback
    rgtp_error_callback_t error_cb;   // Error callback
    void* user_data;                 // User data for callbacks
} rgtp_config_t;
```

### Configuration Helpers

#### `void rgtp_config_default(rgtp_config_t* config)`
Initialize configuration with default values.

#### `void rgtp_config_for_lan(rgtp_config_t* config)`
Optimize configuration for LAN networks (high bandwidth, low latency).

#### `void rgtp_config_for_wan(rgtp_config_t* config)`
Optimize configuration for WAN networks (variable bandwidth, higher latency).

#### `void rgtp_config_for_mobile(rgtp_config_t* config)`
Optimize configuration for mobile networks (limited bandwidth, high latency).

#### `void rgtp_config_for_satellite(rgtp_config_t* config)`
Optimize configuration for satellite links (very high latency).

## Error Handling

### Error Codes

```c
typedef enum {
    RGTP_SUCCESS = 0,
    RGTP_ERROR_INVALID_PARAM = -1,
    RGTP_ERROR_MEMORY_ALLOC = -2,
    RGTP_ERROR_SOCKET_CREATE = -3,
    RGTP_ERROR_SOCKET_BIND = -4,
    RGTP_ERROR_NETWORK = -5,
    RGTP_ERROR_TIMEOUT = -6,
    RGTP_ERROR_FILE_NOT_FOUND = -7,
    RGTP_ERROR_PERMISSION_DENIED = -8,
    RGTP_ERROR_PROTOCOL = -9,
    RGTP_ERROR_CHECKSUM = -10
} rgtp_error_t;
```

### Error Functions

#### `const char* rgtp_error_string(int error_code)`
Get human-readable error message for error code.

**Parameters:**
- `error_code`: RGTP error code

**Returns:** Error message string

## Examples

### Simple File Exposure

```c
#include "rgtp/rgtp_sdk.h"

int main() {
    // Initialize RGTP
    rgtp_init();
    
    // Create session
    rgtp_session_t* session = rgtp_session_create();
    
    // Expose file
    rgtp_session_expose_file(session, "data.bin");
    
    // Wait for completion
    rgtp_session_wait_complete(session);
    
    // Cleanup
    rgtp_session_destroy(session);
    rgtp_cleanup();
    
    return 0;
}
```

### Simple File Pull

```c
#include "rgtp/rgtp_sdk.h"

int main() {
    // Initialize RGTP
    rgtp_init();
    
    // Create client
    rgtp_client_t* client = rgtp_client_create();
    
    // Pull file
    rgtp_client_pull_to_file(client, "192.168.1.100", 9999, "received.bin");
    
    // Cleanup
    rgtp_client_destroy(client);
    rgtp_cleanup();
    
    return 0;
}
```

### Advanced Configuration

```c
#include "rgtp/rgtp_sdk.h"

void progress_callback(size_t bytes_transferred, size_t total_bytes, void* user_data) {
    double percent = (double)bytes_transferred / total_bytes * 100.0;
    printf("Progress: %.1f%%\n", percent);
}

int main() {
    rgtp_init();
    
    // Create custom configuration
    rgtp_config_t config;
    rgtp_config_default(&config);
    rgtp_config_for_wan(&config);  // Optimize for WAN
    
    config.adaptive_mode = true;
    config.progress_cb = progress_callback;
    config.timeout_ms = 60000;  // 60 second timeout
    
    // Create session with config
    rgtp_session_t* session = rgtp_session_create_with_config(&config);
    
    // Use session...
    
    rgtp_session_destroy(session);
    rgtp_cleanup();
    
    return 0;
}
```

### Statistics Monitoring

```c
#include "rgtp/rgtp_sdk.h"
#include <unistd.h>

int main() {
    rgtp_init();
    
    rgtp_session_t* session = rgtp_session_create();
    rgtp_session_expose_file(session, "large_file.bin");
    
    // Monitor progress
    while (true) {
        rgtp_stats_t stats;
        if (rgtp_session_get_stats(session, &stats) == 0) {
            printf("Throughput: %.2f MB/s, Completion: %.1f%%\n",
                   stats.throughput_mbps, stats.completion_percent);
            
            if (stats.completion_percent >= 100.0) {
                break;
            }
        }
        sleep(1);
    }
    
    rgtp_session_destroy(session);
    rgtp_cleanup();
    
    return 0;
}
```

## Thread Safety

- RGTP sessions and clients are **not thread-safe** by default
- Use separate sessions/clients per thread, or add external synchronization
- The library initialization (`rgtp_init`) and cleanup (`rgtp_cleanup`) are thread-safe
- Statistics functions are safe to call from different threads

## Memory Management

- All RGTP objects must be explicitly destroyed using their respective destroy functions
- The library handles internal memory management automatically
- Large files are memory-mapped when possible to reduce memory usage
- Chunk buffers are pre-allocated and reused for efficiency

## Performance Considerations

- **Chunk Size**: Larger chunks = higher throughput, smaller chunks = lower latency
- **Exposure Rate**: Start conservative, let adaptive mode optimize
- **Network Type**: Use appropriate configuration helpers for your network
- **Buffer Sizes**: Increase system socket buffers for high-throughput scenarios
- **CPU Cores**: RGTP automatically uses multiple cores for chunk processing