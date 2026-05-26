# Getting Started with RGTP

This guide covers building the library from source, the core C API, and basic usage examples in C, Go, Node.js, and Python.

---

## Prerequisites

| Dependency | Version | Notes |
|------------|---------|-------|
| CMake | 3.20+ | Build system |
| C compiler | GCC 11+, Clang 14+, or MSVC 19.30+ | C17 mode |
| libsodium | 1.0.18+ | Default crypto backend |
| OpenSSL | 3.0+ | Alternative crypto backend |

Optional:
- `cppcheck` and `clang-tidy` for static analysis targets
- `io_uring` support requires Linux kernel 5.1+
- Raw Ethernet mode requires Linux `AF_PACKET` or WinPcap/Npcap on Windows
- Node.js 18 LTS, 20 LTS, or 22 LTS for the Node.js binding
- Go 1.21+ for the Go binding
- Python 3.9+ for the Python binding

---

## Building from Source

### Configure

```bash
cmake -B build \
  -DRGTP_CRYPTO_BACKEND=libsodium \   # or: openssl
  -DRGTP_ENABLE_FEC=ON \
  -DRGTP_ENABLE_RAW_ETHERNET=OFF \
  -DRGTP_ENABLE_IOURING=OFF \
  -DRGTP_ENABLE_SIMD=ON \
  -DRGTP_BUILD_TESTS=ON \
  -DRGTP_BUILD_EXAMPLES=ON \
  -DRGTP_BUILD_BINDINGS=OFF
```

### Build

```bash
cmake --build build --parallel
```

### Test

```bash
ctest --test-dir build --output-on-failure
```

### Install

```bash
cmake --install build --prefix /usr/local
```

This installs headers under `include/rgtp/`, libraries under `lib/`, and a `rgtp-config.cmake` package file for downstream `find_package(rgtp)` usage.

### Cross-Compilation (ARM)

```bash
# aarch64
cmake -B build-arm64 \
  -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/aarch64-linux-gnu.cmake \
  -DRGTP_CRYPTO_BACKEND=libsodium

# armv7 hard-float
cmake -B build-armhf \
  -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/arm-linux-gnueabihf.cmake \
  -DRGTP_CRYPTO_BACKEND=libsodium
```

### Static Analysis

```bash
cmake --build build --target analyze
```

---

## CMake Options Reference

| Option | Default | Description |
|--------|---------|-------------|
| `RGTP_CRYPTO_BACKEND` | `libsodium` | Crypto backend: `libsodium` or `openssl` |
| `RGTP_ENABLE_FEC` | `OFF` | Enable Reed-Solomon FEC subsystem |
| `RGTP_ENABLE_RAW_ETHERNET` | `OFF` | Enable `AF_PACKET` raw Ethernet mode |
| `RGTP_ENABLE_IOURING` | `OFF` | Enable io_uring I/O backend (Linux 5.1+) |
| `RGTP_ENABLE_SIMD` | `ON` | Enable SSE4.2/AVX2/NEON acceleration |
| `RGTP_BUILD_TESTS` | `OFF` | Build and register tests with CTest |
| `RGTP_BUILD_EXAMPLES` | `OFF` | Build example programs |
| `RGTP_BUILD_BINDINGS` | `OFF` | Build language bindings |
| `RGTP_MEMORY_PROFILE` | `FULL` | Memory profile: `FULL`, `EMBEDDED`, or `MINIMAL` |

---

## C API — Quick Reference

All public symbols are declared in `include/rgtp/rgtp.h`. The library uses opaque handle types; callers never embed or `sizeof` internal structs.

### Library Lifecycle

```c
#include <rgtp/rgtp.h>

// Initialize once at program startup (calls sodium_init / WSAStartup)
rgtp_error_t err = rgtp_init();
if (err != RGTP_OK) {
    fprintf(stderr, "rgtp_init: %s\n", rgtp_strerror(err));
    return 1;
}

// ... use the library ...

rgtp_cleanup();
```

### Exposing Data (Server Side)

```c
#include <rgtp/rgtp.h>

// 1. Create a socket
rgtp_config_t cfg = {
    .chunk_size   = 0,          // auto (1200 bytes for UDP)
    .window_size  = 64,
    .fec_enabled  = true,
    .fec_k        = 223,
    .fec_n        = 255,
    .merkle_proofs = true,
    .port         = 9000,
    .timeout_ms   = -1,         // block indefinitely in rgtp_poll
};

rgtp_socket_t *sock = NULL;
rgtp_error_t err = rgtp_socket_create(&cfg, &sock);
if (err != RGTP_OK) { /* handle */ }

// 2. Expose data — pre-encrypts all chunks and builds the Merkle tree
const uint8_t *data = /* your data */;
size_t data_size    = /* size in bytes */;

rgtp_surface_t *surface = NULL;
err = rgtp_expose(sock, data, data_size, &cfg, &surface);
if (err != RGTP_OK) { /* handle */ }

// Print the exposure ID so pullers can connect
uint8_t id[16];
rgtp_get_exposure_id(surface, id);
// distribute `id` and the encryption key out-of-band to pullers

// 3. Serve pull requests until done
while (/* running */) {
    err = rgtp_poll(surface, 1000 /* ms */);
    if (err != RGTP_OK && err != RGTP_ERR_TIMEOUT) { /* handle */ }
}

// 4. Tear down — zeroizes key material before freeing
rgtp_destroy_surface(surface);
rgtp_socket_destroy(sock);
```

