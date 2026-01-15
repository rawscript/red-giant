# Getting Started with RGTP

This guide will help you get up and running with RGTP Node.js bindings quickly.

## Installation

### Quick Install

```bash
npm install rgtp
```

### Prerequisites

- Node.js 14.0.0 or higher
- npm 6.0.0 or higher
- Build tools (automatically handled)

The package includes pre-built binaries for all major platforms.

## Basic Usage

### Exposing a File (Server Side)

```javascript
const rgtp = require('rgtp');

async function exposeFile() {
  const session = new rgtp.Session({
    port: 9999,
    adaptiveMode: true
  });

  // Event listeners
  session.on('exposeStart', (filePath, fileSize) => {
    console.log(`Exposing ${filePath} (${fileSize} bytes)`);
  });

  session.on('progress', (transferred, total) => {
    const percent = ((transferred / total) * 100).toFixed(1);
    console.log(`Progress: ${percent}%`);
  });

  session.on('error', (error) => {
    console.error('Session error:', error.message);
  });

  try {
    await session.exposeFile('my-large-file.bin');
    console.log('File exposed successfully!');
    
    // Wait for transfers to complete
    await session.waitComplete();
    
    // Get final statistics
    const stats = await session.getStats();
    console.log(`Transferred: ${rgtp.formatBytes(stats.bytesTransferred)}`);
    console.log(`Throughput: ${stats.avgThroughputMbps.toFixed(2)} MB/s`);
  } finally {
    session.close();
  }
}

exposeFile().catch(console.error);
```

### Pulling a File (Client Side)

```javascript
const rgtp = require('rgtp');

async function pullFile() {
  const client = new rgtp.Client({
    timeout: 30000
  });

  try {
    await client.pullToFile('192.168.1.100', 9999, 'downloaded-file.bin');
    console.log('File downloaded successfully!');
    
    const stats = await client.getStats();
    console.log(`Downloaded: ${rgtp.formatBytes(stats.bytesTransferred)}`);
    console.log(`Speed: ${stats.avgThroughputMbps.toFixed(2)} MB/s`);
  } finally {
    client.close();
  }
}

pullFile().catch(console.error);
```

## Configuration Options

### Session Configuration

```javascript
const session = new rgtp.Session({
  port: 9999,              // Port to listen on (0 = auto)
  chunkSize: 256 * 1024,   // Chunk size in bytes (0 = auto)
  exposureRate: 1000,      // Initial exposure rate (chunks/sec)
  adaptiveMode: true,      // Enable adaptive rate control
  timeout: 30000,          // Operation timeout (ms)
  
  // Event callbacks
  onProgress: (transferred, total) => {
    console.log(`Progress: ${(transferred/total*100).toFixed(1)}%`);
  },
  
  onError: (code, message) => {
    console.error(`Error ${code}: ${message}`);
  }
});
```

### Client Configuration

```javascript
const client = new rgtp.Client({
  chunkSize: 256 * 1024,   // Preferred chunk size
  adaptiveMode: true,      // Enable adaptive mode
  timeout: 30000,          // Connection timeout (ms)
  
  // Event callbacks
  onProgress: (transferred, total) => {
    const percent = (transferred / total * 100).toFixed(1);
    console.log(`Downloaded: ${percent}%`);
  }
});
```

## Network-Optimized Configurations

RGTP provides pre-configured settings for different network conditions:

```javascript
// High-bandwidth LAN
const lanConfig = rgtp.createLANConfig();
const session = new rgtp.Session(lanConfig);

// Variable bandwidth WAN
const wanConfig = rgtp.createWANConfig();
const client = new rgtp.Client(wanConfig);

// Mobile/limited bandwidth
const mobileConfig = rgtp.createMobileConfig();
const session = new rgtp.Session(mobileConfig);
```

## Event-Driven Programming

RGTP classes extend EventEmitter for reactive programming:

```javascript
const session = new rgtp.Session({ port: 9999 });

session.on('progress', (transferred, total) => {
  updateProgressBar(transferred / total);
});

session.on('error', (error) => {
  console.error('Transfer error:', error.message);
  // Handle error (retry, notify user, etc.)
});

session.on('close', () => {
  console.log('Session closed');
});

await session.exposeFile('large-file.bin');
```

## Error Handling

Always wrap RGTP operations in try-catch blocks:

```javascript
try {
  const client = new rgtp.Client({ timeout: 10000 });
  await client.pullToFile('remote-host', 9999, 'output.bin');
} catch (error) {
  if (error.message.includes('timeout')) {
    console.log('Connection timed out - server may be unreachable');
  } else if (error.message.includes('not found')) {
    console.log('File not found on server');
  } else {
    console.log('Transfer failed:', error.message);
  }
}
```

## Convenience Functions

For simple use cases, use the convenience functions:

```javascript
// Send a file (creates session, exposes, waits for completion)
const sendStats = await rgtp.sendFile('my-file.bin', { port: 9999 });

// Receive a file (creates client, pulls file)
const receiveStats = await rgtp.receiveFile('host', 9999, 'output.bin');
```

## Next Steps

- Check out the [Examples](examples/README.md) for more complex use cases
- Read the [API Reference](api-reference.md) for detailed documentation
- See the [Performance Guide](performance-guide.md) for optimization tips
- Run the included examples:
  ```bash
  npm run example              # Simple transfer
  npm run examples:server      # Interactive server
  npm run examples:batch       # Batch downloader
  npm run examples:monitor     # Performance monitor
  ```

## Troubleshooting

If you encounter issues:

1. **Native module build fails**: Ensure you have the required build tools installed
2. **Connection timeouts**: Check firewall settings and network connectivity
3. **Poor performance**: Try different chunk sizes or enable adaptive mode
4. **Memory issues**: Monitor memory usage with large files

See [Troubleshooting](troubleshooting.md) for detailed solutions.