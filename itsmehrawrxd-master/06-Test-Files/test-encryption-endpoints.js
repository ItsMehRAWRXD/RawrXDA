#!/usr/bin/env node

/**
 * Comprehensive Encryption Endpoints Test Script
 * Tests all supported algorithms across all encryption endpoints
 * RawrZ Security Platform - Complete System Verification
 */

const fs = require('fs');
const path = require('path');
const FormData = require('form-data');
const fetch = require('node-fetch');

// Test configuration
const BASE_URL = 'http://localhost:3000';
const TEST_FILES = {
    'test-file.txt': 'Hello, RawrZ Security Platform! This is a test file for encryption verification.',
    'test-document.pdf': Buffer.from('PDF test content for encryption testing'),
    'test-image.jpg': Buffer.from('JPEG test content for encryption testing'),
    'test-archive.zip': Buffer.from('ZIP test content for encryption testing'),
    'test-executable.exe': Buffer.from('EXE test content for encryption testing')
};

// Supported algorithms (Node.js compatible)
const SUPPORTED_ALGORITHMS = [
    // AES variants
    'aes-256-gcm', 'aes-256-cbc', 'aes-192-gcm', 'aes-192-cbc', 
    'aes-128-gcm', 'aes-128-cbc', 'aes-256-ctr',
    // ChaCha20 variants
    'chacha20-poly1305', 'chacha20',
    // ARIA variants
    'aria-256-gcm', 'aria-192-gcm', 'aria-128-gcm',
    'aria-256-cbc', 'aria-192-cbc', 'aria-128-cbc',
    'aria-256-ctr', 'aria-192-ctr', 'aria-128-ctr'
];

// Test endpoints
const ENDPOINTS = {
    'ev-encrypt': {
        url: '/ev-encrypt',
        requiredFields: ['algorithm', 'certificate'],
        optionalFields: ['extension', 'format', 'saveLocation']
    },
    'advanced-encrypt': {
        url: '/advanced-encrypt',
        requiredFields: ['algorithm'],
        optionalFields: ['keySize', 'mode', 'format', 'extension', 'obfuscation', 'stealth', 'antiAnalysis']
    },
    'payload-encrypt': {
        url: '/payload-encrypt',
        requiredFields: ['algorithm'],
        optionalFields: ['keySize', 'mode', 'format', 'extension', 'obfuscation', 'stealth', 'antiAnalysis']
    }
};

// Test results tracking
const testResults = {
    total: 0,
    passed: 0,
    failed: 0,
    errors: [],
    details: []
};

/**
 * Create test files
 */
function createTestFiles() {
    console.log(' Creating test files...');
    
    for (const [filename, content] of Object.entries(TEST_FILES)) {
        const filePath = path.join(__dirname, filename);
        
        if (typeof content === 'string') {
            fs.writeFileSync(filePath, content);
        } else {
            fs.writeFileSync(filePath, content);
        }
        
        console.log(` Created: ${filename}`);
    }
}

/**
 * Clean up test files
 */
function cleanupTestFiles() {
    console.log(' Cleaning up test files...');
    
    for (const filename of Object.keys(TEST_FILES)) {
        const filePath = path.join(__dirname, filename);
        if (fs.existsSync(filePath)) {
            fs.unlinkSync(filePath);
            console.log(` Removed: ${filename}`);
        }
    }
}

/**
 * Test a single encryption endpoint with a specific algorithm
 */
async function testEncryptionEndpoint(endpointName, algorithm, testFile) {
    const endpoint = ENDPOINTS[endpointName];
    const formData = new FormData();
    
    // Add file
    formData.append('file', fs.createReadStream(testFile));
    
    // Add required fields
    for (const field of endpoint.requiredFields) {
        switch (field) {
            case 'algorithm':
                formData.append('algorithm', algorithm);
                break;
            case 'certificate':
                formData.append('certificate', 'RawrZ-EV-Certificate-Default');
                break;
        }
    }
    
    // Add optional fields with defaults
    for (const field of endpoint.optionalFields) {
        switch (field) {
            case 'keySize':
                formData.append('keySize', '256');
                break;
            case 'mode':
                formData.append('mode', 'gcm');
                break;
            case 'format':
                formData.append('format', 'base64');
                break;
            case 'extension':
                formData.append('extension', '.enc');
                break;
            case 'obfuscation':
                formData.append('obfuscation', 'none');
                break;
            case 'stealth':
                formData.append('stealth', '');
                break;
            case 'antiAnalysis':
                formData.append('antiAnalysis', '');
                break;
            case 'saveLocation':
                formData.append('saveLocation', 'desktop');
                break;
        }
    }
    
    try {
        const response = await fetch(`${BASE_URL}${endpoint.url}`, {
            method: 'POST',
            body: formData
        });
        
        const result = await response.json();
        
        if (result.success) {
            return {
                success: true,
                algorithm,
                endpoint: endpointName,
                file: path.basename(testFile),
                responseTime: Date.now(),
                data: result.data || result
            };
        } else {
            return {
                success: false,
                algorithm,
                endpoint: endpointName,
                file: path.basename(testFile),
                error: result.error || 'Unknown error',
                responseTime: Date.now()
            };
        }
    } catch (error) {
        return {
            success: false,
            algorithm,
            endpoint: endpointName,
            file: path.basename(testFile),
            error: error.message,
            responseTime: Date.now()
        };
    }
}

