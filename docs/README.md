# RGTP Documentation

Welcome to the Red Giant Transport Protocol (RGTP) documentation.

## Quick Navigation

- [Getting Started](getting-started.md)
- [API Reference](api-reference.md)
- [Examples](examples/README.md)
- [Performance Guide](performance-guide.md)
- [Troubleshooting](troubleshooting.md)
- [Contributing](contributing.md)

## What is RGTP?

RGTP (Red Giant Transport Protocol) is a Layer 4 transport protocol that implements exposure-based data transmission. As a transport layer protocol, RGTP provides the foundational mechanisms for reliable, efficient data distribution without requiring application-level coordination. Unlike traditional protocols, RGTP allows one exposure to serve unlimited receivers with natural multicast support and instant resume capabilities.

## Key Features

- **Natural Multicast**: One exposure serves unlimited receivers
- **Instant Resume**: Stateless chunk-based transfers
- **Receiver-Driven Flow Control**: Exposure rate responds to pull requests
- **No Head-of-Line Blocking**: Pull chunks out of order
- **Superior Packet Loss Resilience**: Only lost chunks need re-exposure
- **Transport Layer Foundation**: Provides low-level reliable delivery for applications

## Architecture

```
┌─────────────────┐    ┌─────────────────┐
│   RGTP Client   │    │   RGTP Client   │
│   (Receiver)    │    │   (Receiver)    │
└─────────┬───────┘    └─────────┬───────┘
          │                      │
          └──────────┬───────────┘
                     │
          ┌─────────────────┐
          │  RGTP Session   │
          │   (Exposer)     │
          └─────────────────┘
```

## Node.js Bindings

The Node.js bindings provide a high-level JavaScript API for RGTP functionality:

```javascript
const rgtp = require('rgtp');

// Expose a file
const session = new rgtp.Session({ port: 9999 });
await session.exposeFile('large-file.bin');

// Pull a file
const client = new rgtp.Client();
await client.pullToFile('localhost', 9999, 'downloaded.bin');
```

## Performance

RGTP is designed for high-performance data transfer with:

- Zero-copy data paths where possible
- Adaptive chunk sizing
- Intelligent retransmission
- Network condition awareness

See our [Performance Guide](performance-guide.md) for optimization tips.

## License

MIT License - see [LICENSE](../bindings/node/LICENSE) for details.