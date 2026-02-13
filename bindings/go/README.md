# RGTP Go Bindings

This package provides Go bindings for the Red Giant Transport Protocol (RGTP), enabling high-performance UDP-based file transfer with advanced congestion control and reliability features.

## Features

- **High-level API**: Simple session and client management
- **Performance Monitoring**: Built-in statistics and metrics
- **Advanced Congestion Control**: Adaptive rate control with packet loss detection
- **Memory Efficient**: Optimized chunk handling and memory pools
- **Cross-platform**: Works on Windows, Linux, and macOS

## Installation

```bash
go get github.com/redgiant/rgtp-go
```

## Quick Start

### Sending a File

```go
package main

import (
    "fmt"
    "log"
    
    "github.com/redgiant/rgtp-go"
)

func main() {
    // Initialize RGTP
    if err := rgtp.Initialize(); err != nil {
        log.Fatal(err)
    }
    defer rgtp.Cleanup()

    // Create configuration
    config := rgtp.CreateConfig()
    config.ChunkSize = 1024 * 1024  // 1MB chunks
    config.ExposureRate = 1000      // 1000 chunks/sec

    // Send file
    stats, err := rgtp.SendFile("myfile.txt", config)
    if err != nil {
        log.Fatal(err)
    }

    fmt.Printf("Transfer completed! Throughput: %.2f Mbps\n", stats.AvgThroughputMbps)
}
```

### Receiving a File

```go
package main

import (
    "log"
    "github.com/redgiant/rgtp-go"
)

func main() {
    if err := rgtp.Initialize(); err != nil {
        log.Fatal(err)
    }
    defer rgtp.Cleanup()

    config := rgtp.CreateConfig()
    stats, err := rgtp.ReceiveFile("server.example.com", 9999, "output.txt", config)
    if err != nil {
        log.Fatal(err)
    }

    log.Printf("Received %d bytes at %.2f Mbps", stats.BytesReceived, stats.AvgThroughputMbps)
}
```

### Manual Session Management

```go
package main

import (
    "log"
    "github.com/redgiant/rgtp-go"
)

func main() {
    if err := rgtp.Initialize(); err != nil {
        log.Fatal(err)
    }
    defer rgtp.Cleanup()

    // Create session
    config := rgtp.CreateConfig()
    session, err := rgtp.CreateSession(config)
    if err != nil {
        log.Fatal(err)
    }
    defer session.Destroy()

    // Expose file
    if err := session.ExposeFile("data.bin"); err != nil {
        log.Fatal(err)
    }

    // Wait for completion
    if err := session.WaitComplete(); err != nil {
        log.Fatal(err)
    }

    // Get final statistics
    stats, err := session.GetStats()
    if err != nil {
        log.Fatal(err)
    }

    log.Printf("Transfer complete: %d bytes sent", stats.BytesSent)
}
```

## Configuration Options

The `Config` struct provides the following options:

```go
type Config struct {
    ChunkSize          uint32  // Size of data chunks (bytes)
    ExposureRate       uint32  // Initial exposure rate (chunks/sec)
    AdaptiveMode       bool    // Enable adaptive rate control
    EnableCompression  bool    // Enable data compression
    EnableEncryption   bool    // Enable encryption
    Port               uint16  // Port number (0 = auto-select)
    TimeoutMs          int     // Operation timeout (milliseconds)
    UserData           unsafe.Pointer // User data for callbacks
}
```

## Statistics

The `Stats` struct provides comprehensive transfer metrics:

```go
type Stats struct {
    BytesSent           uint64  // Total bytes sent
    BytesReceived       uint64  // Total bytes received
    ChunksSent          uint32  // Total chunks sent
    ChunksReceived      uint32  // Total chunks received
    PacketLossRate      float32 // Packet loss percentage
    RttMs               int     // Round-trip time (milliseconds)
    PacketsLost         uint32  // Number of lost packets
    Retransmissions     uint32  // Number of retransmissions
    AvgThroughputMbps   float32 // Average throughput (Mbps)
    CompletionPercent   float32 // Transfer completion percentage
    ActiveConnections   uint32  // Number of active connections
}
```

## Running Examples

To run the example code:

```bash
cd bindings/go
go run example.go
```

This will demonstrate:
1. File sending with statistics
2. File receiving (will fail without a server, but shows the API)
3. Manual session management

## Prerequisites

- Go 1.16 or later
- C compiler (GCC/Clang on Unix, Visual Studio on Windows)
- RGTP C library (automatically linked via CGO)

## Building

The bindings use CGO to interface with the C library. Standard Go build commands work:

```bash
go build
go test
go install
```

## Error Handling

All functions return Go errors for proper error handling:

```go
session, err := rgtp.CreateSession(config)
if err != nil {
    // Handle error appropriately
    log.Printf("Session creation failed: %v", err)
    return
}
```

## Performance Tips

1. **Chunk Size**: Use larger chunks (1-4MB) for better throughput
2. **Adaptive Mode**: Enable for varying network conditions
3. **Connection Reuse**: Reuse sessions/clients when possible
4. **Statistics**: Monitor `PacketLossRate` and `RttMs` for network health

## License

MIT License - see the main RGTP repository for details.