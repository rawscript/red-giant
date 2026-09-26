"""
rgtp._rgtp — ctypes-based implementation of the RGTP Python binding.

Uses ctypes to call into librgtp.so / rgtp.dll without requiring a C
extension build step. Provides complete bindings for all RGTP API functions.

Requirements: 14.6, 14.7, 14.8
"""

import ctypes
import ctypes.util
import asyncio
import os
import sys
import socket
from typing import Optional, Tuple, List, Dict, Callable, Any

# ── Platform socket handling ──────────────────────────────────────────────────

import struct

# ── Load shared library ──────────────────────────────────────────────────────

def _load_lib():
    """Locate and load librgtp."""
    # Try explicit path first (set by installer or environment)
    lib_path = os.environ.get("RGTP_LIB_PATH")
    if lib_path:
        return ctypes.CDLL(lib_path)

    # Platform-specific search
    if sys.platform == "win32":
        names = ["rgtp.dll", "librgtp.dll"]
    elif sys.platform == "darwin":
        names = ["librgtp.dylib", "librgtp.so"]
    else:
        names = ["librgtp.so"]

    for name in names:
        found = ctypes.util.find_library(name.replace("lib", "").replace(".dll", "").replace(".so", "").replace(".dylib", ""))
        if found:
            try:
                return ctypes.CDLL(found)
            except OSError:
                continue
        
        # Try direct path in common locations
        common_paths = [
            name,
            f"/usr/local/lib/{name}",
            f"/usr/lib/{name}",
            f"/usr/lib/x86_64-linux-gnu/{name}",
            os.path.join(os.path.dirname(__file__), name),
        ]
        
        for path in common_paths:
            if os.path.exists(path):
                try:
                    return ctypes.CDLL(path)
                except OSError:
                    continue

    raise OSError(
        f"Cannot find librgtp ({names[0]}). "
        "Set RGTP_LIB_PATH environment variable to the library path, "
        "or ensure librgtp is installed on your system."
    )

try:
    _lib = _load_lib()
except OSError:
    _lib = None   # Allow import to succeed; errors raised at call time


# ── Error codes ──────────────────────────────────────────────────────────────

class RgtpError(Exception):
    """Raised when an RGTP API call returns a non-OK error code."""
    
    # Error code constants
    OK = 0
    ERR_NOMEM = -1
    ERR_INVALID_ARG = -2
    ERR_SOCKET = -3
    ERR_CRYPTO_INIT = -4
    ERR_ENCRYPT = -5
    ERR_DECRYPT = -6
    ERR_AUTH_FAIL = -7
    ERR_MERKLE_FAIL = -8
    ERR_FEC_FAIL = -9
    ERR_TRUNCATED = -10
    ERR_CHUNK_INDEX_OOB = -11
    ERR_TIMEOUT = -12
    ERR_RATE_LIMITED = -13
    ERR_NOT_SUPPORTED = -14
    ERR_INTERNAL = -15
    ERR_SATELLITE_NO_CONTACT = -16

    def __init__(self, code: int, message: str = ""):
        self.code = code
        self.message = message or (strerror(code) if _lib else f"error {code}")
        super().__init__(f"RGTP error {code}: {self.message}")


def _check(code: int) -> None:
    """Raise RgtpError if code != RGTP_OK (0)."""
    if code != 0:
        raise RgtpError(code)


# ── C type declarations ──────────────────────────────────────────────────────

