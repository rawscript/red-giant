#!/usr/bin/env python3
import sys
from pathlib import Path

# Add the bindings directory to path
sys.path.insert(0, str(Path(__file__).parent.parent.parent / "bindings" / "python"))

import rgtp

def test_pull():
    print("Testing pull_data function")
    print(f"rgtp.pull_data type: {type(rgtp.pull_data)}")
    
    # Create a socket
    sockfd = rgtp.rgtp_socket()
    print(f"Socket created: {sockfd}")
    
    # Try to call pull_data
    buffer = bytearray(1024)
    try:
        bytes_received = rgtp.pull_data(sockfd, ('127.0.0.1', 9999), buffer, len(buffer))
        print(f"Bytes received: {bytes_received}")
    except Exception as e:
        print(f"Error calling pull_data: {e}")
        import traceback
        traceback.print_exc()

if __name__ == "__main__":
    test_pull()