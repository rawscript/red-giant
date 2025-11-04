#!/usr/bin/env node

/**
 * RGTP Performance Monitor Example
 * 
 * This example demonstrates real-time performance monitoring
 * and adaptive configuration based on network conditions.
 */

const rgtp = require('../index.js');
const fs = require('fs');
const path = require('path');
const os = require('os');

class RGTPPerformanceMonitor {
  constructor() {
    this.metrics = {
      throughput: [],
      latency: [],
      efficiency: [],
      retransmissions: []
    };
    this.isMonitoring = false;
    this.monitorInterval = null;
  }

  startMonitoring(session, interval = 1000) {
    if (this.isMonitoring) {
      this.stopMonitoring();
    }

    this.isMonitoring = true;
    this.session = session;
    
    console.log('📊 Starting performance monitoring...\n');
    
    this.monitorInterval = setInterval(async () => {
      try {
        const stats = await session.getStats();
        this.recordMetrics(stats);
        this.displayRealTimeStats(stats);
      } catch (error) {
        // Session might be closed
        this.stopMonitoring();
      }
    }, interval);
  }

  stopMonitoring() {
    if (this.monitorInterval) {
      clearInterval(this.monitorInterval);
      this.monitorInterval = null;
    }
    this.isMonitoring = false;
  }

  recordMetrics(stats) {
    const timestamp = Date.now();
    
    this.metrics.throughput.push({
      timestamp,
      value: stats.throughputMbps,
      avg: stats.avgThroughputMbps
    });
    
    this.metrics.efficiency.push({
      timestamp,
      value: stats.efficiencyPercent
    });
    
    this.metrics.retransmissions.push({
      timestamp,
      value: stats.retransmissions
    });

    // Keep only last 60 seconds of data
    const cutoff = timestamp - 60000;
    Object.keys(this.metrics).forEach(key => {
      this.metrics[key] = this.metrics[key].filter(m => m.timestamp > cutoff);
    });
  }

  displayRealTimeStats(stats) {
    if (process.stdout.isTTY) {
      console.clear();
    }
    
    console.log('🚀 RGTP Performance Monitor');
    console.log('===========================\n');
    
    // Current stats
    console.log('📈 Current Performance:');
    console.log(`  Throughput:     ${stats.throughputMbps.toFixed(2)} MB/s (avg: ${stats.avgThroughputMbps.toFixed(2)} MB/s)`);
    console.log(`  Progress:       ${stats.completionPercent.toFixed(1)}% (${rgtp.formatBytes(stats.bytesTransferred)}/${rgtp.formatBytes(stats.totalBytes)})`);
    console.log(`  Efficiency:     ${stats.efficiencyPercent.toFixed(1)}%`);
    console.log(`  Retransmissions: ${stats.retransmissions}`);
    console.log(`  Elapsed:        ${rgtp.formatDuration(stats.elapsedMs)}`);
    console.log(`  ETA:            ${rgtp.formatDuration(stats.estimatedRemainingMs)}\n`);
    
    // Historical analysis
    if (this.metrics.throughput.length > 5) {
      const analysis = this.analyzePerformance();
      console.log('📊 Performance Analysis (last 60s):');
      console.log(`  Peak Throughput:    ${analysis.peakThroughput.toFixed(2)} MB/s`);
      console.log(`  Min Throughput:     ${analysis.minThroughput.toFixed(2)} MB/s`);
      console.log(`  Avg Efficiency:     ${analysis.avgEfficiency.toFixed(1)}%`);
      console.log(`  Stability:          ${analysis.stability}`);
      console.log(`  Network Quality:    ${analysis.networkQuality}\n`);
      
      // Recommendations
      const recommendations = this.getRecommendations(analysis);
      if (recommendations.length > 0) {
        console.log('💡 Recommendations:');
        recommendations.forEach(rec => console.log(`  • ${rec}`));
        console.log('');
      }
    }
    
    // Visual throughput graph (simple ASCII)
    this.displayThroughputGraph();
  }

  analyzePerformance() {
    const throughputValues = this.metrics.throughput.map(m => m.value);
    const efficiencyValues = this.metrics.efficiency.map(m => m.value);
    
    const peakThroughput = Math.max(...throughputValues);
    const minThroughput = Math.min(...throughputValues);
    const avgThroughput = throughputValues.reduce((a, b) => a + b, 0) / throughputValues.length;
    const avgEfficiency = efficiencyValues.reduce((a, b) => a + b, 0) / efficiencyValues.length;
    
    // Calculate stability (coefficient of variation)
    const stdDev = Math.sqrt(throughputValues.reduce((sq, n) => sq + Math.pow(n - avgThroughput, 2), 0) / throughputValues.length);
    const cv = stdDev / avgThroughput;
    
    let stability = 'Excellent';
    if (cv > 0.3) stability = 'Poor';
    else if (cv > 0.2) stability = 'Fair';
    else if (cv > 0.1) stability = 'Good';
    
    let networkQuality = 'Excellent';
    if (avgEfficiency < 70) networkQuality = 'Poor';
    else if (avgEfficiency < 85) networkQuality = 'Fair';
    else if (avgEfficiency < 95) networkQuality = 'Good';
    
    return {
      peakThroughput,
      minThroughput,
      avgThroughput,
      avgEfficiency,
      stability,
      networkQuality,
      coefficientOfVariation: cv
    };
  }

