#!/usr/bin/env python3
"""
Simple HTTPS Client for RGTP Demo
This is a standard HTTPS client to demonstrate how files would be downloaded.
It's not using RGTP - that would require kernel-level modifications.
"""

import urllib.request
import urllib.error
import ssl
import sys
import os

def download_file(url, output_file):
    """Download a file from HTTPS URL"""
    try:
        # Create unverified SSL context for self-signed certificates
        context = ssl.create_default_context()
        context.check_hostname = False
        context.verify_mode = ssl.CERT_NONE
        
        print(f"Downloading {url}...")
        
        # Create a request with the SSL context
        request = urllib.request.Request(url)
        response = urllib.request.urlopen(request, context=context)
        
        # Read the response data
        data = response.read()
        
        # Write to file
        with open(output_file, 'wb') as f:
            f.write(data)
        
        print(f"File downloaded successfully to {output_file}")
        
        # Show file content
        print("\nFile content:")
        print("-" * 40)
        try:
            # Try to decode as UTF-8 for text files
            content = data.decode('utf-8')
            print(content)
        except UnicodeDecodeError:
            # For binary files, show size instead
            print(f"[Binary file - {len(data)} bytes]")
        print("-" * 40)
        
    except urllib.error.URLError as e:
        print(f"Error downloading file: {e}")
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
    url = "https://localhost:8443/index.html"
    
    print("RGTP HTTPS Client Demo")
    print("=" * 30)
    print("Note: This is a standard HTTPS client, not RGTP.")
    print("In a real RGTP implementation, the transport would be different.")
    print()
    
    return 0 if download_file(url, output_file) else 1

if __name__ == "__main__":
    sys.exit(main())