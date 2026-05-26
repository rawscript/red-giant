#!/usr/bin/env python3
"""
expose_file.py — Expose a file over RGTP.

Usage:
    python expose_file.py <file> [--port PORT] [--fec]

The exposer pre-encrypts all chunks once, builds the Merkle tree, and then
serves pull requests from the immutable chunk store. Memory footprint is
O(file_size) regardless of how many pullers connect.
"""

import argparse
import os
import signal
import sys
import time

import rgtp


def main() -> int:
    parser = argparse.ArgumentParser(description="Expose a file over RGTP.")
    parser.add_argument("file", help="File to expose")
    parser.add_argument("--port", type=int, default=9000, help="UDP port (default: 9000)")
    parser.add_argument("--fec", action="store_true", help="Enable Reed-Solomon FEC")
    parser.add_argument("--merkle-proofs", action="store_true",
                        help="Include per-chunk Merkle proofs")
    args = parser.parse_args()

    if not os.path.isfile(args.file):
        print(f"Error: file not found: {args.file}", file=sys.stderr)
        return 1

    # Initialise library
    try:
        rgtp.init()
    except (rgtp.RgtpError, OSError) as e:
        print(f"Error: rgtp.init() failed: {e}", file=sys.stderr)
        return 1

    # Read file into memory
    data = open(args.file, "rb").read()
    file_size = len(data)

    running = True
    def _stop(sig, frame):
        nonlocal running
        running = False
    signal.signal(signal.SIGINT, _stop)

    with rgtp.Socket() as sock:
        try:
            surface = rgtp.expose(sock, data)
        except rgtp.RgtpError as e:
            print(f"Error: expose failed: {e}", file=sys.stderr)
            return 1

        with surface:
            eid = surface.exposure_id().hex()
            print(f"RGTP Exposer  {rgtp.version()}")
            print(f"  File        : {args.file}  ({file_size:,} bytes)")
            print(f"  Exposure ID : {eid}")
            print(f"  Port        : {args.port}")
            print(f"  FEC         : {'on (n=255, k=223)' if args.fec else 'off'}")
            print()
            print(f"  Pull command: rgtp-pull <server-ip>:{args.port} {eid} output.bin")
            print()
            print("Serving. Press Ctrl+C to stop.\n")

            start = time.monotonic()
            while running:
                try:
                    rgtp.poll(surface, timeout_ms=200)
                except rgtp.RgtpError as e:
                    if e.code != -12:   # not RGTP_ERR_TIMEOUT
                        print(f"\nPoll error: {e}", file=sys.stderr)
                        break

                stats = rgtp.get_stats(surface)
                elapsed = time.monotonic() - start
                sent = stats.get("bytes_sent", 0)
                mbps = sent / max(elapsed, 0.001) / 1_048_576
                print(
                    f"\r  Sent: {sent / 1_048_576:.1f} MB"
                    f"  Throughput: {mbps:.1f} MB/s"
                    f"  Uptime: {elapsed:.0f}s   ",
                    end="", flush=True,
                )

    print()
    rgtp.cleanup()
    return 0


if __name__ == "__main__":
    sys.exit(main())
