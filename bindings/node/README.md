# Red Giant Transport Protocol (RGTP) - Node.js Bindings

High-performance UDP-based transport protocol for Node.js with full JavaScript API.

**Key Features:**
- **Session/Client Architecture** - Event-driven file transfer
- **Progress Tracking** - Real-time transfer monitoring
- **Configuration Presets** - Optimized for LAN/WAN/mobile
- **Cross-platform** - Works on Windows, macOS, and Linux
- **Native Performance** - Built on proven C core implementation

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)
[![NPM Version](https://img.shields.io/npm/v/rgtp.svg)](https://www.npmjs.com/package/rgtp)
[![Build Status](https://img.shields.io/github/workflow/status/rawscript/red-giant/CI)](https://github.com/rawscript/red-giant/actions)

## Installation

### Quick Install

```bash
npm install rgtp
```

### Prerequisites

- Node.js 14.0.0 or higher
- npm 6.0.0 or higher
- Build tools (automatically installed on most systems)

The package includes pre-built binaries for major platforms.

## Quick Start

### Server Side (Expose a File)

```javascript
const rgtp = require('rgtp');

// Create server session
const session = new rgtp.Session({
  port: 9999,
  adaptiveMode: true
});

// Listen for events
session.on('exposeStart', (filePath, fileSize) => {
  console.log(`Exposing ${filePath} (${fileSize} bytes)`);
});

session.on('progress', (transferred, total) => {
  const percent = ((transferred / total) * 100).toFixed(1);
  console.log(`Progress: ${percent}%`);
});

session.on('error', (error) => {
  console.error('Error:', error.message);
});

// Expose file
await session.exposeFile('large-file.bin');
console.log('File exposed successfully!');

// Get statistics
const stats = await session.getStats();
console.log(`Transferred: ${rgtp.formatBytes(stats.bytesTransferred)}`);
console.log(`Throughput: ${stats.avgThroughputMbps.toFixed(2)} MB/s`);

// Cleanup
session.close();
```

### Client Side (Pull a File)

```javascript
const rgtp = require('rgtp');

// Create client
const client = new rgtp.Client({
  timeout: 30000
});

// Listen for events
client.on('pullStart', (host, port, outputPath) => {
  console.log(`Downloading from ${host}:${port} to ${outputPath}`);
});

client.on('progress', (transferred, total) => {
  const percent = ((transferred / total) * 100).toFixed(1);
  console.log(`Download progress: ${percent}%`);
});

client.on('pullComplete', (outputPath) => {
  console.log(`Download complete: ${outputPath}`);
});

// Pull file
await client.pullToFile('192.168.1.100', 9999, 'downloaded-file.bin');
console.log('File downloaded successfully!');

// Get statistics
const stats = await client.getStats();
console.log(`Received: ${rgtp.formatBytes(stats.bytesTransferred)}`);

// Cleanup
client.close();
```

## Convenience Functions

For simple use cases:

```javascript
const rgtp = require('rgtp');

// Send a file (one-liner)
const sendStats = await rgtp.sendFile('my-file.bin', { port: 9999 });

// Receive a file (one-liner)
const receiveStats = await rgtp.receiveFile('host', 9999, 'output.bin');
```

## Configuration Presets

```javascript
const rgtp = require('rgtp');

// High-bandwidth LAN
const lanSession = new rgtp.Session(rgtp.createLANConfig());

// Variable bandwidth WAN
const wanClient = new rgtp.Client(rgtp.createWANConfig());

// Mobile/limited bandwidth
const mobileSession = new rgtp.Session(rgtp.createMobileConfig());
```

## API Reference

### RGTPSession

**Constructor:**
```javascript
new rgtp.Session(options)
```

**Options:**
- `port` - Port to listen on (0 = auto)
- `chunkSize` - Chunk size in bytes (default: 1MB)
- `adaptiveMode` - Enable adaptive rate control (default: true)
- `timeout` - Operation timeout in ms (default: 30000)

**Methods:**
- `exposeFile(filePath)` - Expose a file for transfer
- `waitComplete()` - Wait for all transfers to complete
- `getStats()` - Get transfer statistics
- `close()` - Close the session

**Events:**
- `exposeStart` - File exposure started
- `progress` - Transfer progress update
- `error` - Error occurred
- `close` - Session closed

### RGTPClient

**Constructor:**
```javascript
new rgtp.Client(options)
```

**Options:**
- `timeout` - Connection timeout in ms (default: 30000)
- `chunkSize` - Preferred chunk size (default: 1MB)
- `adaptiveMode` - Enable adaptive mode (default: true)

**Methods:**
- `pullToFile(host, port, outputPath)` - Pull file from server
- `getStats()` - Get transfer statistics
- `close()` - Close the client

**Events:**
- `pullStart` - File download started
- `progress` - Download progress update
- `pullComplete` - Download completed
- `error` - Error occurred
- `close` - Client closed

## Examples

Run the included examples:

```bash
# Simple transfer demo
npm run example

# Server demo
npm run examples:server

# Client demo
npm run examples:client
```

## Performance Tips

1. **Chunk Size**: Adjust based on network conditions
2. **Adaptive Mode**: Keep enabled for optimal performance
3. **Event Handling**: Use progress events for real-time feedback
4. **Error Handling**: Always wrap operations in try-catch blocks

## License

MIT - See LICENSE file for details.

## Contributing

Contributions welcome! Please read CONTRIBUTING.md for guidelines.

## Support

For issues and questions, please open an issue on GitHub.