/* Minimal MCP handler stub
   Usage: node src/mcp/server.js
   Listens on port 11435 by default and accepts POST /mcp/events
*/

const http = require('http');
const { StringDecoder } = require('string_decoder');

const PORT = parseInt(process.env.MCP_PORT || '11435', 10);

const server = http.createServer((req, res) => {
  if (req.method === 'POST' && req.url === '/mcp/events') {
    const decoder = new StringDecoder('utf8');
    let body = '';
    req.on('data', (chunk) => { body += decoder.write(chunk); });
    req.on('end', () => {
      body += decoder.end();
      console.log('[mcp] received event:', body);
      res.writeHead(200, { 'Content-Type': 'application/json' });
      res.end(JSON.stringify({ status: 'ok', received: true }));
    });
    return;
  }

  res.writeHead(404, { 'Content-Type': 'application/json' });
  res.end(JSON.stringify({ error: 'not_found' }));
});

server.listen(PORT, () => console.log(`[mcp] listening on http://0.0.0.0:${PORT}`));

process.on('SIGINT', () => {
  console.log('[mcp] shutting down');
  server.close(() => process.exit(0));
});
