#!/usr/bin/env node

// Simple transfer example demonstrating the RGTP API
const rgtp = require('../index.js');
const fs = require('fs').promises;
const path = require('path');

async function runExample() {
  console.log('🚀 Starting RGTP Simple Transfer Example');
  console.log('=====================================\n');
  
  try {
    // Create a test file
    const testFilePath = path.join(__dirname, 'test-file.txt');
    const testData = 'Hello RGTP World! This is a test file for demonstrating the protocol.';
    await fs.writeFile(testFilePath, testData);
    
    console.log('📝 Created test file:', testFilePath);
    
    // Server side - expose the file
    console.log('\n📡 Starting server session...');
    const session = new rgtp.Session({
      port: 9999,
      adaptiveMode: true
    });
    
    session.on('exposeStart', (filePath, fileSize) => {
      console.log(`📤 Exposing file: ${filePath} (${fileSize} bytes)`);
    });
    
    session.on('progress', (transferred, total) => {
      const percent = total > 0 ? ((transferred / total) * 100).toFixed(1) : 0;
      console.log(`📊 Progress: ${percent}% (${rgtp.formatBytes(transferred)}/${rgtp.formatBytes(total)})`);
    });
    
    session.on('error', (error) => {
      console.error('❌ Server error:', error.message);
    });
    
    // Expose the file
    await session.exposeFile(testFilePath);
    console.log('✅ File exposed successfully!');
    
    // Client side - pull the file
    console.log('\n📥 Starting client session...');
    const client = new rgtp.Client({
      timeout: 30000
    });
    
    const outputPath = path.join(__dirname, 'received-file.txt');
    
    client.on('pullStart', (host, port, outputPath) => {
      console.log(`📥 Pulling from ${host}:${port} to ${outputPath}`);
    });
    
    client.on('progress', (transferred, total) => {
      const percent = total > 0 ? ((transferred / total) * 100).toFixed(1) : 0;
      console.log(`📊 Download progress: ${percent}% (${rgtp.formatBytes(transferred)}/${rgtp.formatBytes(total)})`);
    });
    
    client.on('pullComplete', (outputPath) => {
      console.log(`✅ File received: ${outputPath}`);
    });
    
    client.on('error', (error) => {
      console.error('❌ Client error:', error.message);
    });
    
    // Pull the file
    await client.pullToFile('localhost', 9999, outputPath);
    console.log('✅ File pulled successfully!');
    
    // Get statistics
    const serverStats = await session.getStats();
    const clientStats = await client.getStats();
    
    console.log('\n📈 Transfer Statistics:');
    console.log('======================');
    console.log(`Server transferred: ${rgtp.formatBytes(serverStats.bytesTransferred)}`);
    console.log(`Server throughput: ${serverStats.avgThroughputMbps.toFixed(2)} MB/s`);
    console.log(`Client received: ${rgtp.formatBytes(clientStats.bytesTransferred)}`);
    console.log(`Client throughput: ${clientStats.avgThroughputMbps.toFixed(2)} MB/s`);
    
    // Verify file content
    const receivedData = await fs.readFile(outputPath, 'utf8');
    console.log('\n🔍 File verification:');
    console.log('===================');
    console.log('Original:', testData);
    console.log('Received:', receivedData);
    console.log('Match:', testData === receivedData ? '✅ YES' : '❌ NO');
    
    // Cleanup
    session.close();
    client.close();
    
    // Clean up test files
    await fs.unlink(testFilePath);
    await fs.unlink(outputPath);
    
    console.log('\n🎉 Example completed successfully!');
    
  } catch (error) {
    console.error('💥 Example failed:', error.message);
    process.exit(1);
  }
}

// Handle graceful shutdown
process.on('SIGINT', () => {
  console.log('\n🛑 Shutting down gracefully...');
  process.exit(0);
});

runExample();