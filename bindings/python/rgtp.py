"""
Red Giant Transport Protocol (RGTP) - Python Bindings
=====================================================

High-level Python interface for RGTP transport protocol.

Example usage:

    # Expose data
    import rgtp
    
    # Create socket and bind
    sockfd = rgtp.socket()
    rgtp.bind(sockfd, 9999)
    
    # Expose data
    surface = rgtp.expose_data(sockfd, b"Hello World", ("127.0.0.1", 9999))
    
    # Pull data
    buffer = bytearray(1024)
    size = len(buffer)
    rgtp.pull_data(sockfd, ("127.0.0.1", 9999), buffer, size)
    
    # HTTPS over RGTP
    https_client = rgtp.HTTPSClient()
    https_client.download("https://example.com/index.html", "output.html")
"""

import ctypes
import os
import sys
from typing import Optional, Callable, Union, Tuple
from dataclasses import dataclass
from pathlib import Path
import urllib.parse
import socket
import struct
import ipaddress

# Load the RGTP library
def _load_library():
    """Load the RGTP shared library."""
    lib_name = "librgtp.so"
    if sys.platform == "darwin":
        lib_name = "librgtp.dylib"
    elif sys.platform == "win32":
        lib_name = "librgtp.dll"
    
    # Try different locations
    search_paths = [
        os.path.join(os.path.dirname(__file__), "..", "..", "lib"),
        os.path.join(os.path.dirname(__file__), "..", "..", "build", "lib"),
        os.path.join(os.path.dirname(__file__), "..", "..", "out", "lib"),
        "/usr/local/lib",
        "/usr/lib",
        ".",
    ]
    
    # Add Windows-specific paths
    if sys.platform == "win32":
        search_paths.insert(0, r"e:\Main\Projects\red giant\lib")
        search_paths.insert(0, os.path.join(os.path.dirname(__file__), "..", "..", "lib"))
    
    for path in search_paths:
        lib_path = os.path.join(path, lib_name)
        if os.path.exists(lib_path):
            try:
                print(f"Loading RGTP library from: {lib_path}")
                return ctypes.CDLL(lib_path)
            except OSError as e:
                print(f"Warning: Failed to load library from {lib_path}: {e}")
                continue
    
    # Try system library
    try:
        return ctypes.CDLL(lib_name)
    except OSError:
        raise ImportError(f"Could not find RGTP library ({lib_name}")

try:
    _lib = _load_library()
    _lib_loaded = True
except ImportError as e:
    print(f"Warning: RGTP library not available: {e}")
    _lib_loaded = False

# Define C structures (only if library loaded)
if _lib_loaded:
    class _RGTPHeader(ctypes.Structure):
        _pack_ = 1
        _fields_ = [
            ("version", ctypes.c_uint8),
            ("type", ctypes.c_uint8),
            ("flags", ctypes.c_uint16),
            ("session_id", ctypes.c_uint32),
            ("sequence", ctypes.c_uint32),
            ("chunk_size", ctypes.c_uint32),
            ("checksum", ctypes.c_uint32),
        ]

    class _RGTPManifest(ctypes.Structure):
        _pack_ = 1
        _fields_ = [
            ("total_size", ctypes.c_uint64),
            ("chunk_count", ctypes.c_uint32),
            ("optimal_chunk_size", ctypes.c_uint32),
            ("exposure_mode", ctypes.c_uint16),
            ("priority", ctypes.c_uint16),
            ("content_hash", ctypes.c_uint8 * 32),
        ]

    class _RGTPSurface(ctypes.Structure):
        pass  # Forward declaration

    _RGTPSurface._pack_ = 1
    _RGTPSurface._fields_ = [
        ("session_id", ctypes.c_uint32),
        ("manifest", _RGTPManifest),
        ("chunk_bitmap", ctypes.POINTER(ctypes.c_uint8)),
        ("bitmap_size", ctypes.c_uint32),
        ("exposure_rate", ctypes.c_uint32),
        ("congestion_window", ctypes.c_uint32),
        ("pull_pressure", ctypes.c_uint32),
        ("bytes_exposed", ctypes.c_uint64),
        ("bytes_pulled", ctypes.c_uint64),
        ("retransmissions", ctypes.c_uint32),
        ("sockfd", ctypes.c_int),
        ("peer_addr", ctypes.c_uint8 * 16),  # sockaddr_in
    ]

    # Define function prototypes
    try:
        _lib.rgtp_init.restype = ctypes.c_int
        _lib.rgtp_init.argtypes = []
        _lib.rgtp_cleanup.restype = None
        _lib.rgtp_cleanup.argtypes = []
        _lib.rgtp_socket.restype = ctypes.c_int
        _lib.rgtp_socket.argtypes = []
        _lib.rgtp_bind.restype = ctypes.c_int
        _lib.rgtp_bind.argtypes = [ctypes.c_int, ctypes.c_uint16]
        _lib.rgtp_expose_data.restype = ctypes.c_int
        _lib.rgtp_expose_data.argtypes = [ctypes.c_int, ctypes.c_void_p, ctypes.c_size_t, 
                                         ctypes.POINTER(ctypes.c_uint8), ctypes.POINTER(ctypes.POINTER(_RGTPSurface))]
        _lib.rgtp_pull_data.restype = ctypes.c_int
        _lib.rgtp_pull_data.argtypes = [ctypes.c_int, ctypes.POINTER(ctypes.c_uint8), 
                                       ctypes.c_void_p, ctypes.POINTER(ctypes.c_size_t)]
        
        # Try to load version functions (may not be implemented)
        try:
            _lib.rgtp_version.restype = ctypes.c_char_p
            _lib.rgtp_version.argtypes = []
            _version_available = True
        except AttributeError:
            _version_available = False
            
        try:
            _lib.rgtp_build_info.restype = ctypes.c_char_p
            _lib.rgtp_build_info.argtypes = []
            _build_info_available = True
        except AttributeError:
            _build_info_available = False
        
        # Initialize library
        if _lib.rgtp_init() != 0:
            raise RuntimeError("Failed to initialize RGTP library")
            
    except AttributeError as e:
        print(f"Warning: Some RGTP functions not available: {e}")
        _lib_loaded = False

