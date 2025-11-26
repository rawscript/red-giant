#!/usr/bin/env node

/**
 * RGTP Batch Downloader Example
 * 
 * This example demonstrates downloading multiple files from different
 * RGTP servers simultaneously with progress tracking and retry logic.
 */

const rgtp = require('../index.js');
const fs = require('fs');
const path = require('path');
const os = require('os');

class RGTPBatchDownloader {
  constructor(options = {}) {
    this.maxConcurrent = options.maxConcurrent || 3;
    this.retryAttempts = options.retryAttempts || 3;
    this.timeout = options.timeout || 30000;
    this.downloadDir = options.downloadDir || path.join(os.tmpdir(), 'rgtp-downloads');
    
    // Ensure download directory exists
    if (!fs.existsSync(this.downloadDir)) {
      fs.mkdirSync(this.downloadDir, { recursive: true });
    }
    
    this.queue = [];
    this.active = new Map();
    this.completed = [];
    this.failed = [];
  }

  addDownload(host, port, filename, options = {}) {
    const download = {
      id: `${host}:${port}:${filename}`,
      host,
      port,
      filename,
      outputPath: options.outputPath || path.join(this.downloadDir, path.basename(filename)),
      priority: options.priority || 0,
      attempts: 0,
      status: 'queued',
      progress: { transferred: 0, total: 0, percent: 0 },
      stats: null,
      error: null
    };
    
    this.queue.push(download);
    this.queue.sort((a, b) => b.priority - a.priority); // Higher priority first
    
    console.log(`📥 Queued: ${download.filename} from ${host}:${port}`);
    return download.id;
  }

  async startDownloads() {
    console.log(`🚀 Starting batch download (max ${this.maxConcurrent} concurrent)`);
    console.log(`📁 Download directory: ${this.downloadDir}\n`);
    
    const promises = [];
    
    // Start initial downloads up to maxConcurrent
    for (let i = 0; i < Math.min(this.maxConcurrent, this.queue.length); i++) {
      promises.push(this.processNext());
    }
    
    // Wait for all downloads to complete
    await Promise.all(promises);
    
    this.printSummary();
  }

  async processNext() {
    while (this.queue.length > 0 || this.active.size > 0) {
      // If we have capacity and queued items, start a new download
      if (this.active.size < this.maxConcurrent && this.queue.length > 0) {
        const download = this.queue.shift();
        this.active.set(download.id, download);
        
        // Start download without awaiting (fire and forget)
        this.downloadFile(download).then(() => {
          this.active.delete(download.id);
        });
      }
      
      // Wait a bit before checking again
      await new Promise(resolve => setTimeout(resolve, 100));
    }
  }

  async downloadFile(download) {
    download.attempts++;
    download.status = 'downloading';
    
    console.log(`⬇️  Starting: ${download.filename} (attempt ${download.attempts})`);
    
    try {
      const client = new rgtp.Client({
        timeout: this.timeout,
        onProgress: (transferred, total) => {
          download.progress = {
            transferred,
            total,
            percent: total > 0 ? (transferred / total * 100) : 0
          };
          this.updateProgress();
        }
      });

      await client.pullToFile(download.host, download.port, download.outputPath);
      
      download.stats = await client.getStats();
      download.status = 'completed';
      
      client.close();
      
      this.completed.push(download);
      console.log(`✅ Completed: ${download.filename} (${rgtp.formatBytes(download.stats.bytesTransferred)})`);
      
    } catch (error) {
      download.error = error.message;
      download.status = 'failed';
      
      console.log(`❌ Failed: ${download.filename} - ${error.message}`);
      
      // Retry if we haven't exceeded max attempts
      if (download.attempts < this.retryAttempts) {
        download.status = 'retrying';
        console.log(`🔄 Retrying: ${download.filename} (${download.attempts}/${this.retryAttempts})`);
        
        // Add back to queue with slight delay
        setTimeout(() => {
          this.queue.unshift(download); // Add to front for immediate retry
        }, 2000);
      } else {
        this.failed.push(download);
        console.log(`💀 Giving up: ${download.filename} after ${this.retryAttempts} attempts`);
      }
    }
  }

