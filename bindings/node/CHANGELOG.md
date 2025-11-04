# Changelog

All notable changes to the RGTP Node.js bindings will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [1.0.0] - 2025-11-04

### Added
- Initial release of RGTP Node.js bindings
- Session class for exposing files
- Client class for pulling files
- TypeScript definitions
- Comprehensive API documentation
- Event-driven progress monitoring
- Adaptive flow control support
- Cross-platform support (Linux, macOS, Windows)
- Utility functions for formatting and configuration
- Examples and integration guides

### Features
- Natural multicast support (one exposure serves unlimited receivers)
- Instant resume capability (stateless chunk requests)
- Adaptive flow control (exposure rate matches receiver capacity)
- No head-of-line blocking (pull chunks out of order)
- Superior packet loss resilience
- High-performance native implementation