// RawrXD IDE Model Proxy Server — all paths on D:\rawrxd
const express = require('express');
const path = require('path');
const { ModelManager, IDE_ROOT } = require('./model-loader');
const app = express();
const models = new ModelManager(path.join(__dirname, 'config.json'));

app.use(express.json());

// IDE chat endpoint
app.post('/q/chat', (req, res) => {
  res.json({
    response: "Local IDE response for: " + req.body.message,
    conversationId: "rawrxd-" + Date.now()
  });
});

// Code completion endpoint
app.post('/codewhisperer/suggestions', (req, res) => {
  res.json({
    suggestions: [{
      content: "// RawrXD local code suggestion",
      range: { start: 0, end: 0 }
    }]
  });
});

// Telemetry sink (no-op)
app.post('/telemetry', (req, res) => {
  res.status(200).send('OK');
});

// ---------- Model management endpoints ----------

app.post('/models/load', async (req, res) => {
  try {
    const { source, model } = req.body || {};
    const result = await models.loadModel(source, model);
    res.json(result);
  } catch (e) {
    res.status(500).json({ error: e.message });
  }
});

app.post('/models/unload', async (req, res) => {
  try {
    const { source } = req.body || {};
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
    const result = await models.switchActive(source);
    res.json(result);
  } catch (e) {
    res.status(500).json({ error: e.message });
  }
});

app.post('/inference/generate', async (req, res) => {
  try {
    const { prompt, params } = req.body || {};
    const result = await models.generate(prompt, params);
    res.json(result);
  } catch (e) {
    res.status(500).json({ error: e.message });
  }
});

// Auth endpoints (local bypass)
app.post('/auth/token', (req, res) => {
  const crypto = require('crypto');
  res.json({
    accessToken: 'rawrxd-' + crypto.randomBytes(16).toString('hex'),
    refreshToken: 'rawrxd-' + crypto.randomBytes(16).toString('hex'),
    expiresIn: 3600
  });
});

app.post('/auth/validate', (req, res) => {
  res.json({ valid: true, userId: 'rawrxd-local' });
});

const PORT = process.env.PORT || 3000;
app.listen(PORT, () => {
  console.log(`RawrXD IDE Model Server on :${PORT}  (root: ${IDE_ROOT})`);
});
