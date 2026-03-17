// Minimal HTTP server to serve ide_chatbot.html and test in real browser
const http = require('http');
const fs = require('fs');
const path = require('path');

const PORT = 8899;

const server = http.createServer((req, res) => {
  console.log(`${new Date().toISOString()} ${req.method} ${req.url}`);

  if (req.url === '/gui' || req.url === '/gui/' || req.url === '/') {
    const filePath = 'D:/rawrxd/gui/ide_chatbot.html';
    const stat = fs.statSync(filePath);
    console.log(`Serving ${filePath} (${stat.size} bytes)`);

    res.writeHead(200, {
      'Content-Type': 'text/html; charset=utf-8',
      'Content-Length': stat.size,
      'Cache-Control': 'no-store, no-cache, must-revalidate',
      'Access-Control-Allow-Origin': '*',
    });

    // Stream the file (handles large files correctly)
    const stream = fs.createReadStream(filePath);
    stream.pipe(res);
    stream.on('end', () => console.log('  File fully sent'));
    stream.on('error', (e) => console.error('  Stream error:', e.message));
  } else {
    res.writeHead(404);
    res.end('Not found');
  }
});

server.listen(PORT, () => {
  console.log(`Test server at http://localhost:${PORT}/gui`);
  console.log('Open this URL in your browser to test');
  console.log('Press Ctrl+C to stop');
});
