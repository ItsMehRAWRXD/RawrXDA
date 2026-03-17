// Minimal HTTP server to test if the HTML works in a browser
const http = require('http');
const fs = require('fs');
const path = require('path');

const PORT = 9876;
const HTML_DIR = 'D:/rawrxd/gui';

const server = http.createServer((req, res) => {
  let filePath = path.join(HTML_DIR, req.url === '/' ? 'ide_chatbot.html' : req.url);

  const ext = path.extname(filePath);
  const mimeTypes = {
    '.html': 'text/html',
    '.js': 'application/javascript',
    '.css': 'text/css',
    '.json': 'application/json',
  };

  fs.readFile(filePath, (err, data) => {
    if (err) {
      res.writeHead(404);
      res.end('Not found');
      return;
    }
    res.writeHead(200, { 'Content-Type': mimeTypes[ext] || 'text/plain' });
    res.end(data);
  });
});

server.listen(PORT, () => {
  console.log(`Server running at http://localhost:${PORT}/`);
  console.log('Open this URL in Chrome and check the console for errors');
});
