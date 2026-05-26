# RGTP Documentation

Welcome to the Red Giant Transport Protocol (RGTP) documentation.

## Quick Navigation

- [Getting Started](getting-started.md)
- [Architecture & Diagrams](architecture.md)
- [Protocol Specification](protocol-spec.md)
- [Limitations & Roadmap](limitations.md)

## What is RGTP?

RGTP (Red Giant Transport Protocol) is a stateless, receiver-driven, chunk-based, pre-encrypted, Merkle-verified, FEC-protected data transport protocol operating over UDP and raw Ethernet. It is designed for two primary environments: general-purpose high-bandwidth file distribution and deterministic low-latency autonomous vehicle (AV) in-vehicle networks.

The core design is built around a strict separation of roles:

- **Exposer** — pre-encrypts data once, builds a Merkle tree, and serves chunks from an immutable store in response to pull requests. Holds zero per-puller state.
- **Puller** — drives all flow and congestion control by issuing pull requests. All transfer state lives in the puller.

## Key Properties

- **Stateless exposer** — any number of pullers can connect and disconnect without affecting the exposer's memory footprint or correctness
- **Receiver-driven flow control** — pullers apply backpressure naturally; the exposer never pushes unsolicited data
- **Pre-encryption** — data is encrypted once at expose time using AEAD (ChaCha20-Poly1305-IETF or AES-256-GCM); subsequent pull requests are served from the encrypted store
- **Merkle integrity** — a BLAKE2b-256 / SHA-256 Merkle tree over plaintext chunk hashes enables per-chunk integrity verification after decryption
- **Optional FEC** — systematic Reed-Solomon over GF(2^8) with adaptive strength; recovers from burst packet loss without retransmission
- **Raw Ethernet mode** — operates directly over `AF_PACKET` frames for sub-millisecond in-vehicle latency, with TSN/802.1Q VLAN and PCP support
- **Zero-copy paths** — `MSG_ZEROCOPY`, `sendmmsg`/`recvmmsg` batching, and io_uring backend on Linux
- **Cross-platform** — Linux, macOS, Windows, and embedded ARM via a single CMake configuration

## Architecture

See [Architecture & Diagrams](architecture.md) for full Mermaid diagrams covering:

- Layered architecture and module decomposition
- Exposer and puller data flows (sequence diagrams)
- Wire protocol packet flow (all 8 packet types)
- Cryptographic pipeline (encrypt at expose time, decrypt+verify at pull time)
- AIMD flow control and adaptive FEC feedback loop
- I/O backend selection decision tree
- FEC encode/decode pipeline
- Automotive middleware integration (ROS2, DDS, SOME/IP)
- Observability pipeline (Prometheus, OpenTelemetry, structured logging)
- Exposer and puller state machines
- Module dependency graph

## Language Bindings

| Language | Module | Notes |
|----------|--------|-------|
| C | `include/rgtp/rgtp.h` | Primary API, C17 |
| Go | `bindings/go/` | CGo, `context.Context`-aware |
| Node.js | `bindings/node/` | N-API, Promise-based, `Readable` stream |
| Python | `bindings/python/` | C extension, `asyncio` async/await |

## Building

```bash
cmake -B build \
  -DRGTP_CRYPTO_BACKEND=libsodium \
  -DRGTP_ENABLE_FEC=ON \
  -DRGTP_BUILD_TESTS=ON \
  -DRGTP_BUILD_EXAMPLES=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

See [Getting Started](getting-started.md) for full build instructions and configuration options.

## License

MIT License — see `LICENSE` in the repository root.
