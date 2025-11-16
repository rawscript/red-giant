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

### Additional Features
- Directory traversal protection
- MIME type detection
- Content-Length reporting
- HTTP status codes
- Error handling

## Prerequisites

- OpenSSL development libraries
- RGTP library
- C compiler (GCC/Clang)

## Building

```bash
# Compile the server
gcc -o https_rgtp_server https_over_rgtp_server.c -lrgtp -lssl -lcrypto

# Compile the client
gcc -o https_rgtp_client https_over_rgtp_client.c -lrgtp -lssl -lcrypto
```

## Usage

### Server

1. Generate a self-signed certificate (for testing):
```bash
openssl req -x509 -newkey rsa:4096 -keyout server.key -out server.crt -days 365 -nodes
```

2. Create a document root directory:
```bash
mkdir www
echo "<html><body><h1>Hello RGTP!</h1></body></html>" > www/index.html
```

3. Run the server:
```bash
./https_rgtp_server ./www 8443
```

### Client

```bash
# Download a file
./https_rgtp_client https://localhost:8443/index.html downloaded.html
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

### Security Considerations

1. **Certificate Validation**: The client example skips certificate validation for simplicity. In production, proper certificate validation should be implemented.
2. **Key Management**: Secure storage and rotation of private keys is essential.
3. **Cipher Suite Selection**: Modern, secure cipher suites should be configured.

## Limitations

1. **Certificate Management**: Requires proper certificate management infrastructure
2. **Client Support**: Standard web browsers don't support RGTP natively
3. **Firewall Compatibility**: Some firewalls may block raw IP protocols

## Use Cases

1. **Secure File Distribution**: Distributing large files securely to multiple clients
2. **IoT Data Transfer**: Secure telemetry data transmission from IoT devices
3. **Content Delivery Networks**: Efficient, secure content distribution
4. **Enterprise File Sharing**: Secure internal file sharing with better performance than traditional HTTPS

## Future Improvements

1. **DTLS Support**: Integration with Datagram TLS for even better performance
2. **Session Resumption**: Full TLS session resumption support
3. **OCSP Stapling**: Online Certificate Status Protocol support
4. **HTTP/2 over RGTP**: Implementation of HTTP/2 using RGTP transport