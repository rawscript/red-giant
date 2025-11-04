"""
Red Giant Transport Protocol (RGTP) - Python Bindings
=====================================================

High-level Python interface for RGTP transport protocol.

Example usage:

    # Expose data
    import rgtp
    
    session = rgtp.Session()
    session.expose_file("large_file.bin")
    session.wait_complete()
    
    # Pull data
    client = rgtp.Client()
    data = client.pull_from("192.168.1.100", 9999)
    
    # Or pull to file
    client.pull_to_file("192.168.1.100", 9999, "downloaded.bin")
"""

import ctypes
import os
import sys
from typing import Optional, Callable, Union, Tuple
from dataclasses import dataclass
from pathlib import Path

# Load the RGTP library
def _load_library():
    """Load the RGTP shared library."""
    lib_name = "librgtp.so"
    if sys.platform == "darwin":
        lib_name = "librgtp.dylib"
    elif sys.platform == "win32":
        lib_name = "rgtp.dll"
    
    # Try different locations
    search_paths = [
        os.path.join(os.path.dirname(__file__), "..", "..", "lib"),
        "/usr/local/lib",
        "/usr/lib",
        "."
    ]
    
    for path in search_paths:
        lib_path = os.path.join(path, lib_name)
        if os.path.exists(lib_path):
            return ctypes.CDLL(lib_path)
    
    # Try system library
    try:
        return ctypes.CDLL(lib_name)
    except OSError:
        raise ImportError(f"Could not find RGTP library ({lib_name})")

_lib = _load_library()

# Define C structures
class _RGTPConfig(ctypes.Structure):
    _fields_ = [
        ("chunk_size", ctypes.c_uint32),
        ("exposure_rate", ctypes.c_uint32),
        ("adaptive_mode", ctypes.c_bool),
        ("enable_compression", ctypes.c_bool),
        ("enable_encryption", ctypes.c_bool),
        ("port", ctypes.c_uint16),
        ("timeout_ms", ctypes.c_int),
        ("progress_cb", ctypes.c_void_p),
        ("error_cb", ctypes.c_void_p),
        ("user_data", ctypes.c_void_p),
    ]

class _RGTPStats(ctypes.Structure):
    _fields_ = [
        ("bytes_transferred", ctypes.c_size_t),
        ("total_bytes", ctypes.c_size_t),
        ("throughput_mbps", ctypes.c_double),
        ("avg_throughput_mbps", ctypes.c_double),
        ("chunks_transferred", ctypes.c_uint32),
        ("total_chunks", ctypes.c_uint32),
        ("retransmissions", ctypes.c_uint32),
        ("completion_percent", ctypes.c_double),
        ("elapsed_ms", ctypes.c_int64),
        ("estimated_remaining_ms", ctypes.c_int64),
    ]

# Define function prototypes
_lib.rgtp_init.restype = ctypes.c_int
_lib.rgtp_cleanup.restype = None

_lib.rgtp_session_create.restype = ctypes.c_void_p
_lib.rgtp_session_create_with_config.argtypes = [ctypes.POINTER(_RGTPConfig)]
_lib.rgtp_session_create_with_config.restype = ctypes.c_void_p
_lib.rgtp_session_destroy.argtypes = [ctypes.c_void_p]
_lib.rgtp_session_expose_file.argtypes = [ctypes.c_void_p, ctypes.c_char_p]
_lib.rgtp_session_expose_file.restype = ctypes.c_int
_lib.rgtp_session_wait_complete.argtypes = [ctypes.c_void_p]
_lib.rgtp_session_wait_complete.restype = ctypes.c_int
_lib.rgtp_session_get_stats.argtypes = [ctypes.c_void_p, ctypes.POINTER(_RGTPStats)]
_lib.rgtp_session_get_stats.restype = ctypes.c_int

