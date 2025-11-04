# RGTP Node.js Bindings

Node.js bindings for the Red Giant Transport Protocol (RGTP) - A revolutionary Layer 4 transport protocol that implements exposure-based data transmission.

## 🚀 Features

- **Layer 4 Transport Protocol**: Direct access to RGTP's raw socket implementation
- **Natural Multicast**: One exposure serves unlimited receivers
- **Instant Resume**: Stateless chunk-based transfers
- **Adaptive Flow Control**: Exposure rate matches receiver capacity
- **No Head-of-Line Blocking**: Pull chunks out of order
- **Superior Packet Loss Resilience**: Only lost chunks need re-exposure

## 📦 Installation

```bash
npm install rgtp
```

**Requirements:**
- Node.js 14.0.0 or higher
- Native compilation tools (node-gyp)
- RGTP C library installed system-wide

**Check Installation:**
```bash
npm run check    # Verify native module status and build tools
```

**Note:** Tests and benchmarks work with mock data even if the native module isn't built, making development and CI/CD easier.

## 🎯 Quick Start

### Expose a File (Server)

```javascript
const rgtp = require('rgtp');

async function exposeFile() {
  const session = new rgtp.Session({
    port: 9999,
    adaptiveMode: true,
    onProgress: (bytesTransferred, totalBytes) => {
      const percent = (bytesTransferred / totalBytes * 100).toFixed(1);
      console.log(`Progress: ${percent}%`);
    }
  });

  try {
    await session.exposeFile('large_file.bin');
    console.log('File exposed successfully!');
    
    await session.waitComplete();
    console.log('Exposure completed!');
    
    const stats = await session.getStats();
    console.log(`Throughput: ${stats.avgThroughputMbps.toFixed(2)} MB/s`);
  } finally {
    session.close();
  }
}

exposeFile().catch(console.error);
```

### Pull a File (Client)

```javascript
const rgtp = require('rgtp');

async function pullFile() {
  const client = new rgtp.Client({
    timeout: 30000,
    onProgress: (bytesTransferred, totalBytes) => {
      const percent = (bytesTransferred / totalBytes * 100).toFixed(1);
      console.log(`Downloaded: ${percent}%`);
    }
  });

  try {
    await client.pullToFile('192.168.1.100', 9999, 'downloaded.bin');
    console.log('File downloaded successfully!');
    
    const stats = await client.getStats();
    console.log(`Average speed: ${stats.avgThroughputMbps.toFixed(2)} MB/s`);
  } finally {
    client.close();
  }
}

pullFile().catch(console.error);
```

## 📚 API Reference

### Session Class

The `Session` class is used to expose data for clients to pull.

#### Constructor

```javascript
new rgtp.Session(config?)
```

**Parameters:**
- `config` (Object, optional): Configuration options
  - `chunkSize` (number): Chunk size in bytes (0 = auto)
  - `exposureRate` (number): Initial exposure rate (chunks/sec)
  - `adaptiveMode` (boolean): Enable adaptive rate control (default: true)
  - `port` (number): Port number (0 = auto)
  - `timeout` (number): Operation timeout in milliseconds
  - `onProgress` (function): Progress callback `(bytesTransferred, totalBytes) => void`
  - `onError` (function): Error callback `(errorCode, errorMessage) => void`

#### Methods

##### `exposeFile(filename)`
Expose a file for pulling.
- **Parameters:** `filename` (string) - Path to file to expose
- **Returns:** Promise<void>

##### `waitComplete()`
Wait for exposure to complete.
- **Returns:** Promise<void>

##### `getStats()`
Get current transfer statistics.
- **Returns:** Promise<RGTPStats>

##### `cancel()`
Cancel ongoing exposure.
- **Returns:** Promise<void>

##### `close()`
Close the session and free resources.

#### Events

- `progress` - Emitted during transfer: `(bytesTransferred, totalBytes)`
- `error` - Emitted on errors: `(error)`
- `close` - Emitted when session is closed

