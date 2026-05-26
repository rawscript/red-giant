# RGTP Protocol Specification

**Version:** 1.0  
**Status:** Draft  
**Date:** May 26, 2026

---

## 1. Introduction

The Red Giant Transport Protocol (RGTP) is a stateless, receiver-driven, chunk-based, pre-encrypted, Merkle-verified, FEC-protected data transport operating over UDP and raw Ethernet. It is designed for high-bandwidth file distribution and deterministic low-latency autonomous vehicle (AV) Ethernet environments.

### 1.1 Design Principles

- **Stateless exposer** — the exposer holds only an immutable chunk store. All per-puller state lives in the puller.
- **Receiver-driven** — pullers control the data rate by issuing pull requests. The exposer never pushes unsolicited data.
- **Pre-encryption** — data is encrypted once at expose time. Subsequent pull requests are served from the encrypted store without re-encryption.
- **Merkle integrity** — a Merkle tree over plaintext chunk hashes enables per-chunk integrity verification after decryption.
- **Optional FEC** — Reed-Solomon FEC provides loss recovery without retransmission.
- **ABI stability** — all public structs are opaque handles; callers never embed or `sizeof` internal types.

### 1.2 Terminology

| Term | Definition |
|------|-----------|
| Exposure | An immutable, pre-encrypted, Merkle-verified dataset identified by a 128-bit Exposure_ID |
| Exposer | The stateless server-side component that pre-encrypts data and serves chunks |
| Puller | The receiver-side component that drives flow control by issuing pull requests |
| Chunk | The atomic unit of data transfer, identified by a 32-bit chunk index |
| Exposure_ID | A 128-bit cryptographically random identifier (CSPRNG-generated) |
| Merkle_Proof | A set of sibling hashes sufficient to verify a single chunk against the Merkle root |
| FEC | Forward Error Correction — optional Reed-Solomon coding over GF(2^8) |
| AEAD | Authenticated Encryption with Associated Data (ChaCha20-Poly1305-IETF or AES-256-GCM) |

---

## 2. Packet Format

### 2.1 Common Header (4 bytes)

All RGTP packets begin with a 4-byte common header. All multi-byte integers are **big-endian** (network byte order).

```
 0                   1                   2                   3
 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|  Version (8)  |  Type (8)     |         Length (16)           |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
```

- **Version** — protocol version, currently `0x01`.
- **Type** — packet type byte (see §2.2).
- **Length** — total packet length in bytes, including this header.

### 2.2 Packet Types

| Type | Value | Direction | Description |
|------|-------|-----------|-------------|
| Pull_Request | 0x01 | Puller → Exposer | Request chunks from an Exposure |
| Manifest | 0x02 | Exposer → Puller | Describe an Exposure |
| Chunk_Data | 0x03 | Exposer → Puller | Encrypted chunk payload |
| Chunk_Data_With_Proof | 0x04 | Exposer → Puller | Chunk with Merkle proof |
| FEC_Parity | 0x05 | Exposer → Puller | Reed-Solomon parity symbol |
| NAK | 0x06 | Puller → Exposer | Request re-exposure of missing chunks |
| Rate_Report | 0x07 | Puller → Exposer | Report RTT and packet loss rate |
| Keepalive | 0x08 | Puller → Exposer | Maintain NAT binding |

### 2.3 Pull_Request (0x01) — 36 bytes

```
Common Header (4) | Exposure_ID (16) | Window_Size (4) | Loss_Rate_Q16 (4) |
Flags (4) | Version_Min (2) | Version_Max (2)
```

- **Window_Size** — number of chunks the puller can receive before its buffer is full.
- **Loss_Rate_Q16** — measured packet loss rate as a Q16 fixed-point fraction (0x0000 = 0%, 0xFFFF ≈ 100%).
- **Flags** — bit 0: request Merkle proof; bit 1: FEC enabled; bits 2–31: reserved.
- **Version_Min / Version_Max** — protocol version range supported by this puller.

