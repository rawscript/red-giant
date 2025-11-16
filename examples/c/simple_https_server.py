#!/usr/bin/env python3
"""
Simple HTTPS Server for RGTP Demo
This is a standard HTTPS server to demonstrate how the files would be served.
It's not using RGTP - that would require kernel-level modifications.
"""

import http.server
import ssl
import os
import sys

# Check if certificate files exist
if not os.path.exists("server.crt") or not os.path.exists("server.key"):
    print("Error: Certificate files not found!")
    print("Please run the demo script first to generate certificates.")
    sys.exit(1)

# Create HTTP server
server_address = ('localhost', 8443)
httpd = http.server.HTTPServer(server_address, http.server.SimpleHTTPRequestHandler)

# Create SSL context
context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
context.load_cert_chain("server.crt", "server.key")

# Wrap the HTTP server with SSL
httpd.socket = context.wrap_socket(httpd.socket, server_side=True)

print("HTTPS Server starting on https://localhost:8443")
print("Document root is:", os.path.abspath("www"))
print("Press Ctrl+C to stop the server")

try:
    # Change to www directory to serve files from there
    os.chdir("www")
    httpd.serve_forever()
except KeyboardInterrupt:
    print("\nServer stopped.")
except Exception as e:
    print(f"Error: {e}")