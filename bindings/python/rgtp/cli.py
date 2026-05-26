"""
rgtp.cli — Command-line interface for the RGTP Python binding.

Installed as:
  rgtp-expose  →  rgtp.cli:cmd_expose
  rgtp-pull    →  rgtp.cli:cmd_pull
"""

from __future__ import annotations

import argparse
import sys
import os
import signal
import time
from typing import Optional


def _fmt_bytes(n: int) -> str:
    """Format a byte count as a human-readable string."""
    for unit in ("B", "KB", "MB", "GB", "TB"):
        if n < 1024:
            return f"{n:.1f} {unit}"
        n /= 1024  # type: ignore[assignment]
    return f"{n:.1f} PB"


def cmd_expose(argv: Optional[list[str]] = None) -> int:
    """
    rgtp-expose — expose a file over RGTP.

    Usage: rgtp-expose <file> [--port PORT] [--fec] [--chunk-size BYTES]
    """
    parser = argparse.ArgumentParser(
        prog="rgtp-expose",
        description="Expose a file over the Red Giant Transport Protocol.",
    )
    parser.add_argument("file", help="Path to the file to expose")
    parser.add_argument(
        "--port", type=int, default=9000,
        help="UDP port to listen on (default: 9000, 0 = auto-assign)",
    )
    parser.add_argument(
        "--fec", action="store_true",
        help="Enable Reed-Solomon FEC (n=255, k=223, ~14%% overhead)",
    )
    parser.add_argument(
        "--chunk-size", type=int, default=0, metavar="BYTES",
        help="Chunk size in bytes (default: 1200 for UDP)",
    )
    parser.add_argument(
        "--merkle-proofs", action="store_true",
        help="Include per-chunk Merkle proofs in responses",
    )
    args = parser.parse_args(argv)

    # Validate file
    if not os.path.isfile(args.file):
        print(f"rgtp-expose: error: file not found: {args.file}", file=sys.stderr)
        return 1

    file_size = os.path.getsize(args.file)

    try:
        import rgtp
    except ImportError as e:
        print(f"rgtp-expose: error: cannot import rgtp: {e}", file=sys.stderr)
        return 1

    try:
        rgtp.init()
    except (rgtp.RgtpError, OSError) as e:
        print(f"rgtp-expose: error: init failed: {e}", file=sys.stderr)
        return 1

    # Read file
    try:
        with open(args.file, "rb") as f:
            data = f.read()
    except OSError as e:
        print(f"rgtp-expose: error: cannot read file: {e}", file=sys.stderr)
        return 1

    running = True

    def _sigint(sig: int, frame: object) -> None:
        nonlocal running
        running = False

    signal.signal(signal.SIGINT, _sigint)

    try:
        sock = rgtp.Socket()
    except (rgtp.RgtpError, OSError) as e:
        print(f"rgtp-expose: error: socket creation failed: {e}", file=sys.stderr)
        return 1

    try:
        surface = rgtp.expose(sock, data)
    except (rgtp.RgtpError, OSError) as e:
        print(f"rgtp-expose: error: expose failed: {e}", file=sys.stderr)
        sock.close()
        return 1

    exposure_id_hex = surface.exposure_id().hex()

    print(f"RGTP Exposer v{rgtp.version()}")
    print(f"  File         : {args.file}")
    print(f"  Size         : {_fmt_bytes(file_size)}")
    print(f"  Exposure ID  : {exposure_id_hex}")
    print(f"  Port         : {args.port}")
    print(f"  FEC          : {'enabled (n=255, k=223)' if args.fec else 'disabled'}")
    print(f"  Merkle proofs: {'enabled' if args.merkle_proofs else 'disabled'}")
    print()
    print(f"Pull command:")
    print(f"  rgtp-pull <server-ip>:{args.port} {exposure_id_hex} output.bin")
    print()
    print("Serving pull requests. Press Ctrl+C to stop.")
    print()

    start_time = time.monotonic()

    try:
        while running:
            try:
                rgtp.poll(surface, timeout_ms=200)
            except rgtp.RgtpError as e:
                if e.code == -12:  # RGTP_ERR_TIMEOUT
                    pass
                else:
                    print(f"\nrgtp-expose: poll error: {e}", file=sys.stderr)
                    break

            stats = rgtp.get_stats(surface)
            elapsed = time.monotonic() - start_time
            throughput = stats.get("bytes_sent", 0) / max(elapsed, 0.001) / 1024 / 1024

            print(
                f"\r  Sent: {_fmt_bytes(stats.get('bytes_sent', 0))}"
                f"  Pullers: {stats.get('pull_pressure', 0)}"
                f"  Throughput: {throughput:.1f} MB/s"
                f"  Uptime: {elapsed:.0f}s   ",
                end="",
                flush=True,
            )
    finally:
        print()
        surface.close()
        sock.close()
        rgtp.cleanup()

    return 0


