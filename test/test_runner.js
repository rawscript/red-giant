#!/usr/bin/env node

/**
 * Comprehensive RGTP Test Runner
 * Executes all test suites and provides detailed reporting
 */

const Mocha = require('mocha');
const fs = require('fs');
const path = require('path');

// Test configuration
const TEST_CONFIG = {
    timeout: 60000,        // 60 seconds default timeout
    reporter: 'spec',      // Detailed test output
    bail: false,          // Continue running tests after failures
    slow: 5000,           // Mark tests as slow after 5 seconds
    color: true           // Enable colored output
};

class RGTPTestRunner {
    constructor() {
        this.mocha = new Mocha(TEST_CONFIG);
        this.testResults = {
            passed: [],
            failed: [],
            pending: [],
            skipped: [],
            duration: 0
        };
    }

    /**
     * Discover and add test files to the test suite
     */
    discoverTests(testDir = './') {
        const testFiles = [];
        
        // Look for all JS test files in the test directory
        const files = fs.readdirSync(testDir);
        
        for (const file of files) {
            const fullPath = path.join(testDir, file);
            const stat = fs.statSync(fullPath);
            
            if (stat.isDirectory()) {
                // Recursively discover tests in subdirectories
                testFiles.push(...this.discoverTests(fullPath));
            } else if (file.endsWith('.js') && file.startsWith('test_')) {
                // Add test file if it matches the pattern
                testFiles.push(fullPath);
            }
        }
        
        return testFiles;
    }

    /**
     * Add test files to the Mocha instance
     */
    addTestFiles(testFiles) {
        for (const file of testFiles) {
            console.log(`Adding test file: ${file}`);
            this.mocha.addFile(file);
        }
    }

    /**
     * Run the complete test suite
     */
    async run() {
        console.log('🚀 Starting RGTP Comprehensive Test Suite');
        console.log(`📅 ${new Date().toISOString()}`);
        console.log(`⚙️  Configuration:`, TEST_CONFIG);
        console.log('');

        // Discover all test files
        const testDir = __dirname;
        const testFiles = this.discoverTests(testDir);
        
        if (testFiles.length === 0) {
            console.error('❌ No test files found!');
            return false;
        }

        console.log(`📋 Found ${testFiles.length} test files:`);
        testFiles.forEach(file => console.log(`   • ${path.basename(file)}`));
        console.log('');

        // Add test files to Mocha
        this.addTestFiles(testFiles);

        return new Promise((resolve, reject) => {
            try {
                // Set up event handlers for detailed reporting
                this.setupEventHandlers();

                // Run the tests
                this.mocha.run((failures) => {
                    this.reportResults(failures);
                    resolve(failures === 0);
                });
            } catch (error) {
                console.error('❌ Error running tests:', error);
                reject(error);
            }
        });
    }

    /**
     * Set up event handlers for detailed test reporting
     */
    setupEventHandlers() {
        this.mocha.suite.on('start', () => {
            console.log('🧪 Test suite started...\n');
        });

        this.mocha.suite.on('suite', (suite) => {
            if (suite.title) {
                console.log(`\n📂 Suite: ${suite.title}`);
            }
        });

        this.mocha.suite.on('test', (test) => {
            // Individual test started
        });

        this.mocha.suite.on('pass', (test) => {
            console.log(`✅ PASSED: ${test.fullTitle()} (${test.duration}ms)`);
        });

        this.mocha.suite.on('fail', (test, err) => {
            console.log(`❌ FAILED: ${test.fullTitle()}`);
            console.log(`   Reason: ${err.message}`);
            if (err.stack) {
                console.log(`   Stack: ${err.stack.split('\n').slice(0, 3).join('\n        ')}`);
            }
        });

        this.mocha.suite.on('pending', (test) => {
            console.log(`⏭️  PENDING: ${test.fullTitle()}`);
        });

        this.mocha.suite.on('end', (suite) => {
            console.log(`\n🏁 Test suite completed`);
        });
    }

    /**
     * Generate and display test results report
     */
    reportResults(failures) {
        console.log('\n' + '='.repeat(60));
        console.log('📊 TEST RESULTS SUMMARY');
        console.log('='.repeat(60));

        // Get the actual results from the runner
        const runner = this.mocha.runner;
        if (runner) {
            console.log(`✅ Passed: ${runner.stats.passes}`);
            console.log(`❌ Failed: ${runner.stats.failures}`);
            console.log(`⏭️  Pending: ${runner.stats.pending}`);
            console.log(`📝 Total: ${runner.stats.tests}`);
            console.log(`⏱️  Duration: ${(runner.stats.duration / 1000).toFixed(2)}s`);
        }

        console.log('='.repeat(60));

        if (failures > 0) {
            console.log(`\n🚨 ${failures} test(s) failed!`);
        } else {
            console.log('\n🎉 All tests passed!');
        }

        console.log('');
    }
}

/**
 * Run tests with different configurations
 */
async function runConfiguredTests() {
    const runner = new RGTPTestRunner();
    
    // Check if specific test file is requested
    const args = process.argv.slice(2);
    if (args.length > 0) {
        if (args[0] === '--help' || args[0] === '-h') {
            console.log('RGTP Test Runner');
            console.log('');
            console.log('Usage:');
            console.log('  node test_runner.js                    # Run all tests');
            console.log('  node test_runner.js --help            # Show this help');
            console.log('');
            console.log('Examples:');
            console.log('  node test_runner.js                   # Run complete test suite');
            return;
        }
    }

    try {
        const success = await runner.run();
        process.exit(success ? 0 : 1);
    } catch (error) {
        console.error('💥 Test runner failed:', error);
        process.exit(1);
    }
}

// Execute the test runner if this file is run directly
if (require.main === module) {
    runConfiguredTests().catch(err => {
        console.error('💥 Unhandled error:', err);
        process.exit(1);
    });
}

module.exports = RGTPTestRunner;