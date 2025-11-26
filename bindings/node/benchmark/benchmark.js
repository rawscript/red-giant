#!/usr/bin/env node

/**
 * RGTP Performance Benchmark Suite
 * 
 * Comprehensive benchmarks for RGTP Node.js bindings
 */

const fs = require('fs');
const path = require('path');
const os = require('os');
const crypto = require('crypto');

// Test if we can load the module
let rgtp;
let nativeModuleAvailable = false;

try {
  rgtp = require('../index.js');
  nativeModuleAvailable = true;
  console.log('✓ Native module loaded successfully');
} catch (error) {
  console.log('⚠ Native module not available, running mock benchmarks');
  console.log('  This is normal if the native module hasn\'t been built yet');
  
  // Create mock module for benchmarking
  rgtp = {
    formatBytes: (bytes) => {
      if (bytes === 0) return '0 B';
      const k = 1024;
      const sizes = ['B', 'KB', 'MB', 'GB', 'TB'];
      const i = Math.floor(Math.log(bytes) / Math.log(k));
      return parseFloat((bytes / Math.pow(k, i)).toFixed(1)) + ' ' + sizes[i];
    },
    
    formatDuration: (ms) => {
      if (ms < 1000) return `${ms}ms`;
      const seconds = Math.floor(ms / 1000);
      const minutes = Math.floor(seconds / 60);
      if (minutes > 0) {
        return `${minutes}m ${seconds % 60}s`;
      }
      return `${seconds}s`;
    },
    
    sendFile: async (filename, config = {}) => {
      // Simulate transfer time based on file size
      const stats = fs.statSync(filename);
      const fileSize = stats.size;
      const simulatedThroughput = 50 + Math.random() * 100; // 50-150 MB/s
      const simulatedTime = (fileSize / 1024 / 1024) / simulatedThroughput * 1000;
      
      await new Promise(resolve => setTimeout(resolve, Math.min(simulatedTime, 100))); // Cap at 100ms for benchmarks
      
      return {
        bytesTransferred: fileSize,
        totalBytes: fileSize,
        avgThroughputMbps: simulatedThroughput,
        efficiencyPercent: 95 + Math.random() * 5, // 95-100%
        elapsedMs: simulatedTime,
        chunksTransferred: Math.ceil(fileSize / (config.chunkSize || 256 * 1024)),
        retransmissions: Math.floor(Math.random() * 3) // 0-2 retransmissions
      };
    },
    
    getVersion: () => '1.0.0-mock'
  };
}

class RGTPBenchmark {
  constructor() {
    this.results = [];
    this.tempDir = path.join(os.tmpdir(), 'rgtp-benchmark');
    
    // Ensure temp directory exists
    if (!fs.existsSync(this.tempDir)) {
      fs.mkdirSync(this.tempDir, { recursive: true });
    }
  }

  async runAllBenchmarks() {
    console.log('🏁 RGTP Performance Benchmark Suite');
    console.log('===================================\n');
    
    if (!nativeModuleAvailable) {
      console.log('📝 Note: Running with mock data (native module not built)');
      console.log('   For real benchmarks, build the native module first with: npm install');
      console.log('   These mock benchmarks test the benchmark framework itself.\n');
    }

    const benchmarks = [
      { name: 'Small Files (1KB - 100KB)', test: () => this.benchmarkSmallFiles() },
      { name: 'Medium Files (1MB - 10MB)', test: () => this.benchmarkMediumFiles() },
      { name: 'Large Files (50MB - 100MB)', test: () => this.benchmarkLargeFiles() },
      { name: 'Chunk Size Optimization', test: () => this.benchmarkChunkSizes() },
      { name: 'Concurrent Transfers', test: () => this.benchmarkConcurrency() },
      { name: 'Network Conditions', test: () => this.benchmarkNetworkConditions() },
      { name: 'Memory Usage', test: () => this.benchmarkMemoryUsage() }
    ];

    for (const benchmark of benchmarks) {
      console.log(`\n🔬 Running: ${benchmark.name}`);
      console.log('─'.repeat(50));
      
      try {
        const startTime = Date.now();
        await benchmark.test();
        const duration = Date.now() - startTime;
        console.log(`✅ Completed in ${rgtp.formatDuration(duration)}\n`);
      } catch (error) {
        console.error(`❌ Failed: ${error.message}\n`);
      }
    }

    this.generateReport();
    this.cleanup();
  }

  async benchmarkSmallFiles() {
    const sizes = [1024, 5120, 10240, 51200, 102400]; // 1KB to 100KB
    
    for (const size of sizes) {
      const result = await this.runTransferBenchmark(`small-${size}`, size, {
        chunkSize: Math.min(size / 4, 16384), // Adaptive chunk size
        iterations: 5
      });
      
      console.log(`  ${rgtp.formatBytes(size).padEnd(8)}: ${result.avgThroughput.toFixed(2)} MB/s (${result.avgLatency.toFixed(0)}ms)`);
    }
  }