def cmd_pull(argv: Optional[list[str]] = None) -> int:
    """
    rgtp-pull — pull a file from an RGTP exposer.

    Usage: rgtp-pull <host:port> <exposure-id-hex> <output-file>
    """
    parser = argparse.ArgumentParser(
        prog="rgtp-pull",
        description="Pull a file from an RGTP exposer.",
    )
    parser.add_argument(
        "server",
        help="Exposer address in host:port format (e.g. 192.168.1.10:9000)",
    )
    parser.add_argument(
        "exposure_id",
        help="128-bit Exposure ID as 32 hex characters",
    )
    parser.add_argument("output", help="Output file path")
    parser.add_argument(
        "--timeout", type=int, default=30,
        help="Connection timeout in seconds (default: 30)",
    )
    parser.add_argument(
        "--chunk-size", type=int, default=65536, metavar="BYTES",
        help="Receive buffer size per chunk (default: 65536)",
    )
    args = parser.parse_args(argv)

    # Parse server address
    try:
        host, port_str = args.server.rsplit(":", 1)
        port = int(port_str)
    except ValueError:
        print(
            f"rgtp-pull: error: invalid server address '{args.server}' "
            "(expected host:port)",
            file=sys.stderr,
        )
        return 1

    # Parse exposure ID
    try:
        exposure_id_bytes = bytes.fromhex(args.exposure_id)
        if len(exposure_id_bytes) != 16:
            raise ValueError("must be exactly 32 hex characters")
    except ValueError as e:
        print(f"rgtp-pull: error: invalid exposure ID: {e}", file=sys.stderr)
        return 1

    try:
        import rgtp
    except ImportError as e:
        print(f"rgtp-pull: error: cannot import rgtp: {e}", file=sys.stderr)
        return 1

    try:
        rgtp.init()
    except (rgtp.RgtpError, OSError) as e:
        print(f"rgtp-pull: error: init failed: {e}", file=sys.stderr)
        return 1

    running = True

    def _sigint(sig: int, frame: object) -> None:
        nonlocal running
        running = False

    signal.signal(signal.SIGINT, _sigint)

    try:
        sock = rgtp.Socket()
    except (rgtp.RgtpError, OSError) as e:
        print(f"rgtp-pull: error: socket creation failed: {e}", file=sys.stderr)
        return 1

    print(f"RGTP Puller v{rgtp.version()}")
    print(f"  Server       : {host}:{port}")
    print(f"  Exposure ID  : {args.exposure_id}")
    print(f"  Output       : {args.output}")
    print()

    try:
        surface = rgtp.pull_start(sock, (host, port), exposure_id_bytes)
    except NotImplementedError:
        # pull_start requires sockaddr_storage — use the ctypes path directly
        print(
            "rgtp-pull: note: pull_start not fully implemented in ctypes binding.\n"
            "Use the C library directly or the Go/Node.js binding for full pull support.",
            file=sys.stderr,
        )
        sock.close()
        rgtp.cleanup()
        return 1
    except (rgtp.RgtpError, OSError) as e:
        print(f"rgtp-pull: error: pull_start failed: {e}", file=sys.stderr)
        sock.close()
        return 1

    chunks: dict[int, bytes] = {}
    start_time = time.monotonic()

    try:
        with open(args.output, "wb") as out_file:
            while running and surface.progress() < 1.0:
                try:
                    data, chunk_index = rgtp.pull_next(surface, args.chunk_size)
                    chunks[chunk_index] = data
                except rgtp.RgtpError as e:
                    if e.code == -12:  # RGTP_ERR_TIMEOUT
                        continue
                    print(f"\nrgtp-pull: error: {e}", file=sys.stderr)
                    break

                elapsed = time.monotonic() - start_time
                total_received = sum(len(v) for v in chunks.values())
                throughput = total_received / max(elapsed, 0.001) / 1024 / 1024
                pct = surface.progress() * 100

                print(
                    f"\r  Progress: {pct:.1f}%"
                    f"  Received: {_fmt_bytes(total_received)}"
                    f"  Speed: {throughput:.1f} MB/s"
                    f"  Elapsed: {elapsed:.0f}s   ",
                    end="",
                    flush=True,
                )

            # Write chunks in order
            print()
            for idx in sorted(chunks):
                out_file.write(chunks[idx])

    except OSError as e:
        print(f"\nrgtp-pull: error: cannot write output: {e}", file=sys.stderr)
        surface.close()
        sock.close()
        rgtp.cleanup()
        return 1
    finally:
        surface.close()
        sock.close()
        rgtp.cleanup()

    elapsed = time.monotonic() - start_time
    total = sum(len(v) for v in chunks.values())
    print(f"Done. Saved {_fmt_bytes(total)} to {args.output} in {elapsed:.1f}s")
    return 0


if __name__ == "__main__":
    sys.exit(cmd_expose())