### 2.4 Manifest (0x02) — 72 bytes

```
Common Header (4) | Exposure_ID (16) | Total_Size (8) | Chunk_Count (4) |
Chunk_Size (4) | Merkle_Root (32) | FEC_N (1) | FEC_K (1) | Flags (2)
```

- **Total_Size** — total plaintext size in bytes.
- **Chunk_Count** — number of data chunks.
- **Chunk_Size** — size of each data chunk in bytes (last chunk may be smaller).
- **Merkle_Root** — 32-byte BLAKE2b-256 or SHA-256 root hash over all plaintext chunks.
- **FEC_N / FEC_K** — Reed-Solomon parameters; both 0 if FEC is disabled.

### 2.5 Chunk_Data (0x03) — variable

```
Common Header (4) | Exposure_ID (16) | Chunk_Index (4) | Payload (variable)
```

- **Payload** — AEAD-encrypted chunk data. Maximum 1450 bytes (UDP) / 1430 bytes (raw Ethernet).

### 2.6 Chunk_Data_With_Proof (0x04) — variable

```
Common Header (4) | Exposure_ID (16) | Chunk_Index (4) | Proof_Depth (1) |
Proof (Proof_Depth × 32 bytes) | Payload (variable)
```

- **Proof_Depth** — number of sibling hashes in the Merkle proof (= `ceil(log2(chunk_count))`).
- **Proof** — sibling hashes from leaf to root, each 32 bytes.

### 2.7 FEC_Parity (0x05) — variable

```
Common Header (4) | Exposure_ID (16) | Block_Index (4) | Parity_Index (1) |
FEC_N (1) | FEC_K (1) | Reserved (1) | Payload (variable)
```

- **Block_Index** — index of the RS block within the Exposure.
- **Parity_Index** — index of this parity symbol within the block (0 to `FEC_N - FEC_K - 1`).

### 2.8 NAK (0x06) — variable

```
Common Header (4) | Exposure_ID (16) | NAK_Count (2) | Reserved (2) |
Chunk_Indices (NAK_Count × 4 bytes)
```

- **NAK_Count** — number of chunk indices being negatively acknowledged.
- **Chunk_Indices** — array of 32-bit big-endian chunk indices.

### 2.9 Rate_Report (0x07) — 44 bytes

```
Common Header (4) | Exposure_ID (16) | RTT_Us (4) | Loss_Rate_Q16 (4) |
Window_Size (4) | Reserved (4) | Timestamp_Us (8)
```

- **RTT_Us** — EWMA RTT estimate in microseconds.
- **Loss_Rate_Q16** — EWMA packet loss rate as Q16 fixed-point.
- **Timestamp_Us** — sender timestamp in microseconds since epoch.

### 2.10 Keepalive (0x08) — 32 bytes

```
Common Header (4) | Exposure_ID (16) | Timestamp_Us (8) | Reserved (4)
```

Sent by the puller every 25 seconds to maintain NAT binding state.

---

## 3. Protocol State Machines

### 3.1 Exposer

```
IDLE ──[rgtp_expose()]──► ACTIVE
ACTIVE ──[rgtp_poll()]──► ACTIVE   (serves pull requests indefinitely)
ACTIVE ──[rgtp_destroy_surface()]──► DESTROYED
```

On entering ACTIVE: all chunks are pre-encrypted and the Merkle tree is built. No cryptographic work occurs during `rgtp_poll`.

### 3.2 Puller

```
IDLE ──[rgtp_pull_start()]──► sends Pull_Request ──► waits for Manifest
     ──[receives Manifest]──► ACTIVE
ACTIVE ──[rgtp_pull_next()]──► issues pull requests, receives chunks
       ──[all chunks received]──► COMPLETE
ACTIVE ──[rgtp_destroy_surface()]──► DESTROYED
```

### 3.3 Version Negotiation

