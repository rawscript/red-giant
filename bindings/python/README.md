# rgtp — Python bindings for the Red Giant Transport Protocol

[![PyPI version](https://badge.fury.io/py/rgtp.svg)](https://pypi.org/project/rgtp/)
[![Python 3.9+](https://img.shields.io/badge/python-3.9%2B-blue)](https://pypi.org/project/rgtp/)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](../../LICENSE)

RGTP is a stateless, receiver-driven, chunk-based, pre-encrypted, Merkle-verified,
FEC-protected data transport protocol over UDP and raw Ethernet.

This package provides Python 3.9+ bindings via ctypes, with full `asyncio` async/await support.

**Version 1.2.0** includes complete bindings for all RGTP features, including satellite communications.

## Installation

```bash
pip install rgtp
```

The package bundles the `librgtp` shared library and works out of the box. No manual library installation required.

### Advanced Installation Options

If you have a custom build of librgtp:

```bash
# Use a pre-built library
export RGTP_LIB_PATH=/path/to/librgtp.so
pip install rgtp

# Or after installation
export RGTP_LIB_PATH=/path/to/librgtp.so
python -c "import rgtp; print(rgtp.version())"
```

## Quick Start

### Expose data (server side)

```python
import rgtp

rgtp.init()

with rgtp.Socket() as sock:
    data = open("large-file.bin", "rb").read()
    with rgtp.expose(sock, data) as surface:
        exposure_id = surface.exposure_id()
        print(f"Exposure ID: {exposure_id.hex()}")
        # distribute exposure_id and key out-of-band to pullers

        # Serve pull requests until done
        while True:
            rgtp.poll(surface, timeout_ms=1000)

rgtp.cleanup()
```

### Pull data (client side)

```python
import rgtp

rgtp.init()

with rgtp.Socket() as sock:
    # Exposure ID received out-of-band
    exposure_id = bytes.fromhex("0123456789abcdef0123456789abcdef")
    
    surface = rgtp.pull_start(sock, ("192.168.1.10", 9000), exposure_id)
    with surface:
        chunks = {}
        while surface.progress() < 1.0:
            data, chunk_index = rgtp.pull_next(surface)
            chunks[chunk_index] = data
            print(f"Received chunk {chunk_index}: {len(data)} bytes")
    
    # Reassemble chunks in order
    output = b"".join(chunks[i] for i in sorted(chunks))

rgtp.cleanup()
```

### Async/await

```python
import asyncio
import rgtp

async def expose_file(path: str) -> None:
    rgtp.init()
    data = open(path, "rb").read()
    with rgtp.Socket() as sock:
        surface = await rgtp.async_expose(sock, data)
        with surface:
            print(f"Exposure ID: {surface.exposure_id().hex()}")
            while True:
                await rgtp.async_poll(surface, timeout_ms=100)

asyncio.run(expose_file("large-file.bin"))
```

## CLI

The package installs two command-line tools:

```bash
# Expose a file
rgtp-expose large-file.bin --port 9000

# Pull a file
rgtp-pull 192.168.1.10:9000 0123456789abcdef0123456789abcdef output.bin
```

## API Reference

### Library lifecycle

```python
rgtp.init()              # Must be called once before any other function
rgtp.cleanup()           # Release all global resources
rgtp.version()           # Returns version string e.g. "1.2.0"
rgtp.strerror(code)      # Human-readable error description
rgtp.is_initialized()    # Check if library is initialized (returns bool)
```

### Socket

```python
sock = rgtp.Socket()     # Create and bind a UDP socket
sock.close()             # Destroy socket (also called by context manager)
```

### Exposer

```python
surface = rgtp.expose(sock, data: bytes) -> rgtp.Surface
rgtp.poll(surface, timeout_ms=100)        # Serve pull requests
surface.exposure_id() -> bytes            # 16-byte Exposure_ID
surface.progress() -> float               # Always 0.0 for exposer
surface.close()                           # Zeroize key and free
```

### Puller

```python
surface = rgtp.pull_start(sock, (host, port), exposure_id: bytes)
data, chunk_index = rgtp.pull_next(surface, buf_size=65536)
surface.progress() -> float               # [0.0, 1.0]
```

### Statistics

```python
stats = rgtp.get_stats(surface)
# Returns dict with:
#   - bytes_sent, bytes_received
#   - chunks_sent, chunks_received
#   - auth_failures, malformed_packets, fec_recoveries
#   - nak_sent, packet_loss_rate, rtt_us, pull_pressure

latency = rgtp.get_latency_stats(surface)
# Returns dict with:
#   - mean_us, jitter_us, p99_us, min_us, max_us, sample_count
```

### Satellite Communications

```python
# Get satellite-specific statistics
sat_stats = rgtp.get_satellite_stats(surface)
# Returns dict with:
#   - snr_db, ber, link_margin_db, doppler_offset_hz
#   - contact_attempts, successful_contacts, contact_duration_s
#   - stored_bytes, stored_chunks, store_overflow_count
#   - spacecraft_health, power_level, antenna_status
#   - ccsds_frames_sent, ccsds_frames_received, ccsds_frame_errors

# Schedule a contact window
rgtp.schedule_contact(surface, start_time, end_time, "ground_station_id")

# Update link parameters for adaptive coding/modulation
rgtp.update_link_parameters(surface, snr_db=15.0, ber=1e-6, doppler_hz=50000)

# Enable store-and-forward for intermittent connectivity
rgtp.enable_store_forward(surface, max_storage_bytes=1_000_000, max_storage_time_s=3600)

# Get available contact windows
windows = rgtp.get_contact_windows(surface, max_windows=10)

# Configure CCSDS protocols
rgtp.configure_ccsds(surface, enable_tm=True, enable_tc=True)

# Send emergency telecommand
rgtp.send_emergency_tc(surface, tc_data=b"\x01\x02\x03", priority=200)

# Calculate link budget
margin_db, ebno_db = rgtp.calculate_link_budget(surface)

# Configure Doppler compensation
rgtp.configure_doppler(surface, enable_compensation=True, max_doppler_hz=100000, update_rate_hz=10.0)

# Check contact status
in_contact, time_to_contact, time_left = rgtp.check_contact_status(surface)
```

### Async wrappers

```python
surface = await rgtp.async_expose(sock, data)
data, idx = await rgtp.async_pull_next(surface, buf_size=65536)
await rgtp.async_poll(surface, timeout_ms=100)
```

### Exceptions

```python
try:
    rgtp.init()
except rgtp.RgtpError as e:
    print(e.code)     # e.g. -4 (RGTP_ERR_CRYPTO_INIT)
    print(e.message)  # human-readable description
```

## Error Codes

| Code | Constant | Meaning |
|------|----------|---------|
| 0 | `RgtpError.OK` | Success |
| -1 | `RgtpError.ERR_NOMEM` | Memory allocation failed |
| -2 | `RgtpError.ERR_INVALID_ARG` | Invalid argument |
| -3 | `RgtpError.ERR_SOCKET` | Socket operation failed |
| -4 | `RgtpError.ERR_CRYPTO_INIT` | Crypto library init failed |
| -5 | `RgtpError.ERR_ENCRYPT` | AEAD encryption failed |
| -6 | `RgtpError.ERR_DECRYPT` | AEAD decryption failed |
| -7 | `RgtpError.ERR_AUTH_FAIL` | Authentication tag mismatch |
| -8 | `RgtpError.ERR_MERKLE_FAIL` | Merkle proof verification failed |
| -9 | `RgtpError.ERR_FEC_FAIL` | FEC decoding failed |
| -10 | `RgtpError.ERR_TRUNCATED` | Packet truncated |
| -11 | `RgtpError.ERR_CHUNK_INDEX_OOB` | Chunk index out of bounds |
| -12 | `RgtpError.ERR_TIMEOUT` | Operation timed out |
| -13 | `RgtpError.ERR_RATE_LIMITED` | Rate limit exceeded |
| -14 | `RgtpError.ERR_NOT_SUPPORTED` | Feature not supported |
| -15 | `RgtpError.ERR_INTERNAL` | Internal invariant violation |
| -16 | `RgtpError.ERR_SATELLITE_NO_CONTACT` | No active satellite contact window |

## What's New in Version 1.2.0

### Complete API Coverage
- **All functions implemented**: No more `NotImplementedError` for `pull_start` or other functions
- **Full statistics support**: `get_stats()` now returns complete transfer statistics
- **Latency statistics**: New `get_latency_stats()` function
- **Library status**: New `is_initialized()` function

### Satellite Communications
- **Link quality monitoring**: SNR, BER, link margin, Doppler offset tracking
- **Contact window management**: Schedule, query, and monitor contact windows
- **Store-and-forward**: Support for intermittent satellite connectivity
- **CCSDS protocols**: Telemetry (TM), Telecommand (TC), AOS, CFDP support
- **Emergency telecommands**: Priority-based command transmission
- **Link budget calculation**: Automatic link margin and Eb/No calculation
- **Doppler compensation**: Configure automatic Doppler shift handling

### Easy Installation
- **Bundled library**: `librgtp` is automatically built and bundled with the package
- **Zero configuration**: Works out of the box with `pip install rgtp`
- **Cross-platform**: Supports Windows, macOS, and Linux

## Development

### Building from Source

```bash
git clone https://github.com/rawscript/red-giant.git
cd red-giant/bindings/python
pip install -e .
```

### Running Tests

```bash
cd bindings/python
pip install -e ".[dev]"
pytest tests/
```

## License

MIT — see [LICENSE](../../LICENSE).

## Contributing

See [CONTRIBUTING.md](../../CONTRIBUTING.md) for guidelines.
