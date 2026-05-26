"""
rgtp — Python bindings for the Red Giant Transport Protocol.

Provides asyncio-compatible async/await API for exposing and pulling data.
Surfaces RGTP error codes as typed RgtpError exceptions.

Requirements: 14.6, 14.7, 14.8, 23.4
"""

from ._rgtp import (
    RgtpError,
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
)

__all__ = [
    "RgtpError",
    "init",
    "cleanup",
    "version",
    "strerror",
    "Socket",
    "Surface",
    "expose",
    "poll",
    "pull_start",
    "pull_next",
    "progress",
    "get_stats",
]
