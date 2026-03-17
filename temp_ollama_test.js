const http = require('http');

const payload = JSON.stringify({
  model: 'phi3:mini',
  messages: [{ role: 'user', content: 'What is 2+2?' }],
  stream: true,
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
    res.on('data', (chunk) => {
      process.stdout.write(chunk);
    });
    res.on('end', () => {
      process.stdout.write('\n--- end ---\n');
    });
  }
);

req.on('error', (err) => {
  console.error('request error', err);
});

req.write(payload);
req.end();
