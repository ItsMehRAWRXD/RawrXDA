const net = require('net');
const http = require('http');
const fs = require('fs');
const path = require('path');
const WebSocket = require('ws');

// Configuration
const PIPE_NAME = '\\\\.\\pipe\\SovereignThermalAgent';
const HTTP_PORT = 3000;
const POLLING_RATE_MS = 100; // Hyper-polling for <2s latency (aiming for 10Hz updates)

// --- HTTP Server (Serves the Dashboard) ---
const server = http.createServer((req, res) => {
  if (req.url === '/' || req.url === '/index.html') {
    fs.readFile(path.join(__dirname, 'index.html'), (err, data) => {
      if (err) {
        res.writeHead(500);
        res.end('Error loading dashboard');
        return;
      }
      res.writeHead(200, { 'Content-Type': 'text/html' });
      res.end(data);
    });
  } else {
    res.writeHead(404);
    res.end('Not Found');
  }
});

// --- WebSocket Server ---
const wss = new WebSocket.Server({ server });

wss.on('connection', (ws) => {
  console.log('[WS] Client connected');
});

function broadcast(data) {
  wss.clients.forEach(client => {
    if (client.readyState === WebSocket.OPEN) {
      client.send(JSON.stringify(data));
    }
  });
}

// --- Named Pipe Poller ---
// The MASM bridge is a server that accepts one connection, reads "GET_STATUS", writes JSON, and disconnects.
// We must act as the client.

function pollThermalStatus() {
  const client = net.connect(PIPE_NAME, () => {
    // Connected
    client.write('GET_STATUS');
  });

  let buffer = '';

  client.on('data', (data) => {
    buffer += data.toString();
  });

  client.on('end', () => {
    try {
      // The JSON might be null-terminated or have garbage if formatted via wsprintf properly, 
      // but usually wsprintfA produces a null-terminated string.
      // Node's toString() handles the bytes.
      const cleanStr = buffer.replace(/\0/g, '').trim();
      if (cleanStr.length > 0 && cleanStr.startsWith('{')) {
        const json = JSON.parse(cleanStr);
        json.timestamp = Date.now();
        broadcast(json);
        // console.log('[PIPE] Data:', json); 
      }
    } catch (e) {
      console.error('[PIPE] Parse Error:', e.message, buffer);
    }
  });

  client.on('error', (err) => {
    // Suppress connection refused logs to avoid spam if governor isn't running
    if (err.code !== 'ENOENT' && err.code !== 'ECONNREFUSED') {
      console.error('[PIPE] Error:', err.message);
    }
  });

  // Ensure cleanup
  client.on('close', () => {
    // Schedule next poll
    setTimeout(pollThermalStatus, POLLING_RATE_MS);
  });
}

// Start
server.listen(HTTP_PORT, () => {
  console.log(`[HUD] Dashboard active at http://localhost:${HTTP_PORT}`);
  console.log(`[HUD] Bridge polling ${PIPE_NAME} @ ${POLLING_RATE_MS}ms`);
  pollThermalStatus();
});
