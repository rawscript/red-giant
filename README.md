# Red Giant Transport Protocol (RGTP)

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)
[![C17](https://img.shields.io/badge/language-C17-blue)](#)
[![CMake 3.20+](https://img.shields.io/badge/build-CMake%203.20%2B-green)](#building)
[![Platforms](https://img.shields.io/badge/platforms-Linux%20%7C%20macOS%20%7C%20Windows%20%7C%20ARM-lightgrey)](#platform-support)

RGTP is a stateless, receiver-driven, chunk-based, pre-encrypted, Merkle-verified, FEC-protected data transport protocol operating over UDP and raw Ethernet. It is designed for three primary environments: general-purpose high-bandwidth file distribution, deterministic low-latency autonomous vehicle (AV) in-vehicle networks, and satellite/space communications with CCSDS protocol support.

---

## Design Principles

- **Stateless exposer** — the exposer holds only an immutable chunk store. Any number of pullers can connect and disconnect without affecting the exposer's memory footprint or correctness.
- **Receiver-driven** — pullers control the data rate by issuing pull requests. The exposer never pushes unsolicited data.
- **Pre-encryption** — data is encrypted once at expose time using AEAD (ChaCha20-Poly1305-IETF or AES-256-GCM). Subsequent pull requests are served from the encrypted store without re-encryption.
- **Merkle integrity** — a BLAKE2b-256 / SHA-256 Merkle tree over plaintext chunk hashes enables per-chunk integrity verification after decryption.
- **Optional FEC** — systematic Reed-Solomon over GF(2^8) with adaptive strength; recovers from burst packet loss without retransmission.
- **Satellite communications** — native CCSDS protocol support with TM/TC, AOS, and CFDP; adaptive modulation, Doppler compensation, contact window scheduling, and store-and-forward for intermittent links.
- **ABI stability** — all public structs are opaque handles; callers never embed or `sizeof` internal types.

---

## Feature Matrix

| Feature | Status |
|---------|--------|
| CMake 3.20+ build system | ✅ Complete |
| C17 library (static + shared) | ✅ Complete |
| ChaCha20-Poly1305-IETF / AES-256-GCM AEAD | ✅ Complete |
| CSPRNG Exposure IDs | ✅ Complete |
| BLAKE2b-256 / SHA-256 Merkle tree + proofs | ✅ Complete |
| Reed-Solomon FEC over GF(2^8) with SIMD | ✅ Complete |
| Adaptive FEC strength (loss-rate driven) | ✅ Complete |
| Wire protocol — 8 packet types, big-endian | ✅ Complete |
| Non-allocating packet parser state machine | ✅ Complete |
| AIMD congestion control + RTT EWMA | ✅ Complete |
| Anti-replay window (256-bit sliding bitmap) | ✅ Complete |
| Per-source token-bucket rate limiter | ✅ Complete |
| Sliding-window pull + partial pull + streaming | ✅ Complete |
| Out-of-order delivery tolerance | ✅ Complete |
| sendmmsg / recvmmsg batching (Linux 4.14+) | ✅ Complete |
| io_uring backend (Linux 5.1+) | ✅ Complete |
| IOCP backend (Windows) | ✅ Complete |
| AF_PACKET raw Ethernet + TSN 802.1Q | ✅ Complete |
| Priority scheduling (levels 0–7) + jitter buffer | ✅ Complete |
| STUN hole punching + TURN relay fallback | ✅ Complete |
| Embedded memory profile (arena allocator) | ✅ Complete |
| Prometheus metrics registry | ✅ Complete |
| OpenTelemetry span emission | ✅ Complete |
| Structured logging with runtime level control | ✅ Complete |
| ROS2 rmw transport plugin | ✅ Complete |
| DDS/RTPS adapter | ✅ Complete |
| SOME/IP service discovery adapter | ✅ Complete |
| Satellite communications module | ✅ Complete |
| CCSDS TM/TC protocol support | ✅ Complete |
| CCSDS AOS frame handling | ✅ Complete |
| CCSDS CFDP file delivery | ✅ Complete |
| Doppler shift compensation | ✅ Complete |
| Link quality monitoring (SNR/BER) | ✅ Complete |
| Contact window scheduling | ✅ Complete |
| Store-and-forward buffer | ✅ Complete |
| Link budget calculations | ✅ Complete |
| Node.js N-API binding (Promise + Readable stream) | ✅ Complete |
| Go CGo binding (context.Context-aware) | ✅ Complete |
| Python C extension binding (asyncio) | ✅ Complete |
| Unit test suite (55 tests) | ✅ Complete |
| Integration + fuzz + regression tests | ✅ Complete |
| Property-based tests (12 properties) | ✅ Complete |
| Binding test suites (Node.js, Go, Python) | ✅ Complete |
| CI/CD matrix (Linux, macOS, Windows, ARM) | ✅ Complete |
| Benchmark suite | ✅ Complete |
| Conan + vcpkg packaging | ✅ Complete |

---

## Architecture

```
┌──────────────────────────────────────────────────────────────┐
│              Application Layer                               │
│     C / Go / Node.js / Python                                │
└──────────────────┬───────────────────────────────────────────┘
                   │
┌──────────────────▼───────────────────────────────────────────┐
│              Public C API  (include/rgtp/rgtp.h)             │
│  rgtp_expose · rgtp_poll · rgtp_pull_start · rgtp_pull_next  │
└──────────────────┬───────────────────────────────────────────┘
                   │
┌──────────────────▼───────────────────────────────────────────┐
│              Transport Layer                                 │
│  Exposer · Puller · AIMD · Anti-Replay · Rate Limiter        │
├──────────────────────────────────────────────────────────────┤
│              Wire Protocol Layer                             │
│  Parser · Serializer · 8 Packet Types                        │
├──────────────────────────────────────────────────────────────┤
│  Cryptographic Layer          │  FEC Layer                   │
│  AEAD · Merkle · CSPRNG       │  GF(2^8) · RS · SIMD         │
├──────────────────────────────────────────────────────────────┤
│              I/O Layer                                       │
│  UDP · Raw Ethernet · sendmmsg · io_uring · IOCP             │
├──────────────────────────────────────────────────────────────┤
│              Observability                                   │
│  Prometheus · OpenTelemetry · Structured Logging             │
└──────────────────────────────────────────────────────────────┘
```

See [docs/architecture.md](docs/architecture.md) for full Mermaid diagrams covering all subsystems, state machines, and data flows.

---

## Building

### Prerequisites

| Dependency | Version | Notes |
|------------|---------|-------|
| CMake | 3.20+ | Required |
| C compiler | GCC 11+, Clang 14+, MSVC 19.30+ | C17 mode |
| libsodium | 1.0.18+ | Default crypto backend |
| OpenSSL | 3.0+ | Alternative crypto backend |

### Quick Build

```bash
cmake -B build \
  -DRGTP_CRYPTO_BACKEND=libsodium \
  -DRGTP_ENABLE_FEC=ON \
  -DRGTP_BUILD_TESTS=ON \
  -DRGTP_BUILD_EXAMPLES=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

### Install

```bash
cmake --install build --prefix /usr/local
```

Installs headers to `include/rgtp/`, libraries to `lib/`, and a `rgtp-config.cmake` package file for downstream `find_package(rgtp)` usage.

### Cross-Compilation (ARM)

```bash
# aarch64
cmake -B build-arm64 \
  -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/aarch64-linux-gnu.cmake \
  -DRGTP_CRYPTO_BACKEND=libsodium

# armv7 hard-float
cmake -B build-armhf \
  -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/arm-linux-gnueabihf.cmake
```

### CMake Options

| Option | Default | Description |
|--------|---------|-------------|
| `RGTP_CRYPTO_BACKEND` | `libsodium` | `libsodium` or `openssl` |
| `RGTP_ENABLE_FEC` | `OFF` | Reed-Solomon FEC subsystem |
| `RGTP_ENABLE_RAW_ETHERNET` | `OFF` | AF_PACKET raw Ethernet mode |
| `RGTP_ENABLE_IOURING` | `OFF` | io_uring backend (Linux 5.1+) |
| `RGTP_ENABLE_SIMD` | `ON` | SSE4.2 / AVX2 / NEON acceleration |
| `RGTP_ENABLE_SATELLITE` | `ON` | Satellite communications module with CCSDS |
| `RGTP_BUILD_TESTS` | `OFF` | Build and register tests with CTest |
| `RGTP_BUILD_EXAMPLES` | `OFF` | Build example programs |
| `RGTP_BUILD_BINDINGS` | `OFF` | Build language bindings |
| `RGTP_MEMORY_PROFILE` | `FULL` | `FULL`, `EMBEDDED`, or `MINIMAL` |

---

## C API — Quick Start

All public symbols are in `include/rgtp/rgtp.h`. The library uses opaque handle types.

### Exposer (server side)

```c
#include <rgtp/rgtp.h>

rgtp_init();

rgtp_config_t cfg = {
    .chunk_size    = 0,
    .window_size   = 64,
    .fec_enabled   = true,
    .fec_k         = 223,
    .fec_n         = 255,
    .merkle_proofs = true,
    .port          = 9000,
    .satellite_mode = false,
};

rgtp_socket_t  *sock    = NULL;
rgtp_surface_t *surface = NULL;

rgtp_socket_create(&cfg, &sock);
rgtp_expose(sock, data, data_size, &cfg, &surface);

uint8_t id[16];
rgtp_get_exposure_id(surface, id);

while (running) {
    rgtp_poll(surface, 1000);
}

rgtp_destroy_surface(surface);
rgtp_socket_destroy(sock);
rgtp_cleanup();
```

### Satellite Communications Mode

```c
#include <rgtp/rgtp.h>

rgtp_init();

rgtp_config_t cfg = {
    .chunk_size    = 1200,
    .window_size   = 32,
    .fec_enabled   = true,
    .fec_k         = 200,
    .fec_n         = 255,
    .merkle_proofs = true,
    .satellite_mode = true,
    .space_link_type = RGTP_SPACE_LINK_SBAND,
    .max_rtt_ms = 2000,
    .link_asymmetry = 10.0f,
    .store_and_forward = true,
    .ccsds_tm = true,
    .ccsds_tc = true,
    .ccsds_aos = true,
    .apid = 0x3E0,
    .spacecraft_id = 42,
    .min_snr_db = 10.0f,
    .max_ber = 0.001f,
    .ground_station = "GS-MADRID",
};

rgtp_socket_t  *sock    = NULL;
rgtp_surface_t *surface = NULL;

rgtp_socket_create(&cfg, &sock);
rgtp_expose(sock, data, data_size, &cfg, &surface);

uint64_t contact_start = time(NULL) + 300;
uint64_t contact_end = contact_start + 600;
rgtp_schedule_contact(surface, contact_start, contact_end, "GS-MADRID");

rgtp_update_link_parameters(surface, 12.5f, 0.0002f, 5000);

float margin_db, ebno_db;
rgtp_calculate_link_budget(surface, &margin_db, &ebno_db);

rgtp_satellite_stats_t sat_stats;
rgtp_get_satellite_stats(surface, &sat_stats);

while (running) {
    bool in_contact;
    uint32_t time_to_contact, time_left;
    rgtp_check_contact_status(surface, &in_contact, &time_to_contact, &time_left);
    
    if (in_contact) {
        rgtp_poll(surface, 1000);
    }
}

rgtp_destroy_surface(surface);
rgtp_socket_destroy(sock);
rgtp_cleanup();
```

### Puller (client side)

```c
#include <rgtp/rgtp.h>

rgtp_init();

rgtp_config_t cfg = { .window_size = 64, .timeout_ms = 30000 };
rgtp_socket_t  *sock    = NULL;
rgtp_surface_t *surface = NULL;

rgtp_socket_create(&cfg, &sock);
rgtp_pull_start(sock, &server_addr, exposure_id, &cfg, &surface);

uint8_t  buf[65536];
size_t   received;
uint32_t chunk_index;

while (rgtp_progress(surface) < 1.0f) {
    if (rgtp_pull_next(surface, buf, sizeof(buf),
                       &received, &chunk_index) == RGTP_OK) {
        /* process buf[0..received-1] for chunk_index */
    }
}

rgtp_destroy_surface(surface);
rgtp_socket_destroy(sock);
rgtp_cleanup();
```

---

## Language Bindings

### Go

```go
import "github.com/rawscript/rgtp/bindings/go"

rgtp.Init()
sock, _ := rgtp.NewSocket()
surface, _ := rgtp.Expose(ctx, sock, data)
defer surface.Close()

for surface.Progress() < 1.0 {
    result, _ := rgtp.PullNext(ctx, surface, 65536)
    _ = result.Data   // chunk bytes
}
```

### Node.js

```javascript
const rgtp = require('rgtp');

// Expose
const sock = await rgtp.createSocket({ port: 9000 });
const surface = await rgtp.expose(sock, buffer, { fecEnabled: true });
await surface.poll();

// Pull (streaming)
const pullSurface = await rgtp.pullStart(sock, serverAddr, exposureId, {});
pullSurface.createReadStream().pipe(fs.createWriteStream('output.bin'));
```

### Python

```bash
pip install rgtp
```

```python
import rgtp

rgtp.init()

# Expose
with rgtp.Socket() as sock:
    data = open("large-file.bin", "rb").read()
    with rgtp.expose(sock, data) as surface:
        print("Exposure ID:", surface.exposure_id().hex())
        while True:
            rgtp.poll(surface, timeout_ms=1000)

# Pull
with rgtp.Socket() as sock:
    surface = rgtp.pull_start(sock, ("192.168.1.10", 9000), exposure_id)
    with surface:
        chunks = {}
        while surface.progress() < 1.0:
            data, idx = rgtp.pull_next(surface)
            chunks[idx] = data

rgtp.cleanup()
```

CLI tools installed with the package:

```bash
rgtp-expose large-file.bin --port 9000 --fec
rgtp-pull 192.168.1.10:9000 <exposure-id-hex> output.bin
```

---

## Platform Support

| Platform | Compiler | I/O Backend | Status |
|----------|----------|-------------|--------|
| Linux (x86-64) | GCC 11+, Clang 14+ | io_uring, sendmmsg | Full support |
| Linux (aarch64) | GCC 11+ cross | sendmmsg | Full support |
| Linux (armv7hf) | GCC 11+ cross | sendmmsg | Full support |
| macOS 13+ | Apple Clang 15+ | sendto/recvfrom | Full support |
| Windows Server 2022 | MSVC 19.38+ | IOCP | Full support |
| Raw Ethernet (Linux) | Any | AF_PACKET | Full support |
| Raw Ethernet (Windows) | Any | WinPcap/Npcap | Requires WinPcap/Npcap |
| Satellite/Space Links | Any | CCSDS native | Full support with RGTP_ENABLE_SATELLITE |

---

## Testing

```bash
# Full test suite
cmake -B build -DRGTP_BUILD_TESTS=ON -DRGTP_ENABLE_FEC=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure

# Static analysis
cmake --build build --target analyze

# Binding tests
cd bindings/node  && npm test
cd bindings/go    && go test ./...
cd bindings/python && python -m pytest tests/ -v
```

### Test Coverage

| Suite | Tests | Coverage Target |
|-------|-------|----------------|
| Unit tests | 55 | Greater than or equal to 90 percent line |
| Integration tests | 9 transfer + impairment + version + error recovery | Greater than or equal to 90 percent line |
| Property-based tests | 12 properties with 1000-10000 iterations | All 12 properties |
| Fuzz targets | parser, Merkle verifier, FEC decoder, pull handler | Continuous |
| Regression tests | 1 per cataloged bug | All 24 bugs |
| Binding tests | Node.js 35, Go 30, Python 50+ | Greater than or equal to 80 percent line |
| Satellite tests | 6 comprehensive satellite feature tests | Greater than or equal to 85 percent line |

---

## Packaging

### Conan

```bash
conan install rgtp/1.0.0@ --build=missing
```

### vcpkg

```bash
vcpkg install rgtp
```

### CMake find_package

```cmake
find_package(rgtp REQUIRED)
target_link_libraries(my_app PRIVATE rgtp::rgtp)
```

---

## Documentation

| Document | Description |
|----------|-------------|
| [docs/getting-started.md](docs/getting-started.md) | Build instructions and API quick-start for C, Go, Node.js, Python |
| [docs/architecture.md](docs/architecture.md) | Mermaid diagrams: layered architecture, state machines, data flows |
| [docs/protocol-spec.md](docs/protocol-spec.md) | Wire format, packet types, crypto design, FEC, flow control |
| [docs/limitations.md](docs/limitations.md) | Platform constraints, design trade-offs, roadmap |

---

## Security

RGTP's security model:

- **Confidentiality** — AEAD encryption (ChaCha20-Poly1305-IETF or AES-256-GCM) applied per-chunk at expose time.
- **Integrity** — AEAD authentication tag verified before any plaintext is delivered. Merkle proof optionally verifies each chunk against the tree root.
- **Replay protection** — 256-bit sliding anti-replay window per puller session.
- **Key material** — zeroized with `sodium_memzero` / `OPENSSL_cleanse` before `free`. Never transmitted over the wire.
- **DoS mitigation** — per-source token-bucket rate limiter (1,000 req/s per Exposure). Bounded per-surface data structures prevent unbounded memory growth.
- **Memory safety** — zero sanitizer errors under ASan/UBSan/TSan across the full test suite.

See [SECURITY.md](SECURITY.md) for the vulnerability reporting policy.

---

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md) for the development workflow, code style, and PR process.

---

## License

MIT — see [LICENSE](LICENSE).