if _lib:
    # Opaque handle types
    class _SurfaceHandle(ctypes.Structure):
        pass

    class _SocketHandle(ctypes.Structure):
        pass

    _SurfacePtr = ctypes.POINTER(_SurfaceHandle)
    _SocketPtr = ctypes.POINTER(_SocketHandle)

    # ── Library lifecycle ──────────────────────────────────────────────────────
    _lib.rgtp_init.restype = ctypes.c_int
    _lib.rgtp_init.argtypes = []

    _lib.rgtp_cleanup.restype = None
    _lib.rgtp_cleanup.argtypes = []

    _lib.rgtp_version.restype = ctypes.c_char_p
    _lib.rgtp_version.argtypes = []

    _lib.rgtp_strerror.restype = ctypes.c_char_p
    _lib.rgtp_strerror.argtypes = [ctypes.c_int]

    _lib.rgtp_is_initialized.restype = ctypes.c_int
    _lib.rgtp_is_initialized.argtypes = []

    # ── Socket management ──────────────────────────────────────────────────────
    _lib.rgtp_socket_create.restype = ctypes.c_int
    _lib.rgtp_socket_create.argtypes = [ctypes.c_void_p, ctypes.POINTER(_SocketPtr)]

    _lib.rgtp_socket_destroy.restype = None
    _lib.rgtp_socket_destroy.argtypes = [_SocketPtr]

    # ── Exposer API ──────────────────────────────────────────────────────────────
    _lib.rgtp_expose.restype = ctypes.c_int
    _lib.rgtp_expose.argtypes = [
        _SocketPtr,
        ctypes.c_void_p,
        ctypes.c_size_t,
        ctypes.c_void_p,
        ctypes.POINTER(_SurfacePtr)
    ]

    _lib.rgtp_poll.restype = ctypes.c_int
    _lib.rgtp_poll.argtypes = [_SurfacePtr, ctypes.c_int]

    _lib.rgtp_destroy_surface.restype = None
    _lib.rgtp_destroy_surface.argtypes = [_SurfacePtr]

    _lib.rgtp_get_exposure_id.restype = ctypes.c_int
    _lib.rgtp_get_exposure_id.argtypes = [_SurfacePtr, ctypes.c_char_p]

    _lib.rgtp_progress.restype = ctypes.c_float
    _lib.rgtp_progress.argtypes = [_SurfacePtr]

    # ── Puller API ──────────────────────────────────────────────────────────────
    _lib.rgtp_pull_start.restype = ctypes.c_int
    _lib.rgtp_pull_start.argtypes = [
        _SocketPtr,
        ctypes.c_void_p,  # sockaddr_storage
        ctypes.c_char_p,  # exposure_id[16]
        ctypes.c_void_p,
        ctypes.POINTER(_SurfacePtr)
    ]

    _lib.rgtp_pull_next.restype = ctypes.c_int
    _lib.rgtp_pull_next.argtypes = [
        _SurfacePtr,
        ctypes.c_void_p,
        ctypes.c_size_t,
        ctypes.POINTER(ctypes.c_size_t),
        ctypes.POINTER(ctypes.c_uint32)
    ]

    # ── Statistics ──────────────────────────────────────────────────────────────
    class _StatsStruct(ctypes.Structure):
        _fields_ = [
            ("bytes_sent", ctypes.c_uint64),
            ("bytes_received", ctypes.c_uint64),
            ("chunks_sent", ctypes.c_uint32),
            ("chunks_received", ctypes.c_uint32),
            ("auth_failures", ctypes.c_uint32),
            ("malformed_packets", ctypes.c_uint32),
            ("fec_recoveries", ctypes.c_uint32),
            ("nak_sent", ctypes.c_uint32),
            ("packet_loss_rate", ctypes.c_float),
            ("rtt_us", ctypes.c_uint32),
            ("pull_pressure", ctypes.c_uint32),
        ]

    _lib.rgtp_get_stats.restype = ctypes.c_int
    _lib.rgtp_get_stats.argtypes = [_SurfacePtr, ctypes.POINTER(_StatsStruct)]

    class _LatencyStatsStruct(ctypes.Structure):
        _fields_ = [
            ("mean_us", ctypes.c_uint32),
            ("jitter_us", ctypes.c_uint32),
            ("p99_us", ctypes.c_uint32),
            ("min_us", ctypes.c_uint32),
            ("max_us", ctypes.c_uint32),
            ("sample_count", ctypes.c_uint32),
        ]

    _lib.rgtp_get_latency_stats.restype = ctypes.c_int
    _lib.rgtp_get_latency_stats.argtypes = [_SurfacePtr, ctypes.POINTER(_LatencyStatsStruct)]

    # ── Satellite communications ────────────────────────────────────────────────
    class _SatelliteStatsStruct(ctypes.Structure):
        _fields_ = [
            ("snr_db", ctypes.c_float),
            ("ber", ctypes.c_float),
            ("link_margin_db", ctypes.c_float),
            ("doppler_offset_hz", ctypes.c_int32),
            ("contact_attempts", ctypes.c_uint32),
            ("successful_contacts", ctypes.c_uint32),
            ("contact_duration_s", ctypes.c_uint32),
            ("time_to_contact_s", ctypes.c_uint32),
            ("stored_bytes", ctypes.c_uint64),
            ("stored_chunks", ctypes.c_uint32),
            ("store_overflow_count", ctypes.c_uint32),
            ("spacecraft_health", ctypes.c_uint8),
            ("power_level", ctypes.c_uint8),
            ("antenna_status", ctypes.c_uint8),
            ("ccsds_frames_sent", ctypes.c_uint32),
            ("ccsds_frames_received", ctypes.c_uint32),
            ("ccsds_frame_errors", ctypes.c_uint32),
            ("ccsds_vcdu_count", ctypes.c_uint32),
        ]

    _lib.rgtp_get_satellite_stats.restype = ctypes.c_int
    _lib.rgtp_get_satellite_stats.argtypes = [_SurfacePtr, ctypes.POINTER(_SatelliteStatsStruct)]

    _lib.rgtp_schedule_contact.restype = ctypes.c_int
    _lib.rgtp_schedule_contact.argtypes = [
        _SurfacePtr,
        ctypes.c_uint64,
        ctypes.c_uint64,
        ctypes.c_char_p
    ]

    _lib.rgtp_update_link_parameters.restype = ctypes.c_int
    _lib.rgtp_update_link_parameters.argtypes = [
        _SurfacePtr,
        ctypes.c_float,
        ctypes.c_float,
        ctypes.c_int32
    ]

    _lib.rgtp_enable_store_forward.restype = ctypes.c_int
    _lib.rgtp_enable_store_forward.argtypes = [
        _SurfacePtr,
        ctypes.c_uint64,
        ctypes.c_uint32
    ]

    class _ContactWindowStruct(ctypes.Structure):
        _fields_ = [
            ("start_time", ctypes.c_uint64),
            ("end_time", ctypes.c_uint64),
            ("ground_station", ctypes.c_char * 32),
        ]

    _lib.rgtp_get_contact_windows.restype = ctypes.c_int
    _lib.rgtp_get_contact_windows.argtypes = [
        _SurfacePtr,
        ctypes.POINTER(_ContactWindowStruct),
        ctypes.c_size_t,
        ctypes.POINTER(ctypes.c_size_t)
    ]

    _lib.rgtp_configure_ccsds.restype = ctypes.c_int
    _lib.rgtp_configure_ccsds.argtypes = [
        _SurfacePtr,
        ctypes.c_bool,
        ctypes.c_bool,
        ctypes.c_bool,
        ctypes.c_bool
    ]

    _lib.rgtp_send_emergency_tc.restype = ctypes.c_int
    _lib.rgtp_send_emergency_tc.argtypes = [
        _SurfacePtr,
        ctypes.c_void_p,
        ctypes.c_size_t,
        ctypes.c_uint8
    ]

    _lib.rgtp_calculate_link_budget.restype = ctypes.c_int
    _lib.rgtp_calculate_link_budget.argtypes = [
        _SurfacePtr,
        ctypes.POINTER(ctypes.c_float),
        ctypes.POINTER(ctypes.c_float)
    ]

    _lib.rgtp_configure_doppler.restype = ctypes.c_int
    _lib.rgtp_configure_doppler.argtypes = [
        _SurfacePtr,
        ctypes.c_bool,
        ctypes.c_uint32,
        ctypes.c_float
    ]

    _lib.rgtp_check_contact_status.restype = ctypes.c_int
    _lib.rgtp_check_contact_status.argtypes = [
        _SurfacePtr,
        ctypes.POINTER(ctypes.c_bool),
        ctypes.POINTER(ctypes.c_uint32),
        ctypes.POINTER(ctypes.c_uint32)
    ]


