/**
 * Red Giant Transport Protocol (RGTP) - Node.js Bindings
 * 
 * This module provides Node.js bindings for the Red Giant Transport Protocol,
 * a revolutionary Layer 4 transport protocol that implements exposure-based
 * data transmission.
 * 
 * Key Features:
 * - Natural multicast support (one exposure serves unlimited receivers)
 * - Instant resume capability (stateless chunk requests)
 * - Adaptive flow control (exposure rate matches receiver capacity)
 * - No head-of-line blocking (pull chunks out of order)
 * - Superior packet loss resilience
 * 
 * @example
 * ```javascript
 * const rgtp = require('rgtp');
 * 
 * // Expose a file
 * const session = new rgtp.Session({
 *   port: 9999,
 *   adaptiveMode: true,
 *   onProgress: (bytesTransferred, totalBytes) => {
 *     console.log(`Progress: ${(bytesTransferred/totalBytes*100).toFixed(1)}%`);
 *   }
 * });
 * 
 * await session.exposeFile('large_file.bin');
 * await session.waitComplete();
 * session.close();
 * 
 * // Pull a file
 * const client = new rgtp.Client({
 *   timeout: 30000,
 *   onProgress: (bytesTransferred, totalBytes) => {
 *     console.log(`Downloaded: ${bytesTransferred}/${totalBytes} bytes`);
 *   }
 * });
 * 
 * await client.pullToFile('192.168.1.100', 9999, 'downloaded.bin');
 * client.close();
 * ```
 */

const native = require('./build/Release/rgtp_native');
const { EventEmitter } = require('events');
const path = require('path');
const fs = require('fs');

/**
 * RGTP Session for exposing data
 * 
 * A session represents a server-side entity that exposes data for clients
 * to pull. Multiple clients can pull from the same session simultaneously.
 */
class Session extends EventEmitter {
  /**
   * Create a new RGTP session
   * 
   * @param {Object} [config] - Configuration options
   * @param {number} [config.chunkSize] - Chunk size in bytes (0 = auto)
   * @param {number} [config.exposureRate] - Initial exposure rate (chunks/sec)
   * @param {boolean} [config.adaptiveMode=true] - Enable adaptive rate control
   * @param {number} [config.port=0] - Port number (0 = auto)
   * @param {number} [config.timeout=30000] - Operation timeout in milliseconds
   * @param {Function} [config.onProgress] - Progress callback (bytesTransferred, totalBytes)
   * @param {Function} [config.onError] - Error callback (errorCode, errorMessage)
   */
  constructor(config = {}) {
    super();
    
    // Set up event forwarding from native callbacks
    const wrappedConfig = { ...config };
    
    if (config.onProgress) {
      wrappedConfig.onProgress = (bytesTransferred, totalBytes) => {
        this.emit('progress', bytesTransferred, totalBytes);
        if (config.onProgress) {
          config.onProgress(bytesTransferred, totalBytes);
        }
      };
    }
    
    if (config.onError) {
      wrappedConfig.onError = (errorCode, errorMessage) => {
        this.emit('error', new Error(`RGTP Error ${errorCode}: ${errorMessage}`));
        if (config.onError) {
          config.onError(errorCode, errorMessage);
        }
      };
    }
    
    this._native = new native.Session(wrappedConfig);
    this._closed = false;
  }

  /**
   * Expose a file for pulling
   * 
   * @param {string} filename - Path to file to expose
   * @returns {Promise<void>}
   */
  async exposeFile(filename) {
    if (this._closed) {
      throw new Error('Session is closed');
    }
    
    // Verify file exists
    if (!fs.existsSync(filename)) {
      throw new Error(`File not found: ${filename}`);
    }
    
    return this._native.exposeFile(filename);
  }

  /**
   * Wait for exposure to complete
   * 
   * @returns {Promise<void>}
   */
  async waitComplete() {
    if (this._closed) {
      throw new Error('Session is closed');
    }
    
    return this._native.waitComplete();
  }

  /**
   * Get current transfer statistics
   * 
   * @returns {Promise<Object>} Statistics object
   */
  async getStats() {
    if (this._closed) {
      throw new Error('Session is closed');
    }
    
    const stats = this._native.getStats();
    
    // Add computed properties
    stats.efficiencyPercent = stats.chunksTransferred > 0 
      ? (stats.chunksTransferred / (stats.chunksTransferred + stats.retransmissions)) * 100
      : 100;
    
    return stats;
  }

  /**
   * Cancel ongoing exposure
   * 
   * @returns {Promise<void>}
   */
  async cancel() {
    if (this._closed) {
      return;
    }
    
    return this._native.cancel();
  }

  /**
   * Close the session and free resources
   */
  close() {
    if (!this._closed) {
      this._native.close();
      this._closed = true;
      this.emit('close');
    }
  }
}

/**
 * RGTP Client for pulling data
 * 
 * A client connects to sessions and pulls exposed data.
 */
