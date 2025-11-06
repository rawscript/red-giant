# RGTP Examples

This directory contains comprehensive examples demonstrating RGTP as a **Layer 4 transport protocol** that completely replaces TCP/UDP.

## 🔑 Key Concept

**RGTP is NOT built on top of TCP/UDP** - it's a pure Layer 4 protocol that operates directly over IP, just like TCP and UDP do.

```
Traditional Stack:          RGTP Stack:
┌─────────────────┐        ┌─────────────────┐
│   Application   │        │   Application   │
├─────────────────┤        ├─────────────────┤
│   TCP or UDP    │   →    │      RGTP       │
├─────────────────┤        ├─────────────────┤
│       IP        │        │       IP        │
├─────────────────┤        ├─────────────────┤
│    Ethernet     │        │    Ethernet     │
└─────────────────┘        └─────────────────┘
```

## 📁 Directory Structure

```
examples/
├── c/                          # C language examples
│   ├── rgtp_demo.c            # Basic RGTP functionality
│   ├── simple_exposer.c       # Simple data exposer
│   ├── simple_puller.c        # Simple data puller
│   ├── http_over_rgtp_server.c # HTTP server using RGTP transport
│   ├── http_over_rgtp_client.c # HTTP client using RGTP transport
│   ├── rgtp_layer4_demo.c     # Pure Layer 4 RGTP demonstration
│   ├── industrial_iot_rgtp.c  # Industrial IoT scenario
│   └── Makefile               # Build configuration
├── go/                        # Go language examples
├── python/                    # Python examples
├── node/                      # Node.js examples
└── README.md                  # This file
```

## 🚀 Quick Start

### Prerequisites

- Linux/macOS (Windows WSL2 supported)
- GCC compiler with C99 support
- RGTP library installed
- Root privileges (for some examples using raw sockets)

### Building Examples

```bash
cd examples/c
make all
```

### Basic Usage

**1. Simple File Transfer**
```bash
# Terminal 1: Start exposer
./simple_exposer large_file.bin

# Terminal 2: Pull the file
./simple_puller 127.0.0.1 9999 downloaded_file.bin
```

**2. HTTP over RGTP**
```bash
# Terminal 1: Start HTTP server using RGTP transport
mkdir www
echo "<h1>Hello RGTP!</h1>" > www/index.html
./http_over_rgtp_server www 8080

# Terminal 2: Download using RGTP-based HTTP client
./http_over_rgtp_client http://localhost:8080/index.html
```

**3. Industrial IoT Scenario**
```bash
# Terminal 1: Start sensor (exposer)
./industrial_iot_rgtp sensor

# Terminal 2: Start SCADA system (real-time monitoring)
./industrial_iot_rgtp scada

# Terminal 3: Start analytics system (batch processing)
./industrial_iot_rgtp analytics

# Terminal 4: Start safety system (critical alerts only)
./industrial_iot_rgtp safety
```

## 📚 Example Descriptions

### 🔧 Basic Examples

#### `rgtp_demo.c`
Demonstrates fundamental RGTP concepts:
- Creating exposer and puller sessions
- Basic data exposure and pulling
- RGTP configuration options

#### `simple_exposer.c` & `simple_puller.c`
Simple file transfer example:
- Exposer makes a file available for pulling
- Puller retrieves the file using RGTP
- Shows basic error handling and progress reporting

### 🌐 Protocol Integration Examples

#### `http_over_rgtp_server.c` & `http_over_rgtp_client.c`
HTTP protocol running over RGTP transport:
- **Server**: Serves HTTP requests using RGTP instead of TCP
- **Client**: Downloads files via HTTP over RGTP
- **Benefits**: Instant resume, natural CDN behavior, reduced server load

**Key Features:**
- Standard HTTP request/response format
- RGTP chunked transfer encoding
- Multiple clients can download same file simultaneously
- Automatic resume on connection interruption

