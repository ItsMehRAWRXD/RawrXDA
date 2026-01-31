#!/usr/bin/env node

// RawrZ Stealth Engine Demonstration
const stealthEngine = require('./src/engines/stealth-engine');
const { logger } = require('./src/utils/logger');

class StealthEngineDemo {
    constructor() {
        this.demoResults = [];
    }

    async runDemo() {
        console.log(' RawrZ Stealth Engine Demonstration');
        console.log('=====================================\n');

        try {
            // Initialize
            await this.initializeDemo();
            
            // Demo different stealth modes
            await this.demoStealthModes();
            
            // Demo detection capabilities
            await this.demoDetectionCapabilities();
            
            // Demo system analysis
            await this.demoSystemAnalysis();
            
            // Display results
            this.displayDemoResults();
            
        } catch (error) {
            console.error(' Demo failed:', error.message);
        }
    }

    async initializeDemo() {
        console.log(' Initializing Stealth Engine...');
        await stealthEngine.initialize({ stealth: { enabled: true } });
        console.log(' Stealth Engine initialized successfully\n');
    }

    async demoStealthModes() {
        console.log(' Stealth Modes Demonstration');
        console.log('==============================');
        
        const modes = ['basic', 'standard', 'full', 'maximum'];
        
        for (const mode of modes) {
            console.log(`\n Testing ${mode.toUpperCase()} mode:`);
            
            try {
                const result = await stealthEngine.enableStealth(mode);
                
                console.log(`    Mode: ${result.mode}`);
                console.log(`    Enabled: ${result.enabled}`);
                console.log(`    Capabilities: ${Object.keys(result.capabilities).length}`);
                
                // Show active capabilities
                const activeCapabilities = Object.keys(result.capabilities).filter(
                    key => result.capabilities[key].enabled
                );
                console.log(`    Active: ${activeCapabilities.join(', ')}`);
                
                this.demoResults.push({
                    mode,
                    enabled: result.enabled,
                    capabilities: Object.keys(result.capabilities).length,
                    active: activeCapabilities.length
                });
                
            } catch (error) {
                console.log(`    Error: ${error.message}`);
            }
        }
    }

    async demoDetectionCapabilities() {
        console.log('\n\n Detection Capabilities Demonstration');
        console.log('=====================================');
        
        // Run a comprehensive detection scan
        console.log('\n Running comprehensive detection scan...');
        
        try {
            const scanResults = await stealthEngine.runDetectionScan();
            
            console.log('\n Detection Results:');
            
            for (const [category, methods] of Object.entries(scanResults)) {
                console.log(`\n    ${category.toUpperCase()}:`);
                
                let detectedCount = 0;
                let totalConfidence = 0;
                
                for (const [method, result] of Object.entries(methods)) {
                    const status = result.detected ? ' DETECTED' : ' CLEAN';
                    const confidence = (result.confidence * 100).toFixed(1);
                    
                    console.log(`      ${status} ${method}: ${confidence}% confidence`);
                    
                    if (result.detected) detectedCount++;
                    totalConfidence += result.confidence;
                }
                
                const avgConfidence = (totalConfidence / Object.keys(methods).length * 100).toFixed(1);
                console.log(`       Summary: ${detectedCount}/${Object.keys(methods).length} detected, ${avgConfidence}% avg confidence`);
            }
            
        } catch (error) {
            console.log(`    Scan failed: ${error.message}`);
        }
    }

    async demoSystemAnalysis() {
        console.log('\n\n System Analysis Demonstration');
        console.log('================================');
        
        // Get system information
        const os = require('os');
        
        console.log('\n  System Information:');
        console.log(`   OS: ${os.platform()} ${os.arch()}`);
        console.log(`   CPU Cores: ${os.cpus().length}`);
        console.log(`   Total Memory: ${(os.totalmem() / (1024 * 1024 * 1024)).toFixed(2)} GB`);
        console.log(`   Free Memory: ${(os.freemem() / (1024 * 1024 * 1024)).toFixed(2)} GB`);
        console.log(`   Uptime: ${(os.uptime() / 3600).toFixed(2)} hours`);
        
        // Test individual capabilities
        console.log('\n Individual Capability Tests:');
        
        const capabilities = ['anti-debug', 'anti-vm', 'anti-sandbox', 'anti-analysis'];
        
        for (const capability of capabilities) {
            try {
                const result = await stealthEngine.enableStealthCapability(capability);
                
                if (result.enabled) {
                    console.log(`    ${capability}: ${result.protectionLevel} protection`);
                    
                    if (result.methods) {
                        const methodCount = Object.keys(result.methods).length;
                        console.log(`       ${methodCount} methods active`);
                    }
                } else {
                    console.log(`    ${capability}: Failed to enable`);
                }
            } catch (error) {
                console.log(`    ${capability}: ${error.message}`);
            }
        }
    }

    displayDemoResults() {
        console.log('\n\n Demo Results Summary');
        console.log('=======================');
        
        if (this.demoResults.length > 0) {
            console.log('\n Stealth Mode Performance:');
            this.demoResults.forEach(result => {
                console.log(`   ${result.mode.toUpperCase()}: ${result.active}/${result.capabilities} capabilities active`);
            });
        }
        
        // Get final status
        const status = stealthEngine.getStatus();
        console.log('\n Final Status:');
        console.log(`   Stealth Enabled: ${status.enabled ? '' : ''}`);
        console.log(`   Active Modes: ${status.activeModes.length}`);
        console.log(`   Available Modes: ${status.availableModes.join(', ')}`);
        console.log(`   Last Check: ${status.lastCheck || 'Never'}`);
        
        console.log('\n Stealth Engine Demo Complete!');
        console.log('The RawrZ Stealth Engine provides comprehensive anti-detection capabilities');
        console.log('for protecting against debuggers, VMs, sandboxes, and analysis tools.');
    }
}

// Run the demo
async function main() {
    const demo = new StealthEngineDemo();
    await demo.runDemo();
}

// Handle uncaught exceptions
process.on('uncaughtException', (error) => {
    console.error(' Demo failed:', error.message);
    process.exit(1);
});

process.on('unhandledRejection', (reason, promise) => {
    console.error(' Unhandled Rejection:', reason);
    process.exit(1);
});

// Run demo if this file is executed directly
if (require.main === module) {
    main().catch(error => {
        console.error(' Demo failed:', error.message);
        process.exit(1);
    });
}

module.exports = StealthEngineDemo;
