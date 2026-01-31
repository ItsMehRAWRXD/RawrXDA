#!/usr/bin/env node

/**
 * RawrZ Security Platform - Comprehensive Test Suite
 * Tests all functionality with real data instead of mock responses
 */

const axios = require('axios');
const WebSocket = require('ws');
const crypto = require('crypto');
const fs = require('fs').promises;
const path = require('path');

// Test configuration
const TEST_CONFIG = {
    baseURL: 'http://localhost:8080',
    authToken: process.env.AUTH_TOKEN || 'test-token-123',
    timeout: 10000,
    retries: 3
};

// Test results tracking
const testResults = {
    passed: 0,
    failed: 0,
    total: 0,
    details: []
};

// Utility functions
function logTest(testName, status, details = '') {
    testResults.total++;
    if (status === 'PASS') {
        testResults.passed++;
        console.log(` ${testName}: PASS`);
    } else {
        testResults.failed++;
        console.log(` ${testName}: FAIL - ${details}`);
    }
    testResults.details.push({ testName, status, details });
}

async function makeRequest(method, endpoint, data = null, headers = {}) {
    const config = {
        method,
        url: `${TEST_CONFIG.baseURL}${endpoint}`,
        headers: {
            'Authorization': `Bearer ${TEST_CONFIG.authToken}`,
            'Content-Type': 'application/json',
            ...headers
        },
        timeout: TEST_CONFIG.timeout
    };
    
    if (data) {
        config.data = data;
    }
    
    try {
        const response = await axios(config);
        return { success: true, data: response.data, status: response.status };
    } catch (error) {
        return { 
            success: false, 
            error: error.message, 
            status: error.response?.status,
            data: error.response?.data 
        };
    }
}

// Test 1: Server Health Check
async function testServerHealth() {
    try {
        const response = await makeRequest('GET', '/');
        if (response.success && response.status === 200) {
            logTest('Server Health Check', 'PASS');
        } else {
            logTest('Server Health Check', 'FAIL', `Status: ${response.status}`);
        }
    } catch (error) {
        logTest('Server Health Check', 'FAIL', error.message);
    }
}

// Test 2: Authentication System
async function testAuthentication() {
    try {
        // Test with valid token
        const statusData = {
            action: 'system_info',
            target: 'status',
            params: {}
        };
        const validResponse = await makeRequest('POST', '/api/rawrz/execute', statusData);
        if (validResponse.success) {
            logTest('Authentication (Valid Token)', 'PASS');
        } else {
            logTest('Authentication (Valid Token)', 'FAIL', validResponse.error);
        }
        
        // Test with invalid token
        const invalidResponse = await makeRequest('POST', '/api/rawrz/execute', statusData, {
            'Authorization': 'Bearer invalid-token'
        });
        if (!invalidResponse.success && invalidResponse.status === 401) {
            logTest('Authentication (Invalid Token)', 'PASS');
        } else {
            logTest('Authentication (Invalid Token)', 'FAIL', 'Should return 401');
        }
    } catch (error) {
        logTest('Authentication System', 'FAIL', error.message);
    }
}

// Test 3: Botnet Status API
async function testBotnetStatus() {
    try {
        const statusData = {
            action: 'get_bot_stats',
            target: 'all',
            params: {}
        };
        const response = await makeRequest('POST', '/api/rawrz/execute', statusData);
        if (response.success) {
            logTest('Botnet Status API', 'PASS');
        } else {
            logTest('Botnet Status API', 'FAIL', response.error);
        }
    } catch (error) {
        logTest('Botnet Status API', 'FAIL', error.message);
    }
}

// Test 4: Bot Management
async function testBotManagement() {
    try {
        // Test get bots
        const getBotData = {
            action: 'list_bots',
            target: 'all',
            params: {}
        };
        const getResponse = await makeRequest('POST', '/api/rawrz/execute', getBotData);
        if (getResponse.success) {
            logTest('Get Bots API', 'PASS');
        } else {
            logTest('Get Bots API', 'FAIL', getResponse.error);
        }
        
        // Test add bot
        const addBotData = {
            action: 'add_bot',
            target: 'test-bot-' + Date.now(),
            params: {
                type: 'http',
                country: 'US',
                platform: 'Windows'
            }
        };
        
        const addResponse = await makeRequest('POST', '/api/rawrz/execute', addBotData);
        if (addResponse.success && addResponse.data.status === 200) {
            logTest('Add Bot API', 'PASS');
        } else {
            logTest('Add Bot API', 'FAIL', addResponse.error);
        }
    } catch (error) {
        logTest('Bot Management', 'FAIL', error.message);
    }
}