# ── Helper: sockaddr_storage construction ─────────────────────────────────────

def _make_sockaddr_storage(host: str, port: int) -> bytes:
    """Construct a sockaddr_storage structure from host and port."""
    # Try to resolve the address
    try:
        # Get address info
        addrinfo = socket.getaddrinfo(host, port, socket.AF_UNSPEC, socket.SOCK_DGRAM)
        if not addrinfo:
            raise ValueError(f"Cannot resolve {host}:{port}")
        
        # Use the first result
        family, socktype, proto, canonname, sa = addrinfo[0]
        
        if family == socket.AF_INET:
            # IPv4: struct sockaddr_in
            # sin_family (2 bytes) + sin_port (2 bytes) + sin_addr (4 bytes) + sin_zero (8 bytes) = 16 bytes
            sin_family = struct.pack('H', socket.AF_INET)
            sin_port = struct.pack('!H', port)
            sin_addr = socket.inet_pton(socket.AF_INET, sa[0])
            sin_zero = b'\x00' * 8
            return sin_family + sin_port + sin_addr + sin_zero
        
        elif family == socket.AF_INET6:
            # IPv6: struct sockaddr_in6
            # sin6_family (2 bytes) + sin6_port (2 bytes) + sin6_flowinfo (4 bytes) +
            # sin6_addr (16 bytes) + sin6_scope_id (4 bytes) = 28 bytes
            sin6_family = struct.pack('H', socket.AF_INET6)
            sin6_port = struct.pack('!H', port)
            sin6_flowinfo = struct.pack('!I', 0)
            sin6_addr = socket.inet_pton(socket.AF_INET6, sa[0])
            sin6_scope_id = struct.pack('!I', sa[3] if len(sa) > 3 else 0)
            return sin6_family + sin6_port + sin6_flowinfo + sin6_addr + sin6_scope_id
        
        else:
            raise ValueError(f"Unsupported address family: {family}")
    
    except Exception as e:
        raise ValueError(f"Failed to construct sockaddr_storage for {host}:{port}: {e}")


