"""
rgtp — Python bindings for the Red Giant Transport Protocol.

Provides asyncio-compatible async/await API for exposing and pulling data.
Surfaces RGTP error codes as typed RgtpError exceptions.

Requirements: 14.6, 14.7, 14.8, 23.4

Example — expose a file::

    import rgtp

    rgtp.init()
    with rgtp.Socket() as sock:
        data = open("file.bin", "rb").read()
        with rgtp.expose(sock, data) as surface:
            print("Exposure ID:", surface.exposure_id().hex())
            while True:
                rgtp.poll(surface, timeout_ms=1000)
    rgtp.cleanup()

Example — pull a file::

    import rgtp

    rgtp.init()
    with rgtp.Socket() as sock:
        surface = rgtp.pull_start(sock, ("192.168.1.10", 9000), exposure_id)
        with surface:
            chunks = {}
            while surface.progress() < 1.0:
                data, idx = rgtp.pull_next(surface)
                chunks[idx] = data
    rgtp.cleanup()
"""

from __future__ import annotations

from ._rgtp import (
    RgtpError,
    _check,
    init,
    cleanup,
    version,
    strerror,
    Socket,
    Surface,
    expose,
    poll,
    pull_start,
    pull_next,
    progress,
    get_stats,
    async_expose,
    async_pull_next,
)

__version__ = "1.0.0"
__author__ = "Red Giant Team"
__license__ = "MIT"

__all__ = [
    # Exceptions
    "RgtpError",
    # Library lifecycle
    "init",
    "cleanup",
    "version",
    "strerror",
    # Handles
    "Socket",
    "Surface",
    # Exposer API
    "expose",
    "poll",
    # Puller API
    "pull_start",
    "pull_next",
    "progress",
    # Statistics
    "get_stats",
    # Async wrappers
    "async_expose",
    "async_pull_next",
    # Package metadata
    "__version__",
]
