#!/usr/bin/env node

// Client demo showing batch file downloading
const rgtp = require('../index.js');
const fs = require('fs').promises;
const path = require('path');

class RGTPClientDemo {
  constructor() {
    this.clients = [];
    this.downloadsCompleted = 0;
    this.totalDownloads = 0;
  }
  
  async start(servers = [{ host: 'localhost', port: 9999 }]) {
    console.log('📥 Starting RGTP Client Demo');
    console.log('============================\n');
    
    this.servers = servers;
    this.totalDownloads = servers.length;
    
    console.log(`🎯 Target servers: ${servers.length}`);
    servers.forEach((server, i) => {
      console.log(`  ${i + 1}. ${server.host}:${server.port}`);
    });
    
    console.log('\n🚀 Starting downloads...\n');
    
    // Start downloading from all servers concurrently
    const downloadPromises = servers.map((server, index) => 
      this.downloadFromServer(server, index)
    );
    
    await Promise.all(downloadPromises);
    
    this.displayFinalStats();
  }
  
  async downloadFromServer(server, index) {
    const client = new rgtp.Client({
      timeout: 30000,
      adaptiveMode: true
    });
    
    this.clients.push(client);
    
    const outputPath = path.join(__dirname, `downloaded-from-${server.host}-${server.port}.txt`);
    
    try {
      console.log(`📥 Client ${index + 1}: Connecting to ${server.host}:${server.port}`);
      
      client.on('pullStart', (host, port, outputPath) => {
        console.log(`  📥 Client ${index + 1}: Starting download to ${path.basename(outputPath)}`);
      });
      
      client.on('progress', (transferred, total) => {
        const percent = total > 0 ? ((transferred / total) * 100).toFixed(1) : 0;
        process.stdout.write(`\r  🔄 Client ${index + 1}: ${percent}% (${rgtp.formatBytes(transferred)}/${rgtp.formatBytes(total)})`);
      });
      
      client.on('pullComplete', (outputPath) => {
        console.log(`\n  ✅ Client ${index + 1}: Download complete - ${path.basename(outputPath)}`);
        this.downloadsCompleted++;
      });
      
      client.on('error', (error) => {
        console.log(`\n  ❌ Client ${index + 1}: Error - ${error.message}`);
      });
      
      await client.pullToFile(server.host, server.port, outputPath);
      
    } catch (error) {
      console.log(`\n  ❌ Client ${index + 1}: Failed - ${error.message}`);
    } finally {
      client.close();
    }
  }
  
  displayFinalStats() {
    console.log('\n📊 Final Download Statistics:');
    console.log('============================');
    console.log(`Total attempted: ${this.totalDownloads}`);
    console.log(`Successfully completed: ${this.downloadsCompleted}`);
    console.log(`Success rate: ${((this.downloadsCompleted / this.totalDownloads) * 100).toFixed(1)}%`);
    
    // Calculate aggregate stats
    const totalBytes = this.clients.reduce((sum, client) => {
      return sum + (client.stats?.bytesTransferred || 0);
    }, 0);
    
    console.log(`Total data transferred: ${rgtp.formatBytes(totalBytes)}`);
    
    // Average throughput
    const avgThroughput = this.clients.reduce((sum, client) => {
      return sum + (client.stats?.avgThroughputMbps || 0);
    }, 0) / this.clients.length;
    
    console.log(`Average throughput: ${avgThroughput.toFixed(2)} MB/s`);
  }
  
  async cleanup() {
    console.log('\n🧹 Cleaning up downloaded files...');
    
    // Close all clients
    for (const client of this.clients) {
      if (!client.isClosed) {
        client.close();
      }
    }
    
    // Remove downloaded files
    const downloadPattern = path.join(__dirname, 'downloaded-from-*.txt');
    const files = await fs.readdir(__dirname);
    const downloadedFiles = files.filter(f => f.startsWith('downloaded-from-'));
    
    for (const file of downloadedFiles) {
      try {
        await fs.unlink(path.join(__dirname, file));
        console.log(`  🗑️  Removed ${file}`);
      } catch (error) {
        // File might not exist
      }
    }
    
    console.log('✅ Cleanup complete');
  }
}

// Demo scenarios
async function runSingleServerDemo() {
  console.log('🎯 Single Server Demo');
  console.log('====================');
  
  const client = new RGTPClientDemo();
  
  process.on('SIGINT', async () => {
    await client.cleanup();
    process.exit(0);
  });
  
  try {
    await client.start([
      { host: 'localhost', port: 9999 }
    ]);
  } catch (error) {
    console.error('💥 Demo failed:', error.message);
  } finally {
    await client.cleanup();
  }
}

async function runMultipleServersDemo() {
  console.log('🎯 Multiple Servers Demo');
  console.log('========================');
  
  const client = new RGTPClientDemo();
  
  process.on('SIGINT', async () => {
    await client.cleanup();
    process.exit(0);
  });
  
  try {
    await client.start([
      { host: 'localhost', port: 9999 },
      { host: 'localhost', port: 10000 },
      { host: 'localhost', port: 10001 }
    ]);
  } catch (error) {
    console.error('💥 Demo failed:', error.message);
  } finally {
    await client.cleanup();
  }
}

// Main execution
async function main() {
  const args = process.argv.slice(2);
  const demoType = args[0] || 'single';
  
  switch (demoType) {
    case 'single':
      await runSingleServerDemo();
      break;
    case 'multiple':
      await runMultipleServersDemo();
      break;
    default:
      console.log('Usage: node client-demo.js [single|multiple]');
      console.log('  single   - Download from one server (default)');
      console.log('  multiple - Download from multiple servers');
      process.exit(1);
  }
}

main();