# ── Public API ────────────────────────────────────────────────────────────────

def init() -> None:
    """Initialise the RGTP library. Must be called once before any other function."""
    if not _lib:
        raise OSError("librgtp not loaded")
    _check(_lib.rgtp_init())


def cleanup() -> None:
    """Release all global library resources."""
    if _lib:
        _lib.rgtp_cleanup()


def version() -> str:
    """Return the library version string."""
    if not _lib:
        raise OSError("librgtp not loaded")
    return _lib.rgtp_version().decode()


def strerror(code: int) -> str:
    """Return a human-readable description of an error code."""
    if not _lib:
        return f"error {code}"
    return _lib.rgtp_strerror(code).decode()


def is_initialized() -> bool:
    """Check if the library is currently initialized."""
    if not _lib:
        return False
    return _lib.rgtp_is_initialized() != 0


class Socket:
    """Wraps an rgtp_socket_t handle."""

    def __init__(self, config: Optional[Dict[str, Any]] = None):
        if not _lib:
            raise OSError("librgtp not loaded")
        self._ptr = _SocketPtr()
        # For now, pass None for config (use defaults)
        # TODO: Support config dict -> rgtp_config_t conversion
        _check(_lib.rgtp_socket_create(None, ctypes.byref(self._ptr)))

    def close(self) -> None:
        if self._ptr:
            _lib.rgtp_socket_destroy(self._ptr)
            self._ptr = None

    def __enter__(self):
        return self

    def __exit__(self, *args):
        self.close()

    def __del__(self):
        self.close()


