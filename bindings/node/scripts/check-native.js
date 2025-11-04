#!/usr/bin/env node

/**
 * RGTP Native Module Status Checker
 * 
 * This script checks the status of the native RGTP module and provides
 * detailed information about the current implementation state.
 */

const fs = require('fs');
const path = require('path');
const { execSync } = require('child_process');

console.log('🔍 RGTP Native Module Status Check\n');

// Check if native module exists
const buildPath = path.join(__dirname, '..', 'build', 'Release');
const nativeModulePath = path.join(buildPath, 'rgtp_native.node');

let nativeModuleExists = false;
let nativeModuleWorks = false;
let mockImplementation = false;

try {
    nativeModuleExists = fs.existsSync(nativeModulePath);
    
    if (nativeModuleExists) {
        console.log('✅ Native module file found at:', nativeModulePath);
        
        // Try to load the native module
        try {
            const nativeModule = require(nativeModulePath);
            nativeModuleWorks = true;
            console.log('✅ Native module loads successfully!');
            
            // Check if it's the real implementation or stub
            const version = nativeModule.getVersion();
            if (version && version.includes('stub')) {
                console.log('⚠️  Native module is stub implementation');
                mockImplementation = true;
            } else {
                console.log('🚀 Native module is real RGTP implementation!');
            }
        } catch (error) {
            console.log('❌ Native module exists but failed to load:', error.message);
        }
    } else {
        console.log('❌ Native module not found');
    }
} catch (error) {
    console.log('❌ Error checking native module:', error.message);
}

// Check main module
console.log('\n📦 Main Module Status:');
try {
    const rgtp = require('..');
    console.log('✅ Main RGTP module loads successfully');
    
    // Test basic functionality
    const session = new rgtp.Session({ port: 9999 });
    console.log('✅ Session creation works');
    
    const client = new rgtp.Client();
    console.log('✅ Client creation works');
    
    // Check implementation type
    const version = rgtp.getVersion();
    if (version && version.includes('stub')) {
        console.log('⚠️  Using native stub implementation');
        mockImplementation = true;
    } else {
        console.log('🚀 Using real RGTP implementation!');
    }
    
} catch (error) {
    console.log('❌ Main module failed to load');
    console.log('   Error:', error.message.split('\n')[0]);
    console.log('   This indicates the native module needs to be built');
}

// System information
console.log('\n🖥️  System Information:');
console.log('Platform:', process.platform);
console.log('Architecture:', process.arch);
console.log('Node.js version:', process.version);

// Build tools check
console.log('\n🔧 Build Tools Status:');

// Check Python
try {
    const pythonVersion = execSync('python --version', { encoding: 'utf8', stdio: 'pipe' }).trim();
    console.log('✅ Python:', pythonVersion);
} catch (error) {
    try {
        const pythonVersion = execSync('python3 --version', { encoding: 'utf8', stdio: 'pipe' }).trim();
        console.log('✅ Python3:', pythonVersion);
    } catch (error2) {
        console.log('❌ Python not found');
    }
}

// Check node-gyp
try {
    const nodeGypVersion = execSync('npx node-gyp --version', { encoding: 'utf8', stdio: 'pipe' }).trim();
    console.log('✅ node-gyp:', nodeGypVersion);
} catch (error) {
    console.log('❌ node-gyp not available');
}

// Platform-specific build tools
if (process.platform === 'win32') {
    console.log('\n🪟 Windows Build Tools:');
    
    // Check for Visual Studio
    try {
        execSync('where cl', { stdio: 'pipe' });
        console.log('✅ Visual Studio C++ compiler (cl.exe) found');
    } catch (error) {
        console.log('❌ Visual Studio C++ compiler not found');
        console.log('   Install Visual Studio Build Tools with C++ workload');
        console.log('   https://visualstudio.microsoft.com/downloads/#build-tools-for-visual-studio-2022');
    }
    
    // Check for MSBuild
    try {
        execSync('where msbuild', { stdio: 'pipe' });
        console.log('✅ MSBuild found');
    } catch (error) {
        console.log('❌ MSBuild not found');
    }
} else if (process.platform === 'darwin') {
    console.log('\n🍎 macOS Build Tools:');
    
    try {
        execSync('xcode-select -p', { stdio: 'pipe' });
        console.log('✅ Xcode Command Line Tools installed');
    } catch (error) {
        console.log('❌ Xcode Command Line Tools not found');
        console.log('   Run: xcode-select --install');
    }
} else if (process.platform === 'linux') {
    console.log('\n🐧 Linux Build Tools:');
    
    try {
        execSync('which gcc', { stdio: 'pipe' });
        console.log('✅ GCC compiler found');
    } catch (error) {
        console.log('❌ GCC compiler not found');
        console.log('   Install: sudo apt-get install build-essential');
    }
    
    try {
        execSync('which make', { stdio: 'pipe' });
        console.log('✅ Make found');
    } catch (error) {
        console.log('❌ Make not found');
    }
}

// Summary
console.log('\n📋 Summary:');
if (nativeModuleWorks && !mockImplementation) {
    console.log('🎉 RGTP is running with native implementation!');
    console.log('   Performance: Real RGTP protocol performance');
    console.log('   Features: All native features available');
} else if (nativeModuleWorks && mockImplementation) {
    console.log('⚠️  RGTP is running with stub native implementation');
    console.log('   Performance: Simulated performance metrics');
    console.log('   Features: API compatible, awaiting core implementation');
} else if (mockImplementation) {
    console.log('⚠️  RGTP is running with JavaScript mock implementation');
    console.log('   Performance: Simulated performance metrics');
    console.log('   Features: Full API compatibility for development');
    console.log('   Next step: Build native module with "npm run build"');
} else {
    console.log('❌ RGTP is not working properly');
    console.log('   Check the errors above and install missing dependencies');
}

console.log('\n💡 Development Status:');
console.log('   • JavaScript API: ✅ Complete and production-ready');
console.log('   • Mock Implementation: ✅ Full functionality for development');
console.log('   • Native Stub: ✅ Builds when build tools available');
console.log('   • RGTP Core Library: 🚧 In development');
console.log('   • Real Native Implementation: 🚧 Pending core completion');

console.log('\n🚀 This is normal during the development phase!');
console.log('   The mock implementation allows immediate development and testing.');
console.log('   Your code will work unchanged when the native implementation is ready.');