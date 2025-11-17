#!/usr/bin/env python3
"""
Simple HTTPS Server using RGTP as Transport Layer
This demonstrates how an HTTPS server can be implemented using RGTP instead of TCP.
"""

import sys
import os
import ssl
from http.server import BaseHTTPRequestHandler, HTTPServer
from socketserver import ThreadingMixIn

# Add the bindings directory to the path so we can import rgtp
sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "..", "bindings", "python"))

try:
    import rgtp
except ImportError as e:
    print(f"Error importing RGTP module: {e}")
    print("Make sure the RGTP library is built and accessible.")
    sys.exit(1)

class RGTPHTTPHandler(BaseHTTPRequestHandler):
    """HTTP handler that works with RGTP transport"""
    
    def do_GET(self):
        """Handle GET requests"""
        print(f"Received GET request for {self.path}")
        
        # For demo purposes, serve files from the www directory
        if self.path == "/" or self.path == "/index.html":
            self.serve_file("www/index.html", "text/html")
        else:
            self.send_error(404, "File not found")
    
    def serve_file(self, filepath, content_type):
        """Serve a file"""
        try:
            with open(filepath, 'rb') as f:
                content = f.read()
            
            self.send_response(200)
            self.send_header('Content-Type', content_type)
            self.send_header('Content-Length', str(len(content)))
            self.end_headers()
            self.wfile.write(content)
            print(f"Served {filepath} ({len(content)} bytes)")
        except FileNotFoundError:
            self.send_error(404, "File not found")
        except Exception as e:
            print(f"Error serving file: {e}")
            self.send_error(500, "Internal server error")

class ThreadedHTTPServer(ThreadingMixIn, HTTPServer):
    """Handle requests in separate threads"""
    pass

def main():
    # Check if certificate files exist
    if not os.path.exists("server.crt") or not os.path.exists("server.key"):
        print("Error: Certificate files not found!")
        print("Please run the demo script first to generate certificates.")
        sys.exit(1)
    
    server_address = ('127.0.0.1', 8443)  # Use IP directly instead of localhost
    
    # Note: This is still using TCP for demonstration purposes
    # A full RGTP implementation would require kernel-level modifications
    # to replace TCP entirely at Layer 4
    try:
        httpd = ThreadedHTTPServer(server_address, RGTPHTTPHandler)
        
        # Wrap with SSL
        httpd.socket = ssl.wrap_socket(
            httpd.socket,
            certfile="server.crt",
            keyfile="server.key",
            server_side=True
        )
        
        print("HTTPS Server starting on https://127.0.0.1:8443")  # Use IP directly
        print("Document root is:", os.path.abspath("www"))
        print("Note: This demo uses standard TCP/TLS for simplicity.")
        print("In a full RGTP implementation, the transport would be different.")
        print("Press Ctrl+C to stop the server")
        
        # Change to www directory to serve files from there
        os.chdir("www")
        httpd.serve_forever()
        
    except KeyboardInterrupt:
        print("\nServer stopped.")
    except Exception as e:
        print(f"Error: {e}")

if __name__ == "__main__":
    main()