The puller sends `Version_Min=1, Version_Max=N` in the Pull_Request. The exposer responds with a Manifest using the highest version in `[Version_Min, Version_Max]` that it supports. If no overlap exists, the exposer sends no response and the puller times out with `RGTP_ERR_NOT_SUPPORTED`.

### 3.4 Sliding-Window Pull

The puller maintains a sliding window of `window_size` unreceived chunks. When the window is not full, the puller issues pull requests for the next unreceived chunks. When the window is full, the puller waits until at least one chunk is consumed before issuing new requests.

### 3.5 NAK and Retransmission

When a chunk has not arrived within 2× the expected inter-chunk interval, the puller issues a NAK for that chunk. The exposer re-serves the chunk from its immutable store on receipt of the NAK.

### 3.6 Keepalive

The puller sends a Keepalive packet every 25 seconds to maintain NAT binding state. The exposer does not respond to Keepalive packets.

---

## 4. Cryptographic Design

### 4.1 AEAD Algorithm

| Algorithm | Nonce | Tag | Selection |
|-----------|-------|-----|-----------|
| ChaCha20-Poly1305-IETF | 12 bytes | 16 bytes | Default (`RGTP_CRYPTO_BACKEND=libsodium`) |
| AES-256-GCM | 12 bytes | 16 bytes | `RGTP_CRYPTO_BACKEND=openssl` with AES-NI |

### 4.2 Nonce Construction

Each chunk is encrypted with a unique nonce derived from its chunk index. This guarantees nonce uniqueness across all chunks of a single Exposure without requiring a per-chunk random nonce.

```
nonce[12] = { chunk_index_LE[0..3], 0x00 × 8 }
```

This is safe because each Exposure uses a fresh CSPRNG-generated 256-bit key, and chunk indices are unique within an Exposure.

### 4.3 Exposure ID Generation

Exposure IDs are 128-bit values generated using the crypto backend's CSPRNG:

- libsodium: `randombytes_buf(id, 16)`
- OpenSSL: `RAND_bytes(id, 16)`

The previous LCG-based generator (`seed * 1103515245 + 12345`) has been replaced.

### 4.4 Merkle Tree

The Merkle tree is built over **plaintext** chunk hashes (before encryption), enabling the puller to verify integrity after decryption.

**Hash function**: BLAKE2b-256 (libsodium) or SHA-256 (OpenSSL).

**Domain separation** (prevents second-preimage attacks):
- Leaf hash: `H(0x00 || chunk_plaintext)`
- Internal node hash: `H(0x01 || left_child_hash || right_child_hash)`

**Tree construction**:
1. Compute leaf hashes for all `chunk_count` chunks.
2. Pad to the next power of 2 with zero-hash leaves.
3. Build bottom-up; store root in `surface->merkle_root[32]`.

**Proof verification** (puller side):
```
verify(chunk_data, proof, chunk_index, root):
  h = H(0x00 || chunk_data)
  for each sibling in proof:
    if chunk_index is left child:
      h = H(0x01 || h || sibling)
    else:
      h = H(0x01 || sibling || h)
    chunk_index >>= 1
  return h == root
```

### 4.5 Key Distribution

Keys are distributed out-of-band (not over RGTP). The application is responsible for key exchange (e.g., via TLS, pre-shared key, or a dedicated key exchange protocol). The RGTP library never transmits key material over the wire.

### 4.6 Key Zeroization

All key material (`key[32]` and any derived keys) is zeroized using `sodium_memzero` or `OPENSSL_cleanse` in `rgtp_destroy_surface` before `free`. This prevents key material from persisting in freed heap memory.

### 4.7 Anti-Replay Window

The puller maintains a 256-bit sliding bitmap per session:

```c
typedef struct {
    uint64_t bitmap[4];   /* 256 bits */
    uint32_t base;        /* sequence number of bit 0 */
} rgtp_replay_window_t;
```