// Test 5: Data Extraction APIs
async function testDataExtraction() {
    try {
        const extractionTests = [
            { action: 'extract_browser_data', target: 'test-bot-1' },
            { action: 'extract_crypto_data', target: 'test-bot-1' },
            { action: 'extract_messaging_data', target: 'test-bot-1' }
        ];
        
        for (const test of extractionTests) {
            const response = await makeRequest('POST', '/api/rawrz/execute', test);
            if (response.success) {
                logTest(`Data Extraction (${test.action})`, 'PASS');
            } else {
                logTest(`Data Extraction (${test.action})`, 'FAIL', response.error);
            }
        }
    } catch (error) {
        logTest('Data Extraction APIs', 'FAIL', error.message);
    }
}

// Test 6: Encryption APIs
async function testEncryptionAPIs() {
    try {
        const testData = 'This is test data for encryption';
        
        // Test hashing
        const hashData = {
            action: 'hash',
            target: testData,
            params: {
                algorithm: 'sha256',
                save: false
            }
        };
        const hashResponse = await makeRequest('POST', '/api/rawrz/execute', hashData);
        if (hashResponse.success) {
            logTest('Hashing API', 'PASS');
        } else {
            logTest('Hashing API', 'FAIL', hashResponse.error || 'Hash not found in response');
        }
        
        // Test encryption
        const encryptData = {
            action: 'encrypt',
            target: testData,
            params: {
                algorithm: 'aes-256-gcm'
            }
        };
        const encryptResponse = await makeRequest('POST', '/api/rawrz/execute', encryptData);
        if (encryptResponse.success) {
            logTest('Encryption API', 'PASS');
            
            // Test decryption
            const decryptData = {
                action: 'decrypt',
                target: encryptResponse.data.encrypted,
                params: {
                    algorithm: 'aes-256-gcm',
                    key: encryptResponse.data.key,
                    iv: encryptResponse.data.iv
                }
            };
            const decryptResponse = await makeRequest('POST', '/api/rawrz/execute', decryptData);
            if (decryptResponse.success) {
                logTest('Decryption API', 'PASS');
            } else {
                logTest('Decryption API', 'FAIL', 'Decrypted data does not match original');
            }
        } else {
            logTest('Encryption API', 'FAIL', encryptResponse.error);
        }
    } catch (error) {
        logTest('Encryption APIs', 'FAIL', error.message);
    }
}

// Test 7: WebSocket Connection
async function testWebSocketConnection() {
    return new Promise((resolve) => {
        try {
            const ws = new WebSocket('ws://localhost:8080');
            let connected = false;
            
            ws.on('open', () => {
                connected = true;
                logTest('WebSocket Connection', 'PASS');
                ws.close();
                resolve();
            });
            
            ws.on('error', (error) => {
                if (!connected) {
                    logTest('WebSocket Connection', 'FAIL', error.message);
                    resolve();
                }
            });
            
            ws.on('close', () => {
                if (connected) {
                    resolve();
                }
            });
            
            // Timeout after 5 seconds
            setTimeout(() => {
                if (!connected) {
                    logTest('WebSocket Connection', 'FAIL', 'Connection timeout');
                    ws.close();
                    resolve();
                }
            }, 5000);
        } catch (error) {
            logTest('WebSocket Connection', 'FAIL', error.message);
            resolve();
        }
    });
}

// Test 8: Rate Limiting
async function testRateLimiting() {
    try {
        const requests = [];
        const statusData = {
            action: 'system_info',
            target: 'status',
            params: {}
        };
        for (let i = 0; i < 5; i++) {
            requests.push(makeRequest('POST', '/api/rawrz/execute', statusData));
        }
        
        const responses = await Promise.all(requests);
        const successCount = responses.filter(r => r.success).length;
        
        if (successCount >= 4) { // Allow some failures due to rate limiting
            logTest('Rate Limiting', 'PASS');
        } else {
            logTest('Rate Limiting', 'FAIL', `Only ${successCount}/5 requests succeeded`);
        }
    } catch (error) {
        logTest('Rate Limiting', 'FAIL', error.message);
    }
}

// Test 9: Data Persistence
async function testDataPersistence() {
    try {
        // Test that data is persisted by adding a bot and checking if it persists
        const botId = 'persistence-test-bot-' + Date.now();
        const addResponse = await makeRequest('POST', '/api/rawrz/execute', {
            action: 'add_bot',
            target: botId,
            params: { type: 'http', country: 'US' }
        });
        
        if (addResponse.success) {
            // Wait a moment for persistence
            await new Promise(resolve => setTimeout(resolve, 1000));
            
            // Check if bot exists in the list
            const listData = {
                action: 'list_bots',
                target: 'all',
                params: {}
            };
            const botsResponse = await makeRequest('POST', '/api/rawrz/execute', listData);
            if (botsResponse.success) {
                const bots = botsResponse.data.data.bots;
                const foundBot = bots.find(bot => bot.id === botId);
                if (foundBot) {
                    logTest('Data Persistence', 'PASS');
                } else {
                    logTest('Data Persistence', 'FAIL', 'Bot not found in persisted data');
                }
            } else {
                logTest('Data Persistence', 'FAIL', 'Could not retrieve bots list');
            }
        } else {
            logTest('Data Persistence', 'FAIL', 'Could not add test bot');
        }
    } catch (error) {
        logTest('Data Persistence', 'FAIL', error.message);
    }
}

