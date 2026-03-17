/* Sends a sample POST to the local proxy at port 11434 (default)
   Usage: node scripts/sample_request.js
*/

const http = require('http');

const PROXY_PORT = parseInt(process.env.PROXY_PORT || '11434', 10);
const HOST = '127.0.0.1';

const payload = JSON.stringify({
  model: 'claude-2',
  input: 'Hello from sample_request',
  stream: false
});

const req = http.request({
  hostname: HOST,
  port: PROXY_PORT,
  path: '/v1/messages',
  method: 'POST',
  headers: {
    'Content-Type': 'application/json',
    'Content-Length': Buffer.byteLength(payload)
  }
}, (res) => {
  let data = '';
  res.on('data', (chunk) => data += chunk.toString());
  res.on('end', () => {
    console.log('[sample_request] response', res.statusCode, data);
  });
});

req.on('error', (err) => console.error('[sample_request] error', err.message));
req.write(payload);
req.end();
