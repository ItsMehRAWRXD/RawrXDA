// Local AI backend — real Ollama bridge with metrics
// All paths on D:\rawrxd — no hardcoded responses
const express = require('express');
const http = require('http');
const path = require('path');
const fs = require('fs');
const app = express();
app.use(express.json({ limit: '10mb' }));

const IDE_ROOT = path.resolve(__dirname, '..', '..');

// Load Ollama config from main config
let ollamaCfg = { baseUrl: 'http://localhost:11434', model: 'llama2', timeout: 30 };
try {
  const mainCfg = JSON.parse(fs.readFileSync(path.join(IDE_ROOT, 'rawrxd.config.json'), 'utf-8'));
  if (mainCfg.ollama) ollamaCfg = { ...ollamaCfg, ...mainCfg.ollama };
} catch { }

// ── Real HTTP forwarding to Ollama ──
function ollamaRequest(apiPath, body, timeoutMs = 120000) {
  return new Promise((resolve, reject) => {
    const u = new URL(ollamaCfg.baseUrl);
    const data = JSON.stringify(body);
    const opts = {
      hostname: u.hostname, port: u.port, path: apiPath, method: 'POST',
      headers: { 'Content-Type': 'application/json', 'Content-Length': Buffer.byteLength(data) },
      timeout: timeoutMs
    };
    const req = http.request(opts, (res) => {
      let buf = '';
      res.on('data', (c) => buf += c);
      res.on('end', () => { try { resolve(JSON.parse(buf)); } catch { resolve({ raw: buf }); } });
    });
    req.on('error', reject);
    req.on('timeout', () => { req.destroy(); reject(new Error('Ollama timeout')); });
    req.write(data);
    req.end();
  });
}

function ollamaGet(apiPath, timeoutMs = 5000) {
  return new Promise((resolve, reject) => {
    const u = new URL(ollamaCfg.baseUrl);
    http.get({ hostname: u.hostname, port: u.port, path: apiPath, timeout: timeoutMs }, (res) => {
      let buf = '';
      res.on('data', (c) => buf += c);
      res.on('end', () => { try { resolve(JSON.parse(buf)); } catch { resolve({ raw: buf }); } });
    }).on('error', reject).on('timeout', function () { this.destroy(); reject(new Error('timeout')); });
  });
}

// ── Simple request counter for metrics ──
const stats = { completions: 0, chats: 0, errors: 0, totalTokens: 0, startTime: Date.now() };

// Health check
app.get('/health', async (req, res) => {
  try {
    const tags = await ollamaGet('/api/tags', 3000);
    res.json({ healthy: true, ollamaModels: (tags.models || []).length, stats });
  } catch (e) {
    res.status(503).json({ healthy: false, error: e.message, stats });
  }
});

// Real code completions via Ollama
app.post('/codewhisperer/generateCompletions', async (req, res) => {
  try {
    const { context, language, prefix, suffix, model } = req.body || {};
    const codeCtx = context || prefix || '';
    const lang = language || '';
    const targetModel = model || ollamaCfg.model;
    const prompt = `You are a code completion engine. Complete the following ${lang} code. Output ONLY the completion code, nothing else:\n\n${codeCtx}`;
    const result = await ollamaRequest('/api/generate', {
      model: targetModel,
      prompt,
      stream: false,
      options: { temperature: 0.1, num_predict: 256 }
    });
    stats.completions++;
    stats.totalTokens += result.eval_count || 0;
    res.json({
      completions: [{
        content: result.response || '',
        references: [],
        model: result.model || targetModel,
        evalCount: result.eval_count || 0,
        tokensPerSec: result.eval_count && result.eval_duration
          ? (result.eval_count / (result.eval_duration / 1e9)).toFixed(1)
          : '0'
      }]
    });
  } catch (e) {
    stats.errors++;
    res.status(502).json({
      completions: [],
      error: e.message,
      hint: `Ensure Ollama is running at ${ollamaCfg.baseUrl} with model '${req.body?.model || ollamaCfg.model}' pulled`
    });
  }
});

// Real chat via Ollama
app.post('/q/sendMessage', async (req, res) => {
  try {
    const { message, conversationId, model } = req.body || {};
    if (!message) { res.status(400).json({ error: 'message required' }); return; }
    const useModel = model || ollamaCfg.model;
    const result = await ollamaRequest('/api/chat', {
      model: useModel,
      messages: [{ role: 'user', content: message }],
      stream: false
    });
    stats.chats++;
    stats.totalTokens += result.eval_count || 0;
    res.json({
      conversationId: conversationId || ('rawrxd-' + Date.now()),
      messageId: 'msg-' + Date.now(),
      message: result.message?.content || result.response || '',
      model: result.model || useModel,
      evalCount: result.eval_count || 0,
      totalDurationMs: (result.total_duration || 0) / 1e6,
      tokensPerSec: result.eval_count && result.eval_duration
        ? (result.eval_count / (result.eval_duration / 1e9)).toFixed(1)
        : '0'
    });
  } catch (e) {
    stats.errors++;
    res.status(502).json({
      error: e.message,
      hint: `Ensure Ollama is running at ${ollamaCfg.baseUrl} with model '${ollamaCfg.model}' pulled`
    });
  }
});

// List available Ollama models
app.get('/models', async (req, res) => {
  try {
    const tags = await ollamaGet('/api/tags');
    res.json({ models: tags.models || [], configured: ollamaCfg.model });
  } catch (e) {
    res.status(502).json({ error: e.message, models: [] });
  }
});

// Metrics
app.get('/metrics', (req, res) => {
  res.json({ ...stats, uptimeMs: Date.now() - stats.startTime });
});

const PORT = process.env.LOCAL_AI_PORT || 3001;
app.listen(PORT, () => {
  console.log(`RawrXD Local AI Bridge on :${PORT}`);
  console.log(`  Ollama backend: ${ollamaCfg.baseUrl} (model: ${ollamaCfg.model})`);
  console.log(`  No hardcoded responses — all routes forward to Ollama`);
});
