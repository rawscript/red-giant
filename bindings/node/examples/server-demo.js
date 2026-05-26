#!/usr/bin/env node
/**
 * server-demo.js — RGTP exposer (server) demo.
 *
 * Exposes a file and serves pull requests until Ctrl+C.
 * Prints live statistics every second.
 *
 * Usage: node examples/server-demo.js <file> [--port PORT] [--fec]
 *
 * Run: node examples/server-demo.js large-file.bin --port 9000 --fec
 */

'use strict';

const rgtp = require('../index.js');
const fs   = require('fs');
const path = require('path');

function parseArgs() {
    const args = process.argv.slice(2);
    const opts = { file: null, port: 9000, fec: false, merkleProofs: false };

    for (let i = 0; i < args.length; i++) {
        if (args[i] === '--port' && args[i + 1]) {
            opts.port = parseInt(args[++i], 10);
        } else if (args[i] === '--fec') {
            opts.fec = true;
        } else if (args[i] === '--merkle-proofs') {
            opts.merkleProofs = true;
        } else if (!args[i].startsWith('--')) {
            opts.file = args[i];
        }
    }
    return opts;
}

async function main() {
    const opts = parseArgs();

    if (!opts.file) {
        console.error('Usage: node server-demo.js <file> [--port PORT] [--fec] [--merkle-proofs]');
        process.exit(1);
    }

    if (!fs.existsSync(opts.file)) {
        console.error(`Error: file not found: ${opts.file}`);
        process.exit(1);
    }

    const fileSize = fs.statSync(opts.file).size;
    const data     = fs.readFileSync(opts.file);

    console.log(`RGTP Server Demo`);
    console.log(`  File   : ${opts.file}  (${rgtp.formatBytes(fileSize)})`);
    console.log(`  Port   : ${opts.port}`);
    console.log(`  FEC    : ${opts.fec ? 'enabled (n=255, k=223)' : 'disabled'}`);
    console.log();

    // ── Create socket and expose ──────────────────────────────────────
    let sock, surface;
    try {
        sock    = await rgtp.createSocket({ port: opts.port });
        surface = await rgtp.expose(sock, data, {
            fecEnabled:    opts.fec,
            fecK:          223,
            fecN:          255,
            merkleProofs:  opts.merkleProofs,
        });
    } catch (err) {
        console.error('Failed to start exposer:', err.message);
        if (err instanceof rgtp.RgtpError) {
            console.error(`  Error code: ${err.code}`);
        }
        process.exit(1);
    }

    const exposureId = surface.exposureId().toString('hex');
    console.log(`  Exposure ID : ${exposureId}`);
    console.log();
    console.log(`  Pull command:`);
    console.log(`    node examples/client-demo.js 127.0.0.1:${opts.port} ${exposureId} output.bin`);
    console.log();
    console.log('Serving pull requests. Press Ctrl+C to stop.\n');

    // ── Graceful shutdown ─────────────────────────────────────────────
    let running = true;
    process.on('SIGINT', () => { running = false; });

    // ── Stats display ─────────────────────────────────────────────────
    const startTime = Date.now();

    const statsInterval = setInterval(async () => {
        if (!running) return;
        try {
            const stats   = await surface.getStats();
            const elapsed = (Date.now() - startTime) / 1000;
            const mbps    = (stats.bytesSent ?? 0) / elapsed / 1_048_576;

            process.stdout.write(
                `\r  Sent: ${rgtp.formatBytes(stats.bytesSent ?? 0).padEnd(12)}`
                + `  Pullers: ${(stats.pullPressure ?? 0).toString().padEnd(6)}`
                + `  Auth failures: ${stats.authFailures ?? 0}`
                + `  Throughput: ${mbps.toFixed(1)} MB/s`
                + `  Uptime: ${elapsed.toFixed(0)}s   `
            );
        } catch (_) { /* surface may be closing */ }
    }, 1000);

    // ── Poll loop ─────────────────────────────────────────────────────
    while (running) {
        try {
            await surface.poll(200);
        } catch (err) {
            if (err instanceof rgtp.RgtpError && err.code === -12) continue; // timeout
            console.error('\nPoll error:', err.message);
            break;
        }
    }

    // ── Teardown ──────────────────────────────────────────────────────
    clearInterval(statsInterval);
    console.log('\n\nShutting down...');
    surface.close();
    sock.close();
    console.log('Done.');
}

main().catch(err => {
    console.error('Unexpected error:', err.message);
    process.exit(1);
});