class Surface:
    """Wraps an rgtp_surface_t handle (exposer or puller)."""

    def __init__(self, ptr):
        self._ptr = ptr

    def close(self) -> None:
        if self._ptr:
            _lib.rgtp_destroy_surface(self._ptr)
            self._ptr = None

    def exposure_id(self) -> bytes:
        """Return the 16-byte Exposure_ID."""
        buf = ctypes.create_string_buffer(16)
        _check(_lib.rgtp_get_exposure_id(self._ptr, buf))
        return bytes(buf)

    def progress(self) -> float:
        """Return the transfer completion fraction [0.0, 1.0]."""
        return float(_lib.rgtp_progress(self._ptr))

    def __enter__(self):
        return self

    def __exit__(self, *args):
        self.close()

    def __del__(self):
        self.close()


def expose(sock: Socket, data: bytes, config: Optional[Dict[str, Any]] = None) -> Surface:
    """Pre-encrypt data and create an immutable Exposure."""
    if not _lib:
        raise OSError("librgtp not loaded")
    buf = ctypes.create_string_buffer(data, len(data))
    ptr = _SurfacePtr()
    # For now, pass None for config (use defaults)
    # TODO: Support config dict -> rgtp_config_t conversion
    _check(_lib.rgtp_expose(sock._ptr, buf, len(data), None, ctypes.byref(ptr)))
    return Surface(ptr)


def poll(surface: Surface, timeout_ms: int = 100) -> None:
    """Serve pending pull requests for an active Exposure."""
    code = _lib.rgtp_poll(surface._ptr, timeout_ms)
    if code not in (0, -12):   # RGTP_OK or RGTP_ERR_TIMEOUT
        _check(code)


def pull_start(sock: Socket, server_addr: Tuple[str, int],
               exposure_id: bytes, config: Optional[Dict[str, Any]] = None) -> Surface:
    """Begin pulling an Exposure from a remote Exposer.
    
    Args:
        sock: Socket to use for communication
        server_addr: Tuple of (host, port) for the exposer
        exposure_id: 16-byte Exposure_ID
        config: Optional configuration dict
    
    Returns:
        Surface handle for the pull operation
    """
    if not _lib:
        raise OSError("librgtp not loaded")
    
    if len(exposure_id) != 16:
        raise ValueError("exposure_id must be exactly 16 bytes")
    
    host, port = server_addr
    
    # Construct sockaddr_storage
    sockaddr_bytes = _make_sockaddr_storage(host, port)
    
    # Create ctypes buffer for sockaddr_storage
    sockaddr_buf = ctypes.create_string_buffer(sockaddr_bytes, 128)  # sockaddr_storage is 128 bytes
    
    # Create ctypes buffer for exposure_id
    exposure_id_buf = ctypes.create_string_buffer(exposure_id, 16)
    
    ptr = _SurfacePtr()
    _check(_lib.rgtp_pull_start(
        sock._ptr,
        sockaddr_buf,
        exposure_id_buf,
        None,  # config
        ctypes.byref(ptr)
    ))
    return Surface(ptr)


def pull_next(surface: Surface, buf_size: int = 65536) -> Tuple[bytes, int]:
    """Receive the next available chunk.
    
    Args:
        surface: Puller surface
        buf_size: Buffer size for receiving chunk data
    
    Returns:
        Tuple of (data, chunk_index)
    """
    buf = ctypes.create_string_buffer(buf_size)
    received = ctypes.c_size_t(0)
    chunk_idx = ctypes.c_uint32(0)
    _check(_lib.rgtp_pull_next(
        surface._ptr,
        buf,
        buf_size,
        ctypes.byref(received),
        ctypes.byref(chunk_idx)
    ))
    return bytes(buf[:received.value]), chunk_idx.value


