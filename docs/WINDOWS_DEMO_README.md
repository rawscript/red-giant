# RGTP HTTPS Demo Instructions for Windows

## Overview

This document has been moved to a more appropriate location. Please refer to the detailed documentation in the examples directory:

**See: [examples/c/WINDOWS_DEMO.md](../examples/c/WINDOWS_DEMO.md)**

## Quick Reference

For a quick start with the Windows demo, navigate to the examples/c directory:

```bash
cd examples/c
```

Then follow the instructions in the WINDOWS_DEMO.md file in that directory.

## Note

The Windows demo demonstrates the concept of HTTPS over RGTP using standard tools since the full RGTP implementation requires kernel-level modifications that are not available on Windows without WSL2 or extensive platform-specific adaptations.

## What We've Created

1. **Windows-compatible source files**:
   - `https_over_rgtp_server_win.c` - Windows version of the HTTPS server
   - `https_over_rgtp_client_win.c` - Windows version of the HTTPS client
   - `Makefile.win` - Windows-compatible Makefile

2. **Demo scripts**:
   - `run_https_demo.ps1` - PowerShell script that sets up certificates and test files
   - `simple_https_server.py` - Simple Python HTTPS server for testing
   - `simple_https_client.py` - Simple Python HTTPS client for testing
   - `run_full_demo.ps1` - Complete demo runner

## Running the Demo

### Option 1: Quick Setup Only
```powershell
powershell -ExecutionPolicy Bypass -File run_https_demo.ps1
```
This creates certificates and test files needed for HTTPS.

### Option 2: Full Demo
```powershell
powershell -ExecutionPolicy Bypass -File run_full_demo.ps1
```
This runs a complete HTTPS demonstration using standard protocols.

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

## Files Created

- `server.crt` / `server.key` - Self-signed SSL certificates
- `www/index.html` - Sample web page
- `www/test.txt` - Sample text file