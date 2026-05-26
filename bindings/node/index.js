'use strict';

/**
 * rgtp — Node.js N-API binding for the Red Giant Transport Protocol.
 *
 * Exposes the full RGTP C API through N-API with Promise-based async
 * functions and a Node.js Readable stream interface for pull operations.
 *
 * Requirements: 14.1, 14.2, 14.3, 14.8
 */

const { Readable } = require('stream');

// ── Load native addon ─────────────────────────────────────────────────────

let _native;
try {
    _native = require('./build/Release/rgtp.node');
} catch (_) {
    try {
        _native = require('./build/Debug/rgtp.node');
    } catch (err) {
        // Allow import to succeed without the native addon so that
        // type-checking and documentation tooling work without a build.
        _native = null;
    }
}

function _requireNative() {
    if (!_native) {
        throw new RgtpError(-14, 'Native addon not built. Run: npm run build');
    }
    return _native;
}

// ── Error type ────────────────────────────────────────────────────────────

/**
 * Typed error thrown by all RGTP API functions on failure.
 * @property {number} code  - RGTP error code (negative integer)
 * @property {string} message - Human-readable description
 */
class RgtpError extends Error {
    constructor(code, message) {
        super(`RGTP error ${code}: ${message}`);
        this.name = 'RgtpError';
        this.code = code;
        if (Error.captureStackTrace) {
            Error.captureStackTrace(this, RgtpError);
        }
    }
}

// ── Library lifecycle ─────────────────────────────────────────────────────

/**
 * Initialise the RGTP library. Must be called once before any other function.
 * Safe to call multiple times (idempotent).
 * @returns {Promise<void>}
 */
async function init() {
    const n = _requireNative();
    return new Promise((resolve, reject) => {
        n.init((err) => err ? reject(new RgtpError(err.code, err.message)) : resolve());
    });
}

/**
 * Release all global library resources.
 * @returns {void}
 */
function cleanup() {
    if (_native) _native.cleanup();
}

/**
 * Return the library version string (e.g. "1.0.0").
 * @returns {string}
 */
function version() {
    return _requireNative().version();
}

// ── Socket ────────────────────────────────────────────────────────────────

/**
 * Opaque socket handle returned by createSocket().
 */
class RgtpSocket {
    constructor(_handle) {
        this._handle = _handle;
    }

    /** Return the port the socket is bound to. */
    boundPort() {
        return _requireNative().socketBoundPort(this._handle);
    }

    /** Destroy the socket and release all associated resources. */
    close() {
        if (this._handle) {
            _requireNative().socketDestroy(this._handle);
            this._handle = null;
        }
    }
}

/**
 * Create and bind an RGTP UDP socket.
 * @param {object} [options]
 * @param {number} [options.port=0]        - UDP port (0 = auto-assign)
 * @param {number} [options.windowSize=64] - Initial pull window size
 * @param {number} [options.timeoutMs=-1]  - Operation timeout (-1 = infinite)
 * @returns {Promise<RgtpSocket>}
 */
async function createSocket(options = {}) {
    const n = _requireNative();
    return new Promise((resolve, reject) => {
        n.socketCreate(options, (err, handle) => {
            if (err) return reject(new RgtpError(err.code, err.message));
            resolve(new RgtpSocket(handle));
        });
    });
}

// ── Surface ───────────────────────────────────────────────────────────────

/**
 * Opaque surface handle representing an exposer or puller session.
 */
class RgtpSurface {
    constructor(_handle) {
        this._handle = _handle;
    }

    /**
     * Return the 16-byte Exposure_ID as a Buffer.
     * @returns {Buffer}
     */
    exposureId() {
        return _requireNative().surfaceExposureId(this._handle);
    }

    /**
     * Return the transfer completion fraction [0.0, 1.0].
     * @returns {number}
     */
    progress() {
        return _requireNative().surfaceProgress(this._handle);
    }

    /**
     * Return transfer statistics.
     * @returns {Promise<object>}
     */
    async getStats() {
        const n = _requireNative();
        return new Promise((resolve, reject) => {
            n.surfaceGetStats(this._handle, (err, stats) => {
                if (err) return reject(new RgtpError(err.code, err.message));
                resolve(stats);
            });
        });
    }

    /**
     * Serve one round of pending pull requests (exposer side).
     * @param {number} [timeoutMs=100]
     * @returns {Promise<void>}
     */
    async poll(timeoutMs = 100) {
        const n = _requireNative();
        return new Promise((resolve, reject) => {
            n.poll(this._handle, timeoutMs, (err) => {
                if (err && err.code !== -12) { // -12 = RGTP_ERR_TIMEOUT
                    return reject(new RgtpError(err.code, err.message));
                }
                resolve();
            });
        });
    }

    /**
     * Create a Node.js Readable stream that delivers chunks as they arrive.
     * Each chunk is emitted as a Buffer. The stream ends when all chunks
     * have been received (progress === 1.0).
     * @returns {Readable}
     */
    createReadStream() {
        const surface = this;
        const n = _requireNative();

        return new Readable({
            objectMode: false,
            read() {
                const self = this;

                function fetchNext() {
                    if (surface.progress() >= 1.0) {
                        self.push(null); // EOF
                        return;
                    }

                    n.pullNext(surface._handle, 65536, (err, data, _chunkIndex) => {
                        if (err) {
                            if (err.code === -12) { // RGTP_ERR_TIMEOUT
                                setImmediate(fetchNext);
                                return;
                            }
                            self.destroy(new RgtpError(err.code, err.message));
                            return;
                        }
                        if (!self.push(data)) {
                            // Backpressure — wait for drain
                            self.once('drain', fetchNext);
                        } else {
                            setImmediate(fetchNext);
                        }
                    });
                }

                fetchNext();
            }
        });
    }

