#!/usr/bin/env node
/**
 * simple-transfer.js — Minimal RGTP expose + pull example.
 *
 * Demonstrates the core API:
 *   1. Create a socket
 *   2. Expose data (pre-encrypts chunks, builds Merkle tree)
 *   3. Pull data from the same process (loopback)
 *   4. Verify the received data matches the original
 *
 * Run: node examples/simple-transfer.js
 */

'use strict';

const rgtp = require('../index.js');
const assert = require('assert');

async function main() {
    console.log(`RGTP Simple Transfer Example\n`);

    // ── Create test data ──────────────────────────────────────────────
    const original = Buffer.from(
        'Hello from RGTP! This data is pre-encrypted once and served ' +
        'to any number of pullers from an immutable chunk store.\n'
    );
    console.log(`Data size : ${rgtp.formatBytes(original.length)}`);

    // ── Exposer side ──────────────────────────────────────────────────
    let exposerSock, exposerSurface;
    try {
        exposerSock    = await rgtp.createSocket({ port: 0 });
        exposerSurface = await rgtp.expose(exposerSock, original, {
            merkleProofs: true,
        });
    } catch (err) {
        console.error('Expose failed:', err.message);
        console.log('\nNote: this example requires librgtp to be installed.');
        process.exit(0);
    }

    const exposureId = exposerSurface.exposureId();
    console.log(`Exposure ID: ${exposureId.toString('hex')}`);

    // Start serving pull requests in the background
    const pollLoop = (async () => {
        while (exposerSurface.progress() < 1.0) {
            try { await exposerSurface.poll(100); } catch (_) { break; }
        }
    })();

    // ── Puller side ───────────────────────────────────────────────────
    let pullerSock, pullerSurface;
    try {
        pullerSock    = await rgtp.createSocket({ port: 0 });
        pullerSurface = await rgtp.pullStart(
            pullerSock,
            { host: '127.0.0.1', port: exposerSock.boundPort() },
            exposureId,
            { windowSize: 16 }
        );
    } catch (err) {
        console.error('Pull start failed:', err.message);
        exposerSurface.close();
        exposerSock.close();
        process.exit(0);
    }

    // Receive all chunks
    const chunks = new Map();
    while (pullerSurface.progress() < 1.0) {
        try {
            const { data, chunkIndex } = await rgtp.pullNext(pullerSurface);
            chunks.set(chunkIndex, data);
        } catch (err) {
            if (err instanceof rgtp.RgtpError && err.code === -12) continue; // timeout
            console.error('Pull error:', err.message);
            break;
        }
    }

    // Reassemble in order
    const received = Buffer.concat(
        [...chunks.keys()].sort((a, b) => a - b).map(k => chunks.get(k))
    );

    // ── Verify ────────────────────────────────────────────────────────
    const match = original.equals(received);
    console.log(`\nReceived  : ${rgtp.formatBytes(received.length)}`);
    console.log(`Verified  : ${match ? 'PASS — data matches original' : 'FAIL — mismatch'}`);

    if (!match) {
        console.error('Data mismatch — this should not happen.');
        process.exit(1);
    }

    // ── Stats ─────────────────────────────────────────────────────────
    const stats = await exposerSurface.getStats();
    console.log(`\nExposer stats:`);
    console.log(`  Chunks sent    : ${stats.chunksSent ?? 'n/a'}`);
    console.log(`  Bytes sent     : ${rgtp.formatBytes(stats.bytesSent ?? 0)}`);

    // ── Teardown ──────────────────────────────────────────────────────
    pullerSurface.close();
    pullerSock.close();
    exposerSurface.close();
    exposerSock.close();

    console.log('\nDone.');
}

main().catch(err => {
    console.error('Unexpected error:', err.message);
    process.exit(1);
});