def progress(surface: Surface) -> float:
    """Return the transfer completion fraction [0.0, 1.0]."""
    return surface.progress()


def get_stats(surface: Surface) -> Dict[str, Any]:
    """Retrieve transfer statistics for a surface.
    
    Returns a dictionary with:
        - bytes_sent: Total bytes sent (exposer)
        - bytes_received: Total bytes received (puller)
        - chunks_sent: Total chunks sent
        - chunks_received: Total chunks received
        - auth_failures: AEAD tag verification failures
        - malformed_packets: Packets discarded by parser
        - fec_recoveries: Chunks recovered via FEC
        - nak_sent: NAK packets sent (puller)
        - packet_loss_rate: EWMA packet loss rate [0.0, 1.0]
        - rtt_us: EWMA RTT estimate in microseconds
        - pull_pressure: Pull requests received in last 100ms (exposer)
    """
    stats = _StatsStruct()
    _check(_lib.rgtp_get_stats(surface._ptr, ctypes.byref(stats)))
    return {
        "bytes_sent": stats.bytes_sent,
        "bytes_received": stats.bytes_received,
        "chunks_sent": stats.chunks_sent,
        "chunks_received": stats.chunks_received,
        "auth_failures": stats.auth_failures,
        "malformed_packets": stats.malformed_packets,
        "fec_recoveries": stats.fec_recoveries,
        "nak_sent": stats.nak_sent,
        "packet_loss_rate": stats.packet_loss_rate,
        "rtt_us": stats.rtt_us,
        "pull_pressure": stats.pull_pressure,
    }


def get_latency_stats(surface: Surface) -> Dict[str, int]:
    """Retrieve latency statistics for a puller surface.
    
    Returns a dictionary with:
        - mean_us: Mean one-way chunk delay in microseconds
        - jitter_us: Inter-chunk arrival variance in microseconds
        - p99_us: 99th-percentile one-way delay in microseconds
        - min_us: Minimum observed delay
        - max_us: Maximum observed delay
        - sample_count: Number of delay samples in window
    """
    stats = _LatencyStatsStruct()
    _check(_lib.rgtp_get_latency_stats(surface._ptr, ctypes.byref(stats)))
    return {
        "mean_us": stats.mean_us,
        "jitter_us": stats.jitter_us,
        "p99_us": stats.p99_us,
        "min_us": stats.min_us,
        "max_us": stats.max_us,
        "sample_count": stats.sample_count,
    }


# ════════════════════════════════════════════════════════════════════════════
# Satellite Communications API
# ════════════════════════════════════════════════════════════════════════════

def get_satellite_stats(surface: Surface) -> Dict[str, Any]:
    """Retrieve satellite-specific link statistics.
    
    Returns a dictionary with:
        - snr_db: Current signal-to-noise ratio in dB
        - ber: Current bit error rate
        - link_margin_db: Link margin in dB
        - doppler_offset_hz: Measured Doppler offset in Hz
        - contact_attempts: Number of contact attempts
        - successful_contacts: Number of successful contacts
        - contact_duration_s: Duration of last successful contact in seconds
        - time_to_contact_s: Time until next contact window in seconds
        - stored_bytes: Bytes in store-and-forward buffer
        - stored_chunks: Chunks in store-and-forward buffer
        - store_overflow_count: Number of store overflow events
        - spacecraft_health: Spacecraft health indicator (0-100)
        - power_level: Available power percentage (0-100)
        - antenna_status: Antenna status (0=down, 1=pointing, 2=tracking)
        - ccsds_frames_sent: CCSDS frames transmitted
        - ccsds_frames_received: CCSDS frames received
        - ccsds_frame_errors: CCSDS frame errors detected
        - ccsds_vcdu_count: CCSDS Virtual Channel Data Units
    """
    stats = _SatelliteStatsStruct()
    _check(_lib.rgtp_get_satellite_stats(surface._ptr, ctypes.byref(stats)))
    return {
        "snr_db": stats.snr_db,
        "ber": stats.ber,
        "link_margin_db": stats.link_margin_db,
        "doppler_offset_hz": stats.doppler_offset_hz,
        "contact_attempts": stats.contact_attempts,
        "successful_contacts": stats.successful_contacts,
        "contact_duration_s": stats.contact_duration_s,
        "time_to_contact_s": stats.time_to_contact_s,
        "stored_bytes": stats.stored_bytes,
        "stored_chunks": stats.stored_chunks,
        "store_overflow_count": stats.store_overflow_count,
        "spacecraft_health": stats.spacecraft_health,
        "power_level": stats.power_level,
        "antenna_status": stats.antenna_status,
        "ccsds_frames_sent": stats.ccsds_frames_sent,
        "ccsds_frames_received": stats.ccsds_frames_received,
        "ccsds_frame_errors": stats.ccsds_frame_errors,
        "ccsds_vcdu_count": stats.ccsds_vcdu_count,
    }


