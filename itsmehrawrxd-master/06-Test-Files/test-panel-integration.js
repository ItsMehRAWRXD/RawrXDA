const http = require('http');

// Test function to verify API endpoints
async function testAPIEndpoint(path, method = 'GET', data = null) {
    return new Promise((resolve, reject) => {
        const options = {
            hostname: 'localhost',
            port: 8080,
            path: path,
            method: method,
            headers: {
                'Content-Type': 'application/json',
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

        if (data && (method === 'POST' || method === 'PUT')) {
            req.write(JSON.stringify(data));
        }

        req.end();
    });
}

// Test all API endpoints
async function runTests() {
    console.log(' Testing RawrZ Advanced Botnet Panel Integration...\n');

    const tests = [
        { name: 'Health Check', endpoint: '/health', method: 'GET' },
        { name: 'Botnet Status', endpoint: '/api/botnet/status', method: 'GET' },
        { name: 'Bot List', endpoint: '/api/botnet/bots', method: 'GET' },
        { name: 'Botnet Stats', endpoint: '/api/botnet/stats', method: 'GET' },
        { name: 'IRC Bot Features', endpoint: '/api/irc-bot-generator/features', method: 'GET' },
        { name: 'HTTP Bot Features', endpoint: '/api/http-bot-generator/features', method: 'GET' },
        { name: 'HTTP Bot Manager Stats', endpoint: '/api/http-bot-manager/stats', method: 'GET' },
        { name: 'IRC Bot Stats', endpoint: '/api/irc-bot-generator/stats', method: 'GET' },
        { name: 'Test Results', endpoint: '/api/botnet/test-results', method: 'GET' }
    ];

    let passed = 0;
    let failed = 0;

    for (const test of tests) {
        try {
            console.log(`Testing ${test.name}...`);
            const result = await testAPIEndpoint(test.endpoint, test.method);
            
            if (result.status === 200) {
                console.log(` ${test.name}: PASSED (Status: ${result.status})`);
                passed++;
            } else {
                console.log(` ${test.name}: FAILED (Status: ${result.status})`);
                failed++;
            }
        } catch (error) {
            console.log(` ${test.name}: ERROR - ${error.message}`);
            failed++;
        }
    }

    console.log(`\n Test Results:`);
    console.log(` Passed: ${passed}`);
    console.log(` Failed: ${failed}`);
    console.log(` Success Rate: ${((passed / (passed + failed)) * 100).toFixed(1)}%`);

    if (failed === 0) {
        console.log('\n All tests passed! The advanced botnet panel is working perfectly.');
        console.log('\n Access the panel at: http://localhost:8080/advanced-botnet-panel.html');
    } else {
        console.log('\n  Some tests failed. Please check the server logs.');
    }
}

// Run the tests
runTests().catch(console.error);