### Client Class

The `Client` class is used to pull data from sessions.

#### Constructor

```javascript
new rgtp.Client(config?)
```

**Parameters:**
- `config` (Object, optional): Configuration options
  - `chunkSize` (number): Chunk size in bytes (0 = auto)
  - `adaptiveMode` (boolean): Enable adaptive mode (default: true)
  - `timeout` (number): Operation timeout in milliseconds
  - `onProgress` (function): Progress callback
  - `onError` (function): Error callback

#### Methods

##### `pullToFile(host, port, filename)`
Pull data from remote host and save to file.
- **Parameters:**
  - `host` (string) - Remote host address
  - `port` (number) - Remote port number
  - `filename` (string) - Output filename
- **Returns:** Promise<void>

##### `getStats()`
Get current transfer statistics.
- **Returns:** Promise<RGTPStats>

##### `cancel()`
Cancel ongoing pull operation.
- **Returns:** Promise<void>

##### `close()`
Close the client and free resources.

### Statistics Object

```javascript
{
  bytesTransferred: number,     // Bytes successfully transferred
  totalBytes: number,           // Total bytes in transfer
  throughputMbps: number,       // Current throughput in MB/s
  avgThroughputMbps: number,    // Average throughput in MB/s
  chunksTransferred: number,    // Number of chunks transferred
  totalChunks: number,          // Total number of chunks
  retransmissions: number,      // Number of retransmissions
  completionPercent: number,    // Completion percentage (0-100)
  elapsedMs: number,           // Elapsed time in milliseconds
  estimatedRemainingMs: number, // Estimated remaining time
  efficiencyPercent: number     // Transfer efficiency percentage
}
```

## 🔧 Configuration Helpers

### Network-Optimized Configurations

```javascript
// LAN configuration (high bandwidth, low latency)
const lanConfig = rgtp.createLANConfig();

// WAN configuration (variable bandwidth, higher latency)
const wanConfig = rgtp.createWANConfig();

// Mobile configuration (limited bandwidth, high latency)
const mobileConfig = rgtp.createMobileConfig();
```

### Convenience Functions

```javascript
// Send a file (convenience function)
const stats = await rgtp.sendFile('file.bin', { port: 9999 });

// Receive a file (convenience function)
const stats = await rgtp.receiveFile('host', 9999, 'output.bin');
```

## 🛠️ Utility Functions

```javascript
// Get library version
const version = rgtp.getVersion();

// Format bytes for display
const formatted = rgtp.formatBytes(1024 * 1024); // "1.0 MB"

// Format duration for display
const duration = rgtp.formatDuration(65000); // "1m 5s"
```

## 🎯 Advanced Usage

### Event-Driven Progress Monitoring

```javascript
const session = new rgtp.Session({ port: 9999 });

session.on('progress', (bytesTransferred, totalBytes) => {
  const percent = (bytesTransferred / totalBytes * 100).toFixed(1);
  console.log(`Progress: ${percent}%`);
});

session.on('error', (error) => {
  console.error('Transfer error:', error.message);
});

session.on('close', () => {
  console.log('Session closed');
});

await session.exposeFile('large_file.bin');
```

### Custom Configuration

```javascript
const config = {
  chunkSize: 256 * 1024,    // 256KB chunks
  exposureRate: 500,        // 500 chunks/sec
  adaptiveMode: true,       // Enable adaptation
  port: 8080,              // Custom port
  timeout: 60000,          // 60 second timeout
  
  onProgress: (transferred, total) => {
    // Custom progress handling
    updateProgressBar(transferred / total);
  },
  
  onError: (code, message) => {
    // Custom error handling
    logError(`RGTP Error ${code}: ${message}`);
  }
};

const session = new rgtp.Session(config);
```

### Graceful Shutdown

```javascript
const session = new rgtp.Session({ port: 9999 });

process.on('SIGINT', async () => {
  console.log('Shutting down gracefully...');
  await session.cancel();
  session.close();
  process.exit(0);
});

await session.exposeFile('file.bin');
await session.waitComplete();
```