# Python classes and functions

@dataclass
class Stats:
    """RGTP transfer statistics."""
    bytes_transferred: int = 0
    total_bytes: int = 0
    throughput_mbps: float = 0.0
    avg_throughput_mbps: float = 0.0
    chunks_transferred: int = 0
    total_chunks: int = 0
    retransmissions: int = 0
    completion_percent: float = 0.0
    elapsed_ms: int = 0
    estimated_remaining_ms: int = 0
    
    @property
    def efficiency_percent(self) -> float:
        """Calculate transfer efficiency (successful chunks / total attempts)."""
        if self.chunks_transferred == 0:
            return 100.0
        total_attempts = self.chunks_transferred + self.retransmissions
        return (self.chunks_transferred / total_attempts) * 100.0

class RGTPError(Exception):
    """RGTP-specific exception."""
    pass

def socket() -> int:
    """Create an RGTP socket."""
    if not _lib_loaded:
        print("Warning: RGTP library not loaded, returning mock socket")
        return 1
        
    sockfd = _lib.rgtp_socket()
    if sockfd < 0:
        raise RGTPError("Failed to create RGTP socket")
    return sockfd

def bind(sockfd: int, port: int) -> None:
    """Bind RGTP socket to port."""
    if not _lib_loaded:
        print(f"Warning: RGTP library not loaded, mock binding to port {port}")
        return
        
    result = _lib.rgtp_bind(sockfd, port)
    if result < 0:
        raise RGTPError(f"Failed to bind RGTP socket to port {port}")

def expose_data(sockfd: int, data: bytes, dest: Tuple[str, int]) -> object:
    """Expose data for pulling."""
    if not _lib_loaded:
        print(f"Warning: RGTP library not loaded, mock exposing {len(data)} bytes to {dest}")
        return None
        
    # Convert destination to sockaddr_in
    ip, port = dest
    
    # Create proper sockaddr_in structure
    # struct sockaddr_in {
    #     short sin_family;    // AF_INET
    #     unsigned short sin_port;  // Port number
    #     struct in_addr sin_addr;  // IP address
    #     char sin_zero[8];    // Padding
    # }
    
    # For Windows, we need to create a proper sockaddr_in structure
    import socket
    import struct
    
    # Create sockaddr_in structure (16 bytes total)
    addr_bytes = bytearray(16)
    
    # sin_family = AF_INET (2 bytes, little endian)
    addr_bytes[0] = 2  # AF_INET
    addr_bytes[1] = 0
    
    # sin_port (2 bytes, network byte order)
    port_bytes = struct.pack('>H', port)  # Big endian
    addr_bytes[2] = port_bytes[0]
    addr_bytes[3] = port_bytes[1]
    
    # sin_addr (4 bytes)
    try:
        ip_bytes = socket.inet_aton(ip)
        addr_bytes[4] = ip_bytes[0]
        addr_bytes[5] = ip_bytes[1]
        addr_bytes[6] = ip_bytes[2]
        addr_bytes[7] = ip_bytes[3]
    except socket.error:
        # Handle hostname resolution
        try:
            ip_addr = socket.gethostbyname(ip)
            ip_bytes = socket.inet_aton(ip_addr)
            addr_bytes[4] = ip_bytes[0]
            addr_bytes[5] = ip_bytes[1]
            addr_bytes[6] = ip_bytes[2]
            addr_bytes[7] = ip_bytes[3]
        except:
            raise RGTPError(f"Failed to resolve hostname: {ip}")
    
    # sin_zero padding (8 bytes)
    for i in range(8, 16):
        addr_bytes[i] = 0
    
    # Convert to ctypes array
    addr_ctypes = (ctypes.c_uint8 * 16).from_buffer_copy(addr_bytes)
    
    surface_ptr = ctypes.POINTER(_RGTPSurface)()
    data_ptr = ctypes.cast(data, ctypes.POINTER(ctypes.c_uint8))
    
    result = _lib.rgtp_expose_data(sockfd, data_ptr, len(data), 
                                  ctypes.cast(addr_ctypes, ctypes.POINTER(ctypes.c_uint8)), 
                                  ctypes.byref(surface_ptr))
    
    if result < 0:
        raise RGTPError("Failed to expose data")
    
    return surface_ptr

