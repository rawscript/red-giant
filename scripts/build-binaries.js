#!/usr/bin/env node

/**
 * RGTP Binary Build Script
 * 
 * This script helps build and package pre-compiled binaries
 * for different platforms to avoid compilation during npm install.
 */

const fs = require('fs');
const path = require('path');
const { execSync } = require('child_process');
const os = require('os');

class BinaryBuilder {
  constructor() {
    this.nodePackagePath = path.join(__dirname, '..', 'bindings', 'node');
    this.outputDir = path.join(__dirname, '..', 'dist', 'binaries');
    
    // Ensure output directory exists
    if (!fs.existsSync(this.outputDir)) {
      fs.mkdirSync(this.outputDir, { recursive: true });
    }
  }

  async buildAll() {
    console.log('🔨 RGTP Binary Builder');
    console.log('======================\n');

    const platforms = this.getSupportedPlatforms();
    
    for (const platform of platforms) {
      if (this.canBuildForPlatform(platform)) {
        console.log(`\n📦 Building for ${platform.name}...`);
        await this.buildForPlatform(platform);
      } else {
        console.log(`\n⏭️  Skipping ${platform.name} (not supported on current platform)`);
      }
    }

    this.generateIndex();
    console.log('\n✅ Binary build complete!');
  }

  getSupportedPlatforms() {
    return [
      {
        name: 'linux-x64',
        os: 'linux',
        arch: 'x64',
        nodeAbi: ['108', '115', '120', '127'], // Node 18, 19, 20, 22
        extension: '.node'
      },
      {
        name: 'darwin-x64',
        os: 'darwin',
        arch: 'x64',
        nodeAbi: ['108', '115', '120', '127'],
        extension: '.node'
      },
      {
        name: 'darwin-arm64',
        os: 'darwin',
        arch: 'arm64',
        nodeAbi: ['108', '115', '120', '127'],
        extension: '.node'
      },
      {
        name: 'win32-x64',
        os: 'win32',
        arch: 'x64',
        nodeAbi: ['108', '115', '120', '127'],
        extension: '.node'
      }
    ];
  }

  canBuildForPlatform(platform) {
    const currentOS = os.platform();
    const currentArch = os.arch();
    
    // Can build for same OS
    if (platform.os === currentOS) {
      return true;
    }
    
    // Cross-compilation support (limited)
    if (currentOS === 'linux' && platform.os === 'linux') {
      return true;
    }
    
    return false;
  }

  async buildForPlatform(platform) {
    try {
      // Clean previous builds
      const buildDir = path.join(this.nodePackagePath, 'build');
      if (fs.existsSync(buildDir)) {
        fs.rmSync(buildDir, { recursive: true, force: true });
      }

      // Set environment variables for cross-compilation
      const env = { ...process.env };
      if (platform.os !== os.platform()) {
        env.CC = this.getCrossCompiler(platform);
        env.CXX = this.getCrossCompilerCXX(platform);
        env.AR = this.getArchiver(platform);
        env.STRIP = this.getStripper(platform);
      }

      // Build the native module
      console.log('  🔧 Compiling native module...');
      execSync('npm run build', {
        cwd: this.nodePackagePath,
        env,
        stdio: 'pipe'
      });

      // Package binaries for each Node.js ABI version
      for (const abi of platform.nodeAbi) {
        await this.packageBinary(platform, abi);
      }

    } catch (error) {
      console.error(`  ❌ Build failed for ${platform.name}:`, error.message);
    }
  }

  async packageBinary(platform, nodeAbi) {
    const buildPath = path.join(this.nodePackagePath, 'build', 'Release', 'rgtp_native.node');
    
    if (!fs.existsSync(buildPath)) {
      console.warn(`  ⚠️  Binary not found: ${buildPath}`);
      return;
    }

    // Create platform-specific directory
    const platformDir = path.join(this.outputDir, platform.name, `node-${nodeAbi}`);
    if (!fs.existsSync(platformDir)) {
      fs.mkdirSync(platformDir, { recursive: true });
    }

    // Copy binary
    const targetPath = path.join(platformDir, 'rgtp_native.node');
    fs.copyFileSync(buildPath, targetPath);

    // Create package info
    const packageInfo = {
      platform: platform.name,
      nodeAbi,
      arch: platform.arch,
      os: platform.os,
      buildDate: new Date().toISOString(),
      fileSize: fs.statSync(targetPath).size
    };

    fs.writeFileSync(
      path.join(platformDir, 'package.json'),
      JSON.stringify(packageInfo, null, 2)
    );

    console.log(`    ✅ Packaged for Node.js ABI ${nodeAbi}`);
  }

  getCrossCompiler(platform) {
    const compilers = {
      'linux-x64': 'gcc',
      'linux-arm64': 'aarch64-linux-gnu-gcc',
      'win32-x64': 'x86_64-w64-mingw32-gcc'
    };
    return compilers[platform.name] || 'gcc';
  }

  getCrossCompilerCXX(platform) {
    const compilers = {
      'linux-x64': 'g++',
      'linux-arm64': 'aarch64-linux-gnu-g++',
      'win32-x64': 'x86_64-w64-mingw32-g++'
    };
    return compilers[platform.name] || 'g++';
  }

