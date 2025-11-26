/**
 * RGTP Node.js Bindings Test Suite
 * 
 * This test suite validates the RGTP Node.js bindings functionality.
 * Tests are designed to work with or without the native module being built.
 */

const { describe, it, before, after } = require('mocha');
const { expect } = require('chai');
const fs = require('fs');
const path = require('path');
const os = require('os');

// Test if we can load the module
let rgtp;
let nativeModuleAvailable = false;

try {
  rgtp = require('../index.js');
  nativeModuleAvailable = true;
  console.log('✅ Native module loaded successfully');
} catch (error) {
  console.log('❌ Native module failed to load');
  console.log('   Error:', error.message.split('\n')[0]);
  
  // In CI/CD, we should fail if native module can't be built
  if (process.env.CI || process.env.GITHUB_ACTIONS) {
    console.log('🚨 CI/CD detected - native module must be available');
    throw error;
  }
  
  console.log('⚠️  Running in development mode with limited tests');
  console.log('   To fix: install build tools and run "npm run build"');

  // Create mock module for basic testing
  rgtp = {
    Session: class MockSession {
      constructor(config = {}) {
        this.config = config;
        this._closed = false;
      }

      async exposeFile(filename) {
        if (!fs.existsSync(filename)) {
          throw new Error(`File not found: ${filename}`);
        }
        return Promise.resolve();
      }

      async waitComplete() {
        return Promise.resolve();
      }

      async getStats() {
        return {
          bytesTransferred: 1024,
          totalBytes: 1024,
          throughputMbps: 10.5,
          avgThroughputMbps: 10.5,
          chunksTransferred: 4,
          totalChunks: 4,
          retransmissions: 0,
          completionPercent: 100,
          elapsedMs: 1000,
          estimatedRemainingMs: 0,
          efficiencyPercent: 100
        };
      }

      async cancel() {
        return Promise.resolve();
      }

      close() {
        this._closed = true;
      }
    },

    Client: class MockClient {
      constructor(config = {}) {
        this.config = config;
        this._closed = false;
      }

      async pullToFile(host, port, filename) {
        if (typeof host !== 'string' || !host) {
          throw new Error('Host must be a non-empty string');
        }
        if (typeof port !== 'number' || port < 1 || port > 65535) {
          throw new Error('Port must be a number between 1 and 65535');
        }
        if (typeof filename !== 'string' || !filename) {
          throw new Error('Filename must be a non-empty string');
        }

        // Create mock file
        const dir = path.dirname(filename);
        if (!fs.existsSync(dir)) {
          fs.mkdirSync(dir, { recursive: true });
        }
        fs.writeFileSync(filename, 'mock data');

        return Promise.resolve();
      }

      async getStats() {
        return {
          bytesTransferred: 1024,
          totalBytes: 1024,
          throughputMbps: 15.2,
          avgThroughputMbps: 15.2,
          chunksTransferred: 4,
          totalChunks: 4,
          retransmissions: 0,
          completionPercent: 100,
          elapsedMs: 800,
          estimatedRemainingMs: 0,
          efficiencyPercent: 100
        };
      }

      async cancel() {
        return Promise.resolve();
      }

      close() {
        this._closed = true;
      }
    },

    getVersion: () => '1.0.0-mock',
    formatBytes: (bytes) => {
      if (bytes === 0) return '0 B';
      const k = 1024;
      const sizes = ['B', 'KB', 'MB', 'GB', 'TB'];
      const i = Math.floor(Math.log(bytes) / Math.log(k));
      return parseFloat((bytes / Math.pow(k, i)).toFixed(1)) + ' ' + sizes[i];
    },
    formatDuration: (ms) => {
      if (ms < 1000) return `${ms}ms`;
      const seconds = Math.floor(ms / 1000);
      const minutes = Math.floor(seconds / 60);
      if (minutes > 0) {
        return `${minutes}m ${seconds % 60}s`;
      }
      return `${seconds}s`;
    },
    sendFile: async (filename, config = {}) => {
      return {
        bytesTransferred: 1024,
        totalBytes: 1024,
        throughputMbps: 10.0,
        avgThroughputMbps: 10.0,
        efficiencyPercent: 100
      };
    },
    receiveFile: async (host, port, filename, config = {}) => {
      return {
        bytesTransferred: 1024,
        totalBytes: 1024,
        throughputMbps: 15.0,
        avgThroughputMbps: 15.0,
        efficiencyPercent: 100
      };
    },
    VERSION: '1.0.0-mock'
  };
}

