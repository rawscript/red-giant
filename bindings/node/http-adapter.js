const EventEmitter = require('events');
const http = require('http');
const https = require('https');
const url = require('url');
const fs = require('fs').promises;
const path = require('path');

// Load the core RGTP implementation
const rgtp = require('./index.js');

/**
 * HTTP Adapter for RGTP
 * Provides HTTP-like interface over pure UDP RGTP core
 */
class RGTPHTTPAdapter extends EventEmitter {
  constructor(options = {}) {
    super();
    this.options = {
      port: options.port || 8080,
      useSSL: options.useSSL || false,
      rgtpPort: options.rgtpPort || 9999,
      ...options
    };
    
    this.server = null;
    this.rgtpSessions = new Map();
    this.activeTransfers = new Map();
  }
  
  async start() {
    return new Promise((resolve, reject) => {
      const serverClass = this.options.useSSL ? https : http;
      
      this.server = serverClass.createServer(async (req, res) => {
        await this.handleHTTPRequest(req, res);
      });
      
      this.server.listen(this.options.port, () => {
        console.log(`🚀 RGTP HTTP Adapter listening on port ${this.options.port}`);
        console.log(`📡 RGTP core running on port ${this.options.rgtpPort}`);
        resolve();
      });
      
      this.server.on('error', reject);
    });
  }
  
  async handleHTTPRequest(req, res) {
    const parsedUrl = url.parse(req.url, true);
    const pathname = parsedUrl.pathname;
    
    try {
      if (req.method === 'GET') {
        await this.handleGET(req, res, pathname, parsedUrl.query);
      } else if (req.method === 'POST') {
        await this.handlePOST(req, res, pathname);
      } else {
        this.sendError(res, 405, 'Method Not Allowed');
      }
    } catch (error) {
      console.error('HTTP Handler Error:', error);
      this.sendError(res, 500, 'Internal Server Error');
    }
  }
  
  async handleGET(req, res, pathname, query) {
    // Serve static files or file listing
    if (pathname === '/' || pathname === '/files') {
      await this.serveFileListing(res);
      return;
    }
    
    // Serve specific file
    const fileName = pathname.substring(1); // Remove leading slash
    const filePath = path.join(process.cwd(), fileName);
    
    try {
      await fs.access(filePath);
      // Create RGTP session for this file
      const sessionId = this.generateSessionId();
      const session = new rgtp.Session({ port: this.options.rgtpPort + this.rgtpSessions.size });
      
      this.rgtpSessions.set(sessionId, session);
      
      // Start exposing the file
      await session.exposeFile(filePath);
      
      // Store transfer info
      this.activeTransfers.set(sessionId, {
        fileName,
        filePath,
        session,
        startTime: Date.now()
      });
      
      // Redirect to RGTP download endpoint
      res.writeHead(302, {
        'Location': `/download/${sessionId}`,
        'Content-Type': 'text/html'
      });
      res.end(`
        <html>
          <head><title>RGTP Download</title></head>
          <body>
            <h1>Starting RGTP Transfer</h1>
            <p>File: ${fileName}</p>
            <p><a href="/download/${sessionId}">Click here to download via RGTP</a></p>
            <p>Or connect directly to RGTP port: ${this.options.rgtpPort + this.rgtpSessions.size}</p>
          </body>
        </html>
      `);
      
    } catch (error) {
      this.sendError(res, 404, 'File Not Found');
    }
  }
  
  async handlePOST(req, res, pathname) {
    if (pathname === '/upload') {
      await this.handleUpload(req, res);
    } else {
      this.sendError(res, 404, 'Endpoint Not Found');
    }
  }
  
  async handleUpload(req, res) {
    const boundary = this.parseBoundary(req.headers['content-type']);
    if (!boundary) {
      this.sendError(res, 400, 'Invalid Content-Type');
      return;
    }
    
    const uploadDir = path.join(process.cwd(), 'uploads');
    await fs.mkdir(uploadDir, { recursive: true });
    
    const fileName = `upload_${Date.now()}_${Math.random().toString(36).substr(2, 9)}`;
    const filePath = path.join(uploadDir, fileName);
    
    // Stream upload to file (simulating RGTP receive)
    const writeStream = fs.createWriteStream(filePath);
    
    req.pipe(writeStream);
    
    writeStream.on('finish', async () => {
      try {
        // Create RGTP client to "pull" the uploaded file
        const client = new rgtp.Client();
        const sessionId = this.generateSessionId();
        
        this.activeTransfers.set(sessionId, {
          fileName,
          filePath,
          client,
          upload: true,
          startTime: Date.now()
        });
        
        res.writeHead(200, { 'Content-Type': 'application/json' });
        res.end(JSON.stringify({
          success: true,
          fileId: sessionId,
          fileName: fileName,
          size: (await fs.stat(filePath)).size,
          rgtpPort: this.options.rgtpPort
        }));
        
      } catch (error) {
        this.sendError(res, 500, 'Upload Processing Failed');
      }
    });
    
    writeStream.on('error', (error) => {
      this.sendError(res, 500, 'Upload Failed');
    });
  }
  
