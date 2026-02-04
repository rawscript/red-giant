const EventEmitter = require('events');
const fs = require('fs').promises;
const path = require('path');

// Load native addon
let rgtp;
try {
  rgtp = require('./build/Release/rgtp.node');
} catch (err) {
  // Fallback to debug build
  rgtp = require('./build/Debug/rgtp.node');
}

class RGTPSession extends EventEmitter {
  constructor(options = {}) {
    super();
    this.options = {
      port: options.port || 0,
      chunkSize: options.chunkSize || 1024 * 1024,
      exposureRate: options.exposureRate || 1000,
      adaptiveMode: options.adaptiveMode !== false,
      enableCompression: options.enableCompression || false,
      enableEncryption: options.enableEncryption || false,
      timeout: options.timeout || 30000,
      ...options
    };
    
    this.sessionHandle = null;
    this.isClosed = false;
    this.stats = {
      bytesTransferred: 0,
      avgThroughputMbps: 0,
      startTime: Date.now()
    };
    
    this._init();
  }
  
  _init() {
    const result = rgtp.init();
    if (result !== 0) {
      throw new Error(`Failed to initialize RGTP: ${result}`);
    }
    
    this.socket = rgtp.socket();
    if (this.socket < 0) {
      throw new Error('Failed to create UDP socket');
    }
    
    if (this.options.port > 0) {
      const bindResult = rgtp.bind(this.socket, this.options.port);
      if (bindResult !== 0) {
        throw new Error(`Failed to bind to port ${this.options.port}`);
      }
    }
  }
  
  async exposeFile(filePath) {
    if (this.isClosed) {
      throw new Error('Session is closed');
    }
    
    try {
      const data = await fs.readFile(filePath);
      this.surface = rgtp.exposeData(data, this.socket);
      
      if (this.surface === -1) {
        throw new Error('Failed to expose file');
      }
      
      this.emit('exposeStart', filePath, data.length);
      
      // Poll for events
      this._startPolling();
      
    } catch (error) {
      this.emit('error', error);
      throw error;
    }
  }
  
  _startPolling() {
    if (this.isClosed || !this.surface) return;
    
    const poll = () => {
      if (this.isClosed) return;
      
      const result = rgtp.poll(this.surface, 100);
      
      if (result > 0) {
        // Transfer activity detected
        this.stats.bytesTransferred += result;
        this._updateStats();
        this.emit('progress', this.stats.bytesTransferred, this.stats.totalSize || 0);
      } else if (result < 0) {
        // Error occurred
        this.emit('error', new Error(`Poll error: ${result}`));
        return;
      }
      
      // Continue polling
      setTimeout(poll, 10);
    };
    
    poll();
  }
  
  _updateStats() {
    const elapsed = (Date.now() - this.stats.startTime) / 1000;
    this.stats.avgThroughputMbps = (this.stats.bytesTransferred / 1024 / 1024 / elapsed) || 0;
  }
  
  async waitComplete() {
    return new Promise((resolve) => {
      const checkComplete = () => {
        if (this.isClosed) {
          resolve();
          return;
        }
        
        // Simulate completion check
        setTimeout(checkComplete, 100);
      };
      
      checkComplete();
    });
  }
  
  async getStats() {
    return { ...this.stats };
  }
  
  close() {
    if (this.isClosed) return;
    
    this.isClosed = true;
    
    if (this.surface) {
      rgtp.destroySurface(this.surface);
      this.surface = null;
    }
    
    if (this.socket) {
      // Close socket (would need platform-specific implementation)
      this.socket = null;
    }
    
    rgtp.cleanup();
    this.emit('close');
  }
}

class RGTPClient extends EventEmitter {
  constructor(options = {}) {
    super();
    this.options = {
      timeout: options.timeout || 30000,
      chunkSize: options.chunkSize || 1024 * 1024,
      adaptiveMode: options.adaptiveMode !== false,
      ...options
    };
    
    this.socket = null;
    this.surface = null;
    this.isClosed = false;
    this.stats = {
      bytesTransferred: 0,
      avgThroughputMbps: 0,
      startTime: Date.now()
    };
    
    this._init();
  }
  
  _init() {
    const result = rgtp.init();
    if (result !== 0) {
      throw new Error(`Failed to initialize RGTP: ${result}`);
    }
    
    this.socket = rgtp.socket();
    if (this.socket < 0) {
      throw new Error('Failed to create UDP socket');
    }
  }
  
  async pullToFile(host, port, outputPath) {
    if (this.isClosed) {
      throw new Error('Client is closed');
    }
    
    try {
      // In a real implementation, this would connect to the host
      // and pull the file using the RGTP protocol
      
      this.emit('pullStart', host, port, outputPath);
      
      // Simulate file download for demonstration
      const fakeData = Buffer.alloc(1024 * 1024, 'RGTP'); // 1MB fake data
      await fs.writeFile(outputPath, fakeData);
      
      this.stats.bytesTransferred = fakeData.length;
      this._updateStats();
      
      this.emit('progress', this.stats.bytesTransferred, fakeData.length);
      this.emit('pullComplete', outputPath);
      
    } catch (error) {
      this.emit('error', error);
      throw error;
    }
  }
  
  _updateStats() {
    const elapsed = (Date.now() - this.stats.startTime) / 1000;
    this.stats.avgThroughputMbps = (this.stats.bytesTransferred / 1024 / 1024 / elapsed) || 0;
  }
  
  async getStats() {
    return { ...this.stats };
  }
  
  close() {
    if (this.isClosed) return;
    
    this.isClosed = true;
    
    if (this.surface) {
      rgtp.destroySurface(this.surface);
      this.surface = null;
    }
    
    if (this.socket) {
      this.socket = null;
    }
    
    rgtp.cleanup();
    this.emit('close');
  }
}

// Convenience functions
async function sendFile(filePath, options = {}) {
  const session = new RGTPSession(options);
  
  try {
    await session.exposeFile(filePath);
    await session.waitComplete();
    return await session.getStats();
  } finally {
    session.close();
  }
}

async function receiveFile(host, port, outputPath, options = {}) {
  const client = new RGTPClient(options);
  
  try {
    await client.pullToFile(host, port, outputPath);
    return await client.getStats();
  } finally {
    client.close();
  }
}

// Configuration presets
function createLANConfig() {
  return {
    chunkSize: 2 * 1024 * 1024,  // 2MB chunks for high bandwidth
    adaptiveMode: true,
    timeout: 10000
  };
}

function createWANConfig() {
  return {
    chunkSize: 512 * 1024,  // 512KB chunks for variable bandwidth
    adaptiveMode: true,
    timeout: 30000
  };
}

function createMobileConfig() {
  return {
    chunkSize: 256 * 1024,  // 256KB chunks for limited bandwidth
    adaptiveMode: true,
    timeout: 60000
  };
}

// Utility functions
function formatBytes(bytes) {
  if (bytes === 0) return '0 Bytes';
  
  const k = 1024;
  const sizes = ['Bytes', 'KB', 'MB', 'GB', 'TB'];
  const i = Math.floor(Math.log(bytes) / Math.log(k));
  
  return parseFloat((bytes / Math.pow(k, i)).toFixed(2)) + ' ' + sizes[i];
}

// Export everything
module.exports = {
  Session: RGTPSession,
  Client: RGTPClient,
  sendFile,
  receiveFile,
  createLANConfig,
  createWANConfig,
  createMobileConfig,
  formatBytes,
  // Native functions for advanced usage
  init: rgtp.init,
  cleanup: rgtp.cleanup,
  getVersion: rgtp.getVersion,
  socket: rgtp.socket,
  bind: rgtp.bind
};