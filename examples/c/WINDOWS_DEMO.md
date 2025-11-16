# RGTP HTTPS Demo for Windows

## Overview

This directory contains Windows-compatible versions of the RGTP HTTPS examples and scripts to demonstrate the concept on Windows systems.

## Files

### Windows-Compatible Source Files
- `https_over_rgtp_server_win.c` - Windows version of the HTTPS server
- `https_over_rgtp_client_win.c` - Windows version of the HTTPS client
- `Makefile.win` - Windows-compatible Makefile

### Demo Scripts
- `run_https_demo.ps1` - PowerShell script that sets up certificates and test files
- `simple_https_server.py` - Simple Python HTTPS server for testing
- `simple_https_client.py` - Simple Python HTTPS client for testing

### Supporting Files
- `server.crt` - Self-signed SSL certificate
- `server.key` - SSL private key
- `www/` - Directory containing test files
  - `index.html` - Sample web page
  - `test.txt` - Sample text file

## Running the Demo

### Prerequisites
- GCC compiler (TDM-GCC or MinGW)
- OpenSSL for Windows
- Python 3.x

### Quick Setup
Run the PowerShell script to set up certificates and test files:
```powershell
powershell -ExecutionPolicy Bypass -File run_https_demo.ps1
```

### Full Demo
Run the complete demo with server and client:
```powershell
powershell -ExecutionPolicy Bypass -File run_full_demo.ps1
```

## Understanding RGTP

The RGTP (Red Giant Transport Protocol) is an experimental Layer 4 protocol that aims to replace TCP with a pull-based, multicast-capable transport. The key benefits would be:

1. **Natural Multicast**: One server can serve multiple clients simultaneously
2. **Instant Resume**: Connections can resume immediately after interruption
3. **No Head-of-Line Blocking**: Packets can be delivered out of order
4. **Stateless Operation**: No connection state to manage
5. **Built-in Security**: Encryption at the transport layer

## Limitations on Windows

The full RGTP implementation requires:
- Kernel-level modifications
- Platform-specific networking adaptations
- Extensive changes to work with Windows networking stack

The demo shows the concept using standard HTTPS, which helps understand how the application layer would work.

## Building Windows Versions

To build the Windows-compatible versions:
```bash
make -f Makefile.win all
```

To clean build artifacts:
```bash
make -f Makefile.win clean
```