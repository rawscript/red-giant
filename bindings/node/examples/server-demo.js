#!/usr/bin/env node

// Server demo showing continuous file serving
const rgtp = require('../index.js');
const fs = require('fs').promises;
const path = require('path');

class RGTPServerDemo {
  constructor() {
    this.sessions = [];
    this.filesExposed = 0;
  }
  
  async start(port = 9999) {
    console.log(`🚀 Starting RGTP Server Demo on port ${port}`);
    console.log('==========================================\n');
    
    // Create sample files
    await this.createSampleFiles();
    
    // Start serving files continuously
    await this.serveFiles(port);
  }
  
  async createSampleFiles() {
    console.log('📁 Creating sample files...');
    
    const files = [
      { name: 'small-document.txt', content: 'This is a small document for RGTP testing.' },
      { name: 'medium-article.txt', content: 'A'.repeat(10000) }, // 10KB
      { name: 'large-file.txt', content: 'B'.repeat(1000000) }   // 1MB
    ];
    
    for (const file of files) {
      const filePath = path.join(__dirname, file.name);
      await fs.writeFile(filePath, file.content);
      console.log(`  ✅ Created ${file.name} (${file.content.length} bytes)`);
    }
    
    this.sampleFiles = files.map(f => path.join(__dirname, f.name));
  }
  
  async serveFiles(port) {
    console.log(`\n📡 Serving files on port ${port}...`);
    console.log('(Press Ctrl+C to stop)\n');
    
    let fileIndex = 0;
    
    const serveNextFile = async () => {
      if (fileIndex >= this.sampleFiles.length) {
        fileIndex = 0; // Cycle through files
      }
      
      const filePath = this.sampleFiles[fileIndex];
      const fileName = path.basename(filePath);
      
      console.log(`📤 Exposing: ${fileName}`);
      
      try {
        const session = new rgtp.Session({
          port: port + fileIndex, // Different port for each file
          adaptiveMode: true
        });
        
        this.sessions.push(session);
        this.filesExposed++;
        
        session.on('exposeStart', (filePath, fileSize) => {
          console.log(`  📊 Serving ${fileName}: ${rgtp.formatBytes(fileSize)}`);
        });
        
        session.on('progress', (transferred, total) => {
          const percent = ((transferred / total) * 100).toFixed(1);
          process.stdout.write(`\r  🔄 Progress: ${percent}% (${rgtp.formatBytes(transferred)}/${rgtp.formatBytes(total)})`);
        });
        
        session.on('error', (error) => {
          console.log(`\n  ❌ Error serving ${fileName}: ${error.message}`);
        });
        
        await session.exposeFile(filePath);
        console.log(`\n  ✅ ${fileName} exposed successfully`);
        
      } catch (error) {
        console.log(`\n  ❌ Failed to expose ${fileName}: ${error.message}`);
      }
      
      fileIndex++;
      
      // Schedule next file after delay
      setTimeout(serveNextFile, 2000);
    };
    
    // Start serving files
    serveNextFile();
    
    // Display server stats periodically
    setInterval(() => {
      this.displayStats();
    }, 5000);
  }
  
  displayStats() {
    const activeSessions = this.sessions.filter(s => !s.isClosed).length;
    console.log(`\n📊 Server Stats:`);
    console.log(`  Active sessions: ${activeSessions}`);
    console.log(`  Files exposed: ${this.filesExposed}`);
    console.log(`  Memory usage: ${(process.memoryUsage().heapUsed / 1024 / 1024).toFixed(2)} MB`);
  }
  
  async cleanup() {
    console.log('\n🧹 Cleaning up...');
    
    // Close all sessions
    for (const session of this.sessions) {
      if (!session.isClosed) {
        session.close();
      }
    }
    
    // Remove sample files
    if (this.sampleFiles) {
      for (const filePath of this.sampleFiles) {
        try {
          await fs.unlink(filePath);
        } catch (error) {
          // File might not exist
        }
      }
    }
    
    console.log('✅ Cleanup complete');
  }
}

// Main execution
async function main() {
  const server = new RGTPServerDemo();
  
  // Handle graceful shutdown
  process.on('SIGINT', async () => {
    console.log('\n🛑 Shutting down server...');
    await server.cleanup();
    process.exit(0);
  });
  
  try {
    await server.start(9999);
  } catch (error) {
    console.error('💥 Server failed:', error.message);
    await server.cleanup();
    process.exit(1);
  }
}

main();