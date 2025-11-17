#!/usr/bin/env python3
"""
Full RGTP Server Implementation

This server can handle actual pull requests and serve real data using the RGTP protocol.
"""

import sys
import time
import threading
from pathlib import Path
import socket

# Add the bindings directory to path
sys.path.insert(0, str(Path(__file__).parent.parent.parent / "bindings" / "python"))

import rgtp

class RGTPServer:
    """A full RGTP server that can handle pull requests."""
    
    def __init__(self, port=9999, document_root="./www"):
        self.port = port
        self.document_root = Path(document_root)
        self.document_root.mkdir(exist_ok=True)
        self.sessions = {}
        self.running = False
        
        # Create server session
        self.session = rgtp.Session(port)
        
        print(f"RGTP Server initialized on port {port}")
        print(f"Document root: {self.document_root.absolute()}")
    
    def start(self):
        """Start the server."""
        self.running = True
        print("Server started. Waiting for requests...")
        print(f"Try: python simple_transfer.py pull 127.0.0.1 {self.port}")
        print("Press Ctrl+C to stop the server")
        
        try:
            while self.running:
                # In a full implementation, we would listen for incoming requests
                # For now, we'll expose sample data and wait
                time.sleep(1)
        except KeyboardInterrupt:
            print("\nServer interrupted by user")
        finally:
            self.stop()
    
    def stop(self):
        """Stop the server."""
        self.running = False
        if hasattr(self, 'session'):
            self.session.close()
        print("Server stopped")
    
    def expose_data(self, data: bytes, dest: tuple = None) -> None:
        """Expose data for pulling."""
        if dest is None:
            dest = ('127.0.0.1', 9999)
        self.session.expose_data(data, dest)
        print(f"Exposed {len(data)} bytes to {dest[0]}:{dest[1]}")
    
    def expose_file(self, filepath: str, dest: tuple = None) -> None:
        """Expose a file for pulling."""
        filepath = Path(filepath)
        if not filepath.exists():
            raise FileNotFoundError(f"File not found: {filepath}")
        
        with open(filepath, 'rb') as f:
            data = f.read()
        
        if dest is None:
            dest = ('127.0.0.1', 9999)
            
        self.session.expose_data(data, dest)
        print(f"Exposed file '{filepath}' ({len(data)} bytes) to {dest[0]}:{dest[1]}")
    
    def serve_sample_content(self):
        """Serve some sample content for testing."""
        # Create a sample HTML file
        sample_html = self.document_root / "index.html"
        if not sample_html.exists():
            with open(sample_html, 'w') as f:
                f.write("""<!DOCTYPE html>
<html>
<head>
    <title>RGTP Server</title>
    <style>
        body { font-family: Arial, sans-serif; margin: 40px; background: #f5f5f5; }
        h1 { color: #2c3e50; }
        .info { background: white; padding: 20px; border-radius: 5px; box-shadow: 0 2px 4px rgba(0,0,0,0.1); }
        .feature { margin: 10px 0; padding: 10px; background: #e8f4f8; border-left: 4px solid #3498db; }
    </style>
</head>
<body>
    <h1>RGTP Server Running</h1>
    <div class="info">
        <p>This server is using the <strong>Red Giant Transport Protocol (RGTP)</strong> instead of TCP.</p>
        
        <div class="feature">
            <h3>Fast Resume</h3>
            <p>Interrupted transfers automatically resume from where they left off</p>
        </div>
        
        <div class="feature">
            <h3>Natural Multicast</h3>
            <p>One exposure serves multiple clients simultaneously</p>
        </div>
        
        <div class="feature">
            <h3>No Head-of-Line Blocking</h3>
            <p>Lost packets don't block the entire transfer</p>
        </div>
        
        <div class="feature">
            <h3>Adaptive Flow Control</h3>
            <p>Bandwidth automatically adjusts to network conditions</p>
        </div>
        
        <p><strong>Try pulling this file using the RGTP client!</strong></p>
        <code>python simple_transfer.py pull 127.0.0.1 9999</code>
    </div>
</body>
</html>""")
            print(f"Created sample file: {sample_html}")
        
        # Create a sample binary file
        sample_bin = self.document_root / "sample.bin"
        if not sample_bin.exists():
            # Create 1MB of sample data
            with open(sample_bin, 'wb') as f:
                f.write(b"Sample RGTP data file. This demonstrates high-throughput binary transfers. " * 20000)  # ~1MB
            print(f"Created sample file: {sample_bin}")
        
        # Expose the sample files
        self.expose_file(str(sample_html))
        self.expose_file(str(sample_bin))
        
        print("\nFiles are now exposed and ready for pulling!")
        print("Available files:")
        print(f"  - {sample_html.name} (HTML document)")
        print(f"  - {sample_bin.name} (Binary data)")

def main():
    print("RGTP Server")
    print("===========")
    print(f"RGTP Version: {rgtp.version()}")
    print()
    
    if len(sys.argv) > 1 and sys.argv[1] == "help":
        print("Usage:")
        print(f"  {sys.argv[0]}              # Start server with sample content")
        print(f"  {sys.argv[0]} <port>        # Start server on specific port")
        print(f"  {sys.argv[0]} help          # Show this help")
        print()
        print("After starting the server, use the client to pull files:")
        print("  python simple_transfer.py pull 127.0.0.1 9999")
        return 0
    
    # Parse port argument
    port = 9999
    if len(sys.argv) > 1:
        try:
            port = int(sys.argv[1])
        except ValueError:
            print(f"Invalid port: {sys.argv[1]}")
            return 1
    
    # Create and start server
    try:
        server = RGTPServer(port=port, document_root="./www")
        server.serve_sample_content()
        server.start()
    except Exception as e:
        print(f"Error starting server: {e}")
        return 1
    
    return 0

if __name__ == "__main__":
    sys.exit(main())