  async serveFileListing(res) {
    try {
      const files = await fs.readdir(process.cwd());
      const fileList = await Promise.all(
        files
          .filter(f => !f.startsWith('.') && !fs.statSync(f).isDirectory())
          .map(async (f) => {
            const stat = await fs.stat(f);
            return {
              name: f,
              size: stat.size,
              modified: stat.mtime.toISOString()
            };
          })
      );
      
      res.writeHead(200, { 'Content-Type': 'text/html' });
      res.end(`
        <!DOCTYPE html>
        <html>
          <head>
            <title>RGTP File Server</title>
            <style>
              body { font-family: Arial, sans-serif; margin: 40px; }
              table { border-collapse: collapse; width: 100%; }
              th, td { border: 1px solid #ddd; padding: 12px; text-align: left; }
              th { background-color: #f2f2f2; }
              tr:hover { background-color: #f5f5f5; }
              a { color: #007bff; text-decoration: none; }
              a:hover { text-decoration: underline; }
            </style>
          </head>
          <body>
            <h1>RGTP File Server</h1>
            <p>Serving files over UDP protocol with HTTP interface</p>
            <table>
              <thead>
                <tr>
                  <th>File Name</th>
                  <th>Size</th>
                  <th>Last Modified</th>
                  <th>Action</th>
                </tr>
              </thead>
              <tbody>
                ${fileList.map(file => `
                  <tr>
                    <td>${file.name}</td>
                    <td>${rgtp.formatBytes(file.size)}</td>
                    <td>${new Date(file.modified).toLocaleString()}</td>
                    <td><a href="/${file.name}">Download via RGTP</a></td>
                  </tr>
                `).join('')}
              </tbody>
            </table>
            <hr>
            <h2>Upload File</h2>
            <form action="/upload" method="post" enctype="multipart/form-data">
              <input type="file" name="file" required>
              <button type="submit">Upload via RGTP</button>
            </form>
          </body>
        </html>
      `);
    } catch (error) {
      this.sendError(res, 500, 'Failed to read directory');
    }
  }
  
  parseBoundary(contentType) {
    const match = contentType.match(/boundary=(.+)$/);
    return match ? match[1] : null;
  }
  
  sendError(res, statusCode, message) {
    res.writeHead(statusCode, { 'Content-Type': 'application/json' });
    res.end(JSON.stringify({ error: message, code: statusCode }));
  }
  
  generateSessionId() {
    return Math.random().toString(36).substr(2, 9);
  }
  
  async stop() {
    if (this.server) {
      this.server.close();
    }
    
    // Close all RGTP sessions
    for (const [id, session] of this.rgtpSessions) {
      try {
        session.close();
      } catch (error) {
        // Ignore cleanup errors
      }
    }
    
    this.rgtpSessions.clear();
    this.activeTransfers.clear();
  }
}

/**
 * Web3/IPFS-style Interface for RGTP
 */
class RGTPWeb3Interface {
  constructor(options = {}) {
    this.options = {
      port: options.port || 5001,
      ...options
    };
    
    this.cidMap = new Map(); // CID -> file mapping
    this.fileMap = new Map(); // file -> CID mapping
  }
  
  async add(filePath) {
    try {
      const data = await fs.readFile(filePath);
      const cid = this.generateCID(data);
      
      // Store mapping
      this.cidMap.set(cid, filePath);
      this.fileMap.set(filePath, cid);
      
      // In a real implementation, this would use RGTP to distribute
      console.log(`Added ${filePath} with CID: ${cid}`);
      
      return {
        cid: cid,
        path: filePath,
        size: data.length
      };
    } catch (error) {
      throw new Error(`Failed to add file: ${error.message}`);
    }
  }
  
  async get(cid, outputPath) {
    const filePath = this.cidMap.get(cid);
    if (!filePath) {
      throw new Error(`CID ${cid} not found`);
    }
    
    try {
      // In a real implementation, this would use RGTP to fetch from network
      await fs.copyFile(filePath, outputPath);
      console.log(`Retrieved ${cid} to ${outputPath}`);
      return outputPath;
    } catch (error) {
      throw new Error(`Failed to retrieve file: ${error.message}`);
    }
  }
  
  async ls() {
    const entries = [];
    for (const [cid, filePath] of this.cidMap) {
      const stat = await fs.stat(filePath);
      entries.push({
        cid: cid,
        name: path.basename(filePath),
        size: stat.size,
        added: stat.birthtime.toISOString()
      });
    }
    return entries;
  }
  
  generateCID(data) {
    // Simplified CID generation - in reality would use proper hashing
    const hash = require('crypto').createHash('sha256');
    hash.update(data);
    return `Qm${hash.digest('hex').substring(0, 44)}`;
  }
}

// Export both adapters
module.exports = {
  RGTPHTTPAdapter,
  RGTPWeb3Interface,
  // Backwards compatibility
  HTTPAdapter: RGTPHTTPAdapter,
  Web3Interface: RGTPWeb3Interface
};