def schedule_contact(surface: Surface, start_time: int, end_time: int,
                    ground_station: str) -> None:
    """Schedule a contact window with a ground station.
    
    Args:
        surface: Surface configured for satellite communications
        start_time: UTC timestamp of contact start (seconds since epoch)
        end_time: UTC timestamp of contact end (seconds since epoch)
        ground_station: Ground station identifier string
    """
    _check(_lib.rgtp_schedule_contact(
        surface._ptr,
        ctypes.c_uint64(start_time),
        ctypes.c_uint64(end_time),
        ground_station.encode('utf-8')
    ))


def update_link_parameters(surface: Surface, snr_db: float, ber: float,
                          doppler_hz: int) -> None:
    """Update link parameters for adaptive coding and modulation.
    
    Args:
        surface: Surface configured for satellite communications
        snr_db: Current signal-to-noise ratio in dB
        ber: Current bit error rate
        doppler_hz: Measured Doppler shift in Hz
    """
    _check(_lib.rgtp_update_link_parameters(
        surface._ptr,
        ctypes.c_float(snr_db),
        ctypes.c_float(ber),
        ctypes.c_int32(doppler_hz)
    ))


def enable_store_forward(surface: Surface, max_storage_bytes: int = 0,
                        max_storage_time_s: int = 0) -> None:
    """Enable store-and-forward mode for intermittent connectivity.
    
    Args:
        surface: Surface configured for satellite communications
        max_storage_bytes: Maximum storage for store-and-forward (0 = unlimited)
        max_storage_time_s: Maximum storage time in seconds
    """
    _check(_lib.rgtp_enable_store_forward(
        surface._ptr,
        ctypes.c_uint64(max_storage_bytes),
        ctypes.c_uint32(max_storage_time_s)
    ))


def get_contact_windows(surface: Surface, max_windows: int = 10) -> List[Dict[str, Any]]:
    """Get available contact windows for a spacecraft.
    
    Args:
        surface: Surface configured for satellite communications
        max_windows: Maximum number of windows to retrieve
    
    Returns:
        List of contact window dictionaries with start_time, end_time, and ground_station
    """
    windows = (_ContactWindowStruct * max_windows)()
    count = ctypes.c_size_t(0)
    
    _check(_lib.rgtp_get_contact_windows(
        surface._ptr,
        windows,
        ctypes.c_size_t(max_windows),
        ctypes.byref(count)
    ))
    
    result = []
    for i in range(count.value):
        result.append({
            "start_time": windows[i].start_time,
            "end_time": windows[i].end_time,
            "ground_station": windows[i].ground_station.decode('utf-8').rstrip('\x00'),
        })
    
    return result