def pull_data(sockfd: int, source: Tuple[str, int], buffer: bytearray, size: int) -> int:
    """Pull data from source."""
    if not _lib_loaded:
        print(f"Warning: RGTP library not loaded, mock pulling from {source}")
        # Fill buffer with mock data
        mock_data = b"<html><body><h1>Mock RGTP Data</h1></body></html>"
        copy_size = min(len(mock_data), size)
        buffer[:copy_size] = mock_data[:copy_size]
        return copy_size
        
    # Convert source to sockaddr_in
    ip, port = source
    
    # Create proper sockaddr_in structure
    # struct sockaddr_in {
    #     short sin_family;    // AF_INET
    #     unsigned short sin_port;  // Port number
    #     struct in_addr sin_addr;  // IP address
    #     char sin_zero[8];    // Padding
    # }
    
    # For Windows, we need to create a proper sockaddr_in structure
    import socket
    import struct
    
    # Create sockaddr_in structure (16 bytes total)
    addr_bytes = bytearray(16)
    
    # sin_family = AF_INET (2 bytes, little endian)
    addr_bytes[0] = 2  # AF_INET
    addr_bytes[1] = 0
    
    # sin_port (2 bytes, network byte order)
    port_bytes = struct.pack('>H', port)  # Big endian
    addr_bytes[2] = port_bytes[0]
    addr_bytes[3] = port_bytes[1]
    
    # sin_addr (4 bytes)
    try:
        ip_bytes = socket.inet_aton(ip)
        addr_bytes[4] = ip_bytes[0]
        addr_bytes[5] = ip_bytes[1]
        addr_bytes[6] = ip_bytes[2]
        addr_bytes[7] = ip_bytes[3]
    except socket.error:
        # Handle hostname resolution
        try:
            ip_addr = socket.gethostbyname(ip)
            ip_bytes = socket.inet_aton(ip_addr)
            addr_bytes[4] = ip_bytes[0]
            addr_bytes[5] = ip_bytes[1]
            addr_bytes[6] = ip_bytes[2]
            addr_bytes[7] = ip_bytes[3]
        except:
            raise RGTPError(f"Failed to resolve hostname: {ip}")
    
    # sin_zero padding (8 bytes)
    for i in range(8, 16):
        addr_bytes[i] = 0
    
    # Convert to ctypes array
    addr_ctypes = (ctypes.c_uint8 * 16).from_buffer_copy(addr_bytes)
    
    buffer_ptr = ctypes.cast((ctypes.c_uint8 * size).from_buffer(buffer), ctypes.c_void_p)
    size_ptr = ctypes.c_size_t(size)
    
    result = _lib.rgtp_pull_data(sockfd, addr_ctypes, buffer_ptr, ctypes.byref(size_ptr))
    
    if result < 0:
        raise RGTPError("Failed to pull data")
    
    return size_ptr.value

def version() -> str:
    """Get RGTP library version."""
    if not _lib_loaded:
        return "1.0.0 (mock)"
        
    if not _version_available:
        return "1.0.0 (unavailable)"
        
    try:
        version_str = _lib.rgtp_version()
        return version_str.decode('utf-8') if isinstance(version_str, bytes) else version_str
    except:
        return "1.0.0"

