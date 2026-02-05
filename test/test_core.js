const assert = require('assert');
const rgtp = require('../bindings/node/index.js');

describe('RGTP Core Functionality Tests', function() {
    
    describe('Initialization Tests', function() {
        it('should initialize RGTP library successfully', function() {
            const result = rgtp.init();
            assert.strictEqual(result, 0, 'RGTP initialization should return 0 for success');
        });
        
        it('should create UDP socket successfully', function() {
            const socket = rgtp.socket();
            assert(socket >= 0, 'Socket creation should return valid file descriptor');
            // Cleanup
            // Note: In a real test, we'd need platform-specific socket closing
        });
        
        it('should get version information', function() {
            const version = rgtp.getVersion();
            assert.strictEqual(typeof version, 'string', 'Version should be a string');
            assert(version.length > 0, 'Version should not be empty');
        });
    });
    
    describe('Session Management Tests', function() {
        let session;
        
        beforeEach(function() {
            // Create session before each test
            session = new rgtp.Session({
                port: 0, // Auto-assign port
                chunkSize: 64 * 1024,
                exposureRate: 500,
                adaptiveMode: true,
                timeout: 5000
            });
        });
        
        afterEach(function() {
            // Clean up after each test
            if (session && !session.isClosed) {
                session.close();
            }
        });
        
        it('should create session with default configuration', function() {
            const defaultSession = new rgtp.Session();
            assert(defaultSession instanceof rgtp.Session, 'Should create valid Session instance');
            assert.strictEqual(defaultSession.options.chunkSize, 1024 * 1024, 'Default chunk size should be 1MB');
            assert.strictEqual(defaultSession.options.adaptiveMode, true, 'Adaptive mode should be enabled by default');
        });
        
        it('should create session with custom configuration', function() {
            const customSession = new rgtp.Session({
                port: 9999,
                chunkSize: 256 * 1024,
                exposureRate: 1000,
                adaptiveMode: false,
                timeout: 10000
            });
            
            assert.strictEqual(customSession.options.port, 9999, 'Port should be set to 9999');
            assert.strictEqual(customSession.options.chunkSize, 256 * 1024, 'Chunk size should be 256KB');
            assert.strictEqual(customSession.options.exposureRate, 1000, 'Exposure rate should be 1000');
            assert.strictEqual(customSession.options.adaptiveMode, false, 'Adaptive mode should be disabled');
            assert.strictEqual(customSession.options.timeout, 10000, 'Timeout should be 10 seconds');
        });
        
        it('should handle session closure properly', function(done) {
            session.on('close', function() {
                assert.strictEqual(session.isClosed, true, 'Session should be marked as closed');
                done();
            });
            
            session.close();
        });
        
        it('should reject operations on closed session', function() {
            session.close();
            assert.throws(
                () => session.exposeFile('test.txt'),
                /Session is closed/,
                'Should throw error when exposing file on closed session'
            );
        });
    });
    
    describe('Client Management Tests', function() {
        let client;
        
        beforeEach(function() {
            client = new rgtp.Client({
                timeout: 5000,
                chunkSize: 64 * 1024,
                adaptiveMode: true
            });
        });
        
        afterEach(function() {
            if (client && !client.isClosed) {
                client.close();
            }
        });
        
        it('should create client with default configuration', function() {
            const defaultClient = new rgtp.Client();
            assert(defaultClient instanceof rgtp.Client, 'Should create valid Client instance');
        });
        
        it('should create client with custom configuration', function() {
            const customClient = new rgtp.Client({
                timeout: 30000,
                chunkSize: 128 * 1024,
                adaptiveMode: false
            });
            
            assert.strictEqual(customClient.options.timeout, 30000, 'Timeout should be 30 seconds');
            assert.strictEqual(customClient.options.chunkSize, 128 * 1024, 'Chunk size should be 128KB');
            assert.strictEqual(customClient.options.adaptiveMode, false, 'Adaptive mode should be disabled');
        });
        
        it('should handle client closure properly', function(done) {
            client.on('close', function() {
                assert.strictEqual(client.isClosed, true, 'Client should be marked as closed');
                done();
            });
            
            client.close();
        });
    });
    
    describe('Configuration Tests', function() {
        it('should provide LAN configuration preset', function() {
            const config = rgtp.createLANConfig();
            assert.strictEqual(config.chunkSize, 2 * 1024 * 1024, 'LAN config should use 2MB chunks');
            assert.strictEqual(config.adaptiveMode, true, 'LAN config should enable adaptive mode');
            assert.strictEqual(config.timeout, 10000, 'LAN config should have 10 second timeout');
        });
        
        it('should provide WAN configuration preset', function() {
            const config = rgtp.createWANConfig();
            assert.strictEqual(config.chunkSize, 512 * 1024, 'WAN config should use 512KB chunks');
            assert.strictEqual(config.adaptiveMode, true, 'WAN config should enable adaptive mode');
            assert.strictEqual(config.timeout, 30000, 'WAN config should have 30 second timeout');
        });
        
        it('should provide mobile configuration preset', function() {
            const config = rgtp.createMobileConfig();
            assert.strictEqual(config.chunkSize, 256 * 1024, 'Mobile config should use 256KB chunks');
            assert.strictEqual(config.adaptiveMode, true, 'Mobile config should enable adaptive mode');
            assert.strictEqual(config.timeout, 60000, 'Mobile config should have 60 second timeout');
        });
    });
    
    describe('Utility Function Tests', function() {
        it('should format bytes correctly', function() {
            assert.strictEqual(rgtp.formatBytes(0), '0 Bytes', '0 bytes should format as "0 Bytes"');
            assert.strictEqual(rgtp.formatBytes(1023), '1023.00 Bytes', '1023 bytes should format correctly');
            assert.strictEqual(rgtp.formatBytes(1024), '1.00 KB', '1024 bytes should format as "1.00 KB"');
            assert.strictEqual(rgtp.formatBytes(1024 * 1024), '1.00 MB', '1MB should format as "1.00 MB"');
            assert.strictEqual(rgtp.formatBytes(1024 * 1024 * 1024), '1.00 GB', '1GB should format as "1.00 GB"');
        });
        
        it('should generate exposure IDs', function() {
            const id1 = rgtp.generateExposureID();
            const id2 = rgtp.generateExposureID();
            
            assert.strictEqual(typeof id1, 'string', 'Exposure ID should be a string');
            assert.strictEqual(id1.length, 32, 'Exposure ID should be 32 characters long');
            assert.notStrictEqual(id1, id2, 'Consecutive IDs should be different');
        });
    });
    
    describe('Integration Tests', function() {
        this.timeout(30000); // Increase timeout for integration tests
        
        let session, client;
        const testFilePath = './test/test-file.txt';
        const receivedFilePath = './test/received-file.txt';
        
        before(function() {
            // Create test files directory
            const fs = require('fs');
            if (!fs.existsSync('./test')) {
                fs.mkdirSync('./test');
            }
        });
        
        beforeEach(function() {
            session = new rgtp.Session({ port: 0 });
            client = new rgtp.Client();
        });
        
        afterEach(function() {
            if (session && !session.isClosed) session.close();
            if (client && !client.isClosed) client.close();
        });
        
        it('should perform basic send/receive operation', async function() {
            // This is a simplified test - in a real scenario, you'd need
            // actual file creation and network setup
            this.skip(); // Skip for now as it requires more complex setup
        });
        
        it('should handle concurrent sessions', function(done) {
            const sessions = [];
            const sessionCount = 3;
            let closedCount = 0;
            
            // Create multiple sessions
            for (let i = 0; i < sessionCount; i++) {
                const session = new rgtp.Session();
                sessions.push(session);
                
                session.on('close', function() {
                    closedCount++;
                    if (closedCount === sessionCount) {
                        done();
                    }
                });
            }
            
            // Close all sessions
            sessions.forEach(session => session.close());
        });
    });
});

// Run the tests if this file is executed directly
if (require.main === module) {
    const Mocha = require('mocha');
    const mocha = new Mocha();
    mocha.addFile(__filename);
    mocha.run();
}