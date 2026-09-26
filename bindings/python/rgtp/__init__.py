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
    # Exceptions and error codes
    RgtpError,
    
    # Library lifecycle
    init,
    cleanup,
    version,
    strerror,
    is_initialized,
    
    # Handles
    Socket,
    Surface,
    
    # Exposer API
    expose,
    poll,
    
    # Puller API
    pull_start,
    pull_next,
    progress,
    
    # Statistics
    get_stats,
    get_latency_stats,
    
    # Satellite Communications API
    get_satellite_stats,
    schedule_contact,
    update_link_parameters,
    enable_store_forward,
    get_contact_windows,
    configure_ccsds,
    send_emergency_tc,
    calculate_link_budget,
    configure_doppler,
    check_contact_status,
    
    # Async wrappers
    async_expose,
    async_pull_next,
    async_poll,
)

__version__ = "1.2.0"
__author__ = "Red Giant Team"
__license__ = "MIT"

__all__ = [
    # Exceptions and error codes
    "RgtpError",
    
    # Library lifecycle
    "init",
    "cleanup",
    "version",
    "strerror",
    "is_initialized",
    
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
    "get_latency_stats",
    
    # Satellite Communications API
    "get_satellite_stats",
    "schedule_contact",
    "update_link_parameters",
    "enable_store_forward",
    "get_contact_windows",
    "configure_ccsds",
    "send_emergency_tc",
    "calculate_link_budget",
    "configure_doppler",
    "check_contact_status",
    
    # Async wrappers
    "async_expose",
    "async_pull_next",
    "async_poll",
    
    # Package metadata
    "__version__",
]
