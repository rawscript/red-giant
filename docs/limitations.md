# RGTP Limitations and Roadmap

This document describes the current implementation status of the RGTP core overhaul, known limitations, and planned improvements.

---

## Implementation Status

The overhaul is structured as 23 numbered tasks. The table below summarizes completion by subsystem.

| Subsystem | Tasks | Status |
|-----------|-------|--------|
| CMake build system | 1 | Complete |
| Core infrastructure (`rgtp_error`, `rgtp_alloc`, `rgtp_init`) | 2.1–2.3 | Complete |
| Cryptographic subsystem (AEAD, CSPRNG, zeroization) | 3.1–3.3 | Complete |
| Merkle tree (hash, build, proof, verify) | 4.1–4.2 | Complete |
| FEC subsystem (GF(2^8), RS encode, RS decode, SIMD) | 5.1–5.4 | Complete |
| Wire protocol (packet types, serializer, parser) | 7.1–7.3 | Complete |
| Transport — anti-replay, rate limiter, flow control | 8.1, 8.3, 8.4 | Complete |
| Transport — surface, exposer, puller state machines | 9.1–9.3 | Complete |
| Sliding-window, partial pull, streaming modes | 11.1–11.3 | Complete |
| I/O layer (socket, sendmmsg, io_uring, IOCP, raw Ethernet) | 12.1–12.5 | Complete |
| Low-latency, priority scheduling, NAT traversal | 13.1–13.3 | Complete |
| Observability (logging, Prometheus metrics, OpenTelemetry) | 14.1–14.3 | Complete |
| Automotive middleware (ROS2, DDS, SOME/IP) | 16.1–16.3 | Complete |
| Language bindings (Node.js, Go, Python) | 17.1–17.3 | Complete |
| Unit test suite | 18.1–18.5 | Complete |
| Integration, fuzz, and regression tests | 19.1–19.5 | Complete |
| CI/CD pipeline and benchmark suite | 21.1–21.3 | Complete |
| Packaging (Conan, vcpkg) | 22.1 | Complete |
| Property-based tests | 3.4, 4.3, 5.5, 7.4, 8.2, 8.5, 9.4, 11.4, 17.4 | Optional — not yet implemented |
| Documentation | 22.2 | In progress |

---

## Known Limitations

### Property-Based Tests Not Yet Implemented

The 12 correctness properties defined in the design document (see `docs/protocol-spec.md` §11) have corresponding property-based test stubs in the task list but are marked optional and have not been implemented. The unit and integration test suites provide coverage, but the formal PBT harness using [Theft](https://github.com/silentbicycle/theft) is pending.

Affected test files:
- `tests/property/prop_crypto.c` (Properties 1, 7)
- `tests/property/prop_merkle.c` (Property 6)
- `tests/property/prop_fec.c` (Properties 2, 8)
- `tests/property/prop_wire.c` (Property 3)
- `tests/property/prop_replay.c` (Property 4)
- `tests/property/prop_flow.c` (Properties 5, 9)
- `tests/property/prop_streaming.c` (Properties 10, 11, 12)

### Binding Test Coverage

The language binding test suites (`bindings/node/test/`, `bindings/go/*_test.go`, `bindings/python/tests/`) are stubs. The 80% line coverage target for bindings has not been verified.

### Raw Ethernet on Windows

Raw Ethernet mode on Windows requires WinPcap or Npcap. If neither is installed, the build emits a compile-time warning and `rgtp_socket_create` returns `RGTP_ERR_NOT_SUPPORTED` for raw Ethernet configurations.

### io_uring Kernel Version Requirement

The io_uring backend requires Linux kernel 5.1 or later. On older kernels, `rgtp_socket_create` returns `RGTP_ERR_NOT_SUPPORTED` when `RGTP_ENABLE_IOURING=ON` is configured. The library falls back to `sendmmsg`/`recvmmsg` automatically.

### Embedded Memory Profile Constraints

When `RGTP_MEMORY_PROFILE=EMBEDDED` is set:
- Maximum Exposure size is 16 MB.
- Maximum chunk count is 16384.
- All heap allocations are replaced with arena allocations from the caller-supplied allocator.

These constraints are intentional for embedded/RTOS targets and are not bugs.

### Key Distribution

RGTP does not provide a key exchange mechanism. The 256-bit AEAD key must be distributed out-of-band (e.g., via TLS, a pre-shared key, or a dedicated key exchange protocol). This is by design — RGTP operates at the transport layer and does not implement its own session-layer security.

### No TCP-Style Reliable Delivery

RGTP does not guarantee delivery of every chunk. Loss recovery is provided by FEC (for burst loss within a block) and NAK-triggered re-exposure (for isolated losses). Applications requiring guaranteed delivery must implement their own acknowledgment and retry logic at the application layer, or enable FEC with sufficient parity overhead for the expected loss rate.

### Multicast Aggregation Window

The exposer collects pull requests within a 10ms window before sending each chunk once to the multicast group. This introduces up to 10ms of additional latency for the first chunk delivery in multicast mode. For latency-critical applications, use unicast mode or reduce the aggregation window via configuration.

---

## Roadmap

### Near-Term

- Implement property-based test suite (Theft framework, 12 properties, 10,000 iterations each)
- Complete binding test suites to reach ≥ 80% line coverage
- Publish Doxygen-generated C API reference
- Publish AV integration guide (raw Ethernet setup, TSN configuration, ROS2 plugin)
- Publish security guide (threat model, key management, known limitations)
- Publish performance tuning guide (chunk size, FEC params, GSO/GRO, io_uring, SIMD)

### Medium-Term

- IETF draft submission for protocol standardization
- Kernel-bypass support via DPDK for line-rate 100 GbE operation
- Multi-path support (simultaneous use of multiple network interfaces)
- QUIC transport backend as an alternative to raw UDP

### Long-Term

- Native OS integration (Linux kernel module for zero-copy in-kernel chunk serving)
- Hardware offload for AEAD encryption and FEC on SmartNICs
- Formal verification of the cryptographic subsystem using a proof assistant
