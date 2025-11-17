# HTTPS over RGTP Implementation

This directory contains examples demonstrating how to implement HTTPS (HTTP over TLS) using RGTP as the transport layer instead of TCP.

## Overview

Traditional HTTPS uses HTTP over TLS running on top of TCP. In this implementation, we replace TCP with RGTP while maintaining TLS encryption for security. This provides all the benefits of RGTP while preserving the security guarantees of HTTPS.

### Network Stack Comparison

**Traditional HTTPS over TCP:**
```
Application Layer:  HTTPS (HTTP + TLS)
Transport Layer:    TCP
Network Layer:      IP
```

**HTTPS over RGTP:**
```
Application Layer:  HTTPS (HTTP + TLS)
Transport Layer:    RGTP (replaces TCP)
Network Layer:      IP
```

## Files

1. [https_over_rgtp_server.c](https://file:examples/c/https_over_rgtp_server.c) - HTTPS server implementation using RGTP transport
2. [https_over_rgtp_client.c](https://file:examples/c/https_over_rgtp_client.c) - HTTPS client implementation using RGTP transport
3. [simple_https_server.py](https://file:examples/c/simple_https_server.py) - Simple Python HTTPS server for testing
4. [simple_https_client.py](https://file:examples/c/simple_https_client.py) - Simple Python HTTPS client for testing
5. [rgtp_https_demo.py](https://file:examples/c/rgtp_https_demo.py) - Python demo comparing standard HTTPS with RGTP HTTPS
6. [simple_https_rgtp_server.py](https://file:examples/c/simple_https_rgtp_server.py) - Simple HTTPS server using RGTP concepts

## Features

### Security
- Full TLS 1.2/1.3 encryption support using OpenSSL
- Certificate-based authentication
- Encrypted data transmission
- HSTS (HTTP Strict Transport Security) support

### Performance Benefits of RGTP
- **Instant Resume**: Downloads can be resumed from any point without reconnection
- **Multicast Support**: One file exposure can serve multiple clients simultaneously
- **Adaptive Rate Control**: Automatic adjustment of transfer speed based on network conditions
- **Out-of-Order Delivery**: Clients can request chunks in any order for optimal performance
- **Zero Head-of-Line Blocking**: Slow chunks don't block faster ones

## Python Bindings

The RGTP Python bindings now include HTTPS support:

```python
import rgtp

# Create an HTTPS client that uses RGTP as transport
client = rgtp.HTTPSClient()

# Download a file via HTTPS over RGTP
stats = client.download("https://example.com/file.html", "output.html")

print(f"Downloaded {stats.bytes_transferred} bytes")
```

## Building

### Prerequisites
- GCC compiler with C99 support
- OpenSSL development libraries
- Python 3.6+
- RGTP library built and installed

### Unix/Linux/macOS
```bash
# Build C examples
make -C ../.. examples/c/all

# Or build specific examples
make -C ../.. examples/c/https_over_rgtp_server
make -C ../.. examples/c/https_over_rgtp_client
```

### Windows
```bash
# Build with MinGW
cd examples/c
make -f Makefile.win all
```

## Running the Examples

### Start the Server
```bash
# Create test files
mkdir -p www
echo "<h1>Hello RGTP!</h1>" > www/index.html

# Start HTTPS server using RGTP transport
./https_over_rgtp_server www 8443
```

### Download a File
```bash
# Download a file
./https_rgtp_client https://localhost:8443/index.html downloaded.html
```

## Python Examples

### Simple HTTPS Client
```bash
python simple_https_client.py downloaded.html
```

### RGTP HTTPS Comparison
```bash
python rgtp_https_demo.py
```

## Benefits Over Traditional HTTPS

1. **Better Performance**: RGTP's exposure-based architecture eliminates TCP's connection overhead
2. **Improved Resilience**: Automatic resume capability without renegotiating TLS sessions
3. **Scalability**: Natural multicast support allows one server to efficiently serve many clients
4. **Flexibility**: Clients can pull data chunks in any order, enabling features like progressive video streaming
5. **Reduced Server Load**: No connection state management required

## Implementation Details

### TLS Integration

The implementation integrates TLS at the application layer:
- TLS encryption/decryption happens before/after RGTP exposure/pulling
- Each data chunk is individually encrypted/decrypted
- Session resumption is handled through RGTP's resume capabilities

### Chunk-based Transfer

Files are transferred in chunks that can be:
- Pulled in any order by the client
- Resumed from any point
- Served to multiple clients simultaneously
- Adaptively rate-controlled based on network conditions

## Future Improvements

1. **DTLS Support**: Integration with Datagram TLS for even better performance
2. **Session Resumption**: Full TLS session resumption support
3. **OCSP Stapling**: Online Certificate Status Protocol support
4. **HTTP/2 over RGTP**: Implementation of HTTP/2 using RGTP transport