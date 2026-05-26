#!/usr/bin/env python3
"""
pull_file.py — Pull a file from an RGTP exposer.

Usage:
    python pull_file.py <host:port> <exposure-id-hex> <output-file>

The puller drives all flow control by issuing pull requests. Chunks are
verified with AEAD authentication and optional Merkle proofs before delivery.
"""

import argparse
import sys
import time

import rgtp


def main() -> int:
    parser = argparse.ArgumentParser(description="Pull a file from an RGTP exposer.")
    parser.add_argument("server", help="Exposer address, e.g. 192.168.1.10:9000")
    parser.add_argument("exposure_id", help="32-hex-character Exposure ID")
    parser.add_argument("output", help="Output file path")
    parser.add_argument("--timeout", type=int, default=30,
                        help="Timeout in seconds (default: 30)")
    args = parser.parse_args()

    # Parse server address
    try:
        host, port_str = args.server.rsplit(":", 1)
        port = int(port_str)
    except ValueError:
        print(f"Error: invalid server address '{args.server}' (expected host:port)",
              file=sys.stderr)
        return 1

    # Parse exposure ID
    try:
        eid_bytes = bytes.fromhex(args.exposure_id)
        if len(eid_bytes) != 16:
            raise ValueError("must be exactly 32 hex characters")
    except ValueError as e:
        print(f"Error: invalid exposure ID: {e}", file=sys.stderr)
        return 1

    try:
        rgtp.init()
    except (rgtp.RgtpError, OSError) as e:
        print(f"Error: rgtp.init() failed: {e}", file=sys.stderr)
        return 1

    print(f"RGTP Puller  {rgtp.version()}")
    print(f"  Server      : {host}:{port}")
    print(f"  Exposure ID : {args.exposure_id}")
    print(f"  Output      : {args.output}")
    print()

    with rgtp.Socket() as sock:
        try:
            surface = rgtp.pull_start(sock, (host, port), eid_bytes)
        except NotImplementedError:
            print(
                "Error: pull_start is not fully implemented in the ctypes binding.\n"
                "Use the C library, Go binding, or Node.js binding for full pull support.",
                file=sys.stderr,
            )
            return 1
        except rgtp.RgtpError as e:
            print(f"Error: pull_start failed: {e}", file=sys.stderr)
            return 1

        chunks: dict[int, bytes] = {}
        start = time.monotonic()

        with surface:
            while surface.progress() < 1.0:
                try:
                    data, idx = rgtp.pull_next(surface)
                    chunks[idx] = data
                except rgtp.RgtpError as e:
                    if e.code == -12:   # RGTP_ERR_TIMEOUT
                        elapsed = time.monotonic() - start
                        if elapsed > args.timeout:
                            print(f"\nError: timed out after {args.timeout}s",
                                  file=sys.stderr)
                            return 1
                        continue
                    print(f"\nError: pull_next failed: {e}", file=sys.stderr)
                    return 1

                total = sum(len(v) for v in chunks.values())
                elapsed = time.monotonic() - start
                mbps = total / max(elapsed, 0.001) / 1_048_576
                pct = surface.progress() * 100
                print(
                    f"\r  {pct:.1f}%  {total / 1_048_576:.1f} MB"
                    f"  {mbps:.1f} MB/s  {elapsed:.0f}s   ",
                    end="", flush=True,
                )

    # Write chunks in order
    print()
    with open(args.output, "wb") as f:
        for idx in sorted(chunks):
            f.write(chunks[idx])

    total = sum(len(v) for v in chunks.values())
    elapsed = time.monotonic() - start
    print(f"Done. Saved {total / 1_048_576:.2f} MB to {args.output} in {elapsed:.1f}s")

    rgtp.cleanup()
    return 0


if __name__ == "__main__":
    sys.exit(main())
