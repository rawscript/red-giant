#!/usr/bin/env node

/**
 * RGTP Simple Transfer Example
 * 
 * This example demonstrates basic file transfer using RGTP.
 * Run with: node examples/simple_transfer.js
 */

const rgtp = require('../index.js');
const fs = require('fs');
const path = require('path');
const os = require('os');

async function createTestFile() {
  const testFile = path.join(os.tmpdir(), 'rgtp-example-file.bin');
  const testData = Buffer.alloc(1024 * 1024, 'Hello RGTP! '); // 1MB test file
  fs.writeFileSync(testFile, testData);
  console.log(`Created test file: ${testFile} (${rgtp.formatBytes(testData.length)})`);
  return testFile;
}

async function runExample() {
  console.log('🚀 RGTP Simple Transfer Example');
  console.log('================================\n');

  try {
    // Create a test file
    const testFile = await createTestFile();
    const outputFile = path.join(os.tmpdir(), 'rgtp-received-file.bin');

    console.log('📊 Using convenience functions:\n');

    // Example 1: Using convenience functions
    console.log('1. Sending file...');
    const sendStats = await rgtp.sendFile(testFile, {
      port: 9999,
      adaptiveMode: true
    });
    
    console.log(`   ✓ Sent: ${rgtp.formatBytes(sendStats.bytesTransferred)}`);
    console.log(`   ✓ Throughput: ${sendStats.avgThroughputMbps.toFixed(2)} MB/s`);
    console.log(`   ✓ Efficiency: ${sendStats.efficiencyPercent.toFixed(1)}%\n`);

    console.log('2. Receiving file...');
    const receiveStats = await rgtp.receiveFile('localhost', 9999, outputFile, {
      timeout: 10000
    });
    
    console.log(`   ✓ Received: ${rgtp.formatBytes(receiveStats.bytesTransferred)}`);
    console.log(`   ✓ Throughput: ${receiveStats.avgThroughputMbps.toFixed(2)} MB/s`);
    console.log(`   ✓ Efficiency: ${receiveStats.efficiencyPercent.toFixed(1)}%\n`);

    console.log('🔧 Using Session and Client classes:\n');

    // Example 2: Using Session and Client classes with events
    const session = new rgtp.Session({
      port: 9998,
      adaptiveMode: true,
      onProgress: (transferred, total) => {
        const percent = (transferred / total * 100).toFixed(1);
        process.stdout.write(`\r   Progress: ${percent}% (${rgtp.formatBytes(transferred)}/${rgtp.formatBytes(total)})`);
      }
    });

    console.log('3. Starting session...');
    await session.exposeFile(testFile);
    console.log('\n   ✓ File exposed successfully');

    const client = new rgtp.Client({
      timeout: 10000,
      onProgress: (transferred, total) => {
        const percent = (transferred / total * 100).toFixed(1);
        process.stdout.write(`\r   Download: ${percent}% (${rgtp.formatBytes(transferred)}/${rgtp.formatBytes(total)})`);
      }
    });

    const outputFile2 = path.join(os.tmpdir(), 'rgtp-received-file-2.bin');
    await client.pullToFile('localhost', 9998, outputFile2);
    console.log('\n   ✓ File downloaded successfully');

    // Get final statistics
    const finalSessionStats = await session.getStats();
    const finalClientStats = await client.getStats();

    console.log('\n📈 Final Statistics:');
    console.log(`   Session - Transferred: ${rgtp.formatBytes(finalSessionStats.bytesTransferred)}, Efficiency: ${finalSessionStats.efficiencyPercent.toFixed(1)}%`);
    console.log(`   Client  - Transferred: ${rgtp.formatBytes(finalClientStats.bytesTransferred)}, Efficiency: ${finalClientStats.efficiencyPercent.toFixed(1)}%`);

    // Clean up
    session.close();
    client.close();

    // Verify files
    if (fs.existsSync(outputFile) && fs.existsSync(outputFile2)) {
      console.log('\n✅ Transfer completed successfully!');
      console.log(`   Original: ${testFile}`);
      console.log(`   Copy 1:   ${outputFile}`);
      console.log(`   Copy 2:   ${outputFile2}`);
    }

  } catch (error) {
    console.error('\n❌ Error during transfer:', error.message);
    
    if (error.message.includes('Native module')) {
      console.log('\n💡 Note: This example requires the native RGTP module to be built.');
      console.log('   Run "npm install" to build the native module.');
      console.log('   On Windows, you may need Visual Studio Build Tools.');
    }
  }
}

// Run the example
if (require.main === module) {
  runExample().catch(console.error);
}

module.exports = { runExample };