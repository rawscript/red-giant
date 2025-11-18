#!/usr/bin/env python3
"""
Simple RGTP file transfer example in Python.

This example demonstrates basic file transfer using RGTP Python bindings.
"""

import sys
import time
import threading
from pathlib import Path

# Add the bindings directory to path
sys.path.insert(0, str(Path(__file__).parent.parent.parent / "bindings" / "python"))

import rgtp

def progress_monitor(session_or_client, name):
    """Monitor transfer progress in a separate thread."""
    print(f"\n{name} Progress Monitor Started")
    print("=" * 50)
    
    last_bytes = 0
    start_time = time.time()
    
    while True:
        try:
            stats = session_or_client.get_stats()
            
            # Calculate speed
            current_time = time.time()
            elapsed = current_time - start_time
            
            if elapsed > 0:
                speed_mbps = (stats.bytes_transferred - last_bytes) / (1024 * 1024) / elapsed
                last_bytes = stats.bytes_transferred
                start_time = current_time
            else:
                speed_mbps = 0
            
            # Format output
            size_mb = stats.total_bytes / (1024 * 1024)
            transferred_mb = stats.bytes_transferred / (1024 * 1024)
            
            print(f"\r{name}: {transferred_mb:.1f}/{size_mb:.1f} MB "
                  f"({stats.completion_percent:.1f}%) "
                  f"@ {stats.throughput_mbps:.1f} MB/s "
                  f"[{stats.chunks_transferred}/{stats.total_chunks} chunks]", 
                  end="", flush=True)
            
            if stats.completion_percent >= 100.0:
                break
                
            time.sleep(1)
            
        except rgtp.RGTPError:
            break
        except KeyboardInterrupt:
            break
    
    print(f"\n{name} monitoring stopped.")

def expose_file(filename):
    """Expose a file using RGTP."""
    print(f"📤 Exposing file: {filename}")
    
    print(f"Configuration:")
    print(f"  • Port: 9999")
    print(f"  • Adaptive mode: True")
    
    # Create session and expose file
    session = rgtp.Session(9999)
    
    # Start progress monitoring
    monitor_thread = threading.Thread(
        target=progress_monitor, 
        args=(session, "Exposer"),
        daemon=True
    )
    monitor_thread.start()
    
    try:
        session.expose_file(filename)
        print(f"\n File exposed successfully on port 9999")
        print("Waiting for clients to pull... (Press Ctrl+C to stop)")
        
        # In a real implementation, we would wait for completion
        # For now, we'll just sleep for a bit
        time.sleep(10)
        
    except KeyboardInterrupt:
        print("\n Exposure interrupted by user")
    except rgtp.RGTPError as e:
        print(f"\n Exposure failed: {e}")
    
    # Final statistics
    try:
        stats = session.get_stats()
        print(f"\n Final Statistics:")
        print(f"  • Bytes transferred: {stats.bytes_transferred:,}")
        print(f"  • Average throughput: {stats.avg_throughput_mbps:.2f} MB/s")
        print(f"  • Chunks transferred: {stats.chunks_transferred}")
        print(f"  • Retransmissions: {stats.retransmissions}")
        print(f"  • Efficiency: {stats.efficiency_percent:.1f}%")
    except:
        pass

def pull_file(host, port, output_filename):
    """Pull a file using RGTP."""
    print(f"📥 Pulling from {host}:{port} -> {output_filename}")
    
    print(f"Configuration:")
    print(f"  • Timeout: 30000 ms")
    print(f"  • Adaptive mode: True")
    
    try:
        # Create a proper HTTP GET request
        request = f"GET /index.html HTTP/1.1\r\nHost: {host}:{port}\r\n\r\n".encode('utf-8')
        
        # Create client socket
        client_sock = rgtp.rgtp_socket()
        
        # Send request to server (same port for exposing)
        rgtp.expose_data(client_sock, request, (host, port))
        print("Request sent to server")
        
        # Pull the response from the server (same port for pulling)
        start_time = time.time()
        buffer = bytearray(8192)
        bytes_received = rgtp.pull_data(client_sock, (host, port), buffer, len(buffer))
        
        if bytes_received > 0:
            # Write response to file
            with open(output_filename, 'wb') as f:
                f.write(buffer[:bytes_received])
            
            end_time = time.time()
            print(f"\n File pulled successfully!")
            print(f"  • Saved as: {output_filename}")
            print(f"  • Total time: {end_time - start_time:.2f} seconds")
            
            # Show file info
            try:
                file_size = Path(output_filename).stat().st_size
                print(f"  • File size: {file_size:,} bytes ({file_size / (1024*1024):.1f} MB)")
            except:
                pass
        else:
            print("No data received from server")
            return False
        
    except rgtp.RGTPError as e:
        print(f"\n Pull failed: {e}")
        return False
    except Exception as e:
        print(f"\n Unexpected error: {e}")
        return False
    
    return True

def main():
    print("RGTP Python Example")
    print("==================")
    print(f"RGTP Version: {rgtp.version()}")
    print()
    
    if len(sys.argv) < 2:
        print("Usage:")
        print(f"  {sys.argv[0]} expose <filename>")
        print(f"  {sys.argv[0]} pull <host> <port> [output_file]")
        print()
        print("Examples:")
        print(f"  {sys.argv[0]} expose document.pdf")
        print(f"  {sys.argv[0]} pull 127.0.0.1 9999")  # Use IP directly
        print(f"  {sys.argv[0]} pull 192.168.1.100 9999 downloaded.pdf")
        return 1
    
    command = sys.argv[1].lower()
    
    if command == "expose":
        if len(sys.argv) != 3:
            print("Usage: expose <filename>")
            return 1
        
        filename = sys.argv[2]
        if not Path(filename).exists():
            print(f"Error: File '{filename}' not found")
            return 1
        
        expose_file(filename)
        
    elif command == "pull":
        if len(sys.argv) < 4:
            print("Usage: pull <host> <port> [output_file]")
            return 1
        
        host = sys.argv[2]
        port = int(sys.argv[3])
        output_file = sys.argv[4] if len(sys.argv) > 4 else "rgtp_download.bin"
        
        # Use IP address directly to avoid DNS resolution issues
        if host == "localhost":
            host = "127.0.0.1"
        
        success = pull_file(host, port, output_file)
        return 0 if success else 1
        
    else:
        print(f"Unknown command: {command}")
        return 1
    
    return 0

if __name__ == "__main__":
    try:
        sys.exit(main())
    except KeyboardInterrupt:
        print("\n Interrupted by user")
        sys.exit(1)
    except Exception as e:
        print(f"\n Unexpected error: {e}")
        sys.exit(1)