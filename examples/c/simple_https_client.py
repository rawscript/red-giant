#!/usr/bin/env python3
"""
Simple HTTPS Client for RGTP Demo
This demonstrates how HTTPS can be implemented using RGTP as the transport layer.
"""

import sys
import os

# Add the bindings directory to the path so we can import rgtp
sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "..", "bindings", "python"))

try:
    import rgtp
except ImportError as e:
    print(f"Error importing RGTP module: {e}")
    print("Make sure the RGTP library is built and accessible.")
    sys.exit(1)

def download_file(url, output_file):
    """Download a file from HTTPS URL using RGTP transport"""
    try:
        print(f"Downloading {url} via RGTP...")
        
        # Parse URL
        if url.startswith("https://"):
            url = url[8:]
        
        parts = url.split("/", 1)
        host_port = parts[0]
        path = "/" + parts[1] if len(parts) > 1 else "/"
        
        if ":" in host_port:
            host, port = host_port.split(":")
            port = int(port)
        else:
            host = host_port
            port = 8443  # Default HTTPS port
        
        # Use IP address directly to avoid DNS resolution issues
        if host == "localhost":
            host = "127.0.0.1"
        
        print(f"Connecting to {host}:{port}{path}")
        
        # Create HTTPS client that uses RGTP as transport
        client = rgtp.HTTPSClient()
        
        # Download the file
        stats = client.download(f"https://{host}:{port}{path}", output_file)
        
        print(f"File downloaded successfully to {output_file}")
        print(f"Transfer statistics:")
        print(f"  Bytes transferred: {stats.bytes_transferred}")
        print(f"  Throughput: {stats.throughput_mbps:.2f} MB/s")
        print(f"  Completion: {stats.completion_percent:.1f}%")
        
        # Show file content
        print("\nFile content:")
        print("-" * 40)
        try:
            with open(output_file, 'r', encoding='utf-8') as f:
                content = f.read()
                print(content)
        except UnicodeDecodeError:
            # For binary files, show size instead
            file_size = os.path.getsize(output_file)
            print(f"[Binary file - {file_size} bytes]")
        print("-" * 40)
        
    except rgtp.RGTPError as e:
        print(f"RGTP Error downloading file: {e}")
        return False
    except Exception as e:
        print(f"Unexpected error: {e}")
        return False
    
    return True

def main():
    if len(sys.argv) < 2:
        print("Usage: python simple_https_client.py <output_file>")
        print("Example: python simple_https_client.py downloaded.html")
        return 1
    
    output_file = sys.argv[1]
    url = "https://127.0.0.1:8443/index.html"  # Use IP directly instead of localhost
    
    print("RGTP HTTPS Client Demo")
    print("=" * 30)
    print("This client uses RGTP as the transport layer instead of TCP.")
    print("RGTP provides exposure-based data transmission with benefits like:")
    print("  - Instant resume capability")
    print("  - Natural multicast (one source, multiple clients)")
    print("  - No head-of-line blocking")
    print("  - Stateless operation")
    print()
    
    return 0 if download_file(url, output_file) else 1

if __name__ == "__main__":
    sys.exit(main())