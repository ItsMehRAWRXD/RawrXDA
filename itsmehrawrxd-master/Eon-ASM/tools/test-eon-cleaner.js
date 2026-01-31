#!/usr/bin/env node

// Test script for EON Artifact Cleaner
// This demonstrates fixing backwards code generation

const { EONArtifactCleaner } = require('./eon-artifact-cleaner');
const fs = require('fs').promises;
const path = require('path');

async function testEONCleaner() {
    console.log(" Testing EON Artifact Cleaner - Backwards Code Fix");
    
    const cleaner = new EONArtifactCleaner();
    
    // Test backwards code detection
    const testContent = `
    def backwards_function() -> return {
        jmp 1-11-13
        jump 2-12-14
        goto 3-13-15
        call 4-14-16
        return value -> type
        if (condition) { else }
        variable = value1 + value2
    }
    `;
    
    console.log(" Testing blackhole detection...");
    const isBlackhole = cleaner.detectBlackhole(testContent);
    console.log(`Blackhole detected: ${isBlackhole}`);
    
    console.log(" Testing backwards code fixing...");
    const fixedContent = cleaner.fixBackwardsCodeGeneration(testContent);
    console.log("Fixed content:");
    console.log(fixedContent);
    
    console.log(" Artifacts removed:");
    cleaner.artifactsRemoved.forEach(artifact => {
        console.log(`  - ${artifact}`);
    });
    
    // Test with actual file
    console.log("\n Testing with actual file...");
    try {
        await cleaner.cleanEONFiles('./test-backwards-code.eon', './cleaned-output', {
            removeHeaders: true,
            removeMetadata: true,
            removeComments: false,
            validateOutput: true,
            backupOriginal: true,
            dryRun: false
        });
        
        console.log(" Test completed successfully!");
        
    } catch (error) {
        console.error(" Test failed:", error.message);
    }
}

// Run the test
if (require.main === module) {
    testEONCleaner().catch(console.error);
}

module.exports = { testEONCleaner };
