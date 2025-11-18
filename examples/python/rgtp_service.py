#!/usr/bin/env python3
"""
RGTP Service Implementation

This provides a proper service that can handle RGTP requests and serve files.
"""

import sys
import time
import threading
from pathlib import Path
import socket

# Add the bindings directory to path
sys.path.insert(0, str(Path(__file__).parent.parent.parent / "bindings" / "python"))

import rgtp

class RGTPService:
    """A proper RGTP service that handles requests and serves files."""
    
    def __init__(self, port=9999, document_root="./www"):
        self.port = port
        self.document_root = Path(document_root)
        self.document_root.mkdir(exist_ok=True)
        self.running = False
        self.sockfd = None
        
        print(f"RGTP Service initialized on port {port}")
        print(f"Document root: {self.document_root.absolute()}")
    
    def start(self):
        """Start the service and listen for requests."""
        self.running = True
        
        # Create and bind socket
        self.sockfd = rgtp.rgtp_socket()
        rgtp.bind(self.sockfd, self.port)
        
        print(f"RGTP Service listening on port {self.port}")
        print("Press Ctrl+C to stop the service")
        
        try:
            while self.running:
                try:
                    print("Waiting for RGTP requests...")
                    # Listen for incoming requests (use the same port for both exposing and pulling)
                    buffer = bytearray(8192)
                    # Use the same port for pulling data, not port + 1
                    try:
                        bytes_received = rgtp.pull_data(self.sockfd, ('127.0.0.1', self.port), buffer, len(buffer))
                    except Exception as e:
                        print(f"Error calling pull_data: {e}")
                        # Print the type of the pull_data function to debug
                        print(f"Type of rgtp.pull_data: {type(rgtp.pull_data)}")
                        time.sleep(0.1)
                        continue
                    
                    if bytes_received > 0:
                        request_data = buffer[:bytes_received]
                        try:
                            request_text = request_data.decode('utf-8')
                            print(f"Received request: {request_text.strip()}")
                            
                            # Process the request and send response
                            response = self.handle_request(request_text)
                            if response:
                                # Send response back to client (use the same port for exposing)
                                rgtp.expose_data(self.sockfd, response, ('127.0.0.1', self.port))
                                print("Response sent successfully")
                        except Exception as e:
                            print(f"Error processing request: {e}")
                            import traceback
                            traceback.print_exc()
                    else:
                        time.sleep(0.1)  # Small delay to prevent busy waiting
                        
                except Exception as e:
                    if self.running:
                        print(f"Error in service loop: {e}")
                        import traceback
                        traceback.print_exc()
                    break
                    
        except KeyboardInterrupt:
            print("\nService interrupted by user")
        finally:
            self.stop()
    
    def handle_request(self, request_text):
        """Handle an incoming request and return a response."""
        try:
            # Parse the request (simple HTTP-like parsing)
            lines = request_text.split('\n')
            if not lines:
                return None
                
            # Parse the first line (GET /path HTTP/1.1)
            first_line = lines[0].strip()
            parts = first_line.split()
            if len(parts) < 2:
                return None
                
            method = parts[0]
            path = parts[1]
            
            print(f"Processing {method} request for {path}")
            
            # Handle different paths
            if path == "/" or path == "/index.html":
                return self.serve_file("index.html")
            elif path.startswith("/"):
                # Remove leading slash
                filename = path[1:]
                return self.serve_file(filename)
            else:
                # Return 404
                return self.create_error_response(404, "File not found")
                
        except Exception as e:
            print(f"Error handling request: {e}")
            import traceback
            traceback.print_exc()
            return self.create_error_response(500, "Internal server error")
    
    def serve_file(self, filename):
        """Serve a file from the document root."""
        try:
            filepath = self.document_root / filename
            
            # Security check - make sure file is within document root
            try:
                filepath.resolve().relative_to(self.document_root.resolve())
            except ValueError:
                return self.create_error_response(403, "Access denied")
            
            if not filepath.exists():
                return self.create_error_response(404, "File not found")
            
            # Determine content type
            if filepath.suffix == '.html':
                content_type = 'text/html'
            elif filepath.suffix == '.css':
                content_type = 'text/css'
            elif filepath.suffix == '.js':
                content_type = 'application/javascript'
            else:
                content_type = 'application/octet-stream'
            
            # Read file content
            with open(filepath, 'rb') as f:
                content = f.read()
            
            # Create HTTP-like response
            response = f"HTTP/1.1 200 OK\r\nContent-Type: {content_type}\r\nContent-Length: {len(content)}\r\n\r\n".encode('utf-8')
            response += content
            
            print(f"Served file {filename} ({len(content)} bytes)")
            return response
            
        except Exception as e:
            print(f"Error serving file {filename}: {e}")
            import traceback
            traceback.print_exc()
            return self.create_error_response(500, "Error serving file")
    
    def create_error_response(self, status_code, message):
        """Create an error response."""
        status_text = {404: "Not Found", 500: "Internal Server Error", 403: "Forbidden"}
        content = f"<html><body><h1>{status_code} {status_text.get(status_code, 'Error')}</h1><p>{message}</p></body></html>".encode('utf-8')
        response = f"HTTP/1.1 {status_code} {status_text.get(status_code, 'Error')}\r\nContent-Type: text/html\r\nContent-Length: {len(content)}\r\n\r\n".encode('utf-8')
        response += content
        return response
    
    def stop(self):
        """Stop the service."""
        self.running = False
        print("RGTP Service stopped")
    
    def serve_sample_content(self):
        """Serve some sample content for testing."""
        # Create a sample HTML file
        sample_html = self.document_root / "index.html"
        if not sample_html.exists():
            with open(sample_html, 'w') as f:
                f.write("""<!DOCTYPE html>
<html>
<head>
    <title>RGTP Service</title>
    <style>
        body { font-family: Arial, sans-serif; margin: 40px; background: #f5f5f5; }
        h1 { color: #2c3e50; }
        .info { background: white; padding: 20px; border-radius: 5px; box-shadow: 0 2px 4px rgba(0,0,0,0.1); }
        .feature { margin: 10px 0; padding: 10px; background: #e8f4f8; border-left: 4px solid #3498db; }
    </style>
</head>
<body>
    <h1>RGTP Service Running</h1>
    <div class="info">
        <p>This service is using the <strong>Red Giant Transport Protocol (RGTP)</strong> instead of TCP.</p>
        
        <div class="feature">
            <h3>🚀 Fast Resume</h3>
            <p>Interrupted transfers automatically resume from where they left off</p>
        </div>
        
        <div class="feature">
            <h3>📡 Natural Multicast</h3>
            <p>One exposure serves multiple clients simultaneously</p>
        </div>
        
        <div class="feature">
            <h3>⚡ No Head-of-Line Blocking</h3>
            <p>Lost packets don't block the entire transfer</p>
        </div>
        
        <div class="feature">
            <h3>🎛️ Adaptive Flow Control</h3>
            <p>Bandwidth automatically adjusts to network conditions</p>
        </div>
        
        <p><strong>Try pulling this file using the RGTP client!</strong></p>
        <code>python simple_transfer.py pull 127.0.0.1 9999</code>
    </div>
</body>
</html>""")
            print(f"Created sample file: {sample_html}")

def main():
    print("RGTP Service")
    print("============")
    print(f"RGTP Version: {rgtp.version()}")
    print()
    
    # Parse port argument
    port = 9999
    if len(sys.argv) > 1:
        try:
            port = int(sys.argv[1])
        except ValueError:
            print(f"Invalid port: {sys.argv[1]}")
            return 1
    
    # Create and start service
    try:
        service = RGTPService(port=port, document_root="./www")
        service.serve_sample_content()
        service.start()
    except Exception as e:
        print(f"Error starting service: {e}")
        import traceback
        traceback.print_exc()
        return 1
    
    return 0

if __name__ == "__main__":
    sys.exit(main())