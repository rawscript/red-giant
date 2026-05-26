"""
rgtp._rgtp — ctypes-based implementation of the RGTP Python binding.

Uses ctypes to call into librgtp.so / rgtp.dll without requiring a C
extension build step. For production use, replace with a cffi or Cython
extension for better performance and type safety.

Requirements: 14.6, 14.7, 14.8
"""

import ctypes
import ctypes.util
import asyncio
import os
import sys
from typing import Optional, Tuple

# ── Load shared library ──────────────────────────────────────────────────

def _load_lib():
    """Locate and load librgtp."""
    # Try explicit path first (set by installer or environment)
    lib_path = os.environ.get("RGTP_LIB_PATH")
    if lib_path:
        return ctypes.CDLL(lib_path)

    # Platform-specific search
    if sys.platform == "win32":
        name = "rgtp.dll"
    elif sys.platform == "darwin":
        name = "librgtp.dylib"
    else:
        name = "librgtp.so"

    found = ctypes.util.find_library("rgtp")
    if found:
        return ctypes.CDLL(found)

    raise OSError(f"Cannot find librgtp ({name}). "
                  "Set RGTP_LIB_PATH or install the library.")

try:
    _lib = _load_lib()
except OSError:
    _lib = None   # Allow import to succeed; errors raised at call time


# ── Error type ───────────────────────────────────────────────────────────

class RgtpError(Exception):
    """Raised when an RGTP API call returns a non-OK error code."""

    def __init__(self, code: int, message: str = ""):
        self.code = code
        self.message = message or (strerror(code) if _lib else f"error {code}")
        super().__init__(f"RGTP error {code}: {self.message}")


def _check(code: int) -> None:
    """Raise RgtpError if code != RGTP_OK (0)."""
    if code != 0:
        raise RgtpError(code)


# ── C type declarations ──────────────────────────────────────────────────

if _lib:
    # Opaque handle types
    class _SurfaceHandle(ctypes.Structure):
        pass

    class _SocketHandle(ctypes.Structure):
        pass

    _SurfacePtr = ctypes.POINTER(_SurfaceHandle)
    _SocketPtr  = ctypes.POINTER(_SocketHandle)

    # rgtp_init / cleanup / version / strerror
    _lib.rgtp_init.restype  = ctypes.c_int
    _lib.rgtp_init.argtypes = []

    _lib.rgtp_cleanup.restype  = None
    _lib.rgtp_cleanup.argtypes = []

    _lib.rgtp_version.restype  = ctypes.c_char_p
    _lib.rgtp_version.argtypes = []

    _lib.rgtp_strerror.restype  = ctypes.c_char_p
    _lib.rgtp_strerror.argtypes = [ctypes.c_int]

    # rgtp_socket_create / destroy
    _lib.rgtp_socket_create.restype  = ctypes.c_int
    _lib.rgtp_socket_create.argtypes = [ctypes.c_void_p,
                                         ctypes.POINTER(_SocketPtr)]

    _lib.rgtp_socket_destroy.restype  = None
    _lib.rgtp_socket_destroy.argtypes = [_SocketPtr]

    # rgtp_expose
    _lib.rgtp_expose.restype  = ctypes.c_int
    _lib.rgtp_expose.argtypes = [_SocketPtr,
                                   ctypes.c_void_p,
                                   ctypes.c_size_t,
                                   ctypes.c_void_p,
                                   ctypes.POINTER(_SurfacePtr)]

    # rgtp_poll
    _lib.rgtp_poll.restype  = ctypes.c_int
    _lib.rgtp_poll.argtypes = [_SurfacePtr, ctypes.c_int]

    # rgtp_destroy_surface
    _lib.rgtp_destroy_surface.restype  = None
    _lib.rgtp_destroy_surface.argtypes = [_SurfacePtr]

    # rgtp_get_exposure_id
    _lib.rgtp_get_exposure_id.restype  = ctypes.c_int
    _lib.rgtp_get_exposure_id.argtypes = [_SurfacePtr,
                                            ctypes.c_char_p]  # uint8_t[16]

    # rgtp_progress
    _lib.rgtp_progress.restype  = ctypes.c_float
    _lib.rgtp_progress.argtypes = [_SurfacePtr]

    # rgtp_pull_next
    _lib.rgtp_pull_next.restype  = ctypes.c_int
    _lib.rgtp_pull_next.argtypes = [_SurfacePtr,
                                     ctypes.c_void_p,
                                     ctypes.c_size_t,
                                     ctypes.POINTER(ctypes.c_size_t),
                                     ctypes.POINTER(ctypes.c_uint32)]


# ── Public API ───────────────────────────────────────────────────────────

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


class Socket:
    """Wraps an rgtp_socket_t handle."""

    def __init__(self):
        if not _lib:
            raise OSError("librgtp not loaded")
        self._ptr = _SocketPtr()
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


def expose(sock: Socket, data: bytes) -> Surface:
    """Pre-encrypt data and create an immutable Exposure."""
    if not _lib:
        raise OSError("librgtp not loaded")
    buf = ctypes.create_string_buffer(data, len(data))
    ptr = _SurfacePtr()
    _check(_lib.rgtp_expose(sock._ptr, buf, len(data), None, ctypes.byref(ptr)))
    return Surface(ptr)


def poll(surface: Surface, timeout_ms: int = 100) -> None:
    """Serve pending pull requests for an active Exposure."""
    code = _lib.rgtp_poll(surface._ptr, timeout_ms)
    if code not in (0, -12):   # RGTP_OK or RGTP_ERR_TIMEOUT
        _check(code)


def pull_start(sock: Socket, server_addr: Tuple[str, int],
               exposure_id: bytes) -> Surface:
    """Begin pulling an Exposure from a remote Exposer."""
    raise NotImplementedError(
        "pull_start requires sockaddr_storage construction; "
        "use the cffi extension for full implementation"
    )


def pull_next(surface: Surface, buf_size: int = 65536) -> Tuple[bytes, int]:
    """Receive the next available chunk. Returns (data, chunk_index)."""
    buf = ctypes.create_string_buffer(buf_size)
    received   = ctypes.c_size_t(0)
    chunk_idx  = ctypes.c_uint32(0)
    _check(_lib.rgtp_pull_next(surface._ptr, buf, buf_size,
                                ctypes.byref(received),
                                ctypes.byref(chunk_idx)))
    return bytes(buf[:received.value]), chunk_idx.value


def progress(surface: Surface) -> float:
    """Return the transfer completion fraction [0.0, 1.0]."""
    return surface.progress()


def get_stats(surface: Surface) -> dict:
    """Return transfer statistics as a dictionary."""
    # Stats struct access requires ctypes struct definition;
    # simplified version returns progress only
    return {"progress": surface.progress()}


# ── Async wrappers ───────────────────────────────────────────────────────

async def async_expose(sock: Socket, data: bytes) -> Surface:
    """Async wrapper for expose() — runs in a thread pool executor."""
    loop = asyncio.get_event_loop()
    return await loop.run_in_executor(None, expose, sock, data)


async def async_pull_next(surface: Surface,
                           buf_size: int = 65536) -> Tuple[bytes, int]:
    """Async wrapper for pull_next() — runs in a thread pool executor."""
    loop = asyncio.get_event_loop()
    return await loop.run_in_executor(None, pull_next, surface, buf_size)