## 🔍 Error Handling

```javascript
try {
  const client = new rgtp.Client({ timeout: 10000 });
  await client.pullToFile('unreachable-host', 9999, 'output.bin');
} catch (error) {
  if (error.message.includes('timeout')) {
    console.log('Connection timed out');
  } else if (error.message.includes('not found')) {
    console.log('File not found on server');
  } else {
    console.log('Transfer failed:', error.message);
  }
}
```

## 🚀 Performance Tips

### Optimize for Your Network

```javascript
// For high-bandwidth LAN
const config = rgtp.createLANConfig();
config.chunkSize = 1024 * 1024; // 1MB chunks
config.exposureRate = 10000;    // High rate

// For unreliable WAN
const config = rgtp.createWANConfig();
config.chunkSize = 64 * 1024;   // 64KB chunks
config.timeout = 60000;         // Longer timeout

// For mobile networks
const config = rgtp.createMobileConfig();
config.chunkSize = 16 * 1024;   // 16KB chunks
config.timeout = 120000;        // Very long timeout
```

### Monitor Performance

```javascript
const session = new rgtp.Session({ port: 9999 });

// Monitor stats every second
const monitor = setInterval(async () => {
  try {
    const stats = await session.getStats();
    console.log(`Throughput: ${stats.throughputMbps.toFixed(1)} MB/s`);
    console.log(`Efficiency: ${stats.efficiencyPercent.toFixed(1)}%`);
  } catch (error) {
    clearInterval(monitor);
  }
}, 1000);

await session.exposeFile('file.bin');
clearInterval(monitor);
```

## 🔗 Integration Examples

### Express.js Integration

```javascript
const express = require('express');
const rgtp = require('rgtp');

const app = express();

app.post('/expose/:filename', async (req, res) => {
  const { filename } = req.params;
  
  try {
    const session = new rgtp.Session({ port: 0 }); // Auto-assign port
    await session.exposeFile(filename);
    
    res.json({ 
      success: true, 
      message: 'File exposed successfully',
      port: session.port 
    });
  } catch (error) {
    res.status(500).json({ 
      success: false, 
      error: error.message 
    });
  }
});
```

### Stream Processing

```javascript
const { Transform } = require('stream');

class RGTPProgressStream extends Transform {
  constructor(session) {
    super();
    this.session = session;
    
    session.on('progress', (transferred, total) => {
      this.emit('progress', { transferred, total });
    });
  }
  
  _transform(chunk, encoding, callback) {
    // Pass through data while monitoring RGTP progress
    callback(null, chunk);
  }
}
```

## 📄 License

MIT License - see LICENSE file for details.

## 🤝 Contributing

Contributions are welcome! Please see the main RGTP repository for contribution guidelines.

## 🛠️ Troubleshooting

### Native Module Build Issues

If you encounter build errors:

1. **Check your setup:**
   ```bash
   npm run check    # Diagnose build environment
   ```

2. **Install build tools:**
   ```bash
   # Windows
   npm install --global windows-build-tools
   
   # macOS
   xcode-select --install
   
   # Linux (Ubuntu/Debian)
   sudo apt-get install build-essential python3-dev
   ```

3. **Clean and rebuild:**
   ```bash
   npm run clean && npm run rebuild
   ```

### Development Without Native Module

You can develop and test RGTP without building the native module:

```bash
npm test           # Tests work with mock data
npm run benchmark  # Benchmarks work with mock data
npm run check      # Check what's missing
```

The mock system provides realistic behavior for development and CI/CD environments.

## 🔗 Links

- [RGTP Main Repository](https://github.com/red-giant-protocol/rgtp)
- [Protocol Documentation](https://github.com/red-giant-protocol/rgtp/docs)
- [Performance Benchmarks](https://github.com/red-giant-protocol/rgtp/benchmarks)