#!/usr/bin/env node
/**
 * client-demo.js — RGTP puller (client) demo.
 *
 * Pulls a file from an RGTP exposer and writes it to disk.
 * Verifies each chunk with AEAD authentication and optional Merkle proofs.
 *
 * Usage: node examples/client-demo.js <host:port> <exposure-id-hex> <output-file>
 *
 * Run: node examples/client-demo.js 127.0.0.1:9000 <exposure-id> output.bin
 */

'use strict';

const rgtp = require('../index.js');
const fs   = require('fs');
const path = require('path');

function parseArgs() {
    const args = process.argv.slice(2);
    if (args.length < 3) {
        console.error(
            'Usage: node client-demo.js <host:port> <exposure-id-hex> <output-file>\n' +
            '  host:port        — Exposer address, e.g. 127.0.0.1:9000\n' +
            '  exposure-id-hex  — 32 hex characters (128-bit Exposure_ID)\n' +
            '  output-file      — Path to write the received file'
        );
        process.exit(1);
    }

    const [serverArg, exposureIdHex, outputFile] = args;

    const lastColon = serverArg.lastIndexOf(':');
    if (lastColon < 0) {
        console.error(`Error: invalid server address '${serverArg}' (expected host:port)`);
        process.exit(1);
    }
    const host = serverArg.slice(0, lastColon);
    const port = parseInt(serverArg.slice(lastColon + 1), 10);

    if (!exposureIdHex.match(/^[0-9a-fA-F]{32}$/)) {
        console.error(`Error: invalid exposure ID '${exposureIdHex}' (expected 32 hex chars)`);
        process.exit(1);
    }

    return { host, port, exposureIdHex, outputFile };
}

async function main() {
    const { host, port, exposureIdHex, outputFile } = parseArgs();
    const exposureId = Buffer.from(exposureIdHex, 'hex');

    console.log(`RGTP Client Demo`);
    console.log(`  Server      : ${host}:${port}`);
    console.log(`  Exposure ID : ${exposureIdHex}`);
    console.log(`  Output      : ${outputFile}`);
    console.log();

    // ── Create socket and start pull ──────────────────────────────────
    let sock, surface;
    try {
        sock    = await rgtp.createSocket({ windowSize: 64 });
        surface = await rgtp.pullStart(
            sock,
            { host, port },
            exposureId,
            { windowSize: 64, timeoutMs: 30000 }
        );
    } catch (err) {
        console.error('Failed to start pull:', err.message);
        if (err instanceof rgtp.RgtpError) {
            console.error(`  Error code: ${err.code}`);
        }
        process.exit(1);
    }

    // ── Receive chunks ────────────────────────────────────────────────
    const chunks   = new Map();
    const startTime = Date.now();
    let   authFailures = 0;

    console.log('Receiving chunks...\n');

    while (surface.progress() < 1.0) {
        try {
            const { data, chunkIndex } = await rgtp.pullNext(surface, 65536);
            chunks.set(chunkIndex, data);
        } catch (err) {
            if (err instanceof rgtp.RgtpError) {
                if (err.code === -12) continue;           // RGTP_ERR_TIMEOUT
                if (err.code === -7)  { authFailures++; continue; } // RGTP_ERR_AUTH_FAIL
                if (err.code === -8)  { continue; }       // RGTP_ERR_MERKLE_FAIL — skip
            }
            console.error('\nPull error:', err.message);
            break;
        }

        const elapsed  = (Date.now() - startTime) / 1000;
        const received = [...chunks.values()].reduce((s, b) => s + b.length, 0);
        const mbps     = received / Math.max(elapsed, 0.001) / 1_048_576;
        const pct      = (surface.progress() * 100).toFixed(1);

        process.stdout.write(
            `\r  ${pct.padStart(5)}%`
            + `  ${rgtp.formatBytes(received).padEnd(12)}`
            + `  ${mbps.toFixed(1)} MB/s`
            + `  ${elapsed.toFixed(0)}s   `
        );
    }

    // ── Write output file ─────────────────────────────────────────────
    console.log('\n');
    const sortedChunks = [...chunks.keys()].sort((a, b) => a - b).map(k => chunks.get(k));
    const output       = Buffer.concat(sortedChunks);

    fs.writeFileSync(outputFile, output);

    // ── Summary ───────────────────────────────────────────────────────
    const elapsed = (Date.now() - startTime) / 1000;
    console.log(`Done.`);
    console.log(`  Received      : ${rgtp.formatBytes(output.length)}`);
    console.log(`  Output        : ${outputFile}`);
    console.log(`  Time          : ${elapsed.toFixed(1)}s`);
    console.log(`  Avg speed     : ${(output.length / elapsed / 1_048_576).toFixed(1)} MB/s`);
    if (authFailures > 0) {
        console.log(`  Auth failures : ${authFailures} (chunks discarded)`);
    }

    // ── Teardown ──────────────────────────────────────────────────────
    surface.close();
    sock.close();
}

main().catch(err => {
    console.error('Unexpected error:', err.message);
    process.exit(1);
});
