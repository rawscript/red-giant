#!/usr/bin/env node

/**
 * RGTP Streaming Server Example
 * 
 * This example demonstrates how to create a streaming server that can
 * serve multiple files to multiple clients simultaneously.
 */

const rgtp = require('../index.js');
const fs = require('fs');
const path = require('path');
const readline = require('readline');

class RGTPStreamingServer {
  constructor(basePort = 9000) {
    this.basePort = basePort;
    this.sessions = new Map();
    this.stats = new Map();
  }

  async exposeFile(filename, customPort = null) {
    const port = customPort || this.basePort + this.sessions.size;
    
    if (!fs.existsSync(filename)) {
      throw new Error(`File not found: ${filename}`);
    }

    const session = new rgtp.Session({
      port: port,
      adaptiveMode: true,
      onProgress: (transferred, total) => {
        const percent = (transferred / total * 100).toFixed(1);
        const key = `${filename}:${port}`;
        this.stats.set(key, { transferred, total, percent });
        this.updateDisplay();
      }
    });

    await session.exposeFile(filename);
    this.sessions.set(port, { session, filename });
    
    console.log(`📡 Exposing: ${path.basename(filename)} on port ${port}`);
    console.log(`   Size: ${rgtp.formatBytes(fs.statSync(filename).size)}`);
    console.log(`   Access: rgtp://localhost:${port}`);
    
    return port;
  }

  async stopExposure(port) {
    const sessionInfo = this.sessions.get(port);
    if (sessionInfo) {
      await sessionInfo.session.cancel();
      sessionInfo.session.close();
      this.sessions.delete(port);
      this.stats.delete(`${sessionInfo.filename}:${port}`);
      console.log(`🛑 Stopped exposing on port ${port}`);
    }
  }

  async stopAll() {
    const promises = Array.from(this.sessions.keys()).map(port => this.stopExposure(port));
    await Promise.all(promises);
  }

  updateDisplay() {
    // Clear screen and show current status
    console.clear();
    console.log('🚀 RGTP Streaming Server');
    console.log('========================\n');
    
    if (this.sessions.size === 0) {
      console.log('No files currently being exposed.');
      console.log('\nCommands:');
      console.log('  expose <filename> [port] - Expose a file');
      console.log('  stop <port>             - Stop exposing on port');
      console.log('  list                    - List active exposures');
      console.log('  quit                    - Exit server');
      return;
    }

    console.log('Active Exposures:');
    console.log('================');
    
    for (const [port, info] of this.sessions) {
      const key = `${info.filename}:${port}`;
      const stats = this.stats.get(key) || { transferred: 0, total: 0, percent: 0 };
      
      console.log(`Port ${port}: ${path.basename(info.filename)}`);
      console.log(`  Progress: ${stats.percent}% (${rgtp.formatBytes(stats.transferred)}/${rgtp.formatBytes(stats.total)})`);
      console.log(`  URL: rgtp://localhost:${port}`);
      console.log('');
    }
    
    console.log('Commands: expose <file> [port] | stop <port> | list | quit');
  }

  async getSessionStats(port) {
    const sessionInfo = this.sessions.get(port);
    if (sessionInfo) {
      return await sessionInfo.session.getStats();
    }
    return null;
  }

  listSessions() {
    console.log('\n📊 Session Details:');
    console.log('==================');
    
    for (const [port, info] of this.sessions) {
      console.log(`Port ${port}:`);
      console.log(`  File: ${info.filename}`);
      console.log(`  Size: ${rgtp.formatBytes(fs.statSync(info.filename).size)}`);
      
      this.getSessionStats(port).then(stats => {
        if (stats) {
          console.log(`  Throughput: ${stats.avgThroughputMbps.toFixed(2)} MB/s`);
          console.log(`  Efficiency: ${stats.efficiencyPercent.toFixed(1)}%`);
        }
      }).catch(() => {});
    }
  }
}

async function main() {
  const server = new RGTPStreamingServer();
  
  // Create readline interface for interactive commands
  const rl = readline.createInterface({
    input: process.stdin,
    output: process.stdout,
    prompt: 'rgtp-server> '
  });

  console.log('🚀 RGTP Streaming Server Started');
  console.log('================================\n');
  console.log('Commands:');
  console.log('  expose <filename> [port] - Expose a file for download');
  console.log('  stop <port>             - Stop exposing on specific port');
  console.log('  list                    - List all active exposures');
  console.log('  quit                    - Exit server\n');

  // Handle graceful shutdown
  process.on('SIGINT', async () => {
    console.log('\n🛑 Shutting down server...');
    await server.stopAll();
    rl.close();
    process.exit(0);
  });

  rl.prompt();

  rl.on('line', async (input) => {
    const args = input.trim().split(' ');
    const command = args[0].toLowerCase();

    try {
      switch (command) {
        case 'expose':
          if (args.length < 2) {
            console.log('Usage: expose <filename> [port]');
            break;
          }
          const filename = args[1];
          const port = args[2] ? parseInt(args[2]) : null;
          
          if (port && (port < 1024 || port > 65535)) {
            console.log('Port must be between 1024 and 65535');
            break;
          }
          
          const assignedPort = await server.exposeFile(filename, port);
          console.log(`✅ File exposed on port ${assignedPort}`);
          break;

        case 'stop':
          if (args.length < 2) {
            console.log('Usage: stop <port>');
            break;
          }
          const stopPort = parseInt(args[1]);
          await server.stopExposure(stopPort);
          break;

        case 'list':
          server.listSessions();
          break;

        case 'quit':
        case 'exit':
          console.log('👋 Goodbye!');
          await server.stopAll();
          rl.close();
          return;

        case 'help':
          console.log('\nAvailable commands:');
          console.log('  expose <filename> [port] - Expose a file');
          console.log('  stop <port>             - Stop exposing on port');
          console.log('  list                    - List active exposures');
          console.log('  quit                    - Exit server');
          break;

        default:
          if (command) {
            console.log(`Unknown command: ${command}. Type 'help' for available commands.`);
          }
      }
    } catch (error) {
      console.error(`❌ Error: ${error.message}`);
    }

    rl.prompt();
  });

  rl.on('close', async () => {
    await server.stopAll();
    process.exit(0);
  });
}

if (require.main === module) {
  main().catch(console.error);
}

module.exports = { RGTPStreamingServer };