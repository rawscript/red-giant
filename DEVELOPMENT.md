# RGTP Development Guide

This document outlines the development setup and tools for RGTP.

## 🚀 What We've Built

### 1. CI/CD Pipeline (GitHub Actions)
- **Automated Testing**: Tests on Ubuntu, Windows, macOS with Node.js 16, 18, 20, 22
- **Security Audits**: Automated npm audit checks
- **Native Module Building**: Cross-platform compilation testing
- **Automated Publishing**: Publishes to npm on release tags
- **Performance Benchmarks**: Runs benchmarks on main branch pushes

**Files Created:**
- `.github/workflows/ci.yml` - Main CI/CD pipeline
- `.github/workflows/release.yml` - Release automation with binary builds

### 2. Comprehensive Examples
- **Simple Transfer** (`examples/simple_transfer.js`) - Basic usage demonstration
- **Streaming Server** (`examples/streaming_server.js`) - Interactive multi-file server
- **Batch Downloader** (`examples/batch_downloader.js`) - Concurrent downloads with retry logic
- **Performance Monitor** (`examples/performance_monitor.js`) - Real-time performance tracking

**Run Examples:**
```bash
npm run example              # Simple transfer
npm run examples:server      # Interactive server
npm run examples:batch       # Batch downloader  
npm run examples:monitor     # Performance monitor
```

### 3. Performance Benchmark Suite
- **Comprehensive Testing**: Small, medium, large file benchmarks
- **Chunk Size Optimization**: Tests different chunk sizes for optimal performance
- **Concurrency Testing**: Multi-transfer performance analysis
- **Network Condition Simulation**: LAN, WAN, Mobile configurations
- **Memory Usage Analysis**: Memory consumption tracking
- **Detailed Reporting**: JSON reports with system information

**Run Benchmarks:**
```bash
npm run benchmark            # Full benchmark suite
npm run benchmark:memory     # Memory-focused benchmarks
```

### 4. Documentation Site
- **GitHub Pages Ready**: Jekyll configuration for automatic deployment
- **Comprehensive Docs**: Getting started, API reference, examples
- **Professional Theme**: Clean, responsive documentation site

**Documentation Structure:**
```
docs/
├── README.md              # Main documentation hub
├── getting-started.md     # Installation and basic usage
├── api-reference.md       # Detailed API documentation
├── performance-guide.md   # Performance optimization tips
├── troubleshooting.md     # Common issues and solutions
└── _config.yml           # Jekyll configuration
```

**Serve Docs Locally:**
```bash
npm run docs:serve         # Start Jekyll development server
npm run docs:build         # Build static site
```

### 5. Binary Release System
- **Cross-Platform Builds**: Automated binary compilation for Linux, macOS, Windows
- **Node.js ABI Support**: Binaries for multiple Node.js versions
- **Smart Installer**: Falls back to compilation if pre-built binary unavailable
- **Release Automation**: Integrated with GitHub releases

**Build Binaries:**
```bash
npm run build:binaries     # Build platform-specific binaries
npm run create:installer   # Create binary installer script
```

## 🛠️ Development Workflow

### Setting Up Development Environment

1. **Clone Repository**
   ```bash
   git clone https://github.com/red-giant/rgtp.git
   cd rgtp/bindings/node
   ```

2. **Install Dependencies**
   ```bash
   npm install
   ```

3. **Run Tests**
   ```bash
   npm test
   ```

4. **Try Examples**
   ```bash
   npm run example
   ```

### Making Changes

1. **Code Changes**: Edit source files in `src/`, `index.js`, etc.
2. **Add Tests**: Update `test/test.js` for new functionality
3. **Update Documentation**: Modify relevant docs in `docs/`
4. **Check Status**: Run `npm run check` to verify setup
5. **Run Tests**: `npm test` (works with or without native module)
6. **Run Benchmarks**: `npm run benchmark` (works with mock data)

### Release Process

1. **Update Version**: Bump version in `package.json`
2. **Update Changelog**: Add changes to `CHANGELOG.md`
3. **Create Git Tag**: `git tag v1.x.x`
4. **Push Tag**: `git push origin v1.x.x`
5. **GitHub Actions**: Automatically builds, tests, and publishes

## 📊 Performance Monitoring

The benchmark suite provides detailed performance analysis:

- **Throughput Testing**: Measures MB/s across different file sizes
- **Latency Analysis**: Connection and transfer timing
- **Efficiency Metrics**: Packet loss and retransmission rates
- **Memory Profiling**: Heap usage during transfers
- **Network Adaptation**: Performance under different conditions

## 🎭 Mock System

RGTP includes a comprehensive mock system that allows development and testing without building the native module:

### Features
- **Realistic Performance Simulation**: Mock data mimics real transfer characteristics
- **Full API Compatibility**: All methods and events work identically
- **CI/CD Friendly**: Tests and benchmarks run in any environment
- **Development Speed**: No compilation delays during development

### Usage
```bash
npm run check      # Check if native module is available
npm test           # Always works (uses mocks if needed)
npm run benchmark  # Always works (uses mocks if needed)
npm run example    # May fail without native module
```

### Mock vs Real Performance
- **Mock**: Simulated performance data for testing framework
- **Real**: Actual RGTP protocol performance measurements
- **Indicator**: Look for "(mock)" in version strings and output

## 🔧 Troubleshooting Development Issues

### Native Module Build Failures
```bash
# Windows: Install Visual Studio Build Tools
npm install --global windows-build-tools

# macOS: Install Xcode Command Line Tools
xcode-select --install

# Linux: Install build essentials
sudo apt-get install build-essential python3-dev
```

### Test Failures
- Check if native module compiled successfully
- Verify Node.js version compatibility (14+)
- Run tests with verbose output: `npm test -- --verbose`

### Performance Issues
- Run memory benchmarks: `npm run benchmark:memory`
- Check system resources during tests
- Try different chunk sizes and configurations

## 📈 Metrics and Analytics

### CI/CD Metrics
- Build success rate across platforms
- Test coverage and pass rates
- Performance regression detection
- Security vulnerability scanning

### Performance Metrics
- Throughput (MB/s) across file sizes
- Memory usage patterns
- Network efficiency percentages
- Cross-platform performance comparison

## 🤝 Contributing

1. **Fork Repository**: Create your own fork
2. **Create Branch**: `git checkout -b feature/amazing-feature`
3. **Make Changes**: Implement your feature
4. **Add Tests**: Ensure good test coverage
5. **Run Benchmarks**: Verify performance impact
6. **Update Docs**: Document new functionality
7. **Submit PR**: Create pull request with detailed description

## 📚 Additional Resources

- [RGTP Protocol Specification](../docs/protocol-spec.md)
- [Performance Optimization Guide](docs/performance-guide.md)
- [API Reference](docs/api-reference.md)
- [Example Applications](examples/)
- [Benchmark Results](benchmark/)

## 🎯 Next Steps

1. **Enhanced Examples**: More real-world use cases
2. **Performance Optimizations**: Profile and optimize hot paths
3. **Protocol Extensions**: Additional RGTP features
4. **Integration Tests**: End-to-end testing scenarios
5. **Documentation Expansion**: Video tutorials, advanced guides