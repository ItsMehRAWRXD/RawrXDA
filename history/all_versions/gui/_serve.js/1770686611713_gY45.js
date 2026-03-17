const http = require('http');
const fs = require('fs');
const path = require('path');

const PORT = 9876;
const htmlPath = 'D:/rawrxd/gui/ide_chatbot.html';

const server = http.createServer((req, res) => {
  if (req.url === '/' || req.url === '/ide_chatbot.html') {
    const html = fs.readFileSync(htmlPath);
    res.writeHead(200, { 'Content-Type': 'text/html; charset=utf-8' });
    res.end(html);
  } else {
    res.writeHead(404);
    res.end('Not found');
  }
});

server.listen(PORT, () => {
  console.log(`Serving ide_chatbot.html at http://localhost:${PORT}/`);
  console.log('Press Ctrl+C to stop');
});
