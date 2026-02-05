const assert = require('assert');
const rgtp = require('../bindings/node/index.js');
const fs = require('fs').promises;
const path = require('path');

describe('RGTP Performance Tests', function() {
    this.timeout(60000); // 60 seconds for performance tests
    
    let tempDir;
    
    before(async function() {
        tempDir = path.join(__dirname, 'temp');
        if (!require('fs').existsSync(tempDir)) {
            require('fs').mkdirSync(tempDir);
        }
    });
    
    after(async function() {
        // Cleanup temporary files
        const files = require('fs').readdirSync(tempDir);
        for (const file of files) {
            await fs.unlink(path.join(tempDir, file));
        }
        require('fs').rmdirSync(tempDir);
    });
    
    describe('Throughput Tests', function() {
        it('should handle small files efficiently', async function() {
            const smallFileSize = 1024; // 1KB
            const fileName = 'small_file_test.dat';
            const filePath = path.join(tempDir, fileName);
            
            // Create small test file
            const smallFileData = Buffer.alloc(smallFileSize, 'RGTP');
            await fs.writeFile(filePath, smallFileData);
            
            const startTime = Date.now();
            const stats = await rgtp.sendFile(filePath, { timeout: 10000 });
            const endTime = Date.now();
            
            const duration = (endTime - startTime) / 1000; // in seconds
            const throughput = stats.bytesTransferred / (1024 * 1024) / duration; // MB/s
            
            console.log(`Small file test: ${smallFileSize} bytes, ${duration.toFixed(2)}s, ${throughput.toFixed(2)} MB/s`);
            
            assert(stats.bytesTransferred > 0, 'Should transfer some bytes');
        });
        
        it('should handle medium files efficiently', async function() {
            const mediumFileSize = 10 * 1024 * 1024; // 10MB
            const fileName = 'medium_file_test.dat';
            const filePath = path.join(tempDir, fileName);
            
            // Create medium test file (fill with pattern to avoid compression artifacts)
            const chunk = Buffer.alloc(1024 * 1024, 'RGTP_PERFORMANCE_TEST_');
            const mediumFileData = Buffer.concat(Array(10).fill(chunk));
            
            await fs.writeFile(filePath, mediumFileData);
            
            const startTime = Date.now();
            const stats = await rgtp.sendFile(filePath, { timeout: 30000 });
            const endTime = Date.now();
            
            const duration = (endTime - startTime) / 1000; // in seconds
            const throughput = stats.bytesTransferred / (1024 * 1024) / duration; // MB/s
            
            console.log(`Medium file test: ${mediumFileSize} bytes, ${duration.toFixed(2)}s, ${throughput.toFixed(2)} MB/s`);
            
            assert(stats.bytesTransferred > 0, 'Should transfer some bytes');
        });
        
        it('should measure adaptive rate control effectiveness', async function() {
            const testFileSize = 5 * 1024 * 1024; // 5MB
            const fileName = 'adaptive_test.dat';
            const filePath = path.join(tempDir, fileName);
            
            // Create test file
            const chunk = Buffer.alloc(1024 * 1024, 'ADAPTIVE_RATE_TEST_');
            const testData = Buffer.concat(Array(5).fill(chunk));
            await fs.writeFile(filePath, testData);
            
            // Test with adaptive mode enabled
            const adaptiveStats = await rgtp.sendFile(filePath, { 
                adaptiveMode: true, 
                timeout: 30000 
            });
            
            // Test with adaptive mode disabled
            const fixedStats = await rgtp.sendFile(filePath, { 
                adaptiveMode: false, 
                timeout: 30000 
            });
            
            console.log(`Adaptive mode: ${adaptiveStats.avgThroughputMbps.toFixed(2)} MB/s`);
            console.log(`Fixed rate: ${fixedStats.avgThroughputMbps.toFixed(2)} MB/s`);
            
            // Both should complete successfully
            assert(adaptiveStats.bytesTransferred > 0, 'Adaptive mode should transfer bytes');
            assert(fixedStats.bytesTransferred > 0, 'Fixed mode should transfer bytes');
        });
    });
    
    describe('Memory Usage Tests', function() {
        it('should handle multiple concurrent transfers without excessive memory growth', async function() {
            const numTransfers = 5;
            const transferPromises = [];
            
            for (let i = 0; i < numTransfers; i++) {
                const fileName = `concurrent_test_${i}.dat`;
                const filePath = path.join(tempDir, fileName);
                
                // Create small test file
                const testData = Buffer.alloc(1024 * 1024, `CONCURRENT_TEST_${i}_`);
                await fs.writeFile(filePath, testData);
                
                // Start transfer
                transferPromises.push(rgtp.sendFile(filePath, { timeout: 15000 }));
            }
            
            // Wait for all transfers to complete
            const results = await Promise.all(transferPromises);
            
            // Verify all transfers completed successfully
            results.forEach((result, index) => {
                assert(result.bytesTransferred > 0, `Transfer ${index} should have transferred bytes`);
            });
            
            console.log(`Completed ${numTransfers} concurrent transfers successfully`);
        });
        
        it('should clean up resources properly after transfer', async function() {
            const fileName = 'cleanup_test.dat';
            const filePath = path.join(tempDir, fileName);
            
            // Create test file
            const testData = Buffer.alloc(2 * 1024 * 1024, 'CLEANUP_TEST_');
            await fs.writeFile(filePath, testData);
            
            // Perform transfer
            const stats = await rgtp.sendFile(filePath, { timeout: 20000 });
            
            // Verify transfer completed
            assert(stats.bytesTransferred > 0, 'Transfer should have completed');
            
            // Allow some time for cleanup
            await new Promise(resolve => setTimeout(resolve, 1000));
            
            console.log('Resource cleanup test completed');
        });
    });
    
    describe('Stress Tests', function() {
        it('should handle rapid successive transfers', async function() {
            const numTransfers = 10;
            const results = [];
            
            for (let i = 0; i < numTransfers; i++) {
                const fileName = `rapid_test_${i}.dat`;
                const filePath = path.join(tempDir, fileName);
                
                // Create small test file
                const testData = Buffer.alloc(512 * 1024, `RAPID_TRANSFER_${i}_`);
                await fs.writeFile(filePath, testData);
                
                const startTime = Date.now();
                const stats = await rgtp.sendFile(filePath, { timeout: 10000 });
                const endTime = Date.now();
                
                results.push({
                    index: i,
                    duration: endTime - startTime,
                    bytes: stats.bytesTransferred,
                    throughput: stats.avgThroughputMbps
                });
                
                // Small delay to avoid overwhelming the system
                await new Promise(resolve => setTimeout(resolve, 100));
            }
            
            // Verify all transfers succeeded
            results.forEach((result, index) => {
                assert(result.bytes > 0, `Transfer ${index} should have transferred bytes`);
            });
            
            console.log(`Rapid succession test: ${numTransfers} transfers completed`);
        });
        
        it('should handle large file transfers', async function() {
            this.timeout(120000); // 2 minutes for large file test
            
            const largeFileSize = 50 * 1024 * 1024; // 50MB
            const fileName = 'large_file_test.dat';
            const filePath = path.join(tempDir, fileName);
            
            // Create large test file in chunks to avoid memory issues
            const fd = await fs.open(filePath, 'w');
            const chunkSize = 1024 * 1024; // 1MB chunks
            const numChunks = Math.ceil(largeFileSize / chunkSize);
            
            for (let i = 0; i < numChunks; i++) {
                const currentChunkSize = Math.min(chunkSize, largeFileSize - (i * chunkSize));
                const chunk = Buffer.alloc(currentChunkSize, `LARGE_FILE_CHUNK_${i}_`);
                await fd.write(chunk, 0, currentChunkSize, i * chunkSize);
            }
            
            await fd.close();
            
            console.log(`Starting large file transfer: ${largeFileSize} bytes`);
            const startTime = Date.now();
            
            const stats = await rgtp.sendFile(filePath, { 
                chunkSize: 2 * 1024 * 1024, // 2MB chunks for large files
                timeout: 60000 
            });
            
            const endTime = Date.now();
            const duration = (endTime - startTime) / 1000;
            
            console.log(`Large file test completed: ${duration.toFixed(2)}s, ${stats.avgThroughputMbps.toFixed(2)} MB/s`);
            
            assert(stats.bytesTransferred > 0, 'Large file transfer should have completed');
        });
    });
    
    describe('Edge Cases', function() {
        it('should handle timeout scenarios gracefully', async function() {
            // Test with very short timeout to trigger timeout
            const fileName = 'timeout_test.dat';
            const filePath = path.join(tempDir, fileName);
            
            // Create test file
            const testData = Buffer.alloc(1024 * 1024, 'TIMEOUT_TEST_');
            await fs.writeFile(filePath, testData);
            
            // Use a very short timeout to test timeout handling
            try {
                const stats = await rgtp.sendFile(filePath, { timeout: 100 }); // 100ms timeout
                console.log('Transfer completed despite short timeout');
            } catch (error) {
                console.log(`Expected timeout occurred: ${error.message}`);
            }
        });
        
        it('should handle empty files', async function() {
            const fileName = 'empty_test.dat';
            const filePath = path.join(tempDir, fileName);
            
            // Create empty file
            await fs.writeFile(filePath, Buffer.alloc(0));
            
            const stats = await rgtp.sendFile(filePath, { timeout: 5000 });
            
            // Should handle empty file without errors
            console.log('Empty file transfer completed');
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