On receiving a chunk with sequence number `seq`:
- If `seq < base`: discard (too old).
- If `seq >= base + 256`: advance window, clearing old bits.
- If bit `seq - base` is already set: discard (replay).
- Otherwise: set bit and accept.

---

## 5. FEC Subsystem

### 5.1 GF(2^8) Arithmetic

Reed-Solomon is implemented over GF(2^8) with the primitive polynomial `x^8 + x^4 + x^3 + x^2 + 1` (0x11D). The previous implementation used the incorrect polynomial `0x1d`.

### 5.2 Systematic Reed-Solomon (n, k)

The encoder produces a systematic codeword: the first `k` symbols are the original data, and the last `n-k` symbols are parity. Any `k` of the `n` transmitted symbols are sufficient to reconstruct the original data.

Constraints: `1 ≤ k < n ≤ 255`.

Default: `n=255, k=223` (~14% overhead). Configurable per-Exposure via `rgtp_config_t.fec_k` and `rgtp_config_t.fec_n`.

### 5.3 Adaptive FEC Strength

The puller reports its measured packet loss rate in each Rate_Report packet. The exposer adjusts the parity ratio:

```
if loss_rate > 5%:
    fec_overhead = min(fec_overhead + 10%, 50%)
elif loss_rate < 1%:
    fec_overhead = max(fec_overhead - 5%, 0%)
```

### 5.4 SIMD Acceleration

GF(2^8) multiplication is accelerated using 4-bit split lookup tables (VPSHUFB / VTBL):

| Platform | Instruction Set |
|----------|----------------|
| x86-64 | AVX2 (preferred), SSE4.2 |
| ARM | NEON |
| Fallback | Scalar log/exp table lookup |

Runtime dispatch via function pointer set at `rgtp_init()`.

---

## 6. Flow and Congestion Control

### 6.1 Receiver-Driven Model

The puller controls the data rate entirely. The exposer sends chunks only in response to pull requests. When the puller's receive buffer is full, it stops issuing requests, causing the exposer to stop sending without any explicit flow-control signaling.

### 6.2 RTT Estimation

The puller timestamps pull requests and matches them to chunk responses. RTT is updated using an EWMA with α = 0.125:

```
rtt_new = 0.875 × rtt_old + 0.125 × rtt_sample
```

### 6.3 AIMD Congestion Control

- **Congestion detected** (RTT > 2× baseline): reduce window by 50% (minimum 1).
- **Congestion cleared** (RTT within 1.1× baseline for 5 consecutive RTT periods): increase window by 1 chunk per RTT.

### 6.4 Pull Pressure

The exposer tracks `pull_pressure` as the count of pull requests received in the last 100ms sliding window. This is used as a secondary signal for rate adaptation.

---

## 7. I/O Backends

### 7.1 UDP (Default)

Standard UDP sockets with `SO_REUSEADDR` and non-blocking mode. IPv4/IPv6 dual-stack.

### 7.2 sendmmsg / recvmmsg (Linux 4.14+)

Batch send and receive with a default batch size of 64 packets. `UDP_SEGMENT` (GSO) is enabled when available. `MSG_ZEROCOPY` is used for page-aligned buffers when the kernel supports it.

### 7.3 io_uring (Linux 5.1+)

Enabled with `RGTP_ENABLE_IOURING=ON`. Submission queue depth ≥ 256. Fixed buffer registration for zero-copy I/O.

### 7.4 IOCP (Windows)

Asynchronous socket I/O via I/O Completion Ports. Replaces the previous blocking/polling model.

### 7.5 Raw Ethernet (AF_PACKET)

Enabled with `RGTP_ENABLE_RAW_ETHERNET=ON`. Uses `AF_PACKET` / `SOCK_DGRAM` bound to a specified interface and EtherType `0x88B5` (local/experimental). Supports:

- Unicast, multicast, and broadcast Ethernet addressing
- IEEE 802.1Q VLAN tag and PCP for TSN traffic class scheduling
- `sendmmsg`-based batch transmission

