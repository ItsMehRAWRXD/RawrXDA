// RawrXD IDE Model Proxy Server — all paths on D:\rawrxd
// Production: every endpoint routes to a real backend — no hardcoded responses
const express = require('express');
const path = require('path');
const crypto = require('crypto');
const { ModelManager, IDE_ROOT, metrics } = require('./model-loader');
const app = express();
const models = new ModelManager(path.join(__dirname, 'config.json'));

app.use(express.json({ limit: '10mb' }));

// ────────── Health ──────────
app.get('/health', (req, res) => {
  const s = models.status();
  const healthy = s.active !== null;
  res.status(healthy ? 200 : 503).json({ healthy, active: s.active, uptime: process.uptime() });
});

// ────────── IDE Chat — routed to active model ──────────
app.post('/q/chat', async (req, res) => {
  const timer = metrics.startTimer('endpoint.q.chat');
  try {
    const { message, conversationId } = req.body || {};
    if (!message) { res.status(400).json({ error: 'message is required' }); return; }
    const messages = [{ role: 'user', content: message }];
    const result = await models.chat(messages, req.body.params || {});
    metrics.endTimer(timer);
    metrics.inc('chat.requests');
    res.json({
      response: result.message?.content || result.response || '',
      model: result.model,
      conversationId: conversationId || ('rawrxd-' + Date.now()),
      tokensPerSec: result.tokensPerSec || 0,
      evalCount: result.evalCount || 0,
      wallClockMs: result.wallClockMs || 0
    });
  } catch (e) {
    metrics.endTimer(timer);
    metrics.inc('chat.errors');
    res.status(502).json({ error: e.message, hint: 'Load a model first: POST /models/load { source: "ollama" }' });
  }
});

// ────────── Code completion — routed to active model ──────────
app.post('/codewhisperer/suggestions', async (req, res) => {
  const timer = metrics.startTimer('endpoint.suggestions');
  try {
    const { code, language, cursorPosition, fileContext } = req.body || {};
    // Build a code-completion prompt
    const prefix = code || fileContext || '';
    const lang = language || 'unknown';
    const prompt = `Complete the following ${lang} code. Only output the completion, no explanation:\n\n${prefix}`;
    const result = await models.generate(prompt, { maxTokens: 256, temperature: 0.2 });
    metrics.endTimer(timer);
    metrics.inc('suggestions.requests');
    const content = result.response || '';
    res.json({
      suggestions: [{
        content,
        range: { start: cursorPosition || 0, end: (cursorPosition || 0) + content.length },
        model: result.model,
        tokensPerSec: result.tokensPerSec || 0
      }]
    });
  } catch (e) {
    metrics.endTimer(timer);
    metrics.inc('suggestions.errors');
    res.status(502).json({ error: e.message, suggestions: [] });
  }
});

// ────────── Telemetry — real metrics sink ──────────
app.post('/telemetry', (req, res) => {
  // Accept telemetry events, count them
  metrics.inc('telemetry.events');
  res.status(200).json({ ok: true, received: Date.now() });
});

// ────────── Model discovery: scan real files on disk + Ollama ──────────
app.get('/models/discover', async (req, res) => {
  const timer = metrics.startTimer('endpoint.discover');
  try {
    const localModels = models.discoverModels();
    const ollamaModels = await models.discoverOllamaModels();
    metrics.endTimer(timer);
    res.json({ local: localModels, ollama: ollamaModels, totalLocal: localModels.length, totalOllama: ollamaModels.length });
  } catch (e) {
    metrics.endTimer(timer);
    res.status(500).json({ error: e.message });
  }
});

// ────────── Model management ──────────
app.post('/models/load', async (req, res) => {
  try {
    const { source, model } = req.body || {};
    if (!source) { res.status(400).json({ error: 'source is required (tiinyAI|gguf|pyre|ollama)' }); return; }
    const result = await models.loadModel(source, model);
    res.json(result);
  } catch (e) {
    res.status(500).json({ error: e.message });
  }
});

app.post('/models/unload', async (req, res) => {
  try {
    const { source } = req.body || {};
    if (!source) { res.status(400).json({ error: 'source is required' }); return; }
    const result = await models.unloadModel(source);
    res.json(result);
  } catch (e) {
    res.status(500).json({ error: e.message });
  }
});

app.get('/models/status', (req, res) => {
  res.json(models.status());
});

app.post('/models/switch', async (req, res) => {
  try {
    const { source } = req.body || {};
    if (!source) { res.status(400).json({ error: 'source is required' }); return; }
    const result = await models.switchActive(source);
    res.json(result);
  } catch (e) {
    res.status(500).json({ error: e.message });
  }
});

// ────────── Inference ──────────
app.post('/inference/generate', async (req, res) => {
  const timer = metrics.startTimer('endpoint.generate');
  try {
    const { prompt, params } = req.body || {};
    if (!prompt) { res.status(400).json({ error: 'prompt is required' }); return; }
    const result = await models.generate(prompt, params || {});
    metrics.endTimer(timer);
    metrics.inc('generate.requests');
    res.json(result);
  } catch (e) {
    metrics.endTimer(timer);
    metrics.inc('generate.errors');
    res.status(502).json({ error: e.message });
  }
});

app.post('/inference/chat', async (req, res) => {
  const timer = metrics.startTimer('endpoint.chat');
  try {
    const { messages, params } = req.body || {};
    if (!messages || !Array.isArray(messages)) { res.status(400).json({ error: 'messages array is required' }); return; }
    const result = await models.chat(messages, params || {});
    metrics.endTimer(timer);
    res.json(result);
  } catch (e) {
    metrics.endTimer(timer);
    res.status(502).json({ error: e.message });
  }
});

// ────────── Metrics endpoint ──────────
app.get('/metrics', (req, res) => {
  res.json(metrics.snapshot());
});

// ────────── Auth (local — cryptographic tokens, no stubs) ──────────
const tokenStore = new Map();

app.post('/auth/token', (req, res) => {
  const accessToken = crypto.randomBytes(32).toString('hex');
  const refreshToken = crypto.randomBytes(32).toString('hex');
  const expiresAt = Date.now() + 3600000;
  tokenStore.set(accessToken, { userId: 'rawrxd-local', expiresAt, refreshToken });
  // Clean expired tokens
  for (const [k, v] of tokenStore) { if (v.expiresAt < Date.now()) tokenStore.delete(k); }
  res.json({ accessToken, refreshToken, expiresIn: 3600 });
});

app.post('/auth/validate', (req, res) => {
  const token = req.body?.token || req.headers.authorization?.replace('Bearer ', '');
  if (!token) { res.status(401).json({ valid: false, reason: 'No token provided' }); return; }
  const entry = tokenStore.get(token);
  if (!entry || entry.expiresAt < Date.now()) {
    tokenStore.delete(token);
    res.status(401).json({ valid: false, reason: 'Token expired or invalid' });
    return;
  }
  res.json({ valid: true, userId: entry.userId, expiresAt: entry.expiresAt });
});

// ────────── Start ──────────
const PORT = process.env.PORT || 3000;
app.listen(PORT, () => {
  console.log(`RawrXD IDE Model Server on :${PORT}  (root: ${IDE_ROOT})`);
  console.log(`  Endpoints: /q/chat  /codewhisperer/suggestions  /inference/generate  /models/*  /metrics`);
  console.log(`  No hardcoded responses — all routes require an active model backend`);
});