### Pulling Data (Client Side)

```c
#include <rgtp/rgtp.h>
#include <string.h>

// 1. Create a socket
rgtp_config_t cfg = {
    .window_size  = 64,
    .timeout_ms   = 30000,
};

rgtp_socket_t *sock = NULL;
rgtp_socket_create(&cfg, &sock);

// 2. Start a pull — sends Pull_Request, receives Manifest
struct sockaddr_storage server = /* fill in exposer address */;
uint8_t exposure_id[16]        = /* received out-of-band */;

rgtp_surface_t *surface = NULL;
rgtp_error_t err = rgtp_pull_start(sock, &server, exposure_id, &cfg, &surface);
if (err != RGTP_OK) { /* handle */ }

// 3. Receive chunks as they arrive
uint8_t buf[65536];
size_t received;
uint32_t chunk_index;

while (rgtp_progress(surface) < 1.0f) {
    err = rgtp_pull_next(surface, buf, sizeof(buf), &received, &chunk_index);
    if (err == RGTP_OK) {
        // process buf[0..received-1] for chunk_index
    } else if (err == RGTP_ERR_TIMEOUT) {
        continue;
    } else {
        fprintf(stderr, "pull_next: %s\n", rgtp_strerror(err));
        break;
    }
}

// 4. Tear down
rgtp_destroy_surface(surface);
rgtp_socket_destroy(sock);
```

### Error Handling

Every function that can fail returns `rgtp_error_t`. Use `rgtp_strerror()` for human-readable messages:

```c
rgtp_error_t err = rgtp_expose(sock, data, size, &cfg, &surface);
if (err != RGTP_OK) {
    fprintf(stderr, "expose failed: %s\n", rgtp_strerror(err));
}
```

---

## Go Binding

```go
import "github.com/your-org/rgtp/bindings/go"

ctx := context.Background()

// Expose
surface, err := rgtp.Expose(ctx, sock, data, &rgtp.Config{
    FECEnabled: true,
    FEC_K:      223,
    FEC_N:      255,
})
if err != nil { log.Fatal(err) }
defer surface.Destroy()

// Pull
surface, err = rgtp.PullStart(ctx, sock, serverAddr, exposureID, &rgtp.Config{})
if err != nil { log.Fatal(err) }
defer surface.Destroy()

for surface.Progress() < 1.0 {
    chunk, index, err := surface.PullNext(ctx)
    if err != nil { log.Fatal(err) }
    _ = chunk  // process chunk data for index
}
```

All blocking operations respect `context.Context` cancellation.

---

## Node.js Binding

```javascript
const rgtp = require('rgtp');

// Expose
const sock = await rgtp.createSocket({ port: 9000 });
const surface = await rgtp.expose(sock, buffer, {
    fecEnabled: true,
    fecK: 223,
    fecN: 255,
    merkleProofs: true,
});

// Serve pull requests
await surface.poll();

// Pull (streaming)
const pullSurface = await rgtp.pullStart(sock, serverAddr, exposureId, {});
const stream = pullSurface.createReadStream();
stream.pipe(fs.createWriteStream('output.bin'));
await new Promise((resolve, reject) => {
    stream.on('end', resolve);
    stream.on('error', reject);
});
```

All long-running operations return Promises. The pull API is compatible with the Node.js `Readable` stream interface.

---

## Python Binding

```python
import asyncio
import rgtp

async def main():
    sock = await rgtp.create_socket(port=9000)

    # Expose
    surface = await rgtp.expose(sock, data, fec_enabled=True, fec_k=223, fec_n=255)
    await surface.poll()

    # Pull
    pull_surface = await rgtp.pull_start(sock, server_addr, exposure_id)
    async for chunk_index, chunk_data in pull_surface:
        process(chunk_index, chunk_data)

asyncio.run(main())
```

---

## Configuration Tips

**Chunk size** — smaller chunks reduce latency but increase per-packet overhead. For in-vehicle Ethernet use 256–512 bytes; for WAN file transfer use 1200 bytes (UDP default).

**FEC** — enable for lossy links (WiFi, 5G). Start with `fec_k=223, fec_n=255` (~14% overhead). The library adapts the parity ratio automatically based on reported loss rate.

**Merkle proofs** — enable when integrity verification per chunk is required. Adds `log2(chunk_count) × 32` bytes per chunk packet.

**io_uring** — set `RGTP_ENABLE_IOURING=ON` on Linux 5.1+ for lowest CPU overhead at high packet rates.

**SIMD** — enabled by default. Disable with `RGTP_ENABLE_SIMD=OFF` only for debugging or on platforms without SSE4.2/AVX2/NEON.

**Embedded profile** — set `RGTP_MEMORY_PROFILE=EMBEDDED` to replace all heap allocations with arena allocations. Maximum exposure size is 16 MB and maximum chunk count is 16384 in this mode.
