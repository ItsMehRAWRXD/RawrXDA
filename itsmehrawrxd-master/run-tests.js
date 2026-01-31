#!/usr/bin/env node

/**
 * RawrZ Security Platform - Test Runner
 * Runs the comprehensive test suite and generates reports
 */

const { spawn } = require('child_process');
const path = require('path');
const fs = require('fs').promises;

async function runTests() {
    console.log(' Starting RawrZ Security Platform Test Suite...\n');
    
    // Check if server is running
    try {
        const axios = require('axios');
        await axios.get('http://localhost:8080', { timeout: 5000 });
        console.log(' Server is running on port 8080');
    } catch (error) {
        console.log(' Server is not running on port 8080');
        console.log('Please start the server first with: node server.js');
        process.exit(1);
    }
    
    // Run the test suite
    const testPath = path.join(__dirname, 'tests', 'comprehensive-test-suite.js');
    
    return new Promise((resolve, reject) => {
        const testProcess = spawn('node', [testPath], {
            stdio: 'inherit',
            env: { ...process.env }
        });
        
        testProcess.on('close', (code) => {
            if (code === 0) {
                console.log('\n All tests completed successfully!');
                resolve();
            } else {
                console.log(`\n Tests failed with exit code: ${code}`);
                reject(new Error(`Tests failed with exit code: ${code}`));
            }
        });
        
        testProcess.on('error', (error) => {
            console.error('Failed to start test process:', error);
            reject(error);
        });
    });
}

// Main execution
if (require.main === module) {
    runTests().catch(error => {
        console.error('Test runner failed:', error);
        process.exit(1);
    });
}

module.exports = { runTests };
