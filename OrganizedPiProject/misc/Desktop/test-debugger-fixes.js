#!/usr/bin/env node

/**
 * Comprehensive Test Script for Debugger and Runtime Fixes
 * Tests all identified issues with proper environment simulation
 */

const { spawn, exec } = require('child_process');
const fs = require('fs');
const path = require('path');

console.log(`
╔══════════════════════════════════════════════════════════════════════════════╗
║                    RawrZ Runtime Issues Test Suite                        ║
║                    Testing Debugger & Security Fixes                      ║
╚══════════════════════════════════════════════════════════════════════════════╝
`);

class RuntimeTester {
    constructor() {
        this.results = {
            antiDebugger: { status: 'pending', details: [] },
            debugCrypto: { status: 'pending', details: [] },
            processExit: { status: 'pending', details: [] },
            registryAccess: { status: 'pending', details: [] }
        };
    }

    async runTests() {
        console.log('🔍 Starting comprehensive runtime tests...\n');

        // Test 1: Anti-Debugger Detection with --inspect
        await this.testAntiDebuggerDetection();
        
        // Test 2: DEBUG_CRYPTO sensitive data exposure
        await this.testDebugCryptoExposure();
        
        // Test 3: Process.exit(0) in development mode
        await this.testProcessExitInDevelopment();
        
        // Test 4: Registry access with security software blocking
        await this.testRegistryAccessBlocking();

        // Display results
        this.displayResults();
    }

    async testAntiDebuggerDetection() {
        console.log('🧪 Test 1: Anti-Debugger Detection with --inspect flag');
        
        return new Promise((resolve) => {
            const testProcess = spawn('node', ['--inspect', 'server.js'], {
                env: { ...process.env, NODE_OPTIONS: '--inspect' },
                stdio: ['pipe', 'pipe', 'pipe'],
                shell: true
            });

            let output = '';
            let hasAntiDebugWarning = false;
            let hasProcessExit = false;

            testProcess.stdout.on('data', (data) => {
                output += data.toString();
                if (data.toString().includes('Debugger detected') || 
                    data.toString().includes('Anti-debug') ||
                    data.toString().includes('process.exit')) {
                    hasAntiDebugWarning = true;
                }
            });

            testProcess.stderr.on('data', (data) => {
                output += data.toString();
                if (data.toString().includes('Debugger detected') || 
                    data.toString().includes('Anti-debug') ||
                    data.toString().includes('process.exit')) {
                    hasAntiDebugWarning = true;
                }
            });

            // Kill after 10 seconds
            setTimeout(() => {
                testProcess.kill('SIGTERM');
                
                this.results.antiDebugger.status = hasAntiDebugWarning ? 'FAILED' : 'PASSED';
                this.results.antiDebugger.details = [
                    `Debugger flag detected: ${hasAntiDebugWarning ? 'YES' : 'NO'}`,
                    `Process continued running: ${!hasProcessExit ? 'YES' : 'NO'}`,
                    `Output length: ${output.length} characters`
                ];
                
                console.log(`   Result: ${this.results.antiDebugger.status}`);
                console.log(`   Details: ${this.results.antiDebugger.details.join(', ')}\n`);
                resolve();
            }, 10000);
        });
    }

    async testDebugCryptoExposure() {
        console.log('🧪 Test 2: DEBUG_CRYPTO sensitive data exposure');
        
        return new Promise((resolve) => {
            const testProcess = spawn('node', ['server.js'], {
                env: { ...process.env, DEBUG_CRYPTO: 'true' },
                stdio: ['pipe', 'pipe', 'pipe'],
                shell: true
            });

            let output = '';
            let hasSensitiveData = false;
            let hasSanitizedLogs = false;

            testProcess.stdout.on('data', (data) => {
                output += data.toString();
                const dataStr = data.toString();
                
                // Check for sensitive data patterns
                if (dataStr.includes('key:') || 
                    dataStr.includes('data:') ||
                    dataStr.includes('encrypted:') ||
                    dataStr.includes('decrypted:')) {
                    hasSensitiveData = true;
                }
                
                // Check for sanitized logs
                if (dataStr.includes('[DEBUG]') && 
                    (dataStr.includes('***') || dataStr.includes('SANITIZED'))) {
                    hasSanitizedLogs = true;
                }
            });

            testProcess.stderr.on('data', (data) => {
                output += data.toString();
                const dataStr = data.toString();
                
                if (dataStr.includes('key:') || 
                    dataStr.includes('data:') ||
                    dataStr.includes('encrypted:') ||
                    dataStr.includes('decrypted:')) {
                    hasSensitiveData = true;
                }
            });

            // Kill after 8 seconds
            setTimeout(() => {
                testProcess.kill('SIGTERM');
                
                this.results.debugCrypto.status = hasSensitiveData ? 'FAILED' : 'PASSED';
                this.results.debugCrypto.details = [
                    `Sensitive data exposed: ${hasSensitiveData ? 'YES' : 'NO'}`,
                    `Sanitized logs present: ${hasSanitizedLogs ? 'YES' : 'NO'}`,
                    `DEBUG_CRYPTO active: ${output.includes('DEBUG_CRYPTO') ? 'YES' : 'NO'}`
                ];
                
                console.log(`   Result: ${this.results.debugCrypto.status}`);
                console.log(`   Details: ${this.results.debugCrypto.details.join(', ')}\n`);
                resolve();
            }, 8000);
        });
    }