  async benchmarkMediumFiles() {
    const sizes = [1024 * 1024, 5 * 1024 * 1024, 10 * 1024 * 1024]; // 1MB to 10MB
    
    for (const size of sizes) {
      const result = await this.runTransferBenchmark(`medium-${size}`, size, {
        chunkSize: 256 * 1024, // 256KB chunks
        iterations: 3
      });
      
      console.log(`  ${rgtp.formatBytes(size).padEnd(8)}: ${result.avgThroughput.toFixed(2)} MB/s (${result.avgLatency.toFixed(0)}ms)`);
    }
  }

  async benchmarkLargeFiles() {
    const sizes = [50 * 1024 * 1024, 100 * 1024 * 1024]; // 50MB to 100MB
    
    for (const size of sizes) {
      const result = await this.runTransferBenchmark(`large-${size}`, size, {
        chunkSize: 1024 * 1024, // 1MB chunks
        iterations: 2
      });
      
      console.log(`  ${rgtp.formatBytes(size).padEnd(8)}: ${result.avgThroughput.toFixed(2)} MB/s (${result.avgLatency.toFixed(0)}ms)`);
    }
  }

  async benchmarkChunkSizes() {
    const fileSize = 10 * 1024 * 1024; // 10MB test file
    const chunkSizes = [16384, 65536, 262144, 1048576]; // 16KB to 1MB
    
    console.log(`  Testing with ${rgtp.formatBytes(fileSize)} file:`);
    
    for (const chunkSize of chunkSizes) {
      const result = await this.runTransferBenchmark(`chunk-${chunkSize}`, fileSize, {
        chunkSize,
        iterations: 3
      });
      
      console.log(`    ${rgtp.formatBytes(chunkSize).padEnd(8)} chunks: ${result.avgThroughput.toFixed(2)} MB/s (efficiency: ${result.avgEfficiency.toFixed(1)}%)`);
    }
  }

  async benchmarkConcurrency() {
    const fileSize = 5 * 1024 * 1024; // 5MB per file
    const concurrencyLevels = [1, 2, 4, 8];
    
    for (const concurrent of concurrencyLevels) {
      const startTime = Date.now();
      const promises = [];
      
      for (let i = 0; i < concurrent; i++) {
        promises.push(this.runTransferBenchmark(`concurrent-${i}`, fileSize, {
          chunkSize: 256 * 1024,
          iterations: 1,
          port: 9900 + i
        }));
      }
      
      const results = await Promise.all(promises);
      const totalTime = Date.now() - startTime;
      const totalThroughput = results.reduce((sum, r) => sum + r.avgThroughput, 0);
      
      console.log(`  ${concurrent} concurrent: ${totalThroughput.toFixed(2)} MB/s total (${rgtp.formatDuration(totalTime)})`);
    }
  }

  async benchmarkNetworkConditions() {
    const fileSize = 5 * 1024 * 1024; // 5MB
    const configs = [
      { name: 'LAN Config', config: { chunkSize: 1024 * 1024, adaptiveMode: false } },
      { name: 'WAN Config', config: { chunkSize: 256 * 1024, adaptiveMode: true } },
      { name: 'Mobile Config', config: { chunkSize: 64 * 1024, adaptiveMode: true, timeout: 60000 } }
    ];
    
    for (const { name, config } of configs) {
      const result = await this.runTransferBenchmark(`network-${name}`, fileSize, {
        ...config,
        iterations: 3
      });
      
      console.log(`  ${name.padEnd(15)}: ${result.avgThroughput.toFixed(2)} MB/s (efficiency: ${result.avgEfficiency.toFixed(1)}%)`);
    }
  }

  async benchmarkMemoryUsage() {
    const sizes = [1024 * 1024, 10 * 1024 * 1024, 50 * 1024 * 1024]; // 1MB, 10MB, 50MB
    
    for (const size of sizes) {
      const memBefore = process.memoryUsage();
      
      await this.runTransferBenchmark(`memory-${size}`, size, {
        chunkSize: 256 * 1024,
        iterations: 1
      });
      
      // Force garbage collection if available
      if (global.gc) {
        global.gc();
      }
      
      const memAfter = process.memoryUsage();
      const memDiff = memAfter.heapUsed - memBefore.heapUsed;
      
      console.log(`  ${rgtp.formatBytes(size).padEnd(8)}: ${rgtp.formatBytes(Math.max(0, memDiff))} memory delta`);
    }
  }

