# Security Policy

## Supported Versions

| Version | Supported |
|---------|-----------|
| 1.0.x   |  Active security support |
| < 1.0   |  Pre-release; no security support |

## Security Model

RGTP's security guarantees are:

**Confidentiality** — Each chunk is encrypted with AEAD (ChaCha20-Poly1305-IETF by default, AES-256-GCM optional) using a per-Exposure CSPRNG-generated 256-bit key. The nonce is derived deterministically from the chunk index, ensuring uniqueness without per-chunk random state.

**Integrity and authenticity** — The AEAD authentication tag is verified before any plaintext is written to the caller's buffer. A Merkle tree (BLAKE2b-256 or SHA-256) over plaintext chunk hashes provides optional per-chunk integrity verification against the tree root.

**Replay protection** — Each puller session maintains a 256-bit sliding anti-replay window. Duplicate or out-of-window sequence numbers are silently discarded.

**Key lifecycle** — Keys are generated at `rgtp_expose()` time and stored only in the opaque `rgtp_surface_t`. They are never transmitted over the wire. On `rgtp_destroy_surface()`, all key material is zeroized using `sodium_memzero` or `OPENSSL_cleanse` before `free`.

**DoS mitigation** — Pull requests from a single source IP are rate-limited to 1,000 requests per second per Exposure. The exposer's per-surface data structures are bounded in size and do not grow with the number of active pullers.

**Memory safety** — The library is compiled with AddressSanitizer, UndefinedBehaviorSanitizer, and ThreadSanitizer in CI. Zero sanitizer errors are required across the full unit and integration test suite.

## Known Limitations

- **Key distribution is out-of-band.** RGTP does not provide a key exchange mechanism. The application is responsible for distributing the 256-bit AEAD key to authorized pullers (e.g., via TLS, a pre-shared key, or a dedicated key exchange protocol).
- **No forward secrecy.** A single key is used for all chunks of an Exposure. Compromise of the key exposes all chunks of that Exposure.
- **Raw Ethernet mode bypasses IP-layer firewalls.** When `RGTP_ENABLE_RAW_ETHERNET` is used, RGTP frames are not subject to IP-layer firewall rules. Ensure appropriate L2 access controls are in place.
- **TURN relay traffic is unencrypted at the relay.** When NAT traversal falls back to a TURN relay, the relay sees encrypted RGTP packets but can observe traffic patterns. Use a trusted TURN server.

## Reporting a Vulnerability

Please **do not** open a public GitHub issue for security vulnerabilities.

Report vulnerabilities by emailing **[jasemwaura@gmail.com](mailto:jasemwaura@gmail.com)** with:

1. A description of the vulnerability and its potential impact.
2. Steps to reproduce or a proof-of-concept (if available).
3. The affected version(s) and platform(s).
4. Your preferred contact method for follow-up.

**Response timeline:**
- Acknowledgement within **48 hours**.
- Initial assessment within **7 days**.
- Fix or mitigation plan within **30 days** for critical/high severity issues.
- Public disclosure coordinated with the reporter after a fix is available.

We follow responsible disclosure. Reporters who identify valid vulnerabilities will be credited in the release notes unless they prefer to remain anonymous.
