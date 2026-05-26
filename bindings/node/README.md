# rgtp — Node.js binding for the Red Giant Transport Protocol

[![npm version](https://badge.fury.io/js/rgtp.svg)](https://www.npmjs.com/package/rgtp)
[![Node.js 18+](https://img.shields.io/badge/node-%3E%3D18-brightgreen)](https://nodejs.org)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](../../LICENSE)

RGTP is a stateless, receiver-driven, chunk-based, pre-encrypted, Merkle-verified,
FEC-protected data transport protocol over UDP and raw Ethernet.

This package provides Node.js 18/20/22 LTS bindings via N-API with full Promise-based
async/await support and a Node.js `Readable` stream interface for pull operations.

## Installation

```bash
npm install rgtp
```

The package requires `librgtp` to be installed on your system. See the
[main repository](https://github.com/rawscript/red-giant) for build instructions.

## Quick Start

### Expose data (server side)

```javascript
const rgtp = require('rgtp');

await rgtp.init();

const sock    = await rgtp.createSocket({ port: 9000 });
const data    = require('fs').readFileSync('large-file.bin');
const surface = await rgtp.expose(sock, data, {
    fecEnabled:   true,
    fecK:         223,
    fecN:         255,
    merkleProofs: true,
});

console.log('Exposure ID:', surface.exposureId().toString('hex'));
// Distribute the exposure ID and key out-of-band to pullers

// Serve pull requests until done
while (true) {
    await surface.poll(1000);
}

surface.close();
sock.close();
```

### Pull data (client side)

```javascript
const rgtp = require('rgtp');
const fs   = require('fs');

await rgtp.init();

const sock       = await rgtp.createSocket();
const exposureId = Buffer.from('<32-hex-chars>', 'hex');
const surface    = await rgtp.pullStart(sock, { host: '192.168.1.10', port: 9000 }, exposureId);

// Option 1: chunk-by-chunk
const chunks = new Map();
while (surface.progress() < 1.0) {
    const { data, chunkIndex } = await rgtp.pullNext(surface);
    chunks.set(chunkIndex, data);
}
const output = Buffer.concat([...chunks.keys()].sort((a,b)=>a-b).map(k => chunks.get(k)));
fs.writeFileSync('output.bin', output);

// Option 2: Readable stream
surface.createReadStream().pipe(fs.createWriteStream('output.bin'));

surface.close();
sock.close();
```

### TypeScript

```typescript
import * as rgtp from 'rgtp';

await rgtp.init();
const sock    = await rgtp.createSocket({ port: 9000 });
const surface = await rgtp.expose(sock, data, { fecEnabled: true });
const id: Buffer = surface.exposureId();
```

## API Reference

### Library lifecycle

```javascript
await rgtp.init()       // Initialise library (call once)
rgtp.cleanup()          // Release global resources
rgtp.version()          // Returns "1.0.0"
```

### Socket

```javascript
const sock = await rgtp.createSocket({ port: 9000 })
sock.boundPort()        // Returns the actual bound port
sock.close()
```

### Exposer

```javascript
const surface = await rgtp.expose(sock, data, options)
await surface.poll(timeoutMs)     // Serve pull requests
surface.exposureId()              // Buffer(16) — Exposure_ID
surface.progress()                // 0.0 for exposer
await surface.getStats()          // { bytesSent, chunksSent, pullPressure, ... }
surface.close()                   // Zeroize key and free
```

**expose options:**

| Option | Type | Default | Description |
|--------|------|---------|-------------|
| `fecEnabled` | boolean | `false` | Enable Reed-Solomon FEC |
| `fecK` | number | `223` | RS data symbols per block |
| `fecN` | number | `255` | RS total symbols per block |
| `merkleProofs` | boolean | `false` | Include per-chunk Merkle proofs |
| `chunkSize` | number | `0` | Chunk size in bytes (0 = auto: 1200) |

### Puller

```javascript
const surface = await rgtp.pullStart(sock, { host, port }, exposureId, options)
const { data, chunkIndex } = await rgtp.pullNext(surface, bufSize)
surface.progress()                // [0.0, 1.0]
surface.createReadStream()        // Node.js Readable stream
surface.close()
```

**pullStart options:**

| Option | Type | Default | Description |
|--------|------|---------|-------------|
| `windowSize` | number | `64` | Pull window size in chunks |
| `timeoutMs` | number | `30000` | Connection timeout |

### Errors

All functions throw `RgtpError` on failure:

```javascript
try {
    await rgtp.expose(sock, data);
} catch (err) {
    if (err instanceof rgtp.RgtpError) {
        console.error(err.code);    // e.g. -1 (RGTP_ERR_NOMEM)
        console.error(err.message); // human-readable
    }
}
```

| Code | Meaning |
|------|---------|
| -1 | `RGTP_ERR_NOMEM` — memory allocation failed |
| -2 | `RGTP_ERR_INVALID_ARG` — invalid argument |
| -3 | `RGTP_ERR_SOCKET` — socket operation failed |
| -4 | `RGTP_ERR_CRYPTO_INIT` — crypto library init failed |
| -5 | `RGTP_ERR_ENCRYPT` — AEAD encryption failed |
| -6 | `RGTP_ERR_DECRYPT` — AEAD decryption failed |
| -7 | `RGTP_ERR_AUTH_FAIL` — authentication tag mismatch |
| -8 | `RGTP_ERR_MERKLE_FAIL` — Merkle proof verification failed |
| -9 | `RGTP_ERR_FEC_FAIL` — FEC decoding failed |
| -12 | `RGTP_ERR_TIMEOUT` — operation timed out |
| -13 | `RGTP_ERR_RATE_LIMITED` — rate limit exceeded |
| -14 | `RGTP_ERR_NOT_SUPPORTED` — feature not supported |

### Utility

```javascript
rgtp.formatBytes(1048576)   // "1.00 MB"
rgtp.createLANConfig()      // { chunkSize: 1400, windowSize: 256, timeoutMs: 10000 }
rgtp.createWANConfig()      // { chunkSize: 1200, windowSize: 64,  timeoutMs: 30000 }
rgtp.createMobileConfig()   // { ..., fecEnabled: true, fecK: 223, fecN: 255 }
```

## Examples

```bash
# Simple loopback transfer (expose + pull in same process)
npm run example

# File server (expose a file, serve pull requests)
npm run examples:server -- large-file.bin --port 9000 --fec

# File client (pull a file from an exposer)
npm run examples:client -- 127.0.0.1:9000 <exposure-id-hex> output.bin

# HTTP bridge (expose files via HTTP interface)
npm run examples:http -- --dir ./files --http-port 8080 --rgtp-port 9000
```

## License

MIT — see [LICENSE](../../LICENSE).
