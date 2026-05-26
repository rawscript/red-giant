#!/usr/bin/env python3
"""
async_expose.py — Async/await example: expose a file and serve pull requests.

Demonstrates the asyncio-compatible API. The poll loop runs in a thread pool
executor so it does not block the event loop.
"""

import asyncio
import os
import sys

import rgtp


async def serve(path: str, port: int = 9000) -> None:
    """Expose a file and serve pull requests until cancelled."""
    data = open(path, "rb").read()
    print(f"Exposing {path} ({len(data):,} bytes) on port {port}")

    with rgtp.Socket() as sock:
        surface = await rgtp.async_expose(sock, data)
        with surface:
            print(f"Exposure ID: {surface.exposure_id().hex()}")
            print("Serving. Press Ctrl+C to stop.\n")

            loop = asyncio.get_event_loop()
            while True:
                # Run blocking poll in thread pool so event loop stays responsive
                await loop.run_in_executor(
                    None, lambda: rgtp.poll(surface, timeout_ms=200)
                )


async def main() -> None:
    if len(sys.argv) < 2:
        print(f"Usage: {sys.argv[0]} <file> [port]", file=sys.stderr)
        sys.exit(1)

    path = sys.argv[1]
    port = int(sys.argv[2]) if len(sys.argv) > 2 else 9000

    if not os.path.isfile(path):
        print(f"Error: file not found: {path}", file=sys.stderr)
        sys.exit(1)

    rgtp.init()
    try:
        await serve(path, port)
    except asyncio.CancelledError:
        pass
    except KeyboardInterrupt:
        pass
    finally:
        rgtp.cleanup()
        print("\nShutdown complete.")


if __name__ == "__main__":
    asyncio.run(main())
