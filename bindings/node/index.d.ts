/**
 * rgtp — TypeScript declarations for the Node.js RGTP binding.
 */

/// <reference types="node" />

import { Readable } from 'stream';

// ── Error ─────────────────────────────────────────────────────────────────

/** Thrown by all RGTP API functions on failure. */
export class RgtpError extends Error {
    /** RGTP error code (negative integer, e.g. -7 for auth failure). */
    readonly code: number;
    constructor(code: number, message: string);
}

// ── Library lifecycle ─────────────────────────────────────────────────────

/** Initialise the RGTP library. Must be called once before any other function. */
export function init(): Promise<void>;

/** Release all global library resources. */
export function cleanup(): void;

/** Return the library version string (e.g. "1.0.0"). */
export function version(): string;

// ── Socket ────────────────────────────────────────────────────────────────

export interface SocketOptions {
    /** UDP port to bind to. 0 = auto-assign. Default: 0. */
    port?: number;
    /** Initial pull window size in chunks. Default: 64. */
    windowSize?: number;
    /** Operation timeout in milliseconds. -1 = infinite. Default: -1. */
    timeoutMs?: number;
}

/** Opaque socket handle. */
export class RgtpSocket {
    /** Return the port the socket is bound to. */
    boundPort(): number;
    /** Destroy the socket and release all associated resources. */
    close(): void;
}

/** Create and bind an RGTP UDP socket. */
export function createSocket(options?: SocketOptions): Promise<RgtpSocket>;

// ── Surface ───────────────────────────────────────────────────────────────

export interface RgtpStats {
    bytesSent: number;
    bytesReceived: number;
    chunksSent: number;
    chunksReceived: number;
    authFailures: number;
    malformedPackets: number;
    fecRecoveries: number;
    nakSent: number;
    packetLossRate: number;
    rttUs: number;
    pullPressure: number;
}

/** Opaque surface handle representing an exposer or puller session. */
export class RgtpSurface {
    /** Return the 16-byte Exposure_ID as a Buffer. */
    exposureId(): Buffer;
    /** Return the transfer completion fraction [0.0, 1.0]. */
    progress(): number;
    /** Return transfer statistics. */
    getStats(): Promise<RgtpStats>;
    /**
     * Serve one round of pending pull requests (exposer side).
     * Returns when the timeout elapses or a socket error occurs.
     */
    poll(timeoutMs?: number): Promise<void>;
    /**
     * Create a Node.js Readable stream that delivers chunks as they arrive.
     * The stream ends when all chunks have been received (progress === 1.0).
     */
    createReadStream(): Readable;
    /** Destroy the surface and securely zeroize all key material. */
    close(): void;
}

// ── Exposer API ───────────────────────────────────────────────────────────

export interface ExposeOptions {
    /** Enable Reed-Solomon FEC. Default: false. */
    fecEnabled?: boolean;
    /** RS data symbols per block. Default: 223. */
    fecK?: number;
    /** RS total symbols per block. Default: 255. */
    fecN?: number;
    /** Include per-chunk Merkle proofs in responses. Default: false. */
    merkleProofs?: boolean;
    /** Chunk size in bytes. 0 = auto (1200 for UDP). Default: 0. */
    chunkSize?: number;
}

/**
 * Pre-encrypt data and create an immutable Exposure.
 *
 * Generates a CSPRNG Exposure_ID, encrypts all chunks with AEAD, builds
 * the Merkle tree, and optionally pre-computes FEC parity chunks.
 */
export function expose(
    sock: RgtpSocket,
    data: Buffer | Uint8Array,
    options?: ExposeOptions
): Promise<RgtpSurface>;

// ── Puller API ────────────────────────────────────────────────────────────

export interface ServerAddress {
    host: string;
    port: number;
}

export interface PullOptions {
    /** Pull window size in chunks. Default: 64. */
    windowSize?: number;
    /** Connection timeout in milliseconds. Default: 30000. */
    timeoutMs?: number;
}

export interface ChunkResult {
    /** Decrypted, verified chunk data. */
    data: Buffer;
    /** Zero-based chunk index within the Exposure. */
    chunkIndex: number;
}

/**
 * Begin pulling an Exposure from a remote Exposer.
 *
 * Sends a Pull_Request, receives the Manifest, and initialises the puller
 * surface with the Merkle root, chunk count, and FEC parameters.
 */
export function pullStart(
    sock: RgtpSocket,
    server: ServerAddress,
    exposureId: Buffer,
    options?: PullOptions
): Promise<RgtpSurface>;

/**
 * Receive the next available chunk.
 *
 * Verifies the AEAD authentication tag and optional Merkle proof before
 * returning the plaintext chunk data.
 */
export function pullNext(
    surface: RgtpSurface,
    bufSize?: number
): Promise<ChunkResult>;

// ── Utility ───────────────────────────────────────────────────────────────

/** Format a byte count as a human-readable string (e.g. "1.20 MB"). */
export function formatBytes(bytes: number): string;

// ── Configuration presets ─────────────────────────────────────────────────

/** Optimised for high-bandwidth LAN (1 GbE+). */
export function createLANConfig(): SocketOptions & PullOptions;

/** Optimised for variable-bandwidth WAN. */
export function createWANConfig(): SocketOptions & PullOptions;

/** Optimised for mobile / high-loss links (FEC enabled). */
export function createMobileConfig(): SocketOptions & PullOptions & ExposeOptions;
