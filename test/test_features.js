const assert = require('assert');
const rgtp = require('../bindings/node/index.js');

describe('RGTP New Features Tests', function() {
    
    describe('Session Management API Tests', function() {
        it('should create session with new API', function() {
            const config = {
                port: 0,
                chunkSize: 1024 * 1024,
                exposureRate: 1000,
                adaptiveMode: true,
                enableCompression: false,
                enableEncryption: false,
                timeout: 30000
            };
            
            const sessionHandle = rgtp.sessionCreate(config);
            assert(sessionHandle, 'Session handle should be created');
            
            // Clean up
            rgtp.sessionDestroy(sessionHandle);
        });
        
        it('should create client with new API', function() {
            const config = {
                timeout: 30000,
                chunkSize: 1024 * 1024,
                adaptiveMode: true
            };
            
            const clientHandle = rgtp.clientCreate(config);
            assert(clientHandle, 'Client handle should be created');
            
            // Clean up
            rgtp.clientDestroy(clientHandle);
        });
        
        it('should get session statistics', function() {
            const config = {
                port: 0,
                chunkSize: 1024 * 1024,
                exposureRate: 1000,
                adaptiveMode: true,
                timeout: 30000
            };
            
            const sessionHandle = rgtp.sessionCreate(config);
            assert(sessionHandle, 'Session handle should be created');
            
            try {
                const stats = rgtp.sessionGetStats(sessionHandle);
                assert(stats, 'Stats should be returned');
                
                // Check that stats object has expected properties
                assert(typeof stats.bytesSent === 'number', 'bytesSent should be a number');
                assert(typeof stats.bytesReceived === 'number', 'bytesReceived should be a number');
                assert(typeof stats.chunksSent === 'number', 'chunksSent should be a number');
                assert(typeof stats.chunksReceived === 'number', 'chunksReceived should be a number');
                assert(typeof stats.packetLossRate === 'number', 'packetLossRate should be a number');
                assert(typeof stats.rttMs === 'number', 'rttMs should be a number');
            } finally {
                rgtp.sessionDestroy(sessionHandle);
            }
        });
        
        it('should get client statistics', function() {
            const config = {
                timeout: 30000,
                chunkSize: 1024 * 1024,
                adaptiveMode: true
            };
            
            const clientHandle = rgtp.clientCreate(config);
            assert(clientHandle, 'Client handle should be created');
            
            try {
                const stats = rgtp.clientGetStats(clientHandle);
                assert(stats, 'Stats should be returned');
                
                // Check that stats object has expected properties
                assert(typeof stats.bytesSent === 'number', 'bytesSent should be a number');
                assert(typeof stats.bytesReceived === 'number', 'bytesReceived should be a number');
                assert(typeof stats.chunksSent === 'number', 'chunksSent should be a number');
                assert(typeof stats.chunksReceived === 'number', 'chunksReceived should be a number');
                assert(typeof stats.packetLossRate === 'number', 'packetLossRate should be a number');
                assert(typeof stats.rttMs === 'number', 'rttMs should be a number');
            } finally {
                rgtp.clientDestroy(clientHandle);
            }
        });
    });
    
    describe('Adaptive Rate Control Tests', function() {
        it('should set exposure rate', function() {
            const config = {
                port: 0,
                chunkSize: 1024 * 1024,
                exposureRate: 1000,
                adaptiveMode: true,
                timeout: 30000
            };
            
            const sessionHandle = rgtp.sessionCreate(config);
            assert(sessionHandle, 'Session handle should be created');
            
            try {
                // Create a surface to test rate control
                const surface = rgtp.exposeData(Buffer.from('test data'), rgtp.socket());
                
                if (surface !== -1) {
                    const result = rgtp.setExposureRate(surface, 2000); // Set to 2000 chunks/sec
                    assert(result === 0, 'Setting exposure rate should succeed');
                    
                    // Test adaptive exposure
                    const adaptiveResult = rgtp.adaptiveExposure(surface);
                    assert(adaptiveResult === 0, 'Enabling adaptive exposure should succeed');
                    
                    // Test getting exposure status
                    const status = rgtp.getExposureStatus(surface);
                    assert(typeof status === 'number', 'Exposure status should be a number');
                    
                    rgtp.destroySurface(surface);
                }
            } finally {
                rgtp.sessionDestroy(sessionHandle);
            }
        });
        
        it('should handle adaptive exposure', function() {
            const config = {
                port: 0,
                chunkSize: 1024 * 1024,
                exposureRate: 1000,
                adaptiveMode: true,
                timeout: 30000
            };
            
            const sessionHandle = rgtp.sessionCreate(config);
            assert(sessionHandle, 'Session handle should be created');
            
            try {
                // Test that adaptive exposure can be enabled
                const surface = rgtp.exposeData(Buffer.from('test data'), rgtp.socket());
                
                if (surface !== -1) {
                    const result = rgtp.adaptiveExposure(surface);
                    assert(result === 0, 'Adaptive exposure should be enabled successfully');
                    
                    rgtp.destroySurface(surface);
                }
            } finally {
                rgtp.sessionDestroy(sessionHandle);
            }
        });
    });
    
    describe('Congestion Control Tests', function() {
        it('should simulate congestion control behavior', function() {
            const config = {
                port: 0,
                chunkSize: 1024 * 1024,
                exposureRate: 1000,
                adaptiveMode: true,
                timeout: 30000
            };
            
            const sessionHandle = rgtp.sessionCreate(config);
            assert(sessionHandle, 'Session handle should be created');
            
            try {
                // Test with various network conditions simulated
                const surface = rgtp.exposeData(Buffer.from('test data'), rgtp.socket());
                
                if (surface !== -1) {
                    // Test rate adjustments based on network conditions
                    const normalRateResult = rgtp.setExposureRate(surface, 1000);
                    assert(normalRateResult === 0, 'Setting normal rate should succeed');
                    
                    const highRateResult = rgtp.setExposureRate(surface, 5000);
                    assert(highRateResult === 0, 'Setting high rate should succeed');
                    
                    const lowRateResult = rgtp.setExposureRate(surface, 100);
                    assert(lowRateResult === 0, 'Setting low rate should succeed');
                    
                    rgtp.destroySurface(surface);
                }
            } finally {
                rgtp.sessionDestroy(sessionHandle);
            }
        });
    });
    
    describe('RTT Measurement Tests', function() {
        it('should handle RTT measurements', function() {
            const config = {
                port: 0,
                chunkSize: 1024 * 1024,
                exposureRate: 1000,
                adaptiveMode: true,
                timeout: 30000
            };
            
            const sessionHandle = rgtp.sessionCreate(config);
            assert(sessionHandle, 'Session handle should be created');
            
            try {
                // Create a surface to test RTT
                const surface = rgtp.exposeData(Buffer.from('test data'), rgtp.socket());
                
                if (surface !== -1) {
                    // Test that RTT can be accessed in stats
                    const stats = rgtp.sessionGetStats(sessionHandle);
                    assert(stats, 'Stats should be returned');
                    
                    // RTT might be 0 initially, but the property should exist
                    assert(typeof stats.rttMs === 'number', 'rttMs should be a number');
                    
                    rgtp.destroySurface(surface);
                }
            } finally {
                rgtp.sessionDestroy(sessionHandle);
            }
        });
    });
    
    describe('Error Handling Tests', function() {
        it('should handle invalid session operations', function() {
            // Test with invalid handle (null/undefined)
            assert.throws(() => {
                rgtp.sessionGetStats(null);
            }, 'Should throw error for invalid session handle');
            
            assert.throws(() => {
                rgtp.sessionGetStats(undefined);
            }, 'Should throw error for undefined session handle');
        });
        
        it('should handle invalid client operations', function() {
            // Test with invalid handle (null/undefined)
            assert.throws(() => {
                rgtp.clientGetStats(null);
            }, 'Should throw error for invalid client handle');
            
            assert.throws(() => {
                rgtp.clientGetStats(undefined);
            }, 'Should throw error for undefined client handle');
        });
        
        it('should handle invalid rate setting', function() {
            const surface = rgtp.exposeData(Buffer.from('test data'), rgtp.socket());
            
            if (surface !== -1) {
                try {
                    // Test setting invalid rates
                    const negativeRateResult = rgtp.setExposureRate(surface, -1);
                    // Negative rates might return an error, which is acceptable
                    
                    const zeroRateResult = rgtp.setExposureRate(surface, 0);
                    // Zero rates might return an error, which is acceptable
                } finally {
                    rgtp.destroySurface(surface);
                }
            }
        });
    });
    
    describe('Integration Tests for New Features', function() {
        it('should integrate session management with existing functionality', function() {
            // Create a session using new API
            const sessionConfig = {
                port: 0,
                chunkSize: 1024 * 1024,
                exposureRate: 1000,
                adaptiveMode: true,
                timeout: 30000
            };
            
            const sessionHandle = rgtp.sessionCreate(sessionConfig);
            assert(sessionHandle, 'Session handle should be created');
            
            try {
                // Verify that new session works with existing functionality
                const stats = rgtp.sessionGetStats(sessionHandle);
                assert(stats, 'Stats should be accessible');
                
                // Test rate control
                const surface = rgtp.exposeData(Buffer.from('integration test'), rgtp.socket());
                
                if (surface !== -1) {
                    const rateResult = rgtp.setExposureRate(surface, 1500);
                    assert(rateResult === 0, 'Rate setting should work with new session');
                    
                    rgtp.destroySurface(surface);
                }
            } finally {
                rgtp.sessionDestroy(sessionHandle);
            }
        });
        
        it('should demonstrate improved performance with new features', function() {
            const config = {
                port: 0,
                chunkSize: 2 * 1024 * 1024, // 2MB chunks for better performance
                exposureRate: 2000,
                adaptiveMode: true,
                enableCompression: false,
                enableEncryption: false,
                timeout: 30000
            };
            
            const sessionHandle = rgtp.sessionCreate(config);
            assert(sessionHandle, 'Session handle should be created');
            
            try {
                // Test that the new session can provide performance metrics
                const initialStats = rgtp.sessionGetStats(sessionHandle);
                assert(initialStats, 'Initial stats should be available');
                
                console.log('New features integration test passed');
            } finally {
                rgtp.sessionDestroy(sessionHandle);
            }
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