On Windows, WinPcap or Npcap is required.

---

## 8. MTU and Fragmentation

| Mode | Minimum MTU | Maximum Payload | Default Chunk Size |
|------|-------------|-----------------|-------------------|
| UDP | 576 bytes (RFC 791) | 65,507 bytes | 1,200 bytes |
| Raw Ethernet | — | 1,486 bytes (1,500 − 14-byte header) | 1,400 bytes |

---

## 9. NAT Traversal

- **UDP hole punching** — STUN-based procedure for NAT traversal when both exposer and puller are behind NATs.
- **TURN relay fallback** — activated when direct UDP connectivity fails within 5 seconds.
- **NAT rebinding detection** — STUN binding refresh detects IP/port changes; transfer resumes from the last acknowledged chunk.
- **Keepalive** — puller sends a Keepalive packet every 25 seconds to maintain NAT binding state.

---

## 10. Error Codes

| Code | Value | Meaning |
|------|-------|---------|
| `RGTP_OK` | 0 | Success |
| `RGTP_ERR_NOMEM` | -1 | Memory allocation failed |
| `RGTP_ERR_INVALID_ARG` | -2 | Invalid argument |
| `RGTP_ERR_SOCKET` | -3 | Socket operation failed |
| `RGTP_ERR_CRYPTO_INIT` | -4 | Crypto library initialization failed |
| `RGTP_ERR_ENCRYPT` | -5 | AEAD encryption failed |
| `RGTP_ERR_DECRYPT` | -6 | AEAD decryption failed |
| `RGTP_ERR_AUTH_FAIL` | -7 | Authentication tag mismatch |
| `RGTP_ERR_MERKLE_FAIL` | -8 | Merkle proof verification failed |
| `RGTP_ERR_FEC_FAIL` | -9 | FEC decoding failed (too many erasures) |
| `RGTP_ERR_TRUNCATED` | -10 | Packet truncated (declared length > buffer) |
| `RGTP_ERR_CHUNK_INDEX_OOB` | -11 | Chunk index ≥ manifest chunk count |
| `RGTP_ERR_TIMEOUT` | -12 | Operation timed out |
| `RGTP_ERR_RATE_LIMITED` | -13 | Pull request rate limit exceeded |
| `RGTP_ERR_NOT_SUPPORTED` | -14 | Feature or version not supported |
| `RGTP_ERR_INTERNAL` | -15 | Internal invariant violation |

---

## 11. Correctness Properties

The following properties are verified by the property-based test suite using the [Theft](https://github.com/silentbicycle/theft) framework (minimum 10,000 iterations per property in CI).

| Property | Description |
|----------|-------------|
| P1: AEAD Round-Trip | Encrypt then decrypt with same key and nonce produces original plaintext |
| P2: FEC Round-Trip | Encode, erase up to `n-k` symbols, decode produces original data |
| P3: Wire Protocol Round-Trip | Serialize then parse produces field-for-field equivalent packet |
| P4: Anti-Replay Correctness | Accepted sequence rejected on re-presentation; window base monotonically non-decreasing |
| P5: Flow Control Convergence | Steady-state throughput within 10% of link capacity within 10 RTT periods |
| P6: Merkle Proof Verification | All valid proofs pass; tampered proofs fail |
| P7: Nonce Uniqueness | Nonces for distinct chunk indices are distinct byte-for-byte |
| P8: Adaptive FEC Bounds | Parity ratio stays within [0%, 50%] under all loss rate inputs |
| P9: RTT EWMA Convergence | Estimate always positive; converges within 2 std devs after 50 samples |
| P10: Exposer Memory Invariant | Exposer heap growth is O(1) in puller count |
| P11: Out-of-Order Delivery | No chunk delivered more than N positions out of order; each chunk delivered exactly once |
| P12: Partial Pull Coverage | Exposer serves exactly the minimal covering chunk set for a byte range request |