/**
 * Run comprehensive encryption tests
 */
async function runComprehensiveTests() {
    console.log(' Starting Comprehensive Encryption Endpoints Test');
    console.log('=' .repeat(60));
    
    createTestFiles();
    
    const testFiles = Object.keys(TEST_FILES).map(filename => path.join(__dirname, filename));
    
    for (const endpointName of Object.keys(ENDPOINTS)) {
        console.log(`\n Testing Endpoint: ${endpointName}`);
        console.log('-'.repeat(40));
        
        for (const algorithm of SUPPORTED_ALGORITHMS) {
            for (const testFile of testFiles) {
                const filename = path.basename(testFile);
                console.log(`  Testing: ${algorithm} with ${filename}...`);
                
                const result = await testEncryptionEndpoint(endpointName, algorithm, testFile);
                
                testResults.total++;
                
                if (result.success) {
                    testResults.passed++;
                    console.log(`     PASSED`);
                    testResults.details.push({
                        status: 'PASSED',
                        endpoint: endpointName,
                        algorithm,
                        file: filename,
                        responseTime: result.responseTime
                    });
                } else {
                    testResults.failed++;
                    console.log(`     FAILED: ${result.error}`);
                    testResults.errors.push({
                        endpoint: endpointName,
                        algorithm,
                        file: filename,
                        error: result.error
                    });
                    testResults.details.push({
                        status: 'FAILED',
                        endpoint: endpointName,
                        algorithm,
                        file: filename,
                        error: result.error,
                        responseTime: result.responseTime
                    });
                }
                
                // Small delay to avoid overwhelming the server
                await new Promise(resolve => setTimeout(resolve, 100));
            }
        }
    }
    
    cleanupTestFiles();
}

/**
 * Generate comprehensive test report
 */
function generateTestReport() {
    console.log('\n' + '='.repeat(60));
    console.log(' COMPREHENSIVE TEST REPORT');
    console.log('='.repeat(60));
    
    console.log(`\n Test Statistics:`);
    console.log(`  Total Tests: ${testResults.total}`);
    console.log(`  Passed: ${testResults.passed} (${((testResults.passed / testResults.total) * 100).toFixed(1)}%)`);
    console.log(`  Failed: ${testResults.failed} (${((testResults.failed / testResults.total) * 100).toFixed(1)}%)`);
    
    if (testResults.failed === 0) {
        console.log('\n ALL TESTS PASSED! System is 100% operational!');
    } else {
        console.log('\n Some tests failed. Details:');
        testResults.errors.forEach(error => {
            console.log(`  - ${error.endpoint}: ${error.algorithm} with ${error.file} - ${error.error}`);
        });
    }
    
    console.log(`\n Algorithm Coverage:`);
    console.log(`  Total Algorithms Tested: ${SUPPORTED_ALGORITHMS.length}`);
    console.log(`  AES Variants: 7 algorithms`);
    console.log(`  ChaCha20 Variants: 2 algorithms`);
    console.log(`  ARIA Variants: 9 algorithms`);
    
    console.log(`\n Endpoint Coverage:`);
    console.log(`  EV Certificate Encryption: ${ENDPOINTS['ev-encrypt'] ? '' : ''}`);
    console.log(`  Advanced Encryption: ${ENDPOINTS['advanced-encrypt'] ? '' : ''}`);
    console.log(`  Payload Encryption: ${ENDPOINTS['payload-encrypt'] ? '' : ''}`);
    
    console.log(`\n File Type Coverage:`);
    console.log(`  Text Files: `);
    console.log(`  PDF Documents: `);
    console.log(`  Image Files: `);
    console.log(`  Archive Files: `);
    console.log(`  Executable Files: `);
    
    // Save detailed report to file
    const reportData = {
        timestamp: new Date().toISOString(),
        summary: {
            total: testResults.total,
            passed: testResults.passed,
            failed: testResults.failed,
            successRate: ((testResults.passed / testResults.total) * 100).toFixed(1) + '%'
        },
        algorithms: SUPPORTED_ALGORITHMS,
        endpoints: Object.keys(ENDPOINTS),
        testFiles: Object.keys(TEST_FILES),
        details: testResults.details,
        errors: testResults.errors
    };
    
    fs.writeFileSync('encryption-test-report.json', JSON.stringify(reportData, null, 2));
    console.log(`\n Detailed report saved to: encryption-test-report.json`);
    
    console.log('\n' + '='.repeat(60));
    console.log(' RawrZ Security Platform - Encryption Test Complete!');
    console.log('='.repeat(60));
}

/**
 * Main execution
 */
async function main() {
    try {
        await runComprehensiveTests();
        generateTestReport();
        
        // Exit with appropriate code
        process.exit(testResults.failed === 0 ? 0 : 1);
    } catch (error) {
        console.error(' Test execution failed:', error.message);
        process.exit(1);
    }
}

// Run the tests
if (require.main === module) {
    main();
}

module.exports = {
    runComprehensiveTests,
    generateTestReport,
    testResults
};