def configure_ccsds(surface: Surface, enable_tm: bool = False, enable_tc: bool = False,
                   enable_aos: bool = False, enable_cfdp: bool = False) -> None:
    """Configure CCSDS protocol options.
    
    Args:
        surface: Surface configured for satellite communications
        enable_tm: Enable CCSDS Telemetry (TM) protocol
        enable_tc: Enable CCSDS Telecommand (TC) protocol
        enable_aos: Enable CCSDS Advanced Orbiting Systems (AOS)
        enable_cfdp: Enable CCSDS File Delivery Protocol (CFDP)
    """
    _check(_lib.rgtp_configure_ccsds(
        surface._ptr,
        ctypes.c_bool(enable_tm),
        ctypes.c_bool(enable_tc),
        ctypes.c_bool(enable_aos),
        ctypes.c_bool(enable_cfdp)
    ))


def send_emergency_tc(surface: Surface, tc_data: bytes, priority: int = 128) -> None:
    """Send an emergency telecommand to a spacecraft.
    
    Args:
        surface: Surface configured for satellite communications
        tc_data: Telecommand data payload
        priority: Priority level (0=lowest, 255=highest)
    """
    _check(_lib.rgtp_send_emergency_tc(
        surface._ptr,
        tc_data,
        len(tc_data),
        ctypes.c_uint8(priority)
    ))


def calculate_link_budget(surface: Surface) -> Tuple[float, float]:
    """Calculate link budget for current configuration.
    
    Args:
        surface: Surface configured for satellite communications
    
    Returns:
        Tuple of (link_margin_db, ebno_db)
    """
    margin_db = ctypes.c_float(0.0)
    ebno_db = ctypes.c_float(0.0)
    
    _check(_lib.rgtp_calculate_link_budget(
        surface._ptr,
        ctypes.byref(margin_db),
        ctypes.byref(ebno_db)
    ))
    
    return margin_db.value, ebno_db.value


def configure_doppler(surface: Surface, enable_compensation: bool,
                     max_doppler_hz: int, update_rate_hz: float) -> None:
    """Configure Doppler shift compensation.
    
    Args:
        surface: Surface configured for satellite communications
        enable_compensation: Enable Doppler compensation
        max_doppler_hz: Maximum expected Doppler shift in Hz
        update_rate_hz: Doppler update rate in Hz
    """
    _check(_lib.rgtp_configure_doppler(
        surface._ptr,
        ctypes.c_bool(enable_compensation),
        ctypes.c_uint32(max_doppler_hz),
        ctypes.c_float(update_rate_hz)
    ))


def check_contact_status(surface: Surface) -> Tuple[bool, int, int]:
    """Check if current time is within a contact window.
    
    Args:
        surface: Surface configured for satellite communications
    
    Returns:
        Tuple of (in_contact, time_to_contact_s, time_left_s)
    """
    in_contact = ctypes.c_bool(False)
    time_to_contact_s = ctypes.c_uint32(0)
    time_left_s = ctypes.c_uint32(0)
    
    _check(_lib.rgtp_check_contact_status(
        surface._ptr,
        ctypes.byref(in_contact),
        ctypes.byref(time_to_contact_s),
        ctypes.byref(time_left_s)
    ))
    
    return in_contact.value, time_to_contact_s.value, time_left_s.value


# ── Async wrappers ───────────────────────────────────────────────────────────

async def async_expose(sock: Socket, data: bytes,
                      config: Optional[Dict[str, Any]] = None) -> Surface:
    """Async wrapper for expose() — runs in a thread pool executor."""
    loop = asyncio.get_event_loop()
    return await loop.run_in_executor(None, expose, sock, data, config)


async def async_pull_next(surface: Surface, buf_size: int = 65536) -> Tuple[bytes, int]:
    """Async wrapper for pull_next() — runs in a thread pool executor."""
    loop = asyncio.get_event_loop()
    return await loop.run_in_executor(None, pull_next, surface, buf_size)


async def async_poll(surface: Surface, timeout_ms: int = 100) -> None:
    """Async wrapper for poll() — runs in a thread pool executor."""
    loop = asyncio.get_event_loop()
    return await loop.run_in_executor(None, poll, surface, timeout_ms)
