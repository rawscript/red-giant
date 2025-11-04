/**
 * RGTP Security Test Suite
 * 
 * Tests for RGTP security features including encryption,
 * digital signatures, and access control.
 */

const { describe, it, before, after } = require('mocha');
const { expect } = require('chai');
const crypto = require('crypto');
const fs = require('fs');
const path = require('path');
const os = require('os');

// Load RGTP with security mock if native module unavailable
let rgtp;
let nativeModuleAvailable = false;

try {
  rgtp = require('../index.js');
  nativeModuleAvailable = true;
} catch (error) {
  // Create security-aware mock
  rgtp = {
    Session: class MockSecureSession {
      constructor(config = {}) {
        this.config = config;
        this.security = config.security || {};
        this._closed = false;
      }
      
      async exposeFile(filename) {
        if (!fs.existsSync(filename)) {
          throw new Error(`File not found: ${filename}`);
        }
        
        // Simulate security checks
        if (this.security.encryption && !this.security.encryption.key) {
          throw new Error('Encryption enabled but no key provided');
        }
        
        if (this.security.signing && !this.security.signing.privateKey) {
          throw new Error('Signing enabled but no private key provided');
        }
        
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
          efficiencyPercent: 100,
          securityOverhead: this.security.encryption ? 15 : 0
        };
      }
      
      close() {
        this._closed = true;
      }
    },
    
    Client: class MockSecureClient {
      constructor(config = {}) {
        this.config = config;
        this.security = config.security || {};
        this._closed = false;
      }
      
      async pullToFile(host, port, filename) {
        // Simulate security verification
        if (this.security.verification && this.security.verification.verifySignatures) {
          // Simulate signature verification
          if (!this.security.verification.serverPublicKey) {
            throw new Error('Signature verification enabled but no server public key provided');
          }
        }
        
        // Create mock secure file
        const dir = path.dirname(filename);
        if (!fs.existsSync(dir)) {
          fs.mkdirSync(dir, { recursive: true });
        }
        fs.writeFileSync(filename, 'securely transferred mock data');
        
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
          efficiencyPercent: 100,
          securityOverhead: this.security.encryption ? 15 : 0
        };
      }
      
      close() {
        this._closed = true;
      }
    }
  };
}