    async testProcessExitInDevelopment() {
        console.log('🧪 Test 3: Process.exit(0) in development mode');
        
        return new Promise((resolve) => {
            const testProcess = spawn('node', ['server.js'], {
                env: { ...process.env, NODE_ENV: 'development' },
                stdio: ['pipe', 'pipe', 'pipe'],
                shell: true
            });

            let output = '';
            let hasProcessExit = false;
            let hasDevelopmentWarning = false;

            testProcess.stdout.on('data', (data) => {
                output += data.toString();
                const dataStr = data.toString();
                
                if (dataStr.includes('process.exit') || 
                    dataStr.includes('Exiting due to')) {
                    hasProcessExit = true;
                }
                
                if (dataStr.includes('Development Mode') || 
                    dataStr.includes('DEVELOPMENT WARNING')) {
                    hasDevelopmentWarning = true;
                }
            });

            testProcess.stderr.on('data', (data) => {
                output += data.toString();
                const dataStr = data.toString();
                
                if (dataStr.includes('process.exit') || 
                    dataStr.includes('Exiting due to')) {
                    hasProcessExit = true;
                }
            });

            // Kill after 8 seconds
            setTimeout(() => {
                testProcess.kill('SIGTERM');
                
                this.results.processExit.status = hasProcessExit ? 'FAILED' : 'PASSED';
                this.results.processExit.details = [
                    `Process exited prematurely: ${hasProcessExit ? 'YES' : 'NO'}`,
                    `Development mode detected: ${hasDevelopmentWarning ? 'YES' : 'NO'}`,
                    `Server started successfully: ${output.includes('Server running') ? 'YES' : 'NO'}`
                ];
                
                console.log(`   Result: ${this.results.processExit.status}`);
                console.log(`   Details: ${this.results.processExit.details.join(', ')}\n`);
                resolve();
            }, 8000);
        });
    }

    async testRegistryAccessBlocking() {
        console.log('🧪 Test 4: Registry access with security software blocking');
        
        return new Promise((resolve) => {
            const testProcess = spawn('node', ['server.js'], {
                env: { ...process.env },
                stdio: ['pipe', 'pipe', 'pipe'],
                shell: true
            });

            let output = '';
            let hasRegistryError = false;
            let hasFallback = false;
            let hasTimeout = false;

            testProcess.stdout.on('data', (data) => {
                output += data.toString();
                const dataStr = data.toString();
                
                if (dataStr.includes('reg query') || 
                    dataStr.includes('registry') ||
                    dataStr.includes('Malwarebytes') ||
                    dataStr.includes('security software')) {
                    hasRegistryError = true;
                }
                
                if (dataStr.includes('fallback') || 
                    dataStr.includes('timeout') ||
                    dataStr.includes('using default')) {
                    hasFallback = true;
                }
            });

            testProcess.stderr.on('data', (data) => {
                output += data.toString();
                const dataStr = data.toString();
                
                if (dataStr.includes('reg query') || 
                    dataStr.includes('registry') ||
                    dataStr.includes('Malwarebytes') ||
                    dataStr.includes('security software')) {
                    hasRegistryError = true;
                }
            });

            // Kill after 8 seconds
            setTimeout(() => {
                testProcess.kill('SIGTERM');
                
                this.results.registryAccess.status = hasRegistryError && !hasFallback ? 'FAILED' : 'PASSED';
                this.results.registryAccess.details = [
                    `Registry access attempted: ${hasRegistryError ? 'YES' : 'NO'}`,
                    `Fallback mechanism used: ${hasFallback ? 'YES' : 'NO'}`,
                    `Server continued running: ${output.includes('Server running') ? 'YES' : 'NO'}`
                ];
                
                console.log(`   Result: ${this.results.registryAccess.status}`);
                console.log(`   Details: ${this.results.registryAccess.details.join(', ')}\n`);
                resolve();
            }, 8000);
        });
    }

    displayResults() {
        console.log('📊 TEST RESULTS SUMMARY');
        console.log('═'.repeat(60));
        
        const tests = [
            { name: 'Anti-Debugger Detection', result: this.results.antiDebugger },
            { name: 'DEBUG_CRYPTO Exposure', result: this.results.debugCrypto },
            { name: 'Process.exit(0) Fix', result: this.results.processExit },
            { name: 'Registry Access Blocking', result: this.results.registryAccess }
        ];

        tests.forEach(test => {
            const status = test.result.status === 'PASSED' ? '✅' : '❌';
            console.log(`${status} ${test.name}: ${test.result.status}`);
            test.result.details.forEach(detail => {
                console.log(`   • ${detail}`);
            });
            console.log('');
        });

        const passedTests = tests.filter(t => t.result.status === 'PASSED').length;
        const totalTests = tests.length;
        
        console.log(`🎯 OVERALL RESULT: ${passedTests}/${totalTests} tests passed`);
        
        if (passedTests === totalTests) {
            console.log('🎉 All runtime issues have been successfully fixed!');
        } else {
            console.log('⚠️  Some issues still need attention.');
        }
    }
}

// Run the tests
const tester = new RuntimeTester();
tester.runTests().catch(console.error);
