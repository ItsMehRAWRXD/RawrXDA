#!/usr/bin/env node

// RawrZ Stealth Engine Test Suite
const stealthEngine = require('./src/engines/stealth-engine');
const { logger } = require('./src/utils/logger');

class StealthEngineTester {
    constructor() {
        this.testResults = {
            passed: 0,
            failed: 0,
            total: 0,
            details: []
        };
    }

    async runAllTests() {
        console.log(' RawrZ Stealth Engine Test Suite');
        console.log('=====================================\n');

        try {
            // Initialize the stealth engine
            await this.testInitialization();
            
            // Test stealth modes
            await this.testStealthModes();
            
            // Test individual capabilities
            await this.testAntiDebug();
            await this.testAntiVM();
            await this.testAntiSandbox();
            await this.testAntiAnalysis();
            await this.testProcessHiding();
            await this.testMemoryProtection();
            await this.testNetworkStealth();
            
            // Test detection scan
            await this.testDetectionScan();
            
            // Test status and cleanup
            await this.testStatusAndCleanup();
            
            // Display results
            this.displayResults();
            
        } catch (error) {
            console.error(' Test suite failed:', error.message);
            process.exit(1);
        }
    }

    async testInitialization() {
        console.log(' Testing Stealth Engine Initialization...');
        
        try {
            await stealthEngine.initialize({ stealth: { enabled: true } });
            this.recordTest('Initialization', true, 'Stealth engine initialized successfully');
        } catch (error) {
            this.recordTest('Initialization', false, `Initialization failed: ${error.message}`);
        }
    }

    async testStealthModes() {
        console.log('\n Testing Stealth Modes...');
        
        const modes = ['basic', 'standard', 'full', 'maximum'];
        
        for (const mode of modes) {
            try {
                const result = await stealthEngine.enableStealth(mode);
                
                if (result.enabled && result.mode === mode) {
                    this.recordTest(`Stealth Mode: ${mode}`, true, 
                        `Mode enabled with ${Object.keys(result.capabilities).length} capabilities`);
                } else {
                    this.recordTest(`Stealth Mode: ${mode}`, false, 'Mode not properly enabled');
                }
            } catch (error) {
                this.recordTest(`Stealth Mode: ${mode}`, false, `Error: ${error.message}`);
            }
        }
    }

    async testAntiDebug() {
        console.log('\n Testing Anti-Debug Capabilities...');
        
        try {
            const result = await stealthEngine.enableAntiDebug();
            
            if (result.enabled && result.methods) {
                const methodCount = Object.keys(result.methods).length;
                this.recordTest('Anti-Debug', true, 
                    `Enabled with ${methodCount} detection methods, protection level: ${result.protectionLevel}`);
                
                // Test individual methods
                for (const [method, methodResult] of Object.entries(result.methods)) {
                    if (methodResult.detected === false && methodResult.confidence > 0) {
                        this.recordTest(`  - ${method}`, true, 
                            `Not detected (confidence: ${methodResult.confidence})`);
                    } else {
                        this.recordTest(`  - ${method}`, false, 
                            `Detection result: ${methodResult.detected}, confidence: ${methodResult.confidence}`);
                    }
                }
            } else {
                this.recordTest('Anti-Debug', false, 'Anti-debug not properly enabled');
            }
        } catch (error) {
            this.recordTest('Anti-Debug', false, `Error: ${error.message}`);
        }
    }

    async testAntiVM() {
        console.log('\n Testing Anti-VM Capabilities...');
        
        try {
            const result = await stealthEngine.enableAntiVM();
            
            if (result.enabled && result.methods) {
                const methodCount = Object.keys(result.methods).length;
                this.recordTest('Anti-VM', true, 
                    `Enabled with ${methodCount} detection methods, protection level: ${result.protectionLevel}`);
                
                // Test individual methods
                for (const [method, methodResult] of Object.entries(result.methods)) {
                    if (methodResult.detected === false && methodResult.confidence > 0) {
                        this.recordTest(`  - ${method}`, true, 
                            `VM not detected (confidence: ${methodResult.confidence})`);
                    } else {
                        this.recordTest(`  - ${method}`, false, 
                            `VM detection result: ${methodResult.detected}, confidence: ${methodResult.confidence}`);
                    }
                }
            } else {
                this.recordTest('Anti-VM', false, 'Anti-VM not properly enabled');
            }
        } catch (error) {
            this.recordTest('Anti-VM', false, `Error: ${error.message}`);
        }
    }

