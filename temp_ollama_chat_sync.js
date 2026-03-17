const http = require('http');

const payload = JSON.stringify({
  model: 'phi3:mini',
  messages: [{ role: 'user', content: 'What is 2+2?' }],
  stream: false,
});

const req = http.request(
  {
    hostname: 'localhost',
    port: 11434,
    path: '/api/chat',
    method: 'POST',
    headers: {
      'Content-Type': 'application/json',
      'Content-Length': Buffer.byteLength(payload),
    },
  },
  (res) => {
    res.setEncoding('utf8');
    let body = '';
    res.on('data', (chunk) => {
      body += chunk;
    });
    res.on('end', () => {
      console.log('status', res.statusCode);
      console.log('headers', res.headers);
      console.log('body', body);
    });
  }
);

req.on('error', (err) => {
  console.error('request error', err);
});

req.write(payload);
req.end();