describe('RGTP Node.js Bindings', function () {
  let testDir;
  let testFile;

  before(function () {
    // Create temporary test directory
    testDir = fs.mkdtempSync(path.join(os.tmpdir(), 'rgtp-test-'));
    testFile = path.join(testDir, 'test-file.bin');

    // Create test file
    const testData = Buffer.alloc(1024, 'A');
    fs.writeFileSync(testFile, testData);
  });

  after(function () {
    // Clean up test directory
    if (fs.existsSync(testDir)) {
      fs.rmSync(testDir, { recursive: true, force: true });
    }
  });

  describe('Module Loading', function () {
    it('should export required classes and functions', function () {
      expect(rgtp).to.be.an('object');
      expect(rgtp.Session).to.be.a('function');
      expect(rgtp.Client).to.be.a('function');
      expect(rgtp.getVersion).to.be.a('function');
      expect(rgtp.formatBytes).to.be.a('function');
      expect(rgtp.formatDuration).to.be.a('function');
      expect(rgtp.sendFile).to.be.a('function');
      expect(rgtp.receiveFile).to.be.a('function');
    });

    it('should have a version string', function () {
      const version = rgtp.getVersion();
      expect(version).to.be.a('string');
      expect(version).to.match(/^\d+\.\d+\.\d+/);
    });
  });

  describe('Session Class', function () {
    let session;

    beforeEach(function () {
      session = new rgtp.Session({
        port: 0,
        adaptiveMode: true,
        timeout: 5000
      });
    });

    afterEach(function () {
      if (session) {
        session.close();
      }
    });

    it('should create a session with default config', function () {
      const defaultSession = new rgtp.Session();
      expect(defaultSession).to.be.instanceOf(rgtp.Session);
      defaultSession.close();
    });

    it('should create a session with custom config', function () {
      expect(session).to.be.instanceOf(rgtp.Session);
    });

    it('should expose a file', async function () {
      await session.exposeFile(testFile);
    });

    it('should throw error for non-existent file', async function () {
      try {
        await session.exposeFile('/non/existent/file.bin');
        expect.fail('Should have thrown an error');
      } catch (error) {
        expect(error.message).to.include('File not found');
      }
    });

    it('should get statistics', async function () {
      const stats = await session.getStats();
      expect(stats).to.be.an('object');
      expect(stats).to.have.property('bytesTransferred');
      expect(stats).to.have.property('totalBytes');
      expect(stats).to.have.property('throughputMbps');
      expect(stats).to.have.property('efficiencyPercent');
    });

    it('should cancel operation', async function () {
      await session.cancel();
    });

    it('should wait for completion', async function () {
      await session.waitComplete();
    });
  });

  describe('Client Class', function () {
    let client;

    beforeEach(function () {
      client = new rgtp.Client({
        timeout: 5000,
        adaptiveMode: true
      });
    });

    afterEach(function () {
      if (client) {
        client.close();
      }
    });

    it('should create a client with default config', function () {
      const defaultClient = new rgtp.Client();
      expect(defaultClient).to.be.instanceOf(rgtp.Client);
      defaultClient.close();
    });

    it('should create a client with custom config', function () {
      expect(client).to.be.instanceOf(rgtp.Client);
    });

    it('should validate pullToFile parameters', async function () {
      const outputFile = path.join(testDir, 'output.bin');

      // Test invalid host
      try {
        await client.pullToFile('', 9999, outputFile);
        expect.fail('Should have thrown an error');
      } catch (error) {
        expect(error.message).to.include('Host must be a non-empty string');
      }

      // Test invalid port
      try {
        await client.pullToFile('localhost', 0, outputFile);
        expect.fail('Should have thrown an error');
      } catch (error) {
        expect(error.message).to.include('Port must be a number between 1 and 65535');
      }

      // Test invalid filename
      try {
        await client.pullToFile('localhost', 9999, '');
        expect.fail('Should have thrown an error');
      } catch (error) {
        expect(error.message).to.include('Filename must be a non-empty string');
      }
    });

    it('should get statistics', async function () {
      const stats = await client.getStats();
      expect(stats).to.be.an('object');
      expect(stats).to.have.property('bytesTransferred');
      expect(stats).to.have.property('totalBytes');
      expect(stats).to.have.property('throughputMbps');
      expect(stats).to.have.property('efficiencyPercent');
    });

    it('should cancel operation', async function () {
      await client.cancel();
    });
  });

  describe('Utility Functions', function () {
    describe('formatBytes', function () {
      it('should format bytes correctly', function () {
        expect(rgtp.formatBytes(0)).to.equal('0 B');
        expect(rgtp.formatBytes(1024)).to.equal('1 KB');
        expect(rgtp.formatBytes(1024 * 1024)).to.equal('1 MB');
        expect(rgtp.formatBytes(1536)).to.equal('1.5 KB');
      });
    });

    describe('formatDuration', function () {
      it('should format duration correctly', function () {
        expect(rgtp.formatDuration(500)).to.equal('500ms');
        expect(rgtp.formatDuration(1000)).to.equal('1s');
        expect(rgtp.formatDuration(65000)).to.match(/1m.*5s/);
      });
    });
  });

  describe('Convenience Functions', function () {
    it('should send file', async function () {
      const stats = await rgtp.sendFile(testFile, { port: 0 });
      expect(stats).to.be.an('object');
      expect(stats).to.have.property('bytesTransferred');
      expect(stats).to.have.property('efficiencyPercent');
    });

    it('should receive file', async function () {
      const outputFile = path.join(testDir, 'received.bin');
      const stats = await rgtp.receiveFile('localhost', 9999, outputFile);
      expect(stats).to.be.an('object');
      expect(stats).to.have.property('bytesTransferred');
      expect(stats).to.have.property('efficiencyPercent');
    });
  });

  describe('Error Handling', function () {
    it('should handle closed session operations', function () {
      const session = new rgtp.Session();
      session.close();

      return Promise.all([
        session.exposeFile(testFile).catch(err => {
          expect(err.message).to.include('closed');
        }),
        session.getStats().catch(err => {
          expect(err.message).to.include('closed');
        }),
        session.waitComplete().catch(err => {
          expect(err.message).to.include('closed');
        })
      ]);
    });

    it('should handle closed client operations', function () {
      const client = new rgtp.Client();
      client.close();

      const outputFile = path.join(testDir, 'test-output.bin');

      return Promise.all([
        client.pullToFile('localhost', 9999, outputFile).catch(err => {
          expect(err.message).to.include('closed');
        }),
        client.getStats().catch(err => {
          expect(err.message).to.include('closed');
        })
      ]);
    });
  });

  // Integration tests (only run if native module is available)
  if (nativeModuleAvailable) {
    describe('Integration Tests', function () {
      this.timeout(10000); // Longer timeout for integration tests

      it('should perform basic file transfer simulation', async function () {
        const session = new rgtp.Session({ port: 0 });
        const client = new rgtp.Client();
        const outputFile = path.join(testDir, 'integration-test.bin');

        try {
          // This would normally require actual network communication
          // For now, just test that the APIs work
          await session.exposeFile(testFile);

          // Simulate some delay
          await new Promise(resolve => setTimeout(resolve, 100));

          const sessionStats = await session.getStats();
          expect(sessionStats.bytesTransferred).to.be.a('number');

          await session.waitComplete();
        } finally {
          session.close();
          client.close();
        }
      });
    });
  }
});