  getArchiver(platform) {
    const archivers = {
      'linux-x64': 'ar',
      'linux-arm64': 'aarch64-linux-gnu-ar',
      'win32-x64': 'x86_64-w64-mingw32-ar'
    };
    return archivers[platform.name] || 'ar';
  }

  getStripper(platform) {
    const strippers = {
      'linux-x64': 'strip',
      'linux-arm64': 'aarch64-linux-gnu-strip',
      'win32-x64': 'x86_64-w64-mingw32-strip'
    };
    return strippers[platform.name] || 'strip';
  }

  generateIndex() {
    const index = {
      version: require('../bindings/node/package.json').version,
      buildDate: new Date().toISOString(),
      platforms: {}
    };

    // Scan output directory for built binaries
    const platforms = fs.readdirSync(this.outputDir);
    
    for (const platform of platforms) {
      const platformPath = path.join(this.outputDir, platform);
      if (!fs.statSync(platformPath).isDirectory()) continue;

      index.platforms[platform] = {};
      
      const nodeVersions = fs.readdirSync(platformPath);
      for (const nodeVersion of nodeVersions) {
        const versionPath = path.join(platformPath, nodeVersion);
        if (!fs.statSync(versionPath).isDirectory()) continue;

        const packageInfoPath = path.join(versionPath, 'package.json');
        if (fs.existsSync(packageInfoPath)) {
          const packageInfo = JSON.parse(fs.readFileSync(packageInfoPath, 'utf8'));
          index.platforms[platform][nodeVersion] = packageInfo;
        }
      }
    }

    // Write index file
    fs.writeFileSync(
      path.join(this.outputDir, 'index.json'),
      JSON.stringify(index, null, 2)
    );

    console.log(`\n📋 Binary index created with ${Object.keys(index.platforms).length} platforms`);
  }

  async createInstaller() {
    console.log('\n📦 Creating binary installer...');
    
    const installerScript = `#!/usr/bin/env node

/**
 * RGTP Binary Installer
 * 
 * This script downloads and installs pre-built binaries
 * to avoid compilation during npm install.
 */

const fs = require('fs');
const path = require('path');
const https = require('https');
const os = require('os');

class BinaryInstaller {
  constructor() {
    this.baseUrl = 'https://github.com/red-giant/rgtp/releases/download';
    this.version = require('./package.json').version;
    this.platform = this.getPlatformName();
    this.nodeAbi = process.versions.modules;
  }

  getPlatformName() {
    const platform = os.platform();
    const arch = os.arch();
    return \`\${platform}-\${arch}\`;
  }

  async install() {
    console.log('🔍 Checking for pre-built binary...');
    console.log(\`   Platform: \${this.platform}\`);
    console.log(\`   Node ABI: \${this.nodeAbi}\`);

    const binaryUrl = \`\${this.baseUrl}/v\${this.version}/\${this.platform}-node\${this.nodeAbi}.tar.gz\`;
    
    try {
      await this.downloadBinary(binaryUrl);
      console.log('✅ Pre-built binary installed successfully!');
      return true;
    } catch (error) {
      console.log('⚠️  Pre-built binary not available, falling back to compilation');
      return false;
    }
  }

  async downloadBinary(url) {
    return new Promise((resolve, reject) => {
      const request = https.get(url, (response) => {
        if (response.statusCode !== 200) {
          reject(new Error(\`HTTP \${response.statusCode}\`));
          return;
        }

        const buildDir = path.join(__dirname, 'build', 'Release');
        if (!fs.existsSync(buildDir)) {
          fs.mkdirSync(buildDir, { recursive: true });
        }

        const file = fs.createWriteStream(path.join(buildDir, 'rgtp_native.node'));
        response.pipe(file);

        file.on('finish', () => {
          file.close();
          resolve();
        });

        file.on('error', reject);
      });

      request.on('error', reject);
    });
  }
}

if (require.main === module) {
  const installer = new BinaryInstaller();
  installer.install().catch(() => {
    // Fallback to node-gyp build
    const { execSync } = require('child_process');
    try {
      execSync('node-gyp rebuild', { stdio: 'inherit' });
    } catch (error) {
      console.error('❌ Binary installation and compilation both failed');
      process.exit(1);
    }
  });
}

module.exports = BinaryInstaller;
`;

    fs.writeFileSync(
      path.join(this.nodePackagePath, 'install.js'),
      installerScript
    );

    console.log('✅ Binary installer created');
  }
}

async function main() {
  const builder = new BinaryBuilder();
  
  const command = process.argv[2];
  
  switch (command) {
    case 'build':
      await builder.buildAll();
      break;
    case 'installer':
      await builder.createInstaller();
      break;
    case 'all':
      await builder.buildAll();
      await builder.createInstaller();
      break;
    default:
      console.log('Usage: node build-binaries.js [build|installer|all]');
      console.log('');
      console.log('Commands:');
      console.log('  build     - Build binaries for supported platforms');
      console.log('  installer - Create binary installer script');
      console.log('  all       - Build binaries and create installer');
  }
}

if (require.main === module) {
  main().catch(console.error);
}

module.exports = BinaryBuilder;