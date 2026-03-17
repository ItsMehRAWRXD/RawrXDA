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
  // Body may contain: upstream, prompt, max_tokens, headers, provider, model
  const { upstream, prompt, max_tokens, provider, model } = req.body || {};
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

  // Prepare upstream request body (generic). Adjust for known providers.
  const upstreamBody = {
    prompt: prompt,
    max_tokens: max_tokens || 256,
    stream: true
  };
  
  // Detect provider (auto or explicit)
  const detectedProvider = provider?.toLowerCase() || 
    (/\/api\/generate$/i.test(upstreamUrl) ? 'ollama' :
     /\/v1\/chat\/completions$/i.test(upstreamUrl) ? 'openai' :
     /anthropic\.com/i.test(upstreamUrl) ? 'anthropic' :
     /generativelanguage\.googleapis\.com/i.test(upstreamUrl) ? 'gemini' :
     /huggingface\.co/i.test(upstreamUrl) ? 'huggingface' :
     /localhost:1234/i.test(upstreamUrl) ? 'lmstudio' :
     /localhost:5000/i.test(upstreamUrl) ? 'textgen' : 'generic');

  // Ollama: /api/generate
  if (detectedProvider === 'ollama') {
    const effectiveModel = model || process.env.OLLAMA_MODEL;
    if (!effectiveModel) {
      return res.status(400).json({ error: 'Ollama upstream requires `model`. Provide body.model or set OLLAMA_MODEL env.' });
    }
    upstreamBody.model = effectiveModel;
  }
  
  // OpenAI, Azure OpenAI, LM Studio: /v1/chat/completions
  else if (detectedProvider === 'openai' || detectedProvider === 'lmstudio') {
    const effectiveModel = model || process.env.OPENAI_MODEL || 'gpt-4o-mini';
    upstreamBody.model = effectiveModel;
    upstreamBody.stream = true;
    upstreamBody.messages = [
      { role: 'system', content: 'You are a helpful coding assistant.' },
      { role: 'user', content: prompt }
    ];
    delete upstreamBody.prompt;
    delete upstreamBody.max_tokens;
  }
  
  // Anthropic Claude: /v1/messages
  else if (detectedProvider === 'anthropic') {
    const effectiveModel = model || process.env.ANTHROPIC_MODEL || 'claude-3-5-sonnet-20241022';
    upstreamBody.model = effectiveModel;
    upstreamBody.messages = [{ role: 'user', content: prompt }];
    upstreamBody.max_tokens = max_tokens || 1024;
    upstreamBody.stream = true;
    delete upstreamBody.prompt;
  }
  
  // Google Gemini: /v1beta/models/{model}:streamGenerateContent
  else if (detectedProvider === 'gemini') {
    upstreamBody.contents = [{ parts: [{ text: prompt }] }];
    delete upstreamBody.prompt;
    delete upstreamBody.max_tokens;
    delete upstreamBody.stream;
  }
  
  // Hugging Face Inference API
  else if (detectedProvider === 'huggingface') {
    upstreamBody.inputs = prompt;
    upstreamBody.parameters = { max_new_tokens: max_tokens || 256, stream: true };
    delete upstreamBody.prompt;
    delete upstreamBody.max_tokens;
    delete upstreamBody.stream;
  }
  
  // text-generation-webui (oobabooga): /v1/completions
  else if (detectedProvider === 'textgen') {
    upstreamBody.prompt = prompt;
    upstreamBody.max_tokens = max_tokens || 256;
    upstreamBody.stream = true;
  }

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
  const idx = require('path').join(__dirname, 'static', 'editor.html');
  if (require('fs').existsSync(idx)) return res.sendFile(idx);
  res.send('Proxy running. Add static/editor.html for the streaming demo.');
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
