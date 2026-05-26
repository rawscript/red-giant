# RGTP Protocol Specification

**Version:** 1.0  
**Status:** Draft  
**Date:** May 26, 2026

---

## 1. Introduction

The Red Giant Transport Protocol (RGTP) is a stateless, receiver-driven, chunk-based, pre-encrypted, Merkle-verified, FEC-protected data transport operating over UDP and raw Ethernet. It is designed for high-bandwidth file distribution and deterministic low-latency autonomous vehicle (AV) Ethernet environments.

### 1.1 Design Principles

- **Stateless exposer**: The exposer holds only an immutable chunk store. All per-puller state lives in the puller.
- **Receiver-driven**: Pullers control the data rate by issuing pull requests. The exposer never pushes unsolicited data.
- **Pre-encryption**: Data is encrypted once at expose time. Subsequent pull requests are served from the encrypted store without re-encryption.
- **Merkle integrity**: A Merkle tree over plaintext chunk hashes enables per-chunk integrity verification.
- **Optional FEC**: Reed-Solomon FEC provides loss recovery without retransmission.

---

## 2. Packet Format

### 2.1 Common Header (4 bytes)

All RGTP packets begin with a 4-byte common header:

```
 0                   1                   2                   3
 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|  Version (8)  |  Type (8)     |         Length (16)           |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
```

- **Version**: Protocol version, currently `0x01`.
- **Type**: Packet type byte (see §2.2).
- **Length**: Total packet length in bytes, including this header. Big-endian.

All multi-byte integers are **big-endian** (network byte order).

### 2.2 Packet Types

| Type | Value | Description |
|------|-------|-------------|
| Pull_Request | 0x01 | Puller requests chunks from an Exposure |
| Manifest | 0x02 | Exposer describes an Exposure |
| Chunk_Data | 0x03 | Exposer sends an encrypted chunk |
| Chunk_Data_With_Proof | 0x04 | Chunk with Merkle proof |
| FEC_Parity | 0x05 | Reed-Solomon parity symbol |
| NAK | 0x06 | Puller requests retransmission |
| Rate_Report | 0x07 | Puller reports RTT and loss rate |
| Keepalive | 0x08 | Puller maintains NAT binding |

### 2.3 Pull_Request (0x01) — 36 bytes

```
Common Header (4) | Exposure_ID (16) | Window_Size (4) | Loss_Rate_Q16 (4) |
Flags (4) | Version_Min (2) | Version_Max (2)
```

- **Flags**: bit 0 = request Merkle proof, bit 1 = FEC enabled.
- **Version_Min/Max**: Protocol version range supported by this Puller.

### 2.4 Manifest (0x02) — 72 bytes

```
Common Header (4) | Exposure_ID (16) | Total_Size (8) | Chunk_Count (4) |
Chunk_Size (4) | Merkle_Root (32) | FEC_N (1) | FEC_K (1) | Flags (2)
```

### 2.5 Chunk_Data (0x03) — variable

```
Common Header (4) | Exposure_ID (16) | Chunk_Index (4) | Payload (variable)
```

Maximum payload: 1450 bytes (UDP) / 1430 bytes (raw Ethernet).

### 2.6 Chunk_Data_With_Proof (0x04) — variable

```
Common Header (4) | Exposure_ID (16) | Chunk_Index (4) | Proof_Depth (1) |
Proof (Proof_Depth × 32 bytes) | Payload (variable)
```

### 2.7 FEC_Parity (0x05) — variable

```
Common Header (4) | Exposure_ID (16) | Block_Index (4) | Parity_Index (1) |
FEC_N (1) | FEC_K (1) | Reserved (1) | Payload (variable)
```

### 2.8 NAK (0x06) — variable

```
Common Header (4) | Exposure_ID (16) | NAK_Count (2) | Reserved (2) |
Chunk_Indices (NAK_Count × 4 bytes)
```

### 2.9 Rate_Report (0x07) — 44 bytes

```
Common Header (4) | Exposure_ID (16) | RTT_Us (4) | Loss_Rate_Q16 (4) |
Window_Size (4) | Reserved (4) | Timestamp_Us (8)
```