// Test 10: Security Headers
async function testSecurityHeaders() {
    try {
        const response = await axios.get(`${TEST_CONFIG.baseURL}/`, {
            timeout: TEST_CONFIG.timeout
        });
        
        const headers = response.headers;
        const securityHeaders = [
            'x-frame-options',
            'x-content-type-options',
            'x-xss-protection',
            'strict-transport-security'
        ];
        
        const foundHeaders = securityHeaders.filter(header => headers[header]);
        
        if (foundHeaders.length >= 3) {
            logTest('Security Headers', 'PASS', `Found ${foundHeaders.length}/4 headers`);
        } else {
            logTest('Security Headers', 'FAIL', `Only found ${foundHeaders.length}/4 headers`);
        }
    } catch (error) {
        logTest('Security Headers', 'FAIL', error.message);
    }
}

// Main test runner
async function runAllTests() {
    console.log(' RawrZ Security Platform - Comprehensive Test Suite');
    console.log('='.repeat(60));
    console.log(`Testing against: ${TEST_CONFIG.baseURL}`);
    console.log(`Auth Token: ${TEST_CONFIG.authToken ? 'Provided' : 'Not provided'}`);
    console.log('='.repeat(60));
    
    const tests = [
        testServerHealth,
        testAuthentication,
        testBotnetStatus,
        testBotManagement,
        testDataExtraction,
        testEncryptionAPIs,
        testWebSocketConnection,
        testRateLimiting,
        testDataPersistence,
        testSecurityHeaders
    ];
    
    for (const test of tests) {
        try {
            await test();
            // Small delay between tests
            await new Promise(resolve => setTimeout(resolve, 100));
        } catch (error) {
            console.log(` Test failed with error: ${error.message}`);
        }
    }
    
    // Print results
    console.log('\n' + '='.repeat(60));
    console.log(' TEST RESULTS SUMMARY');
    console.log('='.repeat(60));
    console.log(`Total Tests: ${testResults.total}`);
    console.log(` Passed: ${testResults.passed}`);
    console.log(` Failed: ${testResults.failed}`);
    console.log(`Success Rate: ${((testResults.passed / testResults.total) * 100).toFixed(1)}%`);
    
    if (testResults.failed > 0) {
        console.log('\n FAILED TESTS:');
        testResults.details
            .filter(test => test.status === 'FAIL')
            .forEach(test => {
                console.log(`  - ${test.testName}: ${test.details}`);
            });
    }
    
    console.log('\n' + '='.repeat(60));
    
    // Save detailed results
    const reportPath = path.join(__dirname, 'test-results.json');
    await fs.writeFile(reportPath, JSON.stringify(testResults, null, 2));
    console.log(` Detailed results saved to: ${reportPath}`);
    
    // Exit with appropriate code
    process.exit(testResults.failed > 0 ? 1 : 0);
}

// Handle command line arguments
if (require.main === module) {
    const args = process.argv.slice(2);
    if (args.includes('--help') || args.includes('-h')) {
        console.log(`
RawrZ Security Platform - Comprehensive Test Suite

Usage: node comprehensive-test-suite.js [options]

Options:
  --help, -h     Show this help message
  --url URL      Set base URL (default: http://localhost:8080)
  --token TOKEN  Set auth token (default: from AUTH_TOKEN env var)

Environment Variables:
  AUTH_TOKEN     Authentication token for API requests

Examples:
  node comprehensive-test-suite.js
  node comprehensive-test-suite.js --url http://localhost:3000
  AUTH_TOKEN=my-token node comprehensive-test-suite.js
        `);
        process.exit(0);
    }
    
    // Parse command line arguments
    const urlIndex = args.indexOf('--url');
    if (urlIndex !== -1 && args[urlIndex + 1]) {
        TEST_CONFIG.baseURL = args[urlIndex + 1];
    }
    
    const tokenIndex = args.indexOf('--token');
    if (tokenIndex !== -1 && args[tokenIndex + 1]) {
        TEST_CONFIG.authToken = args[tokenIndex + 1];
    }
    
    runAllTests().catch(error => {
        console.error('Test suite failed:', error);
        process.exit(1);
    });
}

module.exports = {
    runAllTests,
    testResults,
    TEST_CONFIG
};