_lib.rgtp_client_create.restype = ctypes.c_void_p
_lib.rgtp_client_create_with_config.argtypes = [ctypes.POINTER(_RGTPConfig)]
_lib.rgtp_client_create_with_config.restype = ctypes.c_void_p
_lib.rgtp_client_destroy.argtypes = [ctypes.c_void_p]
_lib.rgtp_client_pull_to_file.argtypes = [ctypes.c_void_p, ctypes.c_char_p, ctypes.c_uint16, ctypes.c_char_p]
_lib.rgtp_client_pull_to_file.restype = ctypes.c_int
_lib.rgtp_client_get_stats.argtypes = [ctypes.c_void_p, ctypes.POINTER(_RGTPStats)]
_lib.rgtp_client_get_stats.restype = ctypes.c_int

_lib.rgtp_config_default.argtypes = [ctypes.POINTER(_RGTPConfig)]
_lib.rgtp_config_for_lan.argtypes = [ctypes.POINTER(_RGTPConfig)]
_lib.rgtp_config_for_wan.argtypes = [ctypes.POINTER(_RGTPConfig)]
_lib.rgtp_config_for_mobile.argtypes = [ctypes.POINTER(_RGTPConfig)]

# Initialize library
if _lib.rgtp_init() != 0:
    raise RuntimeError("Failed to initialize RGTP library")

# Python classes and functions

@dataclass
class Stats:
    """RGTP transfer statistics."""
    bytes_transferred: int
    total_bytes: int
    throughput_mbps: float
    avg_throughput_mbps: float
    chunks_transferred: int
    total_chunks: int
    retransmissions: int
    completion_percent: float
    elapsed_ms: int
    estimated_remaining_ms: int
    
    @property
    def efficiency_percent(self) -> float:
        """Calculate transfer efficiency (successful chunks / total attempts)."""
        if self.chunks_transferred == 0:
            return 100.0
        total_attempts = self.chunks_transferred + self.retransmissions
        return (self.chunks_transferred / total_attempts) * 100.0

class Config:
    """RGTP configuration."""
    
    def __init__(self):
        self._config = _RGTPConfig()
        _lib.rgtp_config_default(ctypes.byref(self._config))
        self.progress_callback: Optional[Callable[[int, int], None]] = None
        self.error_callback: Optional[Callable[[int, str], None]] = None
    
    @property
    def chunk_size(self) -> int:
        return self._config.chunk_size
    
    @chunk_size.setter
    def chunk_size(self, value: int):
        self._config.chunk_size = value
    
    @property
    def exposure_rate(self) -> int:
        return self._config.exposure_rate
    
    @exposure_rate.setter
    def exposure_rate(self, value: int):
        self._config.exposure_rate = value
    
    @property
    def adaptive_mode(self) -> bool:
        return self._config.adaptive_mode
    
    @adaptive_mode.setter
    def adaptive_mode(self, value: bool):
        self._config.adaptive_mode = value
    
    @property
    def port(self) -> int:
        return self._config.port
    
    @port.setter
    def port(self, value: int):
        self._config.port = value
    
    @property
    def timeout_ms(self) -> int:
        return self._config.timeout_ms
    
    @timeout_ms.setter
    def timeout_ms(self, value: int):
        self._config.timeout_ms = value
    
    def optimize_for_lan(self):
        """Optimize configuration for LAN networks."""
        _lib.rgtp_config_for_lan(ctypes.byref(self._config))
    
    def optimize_for_wan(self):
        """Optimize configuration for WAN networks."""
        _lib.rgtp_config_for_wan(ctypes.byref(self._config))
    
    def optimize_for_mobile(self):
        """Optimize configuration for mobile networks."""
        _lib.rgtp_config_for_mobile(ctypes.byref(self._config))

class RGTPError(Exception):
    """RGTP-specific exception."""
    pass

