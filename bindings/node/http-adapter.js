'use strict';

/**
 * http-adapter.js — HTTP bridge over RGTP.
 *
 * Provides an HTTP server that exposes files via RGTP and serves a
 * browser-accessible file listing. Actual data transfer uses RGTP UDP
 * under the hood.
 *
 * Usage:
 *   const { RGTPHTTPAdapter } = require('rgtp/http-adapter');
 *   const adapter = new RGTPHTTPAdapter({ httpPort: 8080, rgtpPort: 9000 });
 *   await adapter.start('./files');
 */

const http = require('http');
const fs   = require('fs');
const path = require('path');
const url  = require('url');

const rgtp = require('./index.js');

// ── Exposure registry ─────────────────────────────────────────────────────

class ExposureRegistry {
    constructor() {
        /** @type {Map<string, { surface: object, sock: object, filePath: string, size: number, exposureId: string }>} */
        this._map = new Map();
    }

    async expose(filePath, rgtpPort) {
        const name = path.basename(filePath);
        if (this._map.has(name)) return this._map.get(name);

        const data = fs.readFileSync(filePath);
        let sock, surface;
        try {
            sock    = await rgtp.createSocket({ port: rgtpPort });
            surface = await rgtp.expose(sock, data, { merkleProofs: true });
        } catch (err) {
            throw new Error(`RGTP expose failed for ${name}: ${err.message}`);
        }

        const entry = {
            surface,
            sock,
            filePath,
            size:       data.length,
            exposureId: surface.exposureId().toString('hex'),
        };
        this._map.set(name, entry);

        // Background poll loop — serves pull requests until the surface is closed
        (async () => {
            while (entry.surface._handle) {
                try { await surface.poll(200); } catch (_) { break; }
            }
        })();

        return entry;
    }

    list() {
        return [...this._map.entries()].map(([name, e]) => ({
            name,
            size:       e.size,
            exposureId: e.exposureId,
        }));
    }

    close() {
        for (const { surface, sock } of this._map.values()) {
            try { surface.close(); } catch (_) {}
            try { sock.close();    } catch (_) {}
        }
        this._map.clear();
    }
}

// ── HTTP request handler ──────────────────────────────────────────────────

function makeHandler(registry, opts) {
    return async (req, res) => {
        const { pathname } = url.parse(req.url);

        // GET / — file listing
        if (pathname === '/' || pathname === '/index.html') {
            const files = registry.list();
            const rows  = files.map(f =>
                `<tr>
                   <td><a href="/info/${encodeURIComponent(f.name)}">${f.name}</a></td>
                   <td>${rgtp.formatBytes(f.size)}</td>
                   <td style="font-family:monospace;font-size:0.85em">${f.exposureId}</td>
                 </tr>`
            ).join('\n');

            const html = `<!DOCTYPE html>
<html>
<head><title>RGTP File Server</title>
<style>
  body { font-family: system-ui, sans-serif; padding: 2em; max-width: 1100px; margin: 0 auto; }
  h1   { color: #c0392b; }
  table { border-collapse: collapse; width: 100%; margin-top: 1em; }
  th, td { border: 1px solid #ddd; padding: .6em 1em; text-align: left; }
  th { background: #f8f8f8; }
  a  { color: #c0392b; }
  code { background: #f4f4f4; padding: .1em .4em; border-radius: 3px; }
</style></head>
<body>
<h1>RGTP File Server</h1>
<p>Files are served over RGTP (UDP port <strong>${opts.rgtpPort}</strong>).
   Click a file name to get its Exposure ID and pull command.</p>
<table>
  <tr><th>File</th><th>Size</th><th>Exposure ID</th></tr>
  ${rows || '<tr><td colspan="3">No files exposed yet.</td></tr>'}
</table>
<hr>
<p>Pull a file with the Node.js client:</p>
<code>node examples/client-demo.js &lt;server-ip&gt;:${opts.rgtpPort} &lt;exposure-id&gt; output.bin</code>
</body></html>`;

            res.writeHead(200, { 'Content-Type': 'text/html; charset=utf-8' });
            res.end(html);
            return;
        }

        // GET /info/:name — JSON info for a specific file
        const infoMatch = pathname.match(/^\/info\/(.+)$/);
        if (infoMatch) {
            const name     = decodeURIComponent(infoMatch[1]);
            const filePath = path.join(opts.dir, name);

            if (!fs.existsSync(filePath) || !fs.statSync(filePath).isFile()) {
                res.writeHead(404, { 'Content-Type': 'application/json' });
                res.end(JSON.stringify({ error: 'File not found' }));
                return;
            }

            try {
                const entry = await registry.expose(filePath, opts.rgtpPort);
                res.writeHead(200, { 'Content-Type': 'application/json' });
                res.end(JSON.stringify({
                    file:       name,
                    size:       entry.size,
                    exposureId: entry.exposureId,
                    rgtpPort:   opts.rgtpPort,
                    pullCmd:    `node client-demo.js <server-ip>:${opts.rgtpPort} ${entry.exposureId} ${name}`,
                }, null, 2));
            } catch (err) {
                res.writeHead(500, { 'Content-Type': 'application/json' });
                res.end(JSON.stringify({ error: err.message }));
            }
            return;
        }

        res.writeHead(404, { 'Content-Type': 'text/plain' });
        res.end('Not found');
    };
}

// ── RGTPHTTPAdapter ───────────────────────────────────────────────────────

/**
 * HTTP bridge that exposes files via RGTP and serves a browser file listing.
 *
 * @example
 * const adapter = new RGTPHTTPAdapter({ httpPort: 8080, rgtpPort: 9000 });
 * await adapter.start('./files');
 * // Visit http://localhost:8080
 */
class RGTPHTTPAdapter {
    /**
     * @param {object} [options]
     * @param {number} [options.httpPort=8080]  - HTTP server port
     * @param {number} [options.rgtpPort=9000]  - RGTP UDP port for exposures
     */
    constructor(options = {}) {
        this._opts = {
            httpPort: options.httpPort || 8080,
            rgtpPort: options.rgtpPort || 9000,
            dir:      options.dir || process.cwd(),
        };
        this._registry = new ExposureRegistry();
        this._server   = null;
    }

    /**
     * Start the HTTP server and pre-expose all files in the directory.
     * @param {string} [dir]  - Directory to serve (overrides constructor option)
     * @returns {Promise<void>}
     */
    async start(dir) {
        if (dir) this._opts.dir = path.resolve(dir);

        // Pre-expose all files in the directory
        const files = fs.readdirSync(this._opts.dir).filter(f => {
            const full = path.join(this._opts.dir, f);
            return fs.statSync(full).isFile();
        });

        for (const f of files) {
            try {
                await this._registry.expose(path.join(this._opts.dir, f), this._opts.rgtpPort);
            } catch (err) {
                console.error(`[rgtp-http] Failed to expose ${f}: ${err.message}`);
            }
        }

        return new Promise((resolve, reject) => {
            this._server = http.createServer(makeHandler(this._registry, this._opts));
            this._server.listen(this._opts.httpPort, () => resolve());
            this._server.on('error', reject);
        });
    }

    /** Stop the HTTP server and close all RGTP exposures. */
    stop() {
        if (this._server) this._server.close();
        this._registry.close();
    }
}

module.exports = { RGTPHTTPAdapter };