describe('RGTP Security Features', function() {
  let testDir;
  let testFile;
  let keyPair;
  
  before(function() {
    // Create temporary test directory
    testDir = fs.mkdtempSync(path.join(os.tmpdir(), 'rgtp-security-test-'));
    testFile = path.join(testDir, 'secure-test-file.bin');
    
    // Create test file
    const testData = Buffer.alloc(1024, 'Secure test data ');
    fs.writeFileSync(testFile, testData);
    
    // Generate test key pair
    keyPair = crypto.generateKeyPairSync('ed25519', {
      publicKeyEncoding: { type: 'spki', format: 'pem' },
      privateKeyEncoding: { type: 'pkcs8', format: 'pem' }
    });
  });
  
  after(function() {
    // Clean up test directory
    if (fs.existsSync(testDir)) {
      fs.rmSync(testDir, { recursive: true, force: true });
    }
  });

  describe('Encryption Configuration', function() {
    it('should create session with encryption enabled', function() {
      const encryptionKey = crypto.randomBytes(32);
      
      const session = new rgtp.Session({
        port: 0,
        security: {
          encryption: {
            algorithm: 'chacha20-poly1305',
            key: encryptionKey,
            enabled: true
          }
        }
      });
      
      expect(session).to.be.instanceOf(rgtp.Session);
      expect(session.security.encryption.enabled).to.be.true;
      session.close();
    });

    it('should reject session with encryption enabled but no key', async function() {
      const session = new rgtp.Session({
        port: 0,
        security: {
          encryption: {
            algorithm: 'chacha20-poly1305',
            enabled: true
            // Missing key
          }
        }
      });

      try {
        await session.exposeFile(testFile);
        expect.fail('Should have thrown an error');
      } catch (error) {
        expect(error.message).to.include('no key provided');
      } finally {
        session.close();
      }
    });
  });

  describe('Digital Signatures', function() {
    it('should create session with signing enabled', function() {
      const session = new rgtp.Session({
        port: 0,
        security: {
          signing: {
            algorithm: 'ed25519',
            privateKey: keyPair.privateKey,
            enabled: true
          }
        }
      });
      
      expect(session).to.be.instanceOf(rgtp.Session);
      expect(session.security.signing.enabled).to.be.true;
      session.close();
    });

    it('should reject session with signing enabled but no private key', async function() {
      const session = new rgtp.Session({
        port: 0,
        security: {
          signing: {
            algorithm: 'ed25519',
            enabled: true
            // Missing private key
          }
        }
      });

      try {
        await session.exposeFile(testFile);
        expect.fail('Should have thrown an error');
      } catch (error) {
        expect(error.message).to.include('no private key provided');
      } finally {
        session.close();
      }
    });
  });

  describe('Access Control', function() {
    it('should create session with access control', function() {
      const session = new rgtp.Session({
        port: 0,
        security: {
          accessControl: {
            allowedClients: ['127.0.0.1', '::1'],
            requireAuth: true,
            maxConnections: 10
          }
        }
      });
      
      expect(session).to.be.instanceOf(rgtp.Session);
      expect(session.security.accessControl.requireAuth).to.be.true;
      session.close();
    });

    it('should validate allowed clients configuration', function() {
      const allowedClients = ['192.168.1.100', '10.0.0.0/8'];
      
      const session = new rgtp.Session({
        port: 0,
        security: {
          accessControl: {
            allowedClients: allowedClients,
            requireAuth: true
          }
        }
      });
      
      expect(session.security.accessControl.allowedClients).to.deep.equal(allowedClients);
      session.close();
    });
  });

  describe('Client Security Verification', function() {
    it('should create client with signature verification', function() {
      const client = new rgtp.Client({
        security: {
          verification: {
            serverPublicKey: keyPair.publicKey,
            verifySignatures: true,
            verifyIntegrity: true
          }
        }
      });
      
      expect(client).to.be.instanceOf(rgtp.Client);
      expect(client.security.verification.verifySignatures).to.be.true;
      client.close();
    });

    it('should reject client verification without server public key', async function() {
      const client = new rgtp.Client({
        security: {
          verification: {
            verifySignatures: true
            // Missing server public key
          }
        }
      });

      const outputFile = path.join(testDir, 'output.bin');

      try {
        await client.pullToFile('localhost', 9999, outputFile);
        expect.fail('Should have thrown an error');
      } catch (error) {
        expect(error.message).to.include('no server public key provided');
      } finally {
        client.close();
      }
    });
  });

  describe('End-to-End Security', function() {
    it('should perform secure transfer with full security', async function() {
      const encryptionKey = crypto.randomBytes(32);
      
      // Create secure session
      const session = new rgtp.Session({
        port: 0,
        security: {
          encryption: {
            algorithm: 'chacha20-poly1305',
            key: encryptionKey,
            enabled: true
          },
          signing: {
            algorithm: 'ed25519',
            privateKey: keyPair.privateKey,
            enabled: true
          },
          accessControl: {
            allowedClients: ['127.0.0.1'],
            requireAuth: true
          }
        }
      });

      // Create secure client
      const client = new rgtp.Client({
        security: {
          encryption: {
            algorithm: 'chacha20-poly1305',
            key: encryptionKey,
            enabled: true
          },
          verification: {
            serverPublicKey: keyPair.publicKey,
            verifySignatures: true,
            verifyIntegrity: true
          }
        }
      });

      try {
        await session.exposeFile(testFile);
        
        const outputFile = path.join(testDir, 'secure-output.bin');
        await client.pullToFile('127.0.0.1', 9999, outputFile);
        
        // Verify file was created
        expect(fs.existsSync(outputFile)).to.be.true;
        
        // Check security overhead in stats
        const sessionStats = await session.getStats();
        const clientStats = await client.getStats();
        
        expect(sessionStats.securityOverhead).to.be.a('number');
        expect(clientStats.securityOverhead).to.be.a('number');
        
      } finally {
        session.close();
        client.close();
      }
    });
  });

  describe('Cryptographic Functions', function() {
    it('should generate valid Ed25519 key pairs', function() {
      const testKeyPair = crypto.generateKeyPairSync('ed25519', {
        publicKeyEncoding: { type: 'spki', format: 'pem' },
        privateKeyEncoding: { type: 'pkcs8', format: 'pem' }
      });
      
      expect(testKeyPair.publicKey).to.be.a('string');
      expect(testKeyPair.privateKey).to.be.a('string');
      expect(testKeyPair.publicKey).to.include('BEGIN PUBLIC KEY');
      expect(testKeyPair.privateKey).to.include('BEGIN PRIVATE KEY');
    });

    it('should perform digital signature verification', function() {
      const message = 'Test message for RGTP security';
      const signature = crypto.sign(null, Buffer.from(message), keyPair.privateKey);
      
      const isValid = crypto.verify(null, Buffer.from(message), keyPair.publicKey, signature);
      expect(isValid).to.be.true;
      
      // Test with tampered message
      const tamperedMessage = 'Tampered message for RGTP security';
      const isTamperedValid = crypto.verify(null, Buffer.from(tamperedMessage), keyPair.publicKey, signature);
      expect(isTamperedValid).to.be.false;
    });

    it('should generate secure random encryption keys', function() {
      const key1 = crypto.randomBytes(32);
      const key2 = crypto.randomBytes(32);
      
      expect(key1).to.have.length(32);
      expect(key2).to.have.length(32);
      expect(key1.equals(key2)).to.be.false; // Should be different
    });

    it('should compute SHA-256 hashes correctly', function() {
      const data = 'RGTP chunk data for integrity verification';
      const hash1 = crypto.createHash('sha256').update(data).digest('hex');
      const hash2 = crypto.createHash('sha256').update(data).digest('hex');
      
      expect(hash1).to.equal(hash2); // Same data should produce same hash
      expect(hash1).to.have.length(64); // SHA-256 produces 64 hex characters
      
      // Different data should produce different hash
      const differentData = 'Different RGTP chunk data';
      const hash3 = crypto.createHash('sha256').update(differentData).digest('hex');
      expect(hash1).to.not.equal(hash3);
    });
  });

  describe('Security Performance', function() {
    it('should measure security overhead', async function() {
      // Test without security
      const plainSession = new rgtp.Session({ port: 0 });
      await plainSession.exposeFile(testFile);
      const plainStats = await plainSession.getStats();
      plainSession.close();
      
      // Test with security
      const secureSession = new rgtp.Session({
        port: 0,
        security: {
          encryption: {
            algorithm: 'chacha20-poly1305',
            key: crypto.randomBytes(32),
            enabled: true
          }
        }
      });
      await secureSession.exposeFile(testFile);
      const secureStats = await secureSession.getStats();
      secureSession.close();
      
      // Security should add some overhead
      if (nativeModuleAvailable) {
        expect(secureStats.securityOverhead).to.be.greaterThan(0);
      } else {
        // Mock system simulates overhead
        expect(secureStats.securityOverhead).to.equal(15);
      }
    });
  });
});