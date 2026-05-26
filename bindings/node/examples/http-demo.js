#!/usr/bin/env node
/**
 * http-demo.js — HTTP-over-RGTP bridge demo.
 *
 * Starts a minimal HTTP server that exposes files via RGTP and serves
 * a browser-accessible file listing. HTTP clients download files over
 * standard HTTP; the actual data transfer uses RGTP under the hood.
 *
 * This demonstrates how RGTP can be used as a transport layer beneath
 * an existing application protocol.
 *
 * Usage: node examples/http-demo.js [--dir DIR] [--http-port PORT] [--rgtp-port PORT]
 *
 * Run: node examples/http-demo.js --dir ./files --http-port 8080 --rgtp-port 9000
 */

'use strict';

const rgtp = require('../index.js');
const http = require('http');
const fs   = require('fs');
const path = require('path');
const url  = require('url');

function parseArgs() {
    const args = process.argv.slice(2);
    const opts = { dir: process.cwd(), httpPort: 8080, rgtpPort: 9000 };
    for (let i = 0; i < args.length; i++) {
        if (args[i] === '--dir'        && args[i + 1]) opts.dir       = path.resolve(args[++i]);
        if (args[i] === '--http-port'  && args[i + 1]) opts.httpPort  = parseInt(args[++i], 10);
        if (args[i] === '--rgtp-port'  && args[i + 1]) opts.rgtpPort  = parseInt(args[++i], 10);
    }
    return opts;
}

// ── RGTP Exposure registry ────────────────────────────────────────────────

class ExposureRegistry {
    constructor() {
        /** @type {Map<string, { surface: object, sock: object, filePath: string, size: number }>} */
        this._exposures = new Map();
    }

    async expose(filePath, rgtpPort) {
        const key = path.basename(filePath);
        if (this._exposures.has(key)) return this._exposures.get(key);

        const data = fs.readFileSync(filePath);
        let sock, surface;
        try {
            sock    = await rgtp.createSocket({ port: rgtpPort });
            surface = await rgtp.expose(sock, data, { merkleProofs: true });
        } catch (err) {
            throw new Error(`RGTP expose failed for ${filePath}: ${err.message}`);
        }

        const entry = {
            surface,
            sock,
            filePath,
            size:       data.length,
            exposureId: surface.exposureId().toString('hex'),
        };
        this._exposures.set(key, entry);

        // Background poll loop
        (async () => {
            while (true) {
                try { await surface.poll(200); } catch (_) { break; }
            }
        })();

        return entry;
    }

    list() {
        return [...this._exposures.entries()].map(([name, e]) => ({
            name,
            size:       e.size,
            exposureId: e.exposureId,
        }));
    }

    close() {
        for (const { surface, sock } of this._exposures.values()) {
            surface.close();
            sock.close();
        }
        this._exposures.clear();
    }
}

// ── HTTP request handler ──────────────────────────────────────────────────

function makeHandler(registry, opts) {
    return async (req, res) => {
        const parsed   = url.parse(req.url, true);
        const pathname = parsed.pathname;

        // GET / — file listing
        if (pathname === '/' || pathname === '/index.html') {
            const files = registry.list();
            const rows  = files.map(f =>
                `<tr>
                   <td><a href="/download/${encodeURIComponent(f.name)}">${f.name}</a></td>
                   <td>${rgtp.formatBytes(f.size)}</td>
                   <td><code>${f.exposureId}</code></td>
                 </tr>`
            ).join('\n');

            const html = `<!DOCTYPE html>
<html>
<head><title>RGTP File Server</title>
<style>body{font-family:monospace;padding:2em}table{border-collapse:collapse;width:100%}
td,th{border:1px solid #ccc;padding:.5em .8em;text-align:left}</style></head>
<body>
<h1>RGTP File Server</h1>
<p>Files are served over RGTP (UDP). Each row shows the Exposure ID
   needed to pull the file directly with the RGTP client.</p>
<table>
  <tr><th>File</th><th>Size</th><th>Exposure ID</th></tr>
  ${rows || '<tr><td colspan="3">No files exposed yet.</td></tr>'}
</table>
<hr>
<p>RGTP port: <strong>${opts.rgtpPort}</strong> &nbsp;|&nbsp;
   Pull command: <code>node client-demo.js &lt;server-ip&gt;:${opts.rgtpPort} &lt;exposure-id&gt; output.bin</code></p>
</body></html>`;

            res.writeHead(200, { 'Content-Type': 'text/html; charset=utf-8' });
            res.end(html);
            return;
        }

        // GET /download/:name — expose file and redirect to RGTP info
        const dlMatch = pathname.match(/^\/download\/(.+)$/);
        if (dlMatch) {
            const name     = decodeURIComponent(dlMatch[1]);
            const filePath = path.join(opts.dir, name);

            if (!fs.existsSync(filePath) || !fs.statSync(filePath).isFile()) {
                res.writeHead(404, { 'Content-Type': 'text/plain' });
                res.end('File not found');
                return;
            }

            try {
                const entry = await registry.expose(filePath, opts.rgtpPort);
                const info  = JSON.stringify({
                    file:       name,
                    size:       entry.size,
                    exposureId: entry.exposureId,
                    rgtpPort:   opts.rgtpPort,
                    pullCmd:    `node client-demo.js <server-ip>:${opts.rgtpPort} ${entry.exposureId} ${name}`,
                }, null, 2);

                res.writeHead(200, { 'Content-Type': 'application/json' });
                res.end(info);
            } catch (err) {
                res.writeHead(500, { 'Content-Type': 'text/plain' });
                res.end(`Error: ${err.message}`);
            }
            return;
        }

        res.writeHead(404, { 'Content-Type': 'text/plain' });
        res.end('Not found');
    };
}

// ── Main ──────────────────────────────────────────────────────────────────

async function main() {
    const opts     = parseArgs();
    const registry = new ExposureRegistry();

    console.log(`RGTP HTTP Bridge Demo`);
    console.log(`  Serving files from : ${opts.dir}`);
    console.log(`  HTTP port          : ${opts.httpPort}`);
    console.log(`  RGTP port          : ${opts.rgtpPort}`);
    console.log();

    // Pre-expose all files in the directory
    const files = fs.readdirSync(opts.dir).filter(f => {
        const full = path.join(opts.dir, f);
        return fs.statSync(full).isFile();
    });

    if (files.length === 0) {
        console.log('No files found in directory. Place files there and restart.');
    } else {
        console.log(`Pre-exposing ${files.length} file(s)...`);
        for (const f of files) {
            try {
                const entry = await registry.expose(path.join(opts.dir, f), opts.rgtpPort);
                console.log(`  ${f.padEnd(40)} ${entry.exposureId}`);
            } catch (err) {
                console.error(`  Failed to expose ${f}: ${err.message}`);
            }
        }
        console.log();
    }

    // Start HTTP server
    const server = http.createServer(makeHandler(registry, opts));
    server.listen(opts.httpPort, () => {
        console.log(`HTTP server listening on http://localhost:${opts.httpPort}`);
        console.log('Press Ctrl+C to stop.\n');
    });

    // Graceful shutdown
    process.on('SIGINT', () => {
        console.log('\nShutting down...');
        server.close();
        registry.close();
        process.exit(0);
    });
}

main().catch(err => {
    console.error('Unexpected error:', err.message);
    process.exit(1);
});
