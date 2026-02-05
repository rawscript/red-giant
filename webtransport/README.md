# RGTP HTTP/3 Proxy Foundation

This directory contains the foundation for integrating RGTP (Red Giant Transport Protocol) with HTTP/3 and WebTransport technologies. The goal is to enable RGTP's exposure-based transport capabilities to be accessible through standard web protocols.

## Overview

The HTTP/3 proxy foundation provides:

- **HTTP/3 Server Interface**: A foundation for serving RGTP-exposed content via HTTP/3
- **Protocol Translation**: Bridges between RGTP's exposure model and HTTP/3 semantics
- **WebTransport Compatibility**: Foundation for future WebTransport integration
- **Proxy Architecture**: Allows existing web clients to access RGTP resources transparently

## Components

### Core Files
- `rgtp_http3_proxy.h` - Header file defining the HTTP/3 proxy API
- `rgtp_http3_proxy.c` - Implementation of the HTTP/3 proxy foundation
- `test_http3_proxy.c` - Test suite for the proxy foundation

### Key Features
- Configurable HTTP/3 server settings
- RGTP backend integration points
- Thread-safe operation
- Statistics collection
- Callback-based architecture

## Architecture

The proxy operates as a bridge between HTTP/3/WebTransport clients and RGTP backends:

```
HTTP/3 Client <---> HTTP/3 Proxy <---> RGTP Backend
```

The proxy handles:
- HTTP/3 request parsing and response formatting
- Mapping HTTP paths to RGTP exposures
- Protocol translation between HTTP/3 and RGTP
- Connection management and multiplexing

## Usage

The API provides functions for:

1. **Configuration**:
   - Setting HTTP/3 server port
   - Configuring RGTP backend connection
   - Adjusting performance parameters

2. **Lifecycle Management**:
   - Creating and destroying proxy instances
   - Starting and stopping the HTTP/3 server

3. **Route Management**:
   - Adding/removing URL routes
   - Mapping routes to RGTP exposures

4. **Statistics**:
   - Monitoring proxy performance
   - Tracking request/response metrics

## Integration Points

The foundation includes integration points for:
- Exposing RGTP files via HTTP/3 endpoints
- Pulling RGTP data and serving as HTTP/3 responses
- Future WebTransport endpoint compatibility

## Next Steps

This foundation serves as the base for:
- Full HTTP/3 implementation with QUIC transport
- WebTransport endpoint integration
- Production-ready proxy functionality
- Security enhancements (TLS, authentication)
- Performance optimizations

## Building

The proxy is designed to integrate with the main RGTP build system. Include the webtransport files in your build configuration to enable HTTP/3 proxy functionality.

## Testing

Run the test suite with:
```bash
gcc -o test_http3_proxy test_http3_proxy.c rgtp_http3_proxy.c ../src/core/rgtp_core.c -lpthread
./test_http3_proxy
```

## Status

This is a foundational implementation that provides the structure and API for HTTP/3 proxy functionality. The actual HTTP/3/QUIC implementation would require additional libraries like quiche, ngtcp2, or similar.