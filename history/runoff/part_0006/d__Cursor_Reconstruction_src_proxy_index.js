/*
 Minimal local proxy that accepts HTTP POSTs intended for Anthropic Claude API,
 injects a proxy auth header, and forwards to a configured backend (default: https://api2.cursor.sh)

 Usage:
   API2_BACKEND=https://api2.cursor.sh ANTHROPIC_API_KEY=sk_... node src/proxy/index.js
*/

const http = require('http');
const https = require('https');
const url = require('url');
const { StringDecoder } = require('string_decoder');

const BACKEND = process.env.API2_BACKEND || 'https://api2.cursor.sh';
const PORT = parseInt(process.env.PROXY_PORT || '11434', 10);
const API_KEY = process.env.ANTHROPIC_API_KEY || 'demo-key';

function createProxyAuthHeaderValue(apiKey) {
  // Minimal stub to mimic injection logic
  // In real system this might sign/encode key; here we just pass-through
  return `Bearer ${apiKey}`;
}

console.log(`[proxy] starting on port ${PORT}, forwarding to ${BACKEND}`);

const server = http.createServer((req, res) => {
  // Only forward POST to /v1/messages or anything
  const parsed = url.parse(req.url);
  const decoder = new StringDecoder('utf8');
  let body = '';
  req.on('data', (chunk) => { body += decoder.write(chunk); });
  req.on('end', () => {
    body += decoder.end();

    console.log(`[proxy] ${req.method} ${req.url} -> forwarding with auth header`);

    const backendUrl = new url.URL(BACKEND + req.url);

    const options = {
      protocol: backendUrl.protocol,
      hostname: backendUrl.hostname,
      port: backendUrl.port || (backendUrl.protocol === 'https:' ? 443 : 80),
      path: backendUrl.pathname + (backendUrl.search || ''),
      method: req.method,
      headers: Object.assign({}, req.headers, {
        host: backendUrl.host,
        'authorization': createProxyAuthHeaderValue(API_KEY),
        'x-forwarded-for': req.connection.remoteAddress || '127.0.0.1'
      })
    };

    const proxyReq = (backendUrl.protocol === 'https:' ? https : http).request(options, (proxyRes) => {
      let responseData = '';
      proxyRes.on('data', (d) => { responseData += d.toString(); });
      proxyRes.on('end', () => {
        // Pipe response back
        res.writeHead(proxyRes.statusCode, proxyRes.headers);
        res.end(responseData);
      });
    });

    proxyReq.on('error', (err) => {
      console.error('[proxy] error forwarding request', err.message);
      res.writeHead(502);
      res.end(JSON.stringify({ error: 'Bad Gateway', message: err.message }));
    });

    if (body && body.length) proxyReq.write(body);
    proxyReq.end();
  });
});

server.listen(PORT, () => console.log(`[proxy] listening on http://0.0.0.0:${PORT}`));

// Simple graceful shutdown
process.on('SIGINT', () => {
  console.log('[proxy] shutting down');
  server.close(() => process.exit(0));
});