  async runTransferBenchmark(testName, fileSize, options = {}) {
    const {
      chunkSize = 256 * 1024,
      iterations = 1,
      port = 9999,
      timeout = 30000,
      adaptiveMode = true
    } = options;

    const results = [];
    
    for (let i = 0; i < iterations; i++) {
      // Create test file
      const testFile = path.join(this.tempDir, `${testName}-${i}.bin`);
      const outputFile = path.join(this.tempDir, `${testName}-output-${i}.bin`);
      
      // Generate random data for more realistic testing
      const data = crypto.randomBytes(fileSize);
      fs.writeFileSync(testFile, data);
      
      try {
        // Run transfer benchmark
        const startTime = Date.now();
        
        const stats = await rgtp.sendFile(testFile, {
          port,
          chunkSize,
          adaptiveMode,
          timeout
        });
        
        const endTime = Date.now();
        const latency = endTime - startTime;
        
        results.push({
          throughput: stats.avgThroughputMbps,
          latency,
          efficiency: stats.efficiencyPercent,
          bytesTransferred: stats.bytesTransferred
        });
        
      } catch (error) {
        console.warn(`    Iteration ${i + 1} failed: ${error.message}`);
      } finally {
        // Clean up files
        [testFile, outputFile].forEach(file => {
          if (fs.existsSync(file)) {
            fs.unlinkSync(file);
          }
        });
      }
    }
    
    if (results.length === 0) {
      throw new Error('All benchmark iterations failed');
    }
    
    // Calculate averages
    const avgThroughput = results.reduce((sum, r) => sum + r.throughput, 0) / results.length;
    const avgLatency = results.reduce((sum, r) => sum + r.latency, 0) / results.length;
    const avgEfficiency = results.reduce((sum, r) => sum + r.efficiency, 0) / results.length;
    
    const result = {
      testName,
      fileSize,
      iterations: results.length,
      avgThroughput,
      avgLatency,
      avgEfficiency,
      minThroughput: Math.min(...results.map(r => r.throughput)),
      maxThroughput: Math.max(...results.map(r => r.throughput)),
      config: { chunkSize, adaptiveMode, timeout }
    };
    
    this.results.push(result);
    return result;
  }

  generateReport() {
    console.log('\n📊 Benchmark Report');
    console.log('==================\n');
    
    // System information
    console.log('🖥️  System Information:');
    console.log(`   OS: ${os.type()} ${os.release()}`);
    console.log(`   CPU: ${os.cpus()[0].model}`);
    console.log(`   Memory: ${rgtp.formatBytes(os.totalmem())}`);
    console.log(`   Node.js: ${process.version}`);
    console.log(`   RGTP: ${rgtp.getVersion()}${nativeModuleAvailable ? '' : ' (mock)'}\n`);
    
    // Performance summary
    if (this.results.length > 0) {
      const allThroughputs = this.results.map(r => r.avgThroughput);
      const allEfficiencies = this.results.map(r => r.avgEfficiency);
      
      console.log('📈 Performance Summary:');
      console.log(`   Peak Throughput: ${Math.max(...allThroughputs).toFixed(2)} MB/s`);
      console.log(`   Average Throughput: ${(allThroughputs.reduce((a, b) => a + b, 0) / allThroughputs.length).toFixed(2)} MB/s`);
      console.log(`   Average Efficiency: ${(allEfficiencies.reduce((a, b) => a + b, 0) / allEfficiencies.length).toFixed(1)}%\n`);
      
      // Top performers
      const topPerformers = this.results
        .sort((a, b) => b.avgThroughput - a.avgThroughput)
        .slice(0, 5);
      
      console.log('🏆 Top Performers:');
      topPerformers.forEach((result, index) => {
        console.log(`   ${index + 1}. ${result.testName}: ${result.avgThroughput.toFixed(2)} MB/s`);
      });
    }
    
    // Save detailed report
    const reportFile = path.join(this.tempDir, `benchmark-report-${Date.now()}.json`);
    const report = {
      timestamp: new Date().toISOString(),
      system: {
        os: `${os.type()} ${os.release()}`,
        cpu: os.cpus()[0].model,
        memory: os.totalmem(),
        nodeVersion: process.version,
        rgtpVersion: rgtp.getVersion()
      },
      results: this.results
    };
    
    fs.writeFileSync(reportFile, JSON.stringify(report, null, 2));
    console.log(`\n📄 Detailed report saved: ${reportFile}`);
  }

  cleanup() {
    // Clean up temporary files
    try {
      const files = fs.readdirSync(this.tempDir);
      files.forEach(file => {
        const filePath = path.join(this.tempDir, file);
        if (fs.statSync(filePath).isFile() && !file.includes('benchmark-report')) {
          fs.unlinkSync(filePath);
        }
      });
    } catch (error) {
      // Ignore cleanup errors
    }
  }
}

async function runBenchmarks() {
  const benchmark = new RGTPBenchmark();
  
  try {
    await benchmark.runAllBenchmarks();
  } catch (error) {
    console.error('❌ Benchmark suite failed:', error.message);
    process.exit(1);
  }
}

if (require.main === module) {
  // Check if running with --expose-gc for memory benchmarks
  if (!global.gc && process.argv.includes('--memory')) {
    console.log('💡 For accurate memory benchmarks, run with: node --expose-gc benchmark.js --memory');
  }
  
  runBenchmarks().catch(console.error);
}

module.exports = { RGTPBenchmark };