class Client extends EventEmitter {
  /**
   * Create a new RGTP client
   * 
   * @param {Object} [config] - Configuration options
   * @param {number} [config.chunkSize] - Chunk size in bytes (0 = auto)
   * @param {boolean} [config.adaptiveMode=true] - Enable adaptive mode
   * @param {number} [config.timeout=30000] - Operation timeout in milliseconds
   * @param {Function} [config.onProgress] - Progress callback (bytesTransferred, totalBytes)
   * @param {Function} [config.onError] - Error callback (errorCode, errorMessage)
   */
  constructor(config = {}) {
    super();
    
    // Set up event forwarding from native callbacks
    const wrappedConfig = { ...config };
    
    if (config.onProgress) {
      wrappedConfig.onProgress = (bytesTransferred, totalBytes) => {
        this.emit('progress', bytesTransferred, totalBytes);
        if (config.onProgress) {
          config.onProgress(bytesTransferred, totalBytes);
        }
      };
    }
    
    if (config.onError) {
      wrappedConfig.onError = (errorCode, errorMessage) => {
        this.emit('error', new Error(`RGTP Error ${errorCode}: ${errorMessage}`));
        if (config.onError) {
          config.onError(errorCode, errorMessage);
        }
      };
    }
    
    this._native = new native.Client(wrappedConfig);
    this._closed = false;
  }

  /**
   * Pull data from remote host and save to file
   * 
   * @param {string} host - Remote host address
   * @param {number} port - Remote port number
   * @param {string} filename - Output filename
   * @returns {Promise<void>}
   */
  async pullToFile(host, port, filename) {
    if (this._closed) {
      throw new Error('Client is closed');
    }
    
    // Validate parameters
    if (typeof host !== 'string' || !host) {
      throw new Error('Host must be a non-empty string');
    }
    
    if (typeof port !== 'number' || port < 1 || port > 65535) {
      throw new Error('Port must be a number between 1 and 65535');
    }
    
    if (typeof filename !== 'string' || !filename) {
      throw new Error('Filename must be a non-empty string');
    }
    
    // Ensure output directory exists
    const dir = path.dirname(filename);
    if (!fs.existsSync(dir)) {
      fs.mkdirSync(dir, { recursive: true });
    }
    
    return this._native.pullToFile(host, port, filename);
  }

  /**
   * Get current transfer statistics
   * 
   * @returns {Promise<Object>} Statistics object
   */
  async getStats() {
    if (this._closed) {
      throw new Error('Client is closed');
    }
    
    const stats = this._native.getStats();
    
    // Add computed properties
    stats.efficiencyPercent = stats.chunksTransferred > 0 
      ? (stats.chunksTransferred / (stats.chunksTransferred + stats.retransmissions)) * 100
      : 100;
    
    return stats;
  }

  /**
   * Cancel ongoing pull operation
   * 
   * @returns {Promise<void>}
   */
  async cancel() {
    if (this._closed) {
      return;
    }
    
    return this._native.cancel();
  }

  /**
   * Close the client and free resources
   */
  close() {
    if (!this._closed) {
      this._native.close();
      this._closed = true;
      this.emit('close');
    }
  }
}

/**
 * Utility functions
 */

/**
 * Get RGTP library version
 * 
 * @returns {string} Version string
 */
function getVersion() {
  return native.getVersion();
}

/**
 * Format bytes for display
 * 
 * @param {number} bytes - Number of bytes
 * @returns {string} Formatted string (e.g., "1.5 MB")
 */
function formatBytes(bytes) {
  if (bytes === 0) return '0 B';
  
  const k = 1024;
  const sizes = ['B', 'KB', 'MB', 'GB', 'TB'];
  const i = Math.floor(Math.log(bytes) / Math.log(k));
  
  return parseFloat((bytes / Math.pow(k, i)).toFixed(1)) + ' ' + sizes[i];
}

/**
 * Format duration for display
 * 
 * @param {number} milliseconds - Duration in milliseconds
 * @returns {string} Formatted string (e.g., "1m 30s")
 */
function formatDuration(milliseconds) {
  if (milliseconds < 1000) {
    return `${milliseconds}ms`;
  }
  
  const seconds = Math.floor(milliseconds / 1000);
  const minutes = Math.floor(seconds / 60);
  const hours = Math.floor(minutes / 60);
  
  if (hours > 0) {
    return `${hours}h ${minutes % 60}m ${seconds % 60}s`;
  } else if (minutes > 0) {
    return `${minutes}m ${seconds % 60}s`;
  } else {
    return `${seconds}s`;
  }
}

/**
 * Convenience functions
 */

/**
 * Send a file using RGTP (convenience function)
 * 
 * @param {string} filename - File to send
 * @param {Object} [config] - Configuration options
 * @returns {Promise<Object>} Final statistics
 */
async function sendFile(filename, config = {}) {
  const session = new Session(config);
  
  try {
    await session.exposeFile(filename);
    await session.waitComplete();
    return await session.getStats();
  } finally {
    session.close();
  }
}

/**
 * Receive a file using RGTP (convenience function)
 * 
 * @param {string} host - Remote host
 * @param {number} port - Remote port
 * @param {string} filename - Output filename
 * @param {Object} [config] - Configuration options
 * @returns {Promise<Object>} Final statistics
 */
async function receiveFile(host, port, filename, config = {}) {
  const client = new Client(config);
  
  try {
    await client.pullToFile(host, port, filename);
    return await client.getStats();
  } finally {
    client.close();
  }
}

// Export everything
module.exports = {
  Session,
  Client,
  getVersion,
  formatBytes,
  formatDuration,
  sendFile,
  receiveFile,
  
  // Configuration helpers
  createLANConfig: native.createLANConfig,
  createWANConfig: native.createWANConfig,
  createMobileConfig: native.createMobileConfig,
  
  // Constants
  VERSION: getVersion()
};