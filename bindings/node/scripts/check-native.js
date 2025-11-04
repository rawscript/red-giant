#!/usr/bin/env node

/**
 * RGTP Native Module Checker
 * 
 * This script checks if the native module is available and provides
 * helpful guidance for building it.
 */

const fs = require('fs');
const path = require('path');
const os = require('os');

function checkNativeModule() {
  console.log('🔍 RGTP Native Module Status Check');
  console.log('==================================\n');

  // Check if build directory exists
  const buildDir = path.join(__dirname, '..', 'build');
  const releaseDir = path.join(buildDir, 'Release');
  const nativeModule = path.join(releaseDir, 'rgtp_native.node');

  console.log('📁 Checking build directories...');
  console.log(`   Build dir: ${fs.existsSync(buildDir) ? '✅ exists' : '❌ missing'}`);
  console.log(`   Release dir: ${fs.existsSync(releaseDir) ? '✅ exists' : '❌ missing'}`);
  console.log(`   Native module: ${fs.existsSync(nativeModule) ? '✅ exists' : '❌ missing'}\n`);

  // Try to load the module
  let moduleLoadable = false;
  try {
    require('../index.js');
    moduleLoadable = true;
    console.log('✅ Native module loads successfully!\n');
  } catch (error) {
    console.log('❌ Native module failed to load:');
    console.log(`   Error: ${error.message}\n`);
  }

  // System information
  console.log('🖥️  System Information:');
  console.log(`   OS: ${os.type()} ${os.release()}`);
  console.log(`   Architecture: ${os.arch()}`);
  console.log(`   Node.js: ${process.version}`);
  console.log(`   Platform: ${process.platform}\n`);

  // Check build tools
  console.log('🔧 Build Tools Check:');
  
  try {
    const { execSync } = require('child_process');
    
    // Check node-gyp
    try {
      execSync('node-gyp --version', { stdio: 'pipe' });
      console.log('   node-gyp: ✅ available');
    } catch {
      console.log('   node-gyp: ❌ not found');
    }

    // Check Python
    try {
      const pythonVersion = execSync('python --version', { stdio: 'pipe' }).toString().trim();
      console.log(`   Python: ✅ ${pythonVersion}`);
    } catch {
      try {
        const python3Version = execSync('python3 --version', { stdio: 'pipe' }).toString().trim();
        console.log(`   Python: ✅ ${python3Version}`);
      } catch {
        console.log('   Python: ❌ not found');
      }
    }

    // Platform-specific checks
    if (process.platform === 'win32') {
      // Check for Visual Studio
      try {
        execSync('where cl', { stdio: 'pipe' });
        console.log('   Visual Studio: ✅ compiler available');
      } catch {
        console.log('   Visual Studio: ❌ compiler not found');
      }
    } else {
      // Check for GCC/Clang
      try {
        const gccVersion = execSync('gcc --version', { stdio: 'pipe' }).toString().split('\n')[0];
        console.log(`   GCC: ✅ ${gccVersion}`);
      } catch {
        try {
          const clangVersion = execSync('clang --version', { stdio: 'pipe' }).toString().split('\n')[0];
          console.log(`   Clang: ✅ ${clangVersion}`);
        } catch {
          console.log('   Compiler: ❌ not found');
        }
      }
    }
  } catch (error) {
    console.log('   Build tools check failed');
  }

  console.log('');

  // Provide recommendations
  if (!moduleLoadable) {
    console.log('💡 Recommendations:');
    console.log('==================\n');

    if (!fs.existsSync(nativeModule)) {
      console.log('🔨 To build the native module:');
      console.log('   npm install          # Install dependencies and build');
      console.log('   # OR');
      console.log('   npm run build        # Build only');
      console.log('   npm run rebuild      # Clean and rebuild\n');
    }

    if (process.platform === 'win32') {
      console.log('🪟 Windows-specific setup:');
      console.log('   # Install Visual Studio Build Tools');
      console.log('   npm install --global windows-build-tools');
      console.log('   # OR install Visual Studio Community with C++ workload\n');
    } else if (process.platform === 'darwin') {
      console.log('🍎 macOS-specific setup:');
      console.log('   # Install Xcode Command Line Tools');
      console.log('   xcode-select --install\n');
    } else {
      console.log('🐧 Linux-specific setup:');
      console.log('   # Ubuntu/Debian');
      console.log('   sudo apt-get install build-essential python3-dev');
      console.log('   # CentOS/RHEL');
      console.log('   sudo yum groupinstall "Development Tools"');
      console.log('   sudo yum install python3-devel\n');
    }

    console.log('🧪 Testing without native module:');
    console.log('   npm test             # Run tests (works with mocks)');
    console.log('   npm run benchmark    # Run benchmarks (works with mocks)');
    console.log('   npm run example      # Run examples (may fail without native module)\n');

    console.log('📚 For more help:');
    console.log('   https://github.com/nodejs/node-gyp#installation');
    console.log('   https://github.com/red-giant/rgtp/issues');
  } else {
    console.log('🎉 Everything looks good!');
    console.log('=========================\n');
    console.log('✅ Native module is built and working');
    console.log('✅ You can run all examples and benchmarks');
    console.log('✅ Ready for production use\n');
    
    console.log('🚀 Try these commands:');
    console.log('   npm run example              # Simple transfer demo');
    console.log('   npm run examples:server      # Interactive server');
    console.log('   npm run benchmark            # Performance benchmarks');
    console.log('   npm test                     # Run test suite');
  }
}

if (require.main === module) {
  checkNativeModule();
}

module.exports = checkNativeModule;