class Session:
    """RGTP session for exposing data."""
    
    def __init__(self, config: Optional[Config] = None):
        if config is None:
            self._handle = _lib.rgtp_session_create()
        else:
            self._handle = _lib.rgtp_session_create_with_config(ctypes.byref(config._config))
        
        if not self._handle:
            raise RGTPError("Failed to create RGTP session")
    
    def __del__(self):
        if hasattr(self, '_handle') and self._handle:
            _lib.rgtp_session_destroy(self._handle)
    
    def expose_file(self, filename: Union[str, Path]) -> None:
        """Expose a file for pulling."""
        filename_bytes = str(filename).encode('utf-8')
        result = _lib.rgtp_session_expose_file(self._handle, filename_bytes)
        if result != 0:
            raise RGTPError(f"Failed to expose file: {filename}")
    
    def wait_complete(self) -> None:
        """Wait for exposure to complete."""
        result = _lib.rgtp_session_wait_complete(self._handle)
        if result != 0:
            raise RGTPError("Exposure failed or timed out")
    
    def get_stats(self) -> Stats:
        """Get current transfer statistics."""
        stats = _RGTPStats()
        result = _lib.rgtp_session_get_stats(self._handle, ctypes.byref(stats))
        if result != 0:
            raise RGTPError("Failed to get statistics")
        
        return Stats(
            bytes_transferred=stats.bytes_transferred,
            total_bytes=stats.total_bytes,
            throughput_mbps=stats.throughput_mbps,
            avg_throughput_mbps=stats.avg_throughput_mbps,
            chunks_transferred=stats.chunks_transferred,
            total_chunks=stats.total_chunks,
            retransmissions=stats.retransmissions,
            completion_percent=stats.completion_percent,
            elapsed_ms=stats.elapsed_ms,
            estimated_remaining_ms=stats.estimated_remaining_ms,
        )

class Client:
    """RGTP client for pulling data."""
    
    def __init__(self, config: Optional[Config] = None):
        if config is None:
            self._handle = _lib.rgtp_client_create()
        else:
            self._handle = _lib.rgtp_client_create_with_config(ctypes.byref(config._config))
        
        if not self._handle:
            raise RGTPError("Failed to create RGTP client")
    
    def __del__(self):
        if hasattr(self, '_handle') and self._handle:
            _lib.rgtp_client_destroy(self._handle)
    
    def pull_to_file(self, host: str, port: int, filename: Union[str, Path]) -> None:
        """Pull data from remote host and save to file."""
        host_bytes = host.encode('utf-8')
        filename_bytes = str(filename).encode('utf-8')
        
        result = _lib.rgtp_client_pull_to_file(
            self._handle, host_bytes, port, filename_bytes
        )
        
        if result != 0:
            raise RGTPError(f"Failed to pull data from {host}:{port}")
    
    def get_stats(self) -> Stats:
        """Get current transfer statistics."""
        stats = _RGTPStats()
        result = _lib.rgtp_client_get_stats(self._handle, ctypes.byref(stats))
        if result != 0:
            raise RGTPError("Failed to get statistics")
        
        return Stats(
            bytes_transferred=stats.bytes_transferred,
            total_bytes=stats.total_bytes,
            throughput_mbps=stats.throughput_mbps,
            avg_throughput_mbps=stats.avg_throughput_mbps,
            chunks_transferred=stats.chunks_transferred,
            total_chunks=stats.total_chunks,
            retransmissions=stats.retransmissions,
            completion_percent=stats.completion_percent,
            elapsed_ms=stats.elapsed_ms,
            estimated_remaining_ms=stats.estimated_remaining_ms,
        )

# Convenience functions
def send_file(filename: Union[str, Path], host: str, port: int, 
              config: Optional[Config] = None) -> Stats:
    """Send a file using RGTP (convenience function)."""
    session = Session(config)
    session.expose_file(filename)
    session.wait_complete()
    return session.get_stats()

def receive_file(host: str, port: int, filename: Union[str, Path],
                config: Optional[Config] = None) -> Stats:
    """Receive a file using RGTP (convenience function)."""
    client = Client(config)
    client.pull_to_file(host, port, filename)
    return client.get_stats()

# Version information
def version() -> str:
    """Get RGTP library version."""
    try:
        _lib.rgtp_version.restype = ctypes.c_char_p
        return _lib.rgtp_version().decode('utf-8')
    except:
        return "1.0.0"

# Cleanup on module exit
import atexit
atexit.register(_lib.rgtp_cleanup)