    async testAntiSandbox() {
        console.log('\n Testing Anti-Sandbox Capabilities...');
        
        try {
            const result = await stealthEngine.enableAntiSandbox();
            
            if (result.enabled && result.methods) {
                const methodCount = Object.keys(result.methods).length;
                this.recordTest('Anti-Sandbox', true, 
                    `Enabled with ${methodCount} detection methods, protection level: ${result.protectionLevel}`);
                
                // Test individual methods
                for (const [method, methodResult] of Object.entries(result.methods)) {
                    if (methodResult.detected === false && methodResult.confidence > 0) {
                        this.recordTest(`  - ${method}`, true, 
                            `Sandbox not detected (confidence: ${methodResult.confidence})`);
                    } else {
                        this.recordTest(`  - ${method}`, false, 
                            `Sandbox detection result: ${methodResult.detected}, confidence: ${methodResult.confidence}`);
                    }
                }
            } else {
                this.recordTest('Anti-Sandbox', false, 'Anti-Sandbox not properly enabled');
            }
        } catch (error) {
            this.recordTest('Anti-Sandbox', false, `Error: ${error.message}`);
        }
    }

    async testAntiAnalysis() {
        console.log('\n Testing Anti-Analysis Capabilities...');
        
        try {
            const result = await stealthEngine.enableAntiAnalysis();
            
            if (result.enabled && result.methods) {
                const methodCount = Object.keys(result.methods).length;
                this.recordTest('Anti-Analysis', true, 
                    `Enabled with ${methodCount} analysis protection methods, protection level: ${result.protectionLevel}`);
                
                // Test individual methods
                for (const [method, methodResult] of Object.entries(result.methods)) {
                    if (methodResult.enabled) {
                        this.recordTest(`  - ${method}`, true, 
                            `Enabled with level: ${methodResult.level}`);
                    } else {
                        this.recordTest(`  - ${method}`, false, 'Not enabled');
                    }
                }
            } else {
                this.recordTest('Anti-Analysis', false, 'Anti-Analysis not properly enabled');
            }
        } catch (error) {
            this.recordTest('Anti-Analysis', false, `Error: ${error.message}`);
        }
    }

    async testProcessHiding() {
        console.log('\n Testing Process Hiding Capabilities...');
        
        try {
            const result = await stealthEngine.enableProcessHiding();
            
            if (result.enabled && result.methods) {
                const methodCount = Object.keys(result.methods).length;
                this.recordTest('Process Hiding', true, 
                    `Enabled with ${methodCount} hiding methods, protection level: ${result.protectionLevel}`);
                
                // Test individual methods
                for (const [method, methodResult] of Object.entries(result.methods)) {
                    if (methodResult.enabled) {
                        this.recordTest(`  - ${method}`, true, 'Process hiding method enabled');
                    } else {
                        this.recordTest(`  - ${method}`, false, 'Process hiding method not enabled');
                    }
                }
            } else {
                this.recordTest('Process Hiding', false, 'Process Hiding not properly enabled');
            }
        } catch (error) {
            this.recordTest('Process Hiding', false, `Error: ${error.message}`);
        }
    }

    async testMemoryProtection() {
        console.log('\n Testing Memory Protection Capabilities...');
        
        try {
            const result = await stealthEngine.enableMemoryProtection();
            
            if (result.enabled && result.methods) {
                const methodCount = Object.keys(result.methods).length;
                this.recordTest('Memory Protection', true, 
                    `Enabled with ${methodCount} protection methods, protection level: ${result.protectionLevel}`);
                
                // Test individual methods
                for (const [method, methodResult] of Object.entries(result.methods)) {
                    if (methodResult.enabled) {
                        this.recordTest(`  - ${method}`, true, 'Memory protection method enabled');
                    } else {
                        this.recordTest(`  - ${method}`, false, 'Memory protection method not enabled');
                    }
                }
            } else {
                this.recordTest('Memory Protection', false, 'Memory Protection not properly enabled');
            }
        } catch (error) {
            this.recordTest('Memory Protection', false, `Error: ${error.message}`);
        }
    }