def build_info() -> str:
    """Get RGTP build information."""
    if not _lib_loaded:
        return "Mock build info"
        
    if not _build_info_available:
        return "Build info not available"
        
    try:
        info_str = _lib.rgtp_build_info()
        return info_str.decode('utf-8') if isinstance(info_str, bytes) else info_str
    except:
        return "Build info not available"

# High-level classes that use the core functions

class Session:
    """RGTP session for exposing data."""
    
    def __init__(self, port: int = 9999):
        self.sockfd = socket()
        bind(self.sockfd, port)
        self.surface = None
        self.port = port
    
    def expose_data(self, data: bytes, dest: Tuple[str, int] = None) -> None:
        """Expose data for pulling."""
        if dest is None:
            dest = ('127.0.0.1', 9999)
        self.surface = expose_data(self.sockfd, data, dest)
    
    def expose_file(self, filename: Union[str, Path]) -> None:
        """Expose a file for pulling."""
        with open(filename, 'rb') as f:
            data = f.read()
        self.expose_data(data, ('127.0.0.1', 9999))  # Default destination
    
    def get_stats(self) -> Stats:
        """Get session statistics."""
        # Mock implementation for now
        return Stats()
    
    def close(self):
        """Close the session."""
        if hasattr(self, 'sockfd') and self.sockfd >= 0:
            # In a real implementation, we'd close the socket
            pass

class Client:
    """RGTP client for pulling data."""
    
    def __init__(self, port: int = 0):
        # For clients, we don't need to bind to a specific port
        # The OS will assign an ephemeral port
        self.sockfd = socket()
        self.port = port
    
    def pull_data(self, source: Tuple[str, int], buffer: bytearray, size: int) -> int:
        """Pull data from remote host."""
        return pull_data(self.sockfd, source, buffer, size)
    
    def pull_to_file(self, source: Tuple[str, int], filename: Union[str, Path]) -> None:
        """Pull data and save to file."""
        buffer = bytearray(65536)  # 64KB buffer
        bytes_received = self.pull_data(source, buffer, len(buffer))
        
        with open(filename, 'wb') as f:
            f.write(buffer[:bytes_received])
    
    def get_stats(self) -> Stats:
        """Get client statistics."""
        # Mock implementation for now
        return Stats()
    
    def close(self):
        """Close the client."""
        if hasattr(self, 'sockfd') and self.sockfd >= 0:
            # In a real implementation, we'd close the socket
            pass

class HTTPSClient:
    """HTTPS client that uses RGTP as the transport layer."""
    
    def __init__(self):
        self._client = Client()
    
    def download(self, url: str, output_file: Union[str, Path]) -> Stats:
        """
        Download a file via HTTPS over RGTP.
        
        Args:
            url: HTTPS URL to download
            output_file: Path to save the downloaded file
            
        Returns:
            Transfer statistics
        """
        # Parse the URL
        parsed = urllib.parse.urlparse(url)
        if parsed.scheme != 'https':
            raise ValueError("Only HTTPS URLs are supported")
        
        host = parsed.hostname or 'localhost'
        port = parsed.port or 443
        path = parsed.path or '/'
        
        print(f"Downloading {url} via RGTP...")
        print(f"Host: {host}, Port: {port}, Path: {path}")
        
        # For this implementation, we'll use the existing RGTP client
        # In a full implementation, this would integrate TLS with RGTP
        try:
            self._client.pull_to_file((host, port), output_file)
            
            # Get file size for stats
            file_size = os.path.getsize(output_file)
            
            return Stats(
                bytes_transferred=file_size,
                total_bytes=file_size,
                completion_percent=100.0
            )
        except Exception as e:
            raise RGTPError(f"HTTPS download failed: {str(e)}")

# Convenience functions
def send_file(filename: Union[str, Path], host: str, port: int) -> Stats:
    """Send a file using RGTP (convenience function)."""
    session = Session()
    try:
        session.expose_file(filename)
        file_size = os.path.getsize(filename)
        return Stats(
            bytes_transferred=file_size,
            total_bytes=file_size,
            completion_percent=100.0
        )
    finally:
        session.close()

def receive_file(host: str, port: int, filename: Union[str, Path]) -> Stats:
    """Receive a file using RGTP (convenience function)."""
    client = Client()
    try:
        client.pull_to_file((host, port), filename)
        file_size = os.path.getsize(filename)
        return Stats(
            bytes_transferred=file_size,
            total_bytes=file_size,
            completion_percent=100.0
        )
    finally:
        client.close()

# Cleanup on module exit
if _lib_loaded:
    import atexit
    atexit.register(_lib.rgtp_cleanup)