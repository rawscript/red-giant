#!/usr/bin/env node

/**
 * Simple RGTP file transfer example in Node.js
 * 
 * This example demonstrates basic file transfer using RGTP Node.js bindings.
 * It shows both exposing (serving) and pulling (downloading) files.
 */

const rgtp = require('../../bindings/node');
const fs = require('fs');
const path = require('path');
const { performance } = require('perf_hooks');

// ANSI color codes for better output
const colors = {
  reset: '\x1b[0m',
  bright: '\x1b[1m',
  red: '\x1b[31m',
  green: '\x1b[32m',
  yellow: '\x1b[33m',
  blue: '\x1b[34m',
  magenta: '\x1b[35m',
  cyan: '\x1b[36m'
};

function colorize(color, text) {
  return `${colors[color]}${text}${colors.reset}`;
}

function printUsage() {
  console.log(colorize('cyan', 'RGTP Node.js Example'));
  console.log(colorize('cyan', '===================='));
  console.log(`${colorize('bright', 'RGTP Version:')} ${rgtp.VERSION}\n`);
  
  console.log(colorize('yellow', 'Usage:'));
  console.log(`  ${process.argv[1]} expose <filename>`);
  console.log(`  ${process.argv[1]} pull <host> <port> [output_file]\n`);
  
  console.log(colorize('yellow', 'Examples:'));
  console.log(`  ${process.argv[1]} expose document.pdf`);
  console.log(`  ${process.argv[1]} pull localhost 9999`);
  console.log(`  ${process.argv[1]} pull 192.168.1.100 9999 downloaded.pdf`);
}

async function exposeFile(filename) {
  console.log(colorize('blue', `📤 Exposing file: ${filename}`));
  
  // Check if file exists
  if (!fs.existsSync(filename)) {
    console.error(colorize('red', `❌ File not found: ${filename}`));
    process.exit(1);
  }
  
  const stats = fs.statSync(filename);
  console.log(`   📋 Size: ${rgtp.formatBytes(stats.size)}`);
  
  // Create configuration optimized for LAN
  const config = rgtp.createLANConfig();
  config.port = 9999;
  
  // Add progress tracking
  let lastUpdate = 0;
  config.onProgress = (bytesTransferred, totalBytes) => {
    const now = Date.now();
    if (now - lastUpdate > 500) { // Update every 500ms
      const percent = (bytesTransferred / totalBytes * 100).toFixed(1);
      process.stdout.write(`\r   Progress: ${percent}% (${rgtp.formatBytes(bytesTransferred)}/${rgtp.formatBytes(totalBytes)})`);
      lastUpdate = now;
    }
  };
  
  config.onError = (errorCode, errorMessage) => {
    console.error(colorize('red', `\n❌ Error ${errorCode}: ${errorMessage}`));
  };
  
  console.log(`\n${colorize('yellow', 'Configuration:')}`);
  console.log(`   • Port: ${config.port}`);
  console.log(`   • Chunk size: ${rgtp.formatBytes(config.chunkSize)}`);
  console.log(`   • Adaptive mode: ${config.adaptiveMode}`);
  console.log(`   • Exposure rate: ${config.exposureRate} chunks/sec`);
  
  // Create session
  const session = new rgtp.Session(config);
  
  // Handle graceful shutdown
  let shutdownRequested = false;
  process.on('SIGINT', async () => {
    if (shutdownRequested) {
      console.log(colorize('red', '\n🛑 Force shutdown'));
      process.exit(1);
    }
    
    shutdownRequested = true;
    console.log(colorize('yellow', '\n🛑 Shutting down gracefully... (Press Ctrl+C again to force)'));
    
    try {
      await session.cancel();
      session.close();
      process.exit(0);
    } catch (error) {
      console.error(colorize('red', `Error during shutdown: ${error.message}`));
      process.exit(1);
    }
  });
  
  try {
    const startTime = performance.now();
    
    // Start exposure
    await session.exposeFile(filename);
    
    console.log(colorize('green', `\n✅ File exposed successfully on port ${config.port}`));
    console.log(`${colorize('cyan', 'Clients can now pull from:')} <this_host>:${config.port}`);
    console.log(colorize('yellow', 'Press Ctrl+C to stop...\n'));
    
    // Monitor progress
    const monitorInterval = setInterval(async () => {
      try {
        const stats = await session.getStats();
        if (stats.chunksTransferred > 0) {
          process.stdout.write(`\r${colorize('cyan', 'Stats:')} ${rgtp.formatBytes(stats.bytesTransferred)} transferred, ${stats.throughputMbps.toFixed(1)} MB/s, ${stats.completionPercent.toFixed(1)}% complete`);
        }
      } catch (error) {
        // Ignore errors during monitoring
      }
    }, 1000);
    
    // Wait for completion
    await session.waitComplete();
    
    clearInterval(monitorInterval);
    const endTime = performance.now();
    
    console.log(colorize('green', '\n✅ Exposure completed successfully!'));
    
    // Print final statistics
    const finalStats = await session.getStats();
    printFinalStats(finalStats, 'Exposure', endTime - startTime);
    
  } catch (error) {
    console.error(colorize('red', `\n❌ Exposure failed: ${error.message}`));
    process.exit(1);
  } finally {
    session.close();
  }
}