    /**
     * Destroy the surface and securely zeroize all key material.
     */
    close() {
        if (this._handle) {
            _requireNative().surfaceDestroy(this._handle);
            this._handle = null;
        }
    }
}

// ── Exposer API ───────────────────────────────────────────────────────────

/**
 * Pre-encrypt data and create an immutable Exposure.
 *
 * Generates a CSPRNG Exposure_ID, encrypts all chunks with AEAD, builds
 * the Merkle tree, and optionally pre-computes FEC parity chunks.
 *
 * @param {RgtpSocket} sock
 * @param {Buffer|Uint8Array} data
 * @param {object} [options]
 * @param {boolean} [options.fecEnabled=false]
 * @param {number}  [options.fecK=223]
 * @param {number}  [options.fecN=255]
 * @param {boolean} [options.merkleProofs=false]
 * @param {number}  [options.chunkSize=0]  0 = auto (1200 bytes for UDP)
 * @returns {Promise<RgtpSurface>}
 */
async function expose(sock, data, options = {}) {
    const n = _requireNative();
    if (!Buffer.isBuffer(data)) data = Buffer.from(data);

    return new Promise((resolve, reject) => {
        n.expose(sock._handle, data, options, (err, handle) => {
            if (err) return reject(new RgtpError(err.code, err.message));
            resolve(new RgtpSurface(handle));
        });
    });
}

// ── Puller API ────────────────────────────────────────────────────────────

/**
 * Begin pulling an Exposure from a remote Exposer.
 *
 * Sends a Pull_Request, receives the Manifest, and initialises the puller
 * surface with the Merkle root, chunk count, and FEC parameters.
 *
 * @param {RgtpSocket} sock
 * @param {{ host: string, port: number }} server
 * @param {Buffer} exposureId  - 16-byte Exposure_ID
 * @param {object} [options]
 * @param {number} [options.windowSize=64]
 * @param {number} [options.timeoutMs=30000]
 * @returns {Promise<RgtpSurface>}
 */
async function pullStart(sock, server, exposureId, options = {}) {
    const n = _requireNative();
    if (!Buffer.isBuffer(exposureId)) exposureId = Buffer.from(exposureId);
    if (exposureId.length !== 16) {
        throw new RgtpError(-2, 'exposureId must be exactly 16 bytes');
    }

    return new Promise((resolve, reject) => {
        n.pullStart(sock._handle, server.host, server.port, exposureId, options,
            (err, handle) => {
                if (err) return reject(new RgtpError(err.code, err.message));
                resolve(new RgtpSurface(handle));
            });
    });
}

/**
 * Receive the next available chunk.
 *
 * Validates the packet type before writing to the buffer, verifies the AEAD
 * authentication tag, and optionally verifies the Merkle proof.
 *
 * @param {RgtpSurface} surface
 * @param {number} [bufSize=65536]
 * @returns {Promise<{ data: Buffer, chunkIndex: number }>}
 */
async function pullNext(surface, bufSize = 65536) {
    const n = _requireNative();
    return new Promise((resolve, reject) => {
        n.pullNext(surface._handle, bufSize, (err, data, chunkIndex) => {
            if (err) return reject(new RgtpError(err.code, err.message));
            resolve({ data, chunkIndex });
        });
    });
}

// ── Utility ───────────────────────────────────────────────────────────────

/**
 * Format a byte count as a human-readable string.
 * @param {number} bytes
 * @returns {string}  e.g. "1.20 MB"
 */
function formatBytes(bytes) {
    if (bytes === 0) return '0 Bytes';
    const units = ['Bytes', 'KB', 'MB', 'GB', 'TB'];
    const i = Math.min(Math.floor(Math.log(bytes) / Math.log(1024)), units.length - 1);
    return `${(bytes / Math.pow(1024, i)).toFixed(2)} ${units[i]}`;
}

// ── Configuration presets ─────────────────────────────────────────────────

/** Optimised for high-bandwidth LAN (1 GbE+). */
function createLANConfig() {
    return { chunkSize: 1400, windowSize: 256, timeoutMs: 10000 };
}

/** Optimised for variable-bandwidth WAN. */
function createWANConfig() {
    return { chunkSize: 1200, windowSize: 64, timeoutMs: 30000 };
}

/** Optimised for mobile / high-loss links. */
function createMobileConfig() {
    return {
        chunkSize: 1200,
        windowSize: 32,
        timeoutMs: 60000,
        fecEnabled: true,
        fecK: 223,
        fecN: 255,
    };
}

// ── Exports ───────────────────────────────────────────────────────────────

module.exports = {
    // Error type
    RgtpError,

    // Library lifecycle
    init,
    cleanup,
    version,

    // Socket
    createSocket,
    RgtpSocket,

    // Surface
    RgtpSurface,

    // Exposer
    expose,

    // Puller
    pullStart,
    pullNext,

    // Utility
    formatBytes,

    // Configuration presets
    createLANConfig,
    createWANConfig,
    createMobileConfig,
};