### 2.10 Keepalive (0x08) — 32 bytes

```
Common Header (4) | Exposure_ID (16) | Timestamp_Us (8) | Reserved (4)
```

---

## 3. Protocol State Machine

### 3.1 Exposer

```
IDLE → [rgtp_expose()] → ACTIVE
ACTIVE → [rgtp_poll()] → ACTIVE  (serves pull requests)
ACTIVE → [rgtp_destroy_surface()] → DESTROYED
```

### 3.2 Puller

```
IDLE → [rgtp_pull_start()] → sends Pull_Request → waits for Manifest
     → [receives Manifest] → ACTIVE
ACTIVE → [rgtp_pull_next()] → sends Pull_Request → receives Chunk_Data
       → [all chunks received] → COMPLETE
ACTIVE → [rgtp_destroy_surface()] → DESTROYED
```

### 3.3 Version Negotiation

The Puller sends `Version_Min=1, Version_Max=N` in the Pull_Request. The Exposer responds with a Manifest using the highest version in `[Version_Min, Version_Max]` that it supports. If no overlap, the Exposer sends no response and the Puller times out with `RGTP_ERR_NOT_SUPPORTED`.

---

## 4. Cryptographic Design

### 4.1 AEAD Algorithm

Default: ChaCha20-Poly1305-IETF (12-byte nonce, 16-byte tag).  
Alternative: AES-256-GCM (selected at build time via `RGTP_CRYPTO_BACKEND`).

### 4.2 Nonce Construction

Each chunk is encrypted with a unique nonce derived from its chunk index:

```
nonce[12] = { chunk_index_LE[0..3], 0x00 × 8 }
```

This is safe because each Exposure uses a fresh random 256-bit key, and chunk indices are unique within an Exposure.

### 4.3 Merkle Tree

- Leaf hash: `H(0x00 || chunk_plaintext)`
- Internal node hash: `H(0x01 || left_child || right_child)`
- Hash function: BLAKE2b-256 (libsodium) or SHA-256 (OpenSSL)
- Tree is padded to the next power of 2 with zero-hash leaves

### 4.4 Key Distribution

Keys are distributed out-of-band (not over RGTP). The application is responsible for key exchange (e.g., via TLS, pre-shared key, or a key exchange protocol).

---

## 5. MTU and Fragmentation

- UDP mode: minimum MTU 576 bytes (RFC 791), maximum payload 65,507 bytes. Default chunk size 1,200 bytes.
- Raw Ethernet mode: maximum payload 1,486 bytes (1,500 − 14-byte Ethernet header). Default chunk size 1,400 bytes.

---

## 6. Error Codes

| Code | Value | Meaning |
|------|-------|---------|
| RGTP_OK | 0 | Success |
| RGTP_ERR_NOMEM | -1 | Memory allocation failed |
| RGTP_ERR_INVALID_ARG | -2 | Invalid argument |
| RGTP_ERR_SOCKET | -3 | Socket operation failed |
| RGTP_ERR_CRYPTO_INIT | -4 | Crypto library init failed |
| RGTP_ERR_ENCRYPT | -5 | AEAD encryption failed |
| RGTP_ERR_DECRYPT | -6 | AEAD decryption failed |
| RGTP_ERR_AUTH_FAIL | -7 | Authentication tag mismatch |
| RGTP_ERR_MERKLE_FAIL | -8 | Merkle proof verification failed |
| RGTP_ERR_FEC_FAIL | -9 | FEC decoding failed |
| RGTP_ERR_TRUNCATED | -10 | Packet truncated |
| RGTP_ERR_CHUNK_INDEX_OOB | -11 | Chunk index out of bounds |
| RGTP_ERR_TIMEOUT | -12 | Operation timed out |
| RGTP_ERR_RATE_LIMITED | -13 | Rate limit exceeded |
| RGTP_ERR_NOT_SUPPORTED | -14 | Feature not supported |
| RGTP_ERR_INTERNAL | -15 | Internal invariant violation |
