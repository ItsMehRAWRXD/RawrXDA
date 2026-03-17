require('dotenv').config();
const express = require('express');
const fetch = require('node-fetch');
const cors = require('cors');
const app = express();
app.use(cors());
app.use(express.json());

const PORT = parseInt(process.env.PORT || '3200', 10);
const AUTH_SERVICE = process.env.AUTH_SERVICE || 'http://localhost:3000';
const DEFAULT_UPSTREAM = process.env.COPILOT_UPSTREAM_URL || null;

app.get('/health', (req, res) => res.json({ status: 'ok', upstream: !!DEFAULT_UPSTREAM }));

async function getLocalToken() {
  try {
    const r = await fetch(`${AUTH_SERVICE}/token`);
    if (!r.ok) return null;
    const j = await r.json();
    return j.access_token || j.token || null;
  } catch (e) {
    console.warn('Failed to fetch token from auth service', e.message);
    return null;
  }
}

// SSE streaming endpoint
app.post('/stream', async (req, res) => {
  // Body may contain: upstream, prompt, max_tokens, headers, provider
  const { upstream, prompt, max_tokens } = req.body || {};
  const upstreamUrl = upstream || DEFAULT_UPSTREAM;
  if (!upstreamUrl) return res.status(400).json({ error: 'No upstream configured. Set COPILOT_UPSTREAM_URL or provide upstream in body.' });
  if (!prompt) return res.status(400).json({ error: 'Missing prompt' });

  // Determine Authorization
  let token = null;
  const authHeader = req.headers['authorization'] || req.headers['Authorization'];
  if (authHeader && authHeader.toLowerCase().startsWith('bearer ')) {
    token = authHeader.split(' ')[1];
  } else {
    token = await getLocalToken();
  }

  // Prepare upstream request body (generic OpenAI-like shape). Adjust as needed for provider.
  const upstreamBody = {
    prompt: prompt,
    max_tokens: max_tokens || 256,
    stream: true
  };

  const upstreamHeaders = { 'Content-Type': 'application/json' };
  if (token) upstreamHeaders['Authorization'] = `Bearer ${token}`;

  res.setHeader('Content-Type', 'text/event-stream');
  res.setHeader('Cache-Control', 'no-cache');
  res.setHeader('Connection', 'keep-alive');
  res.flushHeaders && res.flushHeaders();

  try {
    const upstreamRes = await fetch(upstreamUrl, {
      method: 'POST',
      headers: upstreamHeaders,
      body: JSON.stringify(upstreamBody),
    });

    if (!upstreamRes.ok) {
      const text = await upstreamRes.text();
      res.write(`event: error\ndata: ${JSON.stringify({ status: upstreamRes.status, body: text })}\n\n`);
      return res.end();
    }

    const upstreamStream = upstreamRes.body;
    upstreamStream.on('data', (chunk) => {
      try {
        const s = chunk.toString('utf8');
        // Split into lines; forward each line as an SSE data event
        const lines = s.split(/\r?\n/).filter(l => l.length > 0);
        for (const line of lines) {
          // If provider sends `data: [DONE]` or similar, pass through
          // Normalize by removing leading `data: ` if present
          const payload = line.startsWith('data:') ? line.replace(/^data:\s*/i, '') : line;
          // send as SSE
          res.write(`data: ${payload}\n\n`);
        }
      } catch (e) {
        console.warn('Forward error', e.message);
      }
    });

    upstreamStream.on('end', () => {
      res.write('event: done\ndata: [DONE]\n\n');
      res.end();
    });

    upstreamStream.on('error', (err) => {
      res.write(`event: error\ndata: ${JSON.stringify({ message: err.message })}\n\n`);
      res.end();
    });

    // Keep connection alive until upstream ends
  } catch (e) {
    res.write(`event: error\ndata: ${JSON.stringify({ message: e.message })}\n\n`);
    res.end();
  }
});

// Simple static test client
app.get('/', (req, res) => {
  res.send(`<html><body>
  <h3>Copilot Proxy Test</h3>
  <form id=frm>
    <textarea id=prompt rows=6 cols=60>console.log('hello world');</textarea><br>
    <input id=up default placeholder='Upstream URL (optional)'><br>
    <button type=button id=go>Start Stream</button>
  </form>
  <pre id=out style='background:#111;color:#eee;padding:10px;max-height:400px;overflow:auto'></pre>
  <script>
    document.getElementById('go').addEventListener('click', async () => {
      const prompt = document.getElementById('prompt').value;
      const upstream = document.getElementById('up').value || undefined;
      const evtSource = new EventSource('/sse-proxy?prompt=' + encodeURIComponent(prompt) + (upstream? '&up=' + encodeURIComponent(upstream):''));
      const out = document.getElementById('out');
      evtSource.onmessage = (e) => { out.textContent += e.data; };
      evtSource.addEventListener('done', () => { out.textContent += '\n[DONE]\n'; evtSource.close(); });
      evtSource.onerror = (e) => { out.textContent += '\n[ERR] '+ JSON.stringify(e); evtSource.close(); };
    });
  </script>
  </body></html>`);
});

// Helper SSE proxy endpoint using query params (convenience for browser EventSource)
app.get('/sse-proxy', async (req, res) => {
  const prompt = req.query.prompt || '';
  const upstream = req.query.up || DEFAULT_UPSTREAM;
  // Reuse the /stream POST handler by making an internal request
  // For simplicity, call fetch to our own /stream
  res.setHeader('Content-Type', 'text/event-stream');
  res.setHeader('Cache-Control', 'no-cache');
  res.setHeader('Connection', 'keep-alive');
  res.flushHeaders && res.flushHeaders();
  try {
    const body = { upstream: upstream, prompt: prompt, max_tokens: 128 };
    const headers = { 'Content-Type': 'application/json' };
    const token = await getLocalToken();
    if (token) headers['Authorization'] = `Bearer ${token}`;
    const upstreamRes = await fetch(DEFAULT_UPSTREAM || upstream, { method: 'POST', headers, body: JSON.stringify(body) });
    if (!upstreamRes.ok) { const t = await upstreamRes.text(); res.write(`event: error\ndata: ${JSON.stringify(t)}\n\n`); return res.end(); }
    const upstreamStream = upstreamRes.body;
    upstreamStream.on('data', chunk => {
      const s = chunk.toString('utf8');
      const lines = s.split(/\r?\n/).filter(l => l.length > 0);
      for (const line of lines) {
        const payload = line.startsWith('data:') ? line.replace(/^data:\s*/i, '') : line;
        res.write(`data: ${payload}\n\n`);
      }
    });
    upstreamStream.on('end', () => { res.write('event: done\ndata: [DONE]\n\n'); res.end(); });
    upstreamStream.on('error', e => { res.write(`event: error\ndata: ${JSON.stringify(e.message)}\n\n`); res.end(); });
  } catch (e) {
    res.write(`event: error\ndata: ${JSON.stringify(e.message)}\n\n`);
    res.end();
  }
});

app.listen(PORT, () => console.log(`Copilot Proxy listening on http://localhost:${PORT}`));