async function pullFile(host, port, outputFile) {
  console.log(colorize('blue', `📥 Pulling from ${host}:${port} -> ${outputFile}`));
  
  // Create configuration optimized for WAN
  const config = rgtp.createWANConfig();
  
  // Add progress tracking with progress bar
  let lastUpdate = 0;
  config.onProgress = (bytesTransferred, totalBytes) => {
    const now = Date.now();
    if (now - lastUpdate > 200) { // Update every 200ms
      const percent = bytesTransferred / totalBytes * 100;
      
      // Simple progress bar
      const barWidth = 40;
      const filled = Math.floor(percent / 100 * barWidth);
      
      let bar = '[';
      for (let i = 0; i < barWidth; i++) {
        if (i < filled) {
          bar += '=';
        } else if (i === filled) {
          bar += '>';
        } else {
          bar += ' ';
        }
      }
      bar += ']';
      
      process.stdout.write(`\r   ${bar} ${percent.toFixed(1)}% (${rgtp.formatBytes(bytesTransferred)}/${rgtp.formatBytes(totalBytes)})`);
      lastUpdate = now;
    }
  };
  
  config.onError = (errorCode, errorMessage) => {
    console.error(colorize('red', `\n❌ Error ${errorCode}: ${errorMessage}`));
  };
  
  console.log(`\n${colorize('yellow', 'Configuration:')}`);
  console.log(`   • Timeout: ${rgtp.formatDuration(config.timeout)}`);
  console.log(`   • Chunk size: ${rgtp.formatBytes(config.chunkSize)}`);
  console.log(`   • Adaptive mode: ${config.adaptiveMode}`);
  
  // Create client
  const client = new rgtp.Client(config);
  
  // Handle graceful shutdown
  process.on('SIGINT', async () => {
    console.log(colorize('yellow', '\n🛑 Cancelling pull operation...'));
    try {
      await client.cancel();
      client.close();
      process.exit(0);
    } catch (error) {
      console.error(colorize('red', `Error during cancellation: ${error.message}`));
      process.exit(1);
    }
  });
  
  try {
    console.log('\nStarting pull operation...');
    const startTime = performance.now();
    
    // Pull the file
    await client.pullToFile(host, port, outputFile);
    
    const endTime = performance.now();
    
    console.log(colorize('green', '\n✅ Pull completed successfully!'));
    console.log(`   • File saved as: ${outputFile}`);
    console.log(`   • Total time: ${rgtp.formatDuration(endTime - startTime)}`);
    
    // Print final statistics
    const finalStats = await client.getStats();
    printFinalStats(finalStats, 'Pull', endTime - startTime);
    
  } catch (error) {
    console.error(colorize('red', `\n❌ Pull failed: ${error.message}`));
    
    // Try to get partial statistics
    try {
      const stats = await client.getStats();
      if (stats.bytesTransferred > 0) {
        console.log(`   • Partial data received: ${rgtp.formatBytes(stats.bytesTransferred)} (${stats.completionPercent.toFixed(1)}%)`);
      }
    } catch (statsError) {
      // Ignore stats error
    }
    
    process.exit(1);
  } finally {
    client.close();
  }
}

function printFinalStats(stats, operation, totalTimeMs) {
  console.log(`\n${colorize('cyan', `📊 ${operation} Statistics:`)}`);
  console.log(`   • File size: ${rgtp.formatBytes(stats.totalBytes)}`);
  console.log(`   • Duration: ${rgtp.formatDuration(stats.elapsedMs)}`);
  console.log(`   • Average throughput: ${stats.avgThroughputMbps.toFixed(2)} MB/s`);
  console.log(`   • Peak throughput: ${stats.throughputMbps.toFixed(2)} MB/s`);
  console.log(`   • Chunks transferred: ${stats.chunksTransferred}/${stats.totalChunks}`);
  console.log(`   • Retransmissions: ${stats.retransmissions}`);
  console.log(`   • Efficiency: ${stats.efficiencyPercent.toFixed(1)}%`);
  
  if (stats.totalBytes > 0 && totalTimeMs > 0) {
    const effectiveSpeed = (stats.totalBytes / (1024 * 1024)) / (totalTimeMs / 1000);
    console.log(`   • Effective speed: ${effectiveSpeed.toFixed(2)} MB/s`);
  }
}

async function main() {
  if (process.argv.length < 3) {
    printUsage();
    process.exit(1);
  }
  
  const command = process.argv[2].toLowerCase();
  
  try {
    switch (command) {
      case 'expose':
        if (process.argv.length !== 4) {
          console.error(colorize('red', 'Usage: expose <filename>'));
          process.exit(1);
        }
        await exposeFile(process.argv[3]);
        break;
        
      case 'pull':
        if (process.argv.length < 5 || process.argv.length > 6) {
          console.error(colorize('red', 'Usage: pull <host> <port> [output_file]'));
          process.exit(1);
        }
        
        const host = process.argv[3];
        const port = parseInt(process.argv[4]);
        const outputFile = process.argv[5] || 'rgtp_download.bin';
        
        if (isNaN(port) || port < 1 || port > 65535) {
          console.error(colorize('red', 'Invalid port number'));
          process.exit(1);
        }
        
        await pullFile(host, port, outputFile);
        break;
        
      default:
        console.error(colorize('red', `Unknown command: ${command}`));
        printUsage();
        process.exit(1);
    }
  } catch (error) {
    console.error(colorize('red', `💥 Unexpected error: ${error.message}`));
    if (process.env.NODE_ENV === 'development') {
      console.error(error.stack);
    }
    process.exit(1);
  }
}

// Run the main function
if (require.main === module) {
  main().catch(error => {
    console.error(colorize('red', `Fatal error: ${error.message}`));
    process.exit(1);
  });
}