  getRecommendations(analysis) {
    const recommendations = [];
    
    if (analysis.avgEfficiency < 80) {
      recommendations.push('Consider reducing chunk size for better reliability');
    }
    
    if (analysis.coefficientOfVariation > 0.3) {
      recommendations.push('Network is unstable - enable adaptive mode');
    }
    
    if (analysis.avgThroughput < 1.0) {
      recommendations.push('Low throughput detected - check network capacity');
    }
    
    if (analysis.networkQuality === 'Poor') {
      recommendations.push('Poor network quality - consider using mobile config');
    }
    
    return recommendations;
  }

  displayThroughputGraph() {
    if (this.metrics.throughput.length < 2) return;
    
    console.log('📈 Throughput Graph (last 30 samples):');
    
    const recent = this.metrics.throughput.slice(-30);
    const maxVal = Math.max(...recent.map(m => m.value));
    const minVal = Math.min(...recent.map(m => m.value));
    const range = maxVal - minVal || 1;
    
    const height = 8;
    const width = Math.min(recent.length, 50);
    
    for (let row = height - 1; row >= 0; row--) {
      let line = '  ';
      const threshold = minVal + (range * row / (height - 1));
      
      for (let col = 0; col < width; col++) {
        const dataIndex = Math.floor((col / width) * recent.length);
        const value = recent[dataIndex]?.value || 0;
        
        if (value >= threshold) {
          line += '█';
        } else {
          line += ' ';
        }
      }
      
      line += ` ${threshold.toFixed(1)} MB/s`;
      console.log(line);
    }
    
    console.log('  ' + '─'.repeat(width));
    console.log(`  ${minVal.toFixed(1)} MB/s (min) → ${maxVal.toFixed(1)} MB/s (max)\n`);
  }

  generateReport() {
    const analysis = this.analyzePerformance();
    
    const report = {
      timestamp: new Date().toISOString(),
      duration: this.metrics.throughput.length > 0 ? 
        (this.metrics.throughput[this.metrics.throughput.length - 1].timestamp - this.metrics.throughput[0].timestamp) / 1000 : 0,
      performance: analysis,
      rawMetrics: {
        throughputSamples: this.metrics.throughput.length,
        efficiencySamples: this.metrics.efficiency.length
      }
    };
    
    return report;
  }

  saveReport(filename) {
    const report = this.generateReport();
    fs.writeFileSync(filename, JSON.stringify(report, null, 2));
    console.log(`📄 Performance report saved to: ${filename}`);
  }
}

async function runMonitoringExample() {
  console.log('🔍 RGTP Performance Monitoring Example');
  console.log('======================================\n');

  // Create a test file
  const testFile = path.join(os.tmpdir(), 'rgtp-perf-test.bin');
  const testData = Buffer.alloc(5 * 1024 * 1024, 'Performance test data '); // 5MB
  fs.writeFileSync(testFile, testData);
  
  console.log(`Created test file: ${rgtp.formatBytes(testData.length)}\n`);

  const monitor = new RGTPPerformanceMonitor();
  
  try {
    // Start session with monitoring
    const session = new rgtp.Session({
      port: 9876,
      adaptiveMode: true,
      chunkSize: 64 * 1024 // 64KB chunks
    });

    console.log('🚀 Starting monitored file exposure...\n');
    
    // Start monitoring
    monitor.startMonitoring(session, 500); // Update every 500ms
    
    await session.exposeFile(testFile);
    
    // Simulate some transfer time
    await new Promise(resolve => setTimeout(resolve, 10000));
    
    await session.waitComplete();
    
    monitor.stopMonitoring();
    session.close();
    
    // Generate final report
    console.log('\n📊 Final Performance Report:');
    console.log('============================');
    
    const report = monitor.generateReport();
    console.log(`Duration: ${report.duration.toFixed(1)}s`);
    console.log(`Peak Throughput: ${report.performance.peakThroughput.toFixed(2)} MB/s`);
    console.log(`Average Efficiency: ${report.performance.avgEfficiency.toFixed(1)}%`);
    console.log(`Network Quality: ${report.performance.networkQuality}`);
    console.log(`Stability: ${report.performance.stability}`);
    
    // Save detailed report
    const reportFile = path.join(os.tmpdir(), `rgtp-performance-${Date.now()}.json`);
    monitor.saveReport(reportFile);
    
  } catch (error) {
    console.error('❌ Monitoring failed:', error.message);
    monitor.stopMonitoring();
  } finally {
    // Clean up
    if (fs.existsSync(testFile)) {
      fs.unlinkSync(testFile);
    }
  }
}

if (require.main === module) {
  runMonitoringExample().catch(console.error);
}

module.exports = { RGTPPerformanceMonitor };