    async testNetworkStealth() {
        console.log('\n Testing Network Stealth Capabilities...');
        
        try {
            const result = await stealthEngine.enableNetworkStealth();
            
            if (result.enabled && result.methods) {
                const methodCount = Object.keys(result.methods).length;
                this.recordTest('Network Stealth', true, 
                    `Enabled with ${methodCount} stealth methods, protection level: ${result.protectionLevel}`);
                
                // Test individual methods
                for (const [method, methodResult] of Object.entries(result.methods)) {
                    if (methodResult.enabled) {
                        this.recordTest(`  - ${method}`, true, 'Network stealth method enabled');
                    } else {
                        this.recordTest(`  - ${method}`, false, 'Network stealth method not enabled');
                    }
                }
            } else {
                this.recordTest('Network Stealth', false, 'Network Stealth not properly enabled');
            }
        } catch (error) {
            this.recordTest('Network Stealth', false, `Error: ${error.message}`);
        }
    }

    async testDetectionScan() {
        console.log('\n Testing Detection Scan...');
        
        try {
            const results = await stealthEngine.runDetectionScan();
            
            if (results && Object.keys(results).length > 0) {
                this.recordTest('Detection Scan', true, 
                    `Scan completed with ${Object.keys(results).length} categories`);
                
                // Test each category
                for (const [category, methods] of Object.entries(results)) {
                    const methodCount = Object.keys(methods).length;
                    this.recordTest(`  - ${category}`, true, 
                        `Scanned ${methodCount} detection methods`);
                }
            } else {
                this.recordTest('Detection Scan', false, 'Detection scan returned no results');
            }
        } catch (error) {
            this.recordTest('Detection Scan', false, `Error: ${error.message}`);
        }
    }

    async testStatusAndCleanup() {
        console.log('\n Testing Status and Cleanup...');
        
        try {
            // Test status
            const status = stealthEngine.getStatus();
            if (status && status.availableModes && status.detectionMethods) {
                this.recordTest('Get Status', true, 
                    `Status retrieved with ${status.availableModes.length} available modes`);
            } else {
                this.recordTest('Get Status', false, 'Status not properly retrieved');
            }
            
            // Test disable
            const disableResult = await stealthEngine.disableStealth();
            if (disableResult && disableResult.enabled === false) {
                this.recordTest('Disable Stealth', true, 'Stealth mode disabled successfully');
            } else {
                this.recordTest('Disable Stealth', false, 'Stealth mode not properly disabled');
            }
            
            // Test cleanup
            await stealthEngine.cleanup();
            this.recordTest('Cleanup', true, 'Stealth engine cleanup completed');
            
        } catch (error) {
            this.recordTest('Status and Cleanup', false, `Error: ${error.message}`);
        }
    }

    recordTest(testName, passed, message) {
        this.testResults.total++;
        if (passed) {
            this.testResults.passed++;
            console.log(` ${testName}: ${message}`);
        } else {
            this.testResults.failed++;
            console.log(` ${testName}: ${message}`);
        }
        
        this.testResults.details.push({
            name: testName,
            passed,
            message
        });
    }

    displayResults() {
        console.log('\n Test Results Summary');
        console.log('========================');
        console.log(`Total Tests: ${this.testResults.total}`);
        console.log(`Passed: ${this.testResults.passed} `);
        console.log(`Failed: ${this.testResults.failed} `);
        console.log(`Success Rate: ${((this.testResults.passed / this.testResults.total) * 100).toFixed(1)}%`);
        
        if (this.testResults.failed > 0) {
            console.log('\n Failed Tests:');
            this.testResults.details
                .filter(test => !test.passed)
                .forEach(test => console.log(`  - ${test.name}: ${test.message}`));
        }
        
        console.log('\n Stealth Engine Test Complete!');
        
        if (this.testResults.failed === 0) {
            console.log(' All tests passed! Stealth engine is working perfectly.');
            process.exit(0);
        } else {
            console.log('  Some tests failed. Please review the results above.');
            process.exit(1);
        }
    }
}

// Run the test suite
async function main() {
    const tester = new StealthEngineTester();
    await tester.runAllTests();
}

// Handle uncaught exceptions
process.on('uncaughtException', (error) => {
    console.error(' Uncaught Exception:', error.message);
    process.exit(1);
});

process.on('unhandledRejection', (reason, promise) => {
    console.error(' Unhandled Rejection at:', promise, 'reason:', reason);
    process.exit(1);
});

// Run tests if this file is executed directly
if (require.main === module) {
    main().catch(error => {
        console.error(' Test suite failed:', error.message);
        process.exit(1);
    });
}

module.exports = StealthEngineTester;