  updateProgress() {
    // Clear screen and show progress
    if (process.stdout.isTTY) {
      console.clear();
      console.log('📊 RGTP Batch Downloader Progress');
      console.log('=================================\n');
      
      // Show active downloads
      if (this.active.size > 0) {
        console.log('Active Downloads:');
        for (const download of this.active.values()) {
          const percent = download.progress.percent.toFixed(1);
          const transferred = rgtp.formatBytes(download.progress.transferred);
          const total = rgtp.formatBytes(download.progress.total);
          
          console.log(`  ${download.filename}: ${percent}% (${transferred}/${total})`);
        }
        console.log('');
      }
      
      // Show queue status
      console.log(`Queue: ${this.queue.length} | Active: ${this.active.size} | Completed: ${this.completed.length} | Failed: ${this.failed.length}`);
    }
  }

  printSummary() {
    console.clear();
    console.log('📋 Download Summary');
    console.log('==================\n');
    
    if (this.completed.length > 0) {
      console.log('✅ Completed Downloads:');
      let totalBytes = 0;
      let totalTime = 0;
      
      for (const download of this.completed) {
        console.log(`  ${download.filename}`);
        console.log(`    Size: ${rgtp.formatBytes(download.stats.bytesTransferred)}`);
        console.log(`    Speed: ${download.stats.avgThroughputMbps.toFixed(2)} MB/s`);
        console.log(`    Time: ${rgtp.formatDuration(download.stats.elapsedMs)}`);
        console.log(`    Path: ${download.outputPath}`);
        
        totalBytes += download.stats.bytesTransferred;
        totalTime = Math.max(totalTime, download.stats.elapsedMs);
      }
      
      console.log(`\n  Total: ${rgtp.formatBytes(totalBytes)} in ${rgtp.formatDuration(totalTime)}`);
      console.log(`  Average Speed: ${(totalBytes / 1024 / 1024 / (totalTime / 1000)).toFixed(2)} MB/s\n`);
    }
    
    if (this.failed.length > 0) {
      console.log('❌ Failed Downloads:');
      for (const download of this.failed) {
        console.log(`  ${download.filename}: ${download.error}`);
      }
      console.log('');
    }
    
    console.log(`📁 Downloads saved to: ${this.downloadDir}`);
  }

  getStatus() {
    return {
      queued: this.queue.length,
      active: this.active.size,
      completed: this.completed.length,
      failed: this.failed.length,
      totalBytes: this.completed.reduce((sum, d) => sum + (d.stats?.bytesTransferred || 0), 0)
    };
  }
}

async function runExample() {
  console.log('📦 RGTP Batch Downloader Example');
  console.log('================================\n');

  const downloader = new RGTPBatchDownloader({
    maxConcurrent: 2,
    retryAttempts: 2,
    timeout: 15000
  });

  // Create some test files to download
  const testFiles = [
    { name: 'test1.bin', size: 1024 * 100 },  // 100KB
    { name: 'test2.bin', size: 1024 * 500 },  // 500KB
    { name: 'test3.bin', size: 1024 * 1024 }  // 1MB
  ];

  console.log('Creating test files...');
  for (const file of testFiles) {
    const filePath = path.join(os.tmpdir(), file.name);
    const data = Buffer.alloc(file.size, `Test data for ${file.name} `);
    fs.writeFileSync(filePath, data);
    console.log(`  Created: ${file.name} (${rgtp.formatBytes(file.size)})`);
  }

  // Add downloads to queue
  console.log('\nAdding downloads to queue...');
  downloader.addDownload('localhost', 9001, 'test1.bin', { priority: 1 });
  downloader.addDownload('localhost', 9002, 'test2.bin', { priority: 2 });
  downloader.addDownload('localhost', 9003, 'test3.bin', { priority: 3 });
  
  // Add a download that will fail (non-existent server)
  downloader.addDownload('nonexistent.host', 9999, 'missing.bin');

  console.log('\n💡 Note: This example requires RGTP servers running on ports 9001-9003');
  console.log('You can start them with the streaming_server.js example\n');

  try {
    await downloader.startDownloads();
  } catch (error) {
    console.error('❌ Batch download failed:', error.message);
  }

  // Clean up test files
  console.log('\nCleaning up test files...');
  for (const file of testFiles) {
    const filePath = path.join(os.tmpdir(), file.name);
    if (fs.existsSync(filePath)) {
      fs.unlinkSync(filePath);
    }
  }
}

if (require.main === module) {
  runExample().catch(console.error);
}

module.exports = { RGTPBatchDownloader };