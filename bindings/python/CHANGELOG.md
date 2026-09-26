# Changelog

All notable changes to the RGTP Python bindings will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [1.2.0] - 2026-09-26

### Added

#### Complete API Coverage
- **`pull_start()`**: Fully implemented with `sockaddr_storage` construction for IPv4 and IPv6 support. No more `NotImplementedError`.
- **`get_stats()`**: Returns complete transfer statistics including bytes sent/received, chunks, errors, FEC recoveries, packet loss rate, RTT, and pull pressure.
- **`get_latency_stats()`**: New function to retrieve latency statistics (mean, jitter, p99, min, max).
- **`is_initialized()`**: New function to check if the library has been initialized.

#### Satellite Communications API
- **`get_satellite_stats()`**: Retrieve satellite-specific link statistics (SNR, BER, link margin, Doppler offset, contact windows, store-and-forward status, spacecraft health, CCSDS frames).
- **`schedule_contact()`**: Schedule contact windows with ground stations.
- **`update_link_parameters()`**: Update link parameters for adaptive coding and modulation.
- **`enable_store_forward()`**: Enable store-and-forward mode for intermittent satellite connectivity.
- **`get_contact_windows()`**: Query available contact windows for a spacecraft.
- **`configure_ccsds()`**: Configure CCSDS protocols (TM, TC, AOS, CFDP).
- **`send_emergency_tc()`**: Send priority emergency telecommands to spacecraft.
- **`calculate_link_budget()`**: Calculate link budget (margin and Eb/No).
- **`configure_doppler()`**: Configure Doppler shift compensation.
- **`check_contact_status()`**: Check if currently within a contact window.

#### Error Codes
- Added `RgtpError.ERR_SATELLITE_NO_CONTACT` (-16) error code.
- All error codes are now accessible as class attributes on `RgtpError`.

#### Development & Build
- **`setup.py`**: Custom build script that automatically builds and bundles `librgtp` with the Python package.
- **`build_library.py`**: Standalone script to build the C library for bundling.
- **`verify_package.py`**: Comprehensive verification script to check package completeness.
- **`dev_setup.bat` / `dev_setup.sh`**: Development environment setup scripts for Windows and Unix.

### Changed

#### Installation
- **Zero-configuration install**: `pip install rgtp` now works out of the box with bundled library.
- The package automatically builds `librgtp` during installation if it's not already available.
- No need to manually install the C library or set `RGTP_LIB_PATH`.

#### Documentation
- **README.md**: Completely rewritten with full API reference, examples, and feature list.
- Added "What's New in Version 1.2.0" section.
- Added satellite communications documentation with all parameters and return values.
- Updated error code table to include all 17 error codes.

#### Package Metadata
- Version updated to `1.2.0`.
- Added `satellite`, `ccsds`, and `space-communications` keywords.
- Added additional classifiers for scientific/engineering use cases.

### Fixed

#### Critical Fixes
- **`pull_start()`**: Previously raised `NotImplementedError` with message to use other language bindings. Now fully implemented.
- **`get_stats()`**: Previously returned only `{"progress": value}`. Now returns complete statistics dictionary with 11 fields.
- All advertised features in README now work as documented.

### Removed

- Removed mock/incomplete implementation warnings.
- Removed misleading "Production/Stable" claims before implementation was complete.

## [1.0.0] - 2025-09-26

### Added
- Initial release of RGTP Python bindings.
- Basic ctypes-based implementation.
- Core API: `init()`, `cleanup()`, `version()`, `strerror()`.
- Socket and Surface handles.
- Exposer API: `expose()`, `poll()`.
- Puller API: `pull_next()` (partial).
- Basic CLI tools: `rgtp-expose`, `rgtp-pull`.
- Async wrappers: `async_expose()`, `async_pull_next()`.

### Known Issues (1.0.0)
- `pull_start()` raised `NotImplementedError`.
- `get_stats()` returned incomplete data.
- Required manual library installation.
- Satellite communications not implemented.

---

## Version History

- **1.2.0** (2026-01-XX): Complete implementation with satellite communications
- **1.0.0** (2025-XX-XX): Initial release

---

## Migration Guide

### Upgrading from 1.0.0 to 1.2.0

#### Installation
No changes needed. The new version bundles the library automatically:

```bash
pip install --upgrade rgtp
```

#### API Changes
All 1.0.0 code continues to work. New features are additive:

```python
# Old code (still works)
import rgtp
rgtp.init()
surface = rgtp.expose(sock, data)
stats = rgtp.get_stats(surface)  # Now returns complete stats
rgtp.cleanup()

# New features (1.2.0)
latency = rgtp.get_latency_stats(surface)  # New
sat_stats = rgtp.get_satellite_stats(surface)  # New
rgtp.schedule_contact(surface, start, end, "ground_station")  # New
```

#### pull_start() Now Works
Previously, `pull_start()` raised `NotImplementedError`. In 1.2.0, it's fully implemented:

```python
# This now works in 1.2.0
surface = rgtp.pull_start(sock, ("192.168.1.10", 9000), exposure_id)
```

#### Enhanced Statistics
`get_stats()` now returns complete data:

```python
# 1.0.0: Only returned progress
stats = {"progress": 0.5}

# 1.2.0: Returns complete statistics
stats = {
    "bytes_sent": 1024000,
    "bytes_received": 512000,
    "chunks_sent": 85,
    "chunks_received": 42,
    "auth_failures": 0,
    "malformed_packets": 2,
    "fec_recoveries": 5,
    "nak_sent": 3,
    "packet_loss_rate": 0.02,
    "rtt_us": 1500,
    "pull_pressure": 8
}
```