#### `rgtp_layer4_demo.c`
Pure Layer 4 RGTP implementation:
- Uses raw IP sockets (requires root privileges)
- Shows RGTP packets directly over IP
- Demonstrates protocol-level operations
- Custom RGTP header format and packet handling

**Usage:**
```bash
# Requires root for raw sockets
sudo ./rgtp_layer4_demo exposer
sudo ./rgtp_layer4_demo puller [target_ip]
```

### 🏭 Industrial Examples

#### `industrial_iot_rgtp.c`
Real-world industrial IoT scenario:
- **Sensor**: Exposes temperature/pressure readings
- **SCADA**: Pulls real-time data for monitoring
- **Analytics**: Pulls batch data for machine learning
- **Safety**: Pulls only critical alerts with high priority

**RGTP Advantages:**
- One sensor serves all consumers simultaneously
- No connection management overhead
- Natural load balancing through pull pressure
- Different consumers can have different priorities and pull strategies

## 🎯 Key RGTP Concepts Demonstrated

### 1. **Exposure-Based Paradigm**
```c
// Traditional TCP: Push data to clients
send(client_socket, data, size, 0);

// RGTP: Expose data, let clients pull on demand
rgtp_expose_data(session, data, size);
```

### 2. **Natural Multicast**
```c
// One exposure serves unlimited pullers
rgtp_expose_file(session, "large_file.bin");
// Multiple clients can pull simultaneously without additional server load
```

### 3. **Stateless Operation**
```c
// No connection state to manage
// Clients can start/stop pulling at any time
// Server doesn't track individual client connections
```

### 4. **Instant Resume**
```c
// Client automatically resumes from where it left off
// No need to restart entire transfer
rgtp_pull_from_offset(session, last_received_chunk);
```

### 5. **Adaptive Flow Control**
```c
// Pull pressure naturally regulates data flow
// Fast clients pull more, slow clients pull less
// No explicit congestion control needed
```

## 🔧 Building and Testing

### Build All Examples
```bash
make all
```

### Run Tests
```bash
# Test basic functionality
make test-basic

# Test HTTP over RGTP
make test-http
```

### Debug Build
```bash
make debug
```

### Clean Build Files
```bash
make clean
```

## 🌟 Real-World Benefits

### **Content Distribution**
- Origin server exposes content once
- Edge servers pull chunks as needed
- Natural load balancing without coordination

### **Live Streaming**
- Encoder exposes video chunks in real-time
- Viewers pull based on buffer state and bandwidth
- Automatic quality adaptation

### **Database Replication**
- Master exposes transaction logs
- Slaves pull only needed transactions
- Instant catch-up for offline replicas

### **Peer-to-Peer Networks**
- Seeders expose file chunks
- Leechers pull from multiple sources simultaneously
- Natural swarming without central coordination

## 🚨 Important Notes

1. **Root Privileges**: Some examples (`rgtp_layer4_demo`) require root privileges for raw socket access.

2. **Firewall Configuration**: Ensure RGTP protocol (or custom protocol numbers) are allowed through firewalls.

3. **Network Requirements**: RGTP operates at Layer 4, so it requires IP connectivity between exposers and pullers.

4. **Platform Support**: Examples are designed for Linux/Unix systems. Windows support requires WSL2 or native Windows socket adaptations.

## 🤝 Contributing

To add new examples:

1. Create your example in the appropriate language directory
2. Follow the existing naming convention
3. Include comprehensive comments explaining RGTP concepts
4. Add build targets to the Makefile
5. Update this README with example description

## 📖 Further Reading

- [RGTP Protocol Specification](../docs/protocol.md)
- [API Reference](../docs/api.md)
- [Performance Guide](../docs/performance.md)
- [Main README](../README.md)

---

**Remember**: RGTP is a Layer 4 protocol that replaces TCP/UDP, not a protocol that runs on top of them. These examples demonstrate this fundamental concept through practical, real-world scenarios.