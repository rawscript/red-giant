#!/usr/bin/env node

// HTTP/Web3 adapter demo
const { RGTPHTTPAdapter, RGTPWeb3Interface } = require('../http-adapter.js');
const fs = require('fs').promises;
const path = require('path');

async function runHTTPDemo() {
  console.log('🌐 RGTP HTTP/Web3 Adapter Demo');
  console.log('==============================\n');
  
  // Create sample files
  await createSampleFiles();
  
  // Start HTTP adapter
  const httpAdapter = new RGTPHTTPAdapter({
    port: 8080,
    rgtpPort: 9999,
    useSSL: false
  });
  
  try {
    await httpAdapter.start();
    console.log('✅ HTTP adapter started on port 8080');
    console.log('✅ RGTP core running on ports 9999+');
    console.log('\n🚀 Visit http://localhost:8080 to see the file browser');
    console.log('   Files will be served over RGTP UDP protocol!\n');
    
    // Demonstrate Web3 interface
    await demonstrateWeb3Interface();
    
    // Keep running
    console.log('Press Ctrl+C to stop');
    
    // Wait indefinitely
    await new Promise(() => {});
    
  } catch (error) {
    console.error('❌ Demo failed:', error.message);
  } finally {
    await httpAdapter.stop();
    await cleanupSampleFiles();
  }
}

async function createSampleFiles() {
  console.log('📁 Creating sample files...');
  
  const files = [
    { name: 'document.txt', content: 'This is a sample document served via RGTP over HTTP interface.' },
    { name: 'image.txt', content: 'Fake image data for demonstration purposes.' },
    { name: 'archive.txt', content: 'Compressed archive content simulation.' }
  ];
  
  for (const file of files) {
    const filePath = path.join(process.cwd(), file.name);
    await fs.writeFile(filePath, file.content);
    console.log(`  ✅ Created ${file.name}`);
  }
}

async function demonstrateWeb3Interface() {
  console.log('\n🔄 Demonstrating Web3/IPFS-style interface:');
  console.log('===========================================\n');
  
  const web3 = new RGTPWeb3Interface();
  
  try {
    // Add files (like ipfs add)
    const files = ['document.txt', 'image.txt', 'archive.txt'];
    const results = [];
    
    for (const fileName of files) {
      const result = await web3.add(fileName);
      results.push(result);
      console.log(`  ➕ Added ${fileName}: ${result.cid}`);
    }
    
    // List files (like ipfs ls)
    console.log('\n  📋 File listing:');
    const listing = await web3.ls();
    listing.forEach(entry => {
      console.log(`    ${entry.cid} - ${entry.name} (${entry.size} bytes)`);
    });
    
    // Retrieve a file (like ipfs get)
    if (results.length > 0) {
      const firstFile = results[0];
      const outputPath = `retrieved_${firstFile.path}`;
      await web3.get(firstFile.cid, outputPath);
      console.log(`\n  ⬇️  Retrieved ${firstFile.cid} to ${outputPath}`);
    }
    
  } catch (error) {
    console.error('  ❌ Web3 demo failed:', error.message);
  }
}

async function cleanupSampleFiles() {
  console.log('\n🧹 Cleaning up sample files...');
  
  const files = ['document.txt', 'image.txt', 'archive.txt'];
  const retrievedFiles = files.map(f => `retrieved_${f}`);
  const uploadDir = 'uploads';
  
  // Remove sample files
  for (const file of [...files, ...retrievedFiles]) {
    try {
      await fs.unlink(file);
      console.log(`  🗑️  Removed ${file}`);
    } catch (error) {
      // File might not exist
    }
  }
  
  // Remove uploads directory
  try {
    await fs.rm(uploadDir, { recursive: true, force: true });
    console.log(`  🗑️  Removed ${uploadDir} directory`);
  } catch (error) {
    // Directory might not exist
  }
  
  console.log('✅ Cleanup complete');
}

// Handle graceful shutdown
process.on('SIGINT', async () => {
  console.log('\n🛑 Shutting down HTTP adapter demo...');
  process.exit(0);
});

// Main execution
if (require.main === module) {
  runHTTPDemo().catch(console.error);
}

module.exports = { runHTTPDemo };