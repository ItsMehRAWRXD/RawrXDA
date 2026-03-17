import express from 'express';
import cors from 'cors';
import fetch from 'node-fetch';

const app = express();
const PORT = 11442;
const OLLAMA_URL = 'http://localhost:11434';

app.use(cors());
app.use(express.json());

// Health check
app.get('/health', (req, res) => {
  res.json({ status: 'ok', timestamp: new Date().toISOString() });
});

// List models (Ollama format)
app.get('/models', async (req, res) => {
  try {
    const response = await fetch(`${OLLAMA_URL}/api/tags`);
    const data = await response.json();
    res.json({ models: data.models || [] });
  } catch (error) {
    console.error('Failed to fetch models:', error);
    res.status(500).json({ error: 'Failed to fetch models from Ollama' });
  }
});

// List models (OpenAI format for IDE compatibility)
app.get('/v1/models', async (req, res) => {
  try {
    const response = await fetch(`${OLLAMA_URL}/api/tags`);
    const data = await response.json();
    // Convert Ollama format to OpenAI format
    const models = (data.models || []).map(model => ({
      id: model.name,
      object: 'model',
      created: Date.now(),
      owned_by: 'ollama'
    }));
    res.json({ object: 'list', data: models });
  } catch (error) {
    console.error('Failed to fetch models:', error);
    res.status(500).json({ error: 'Failed to fetch models from Ollama' });
  }
});

// Ollama tags endpoint (for compatibility)
app.get('/api/tags', async (req, res) => {
  try {
    const response = await fetch(`${OLLAMA_URL}/api/tags`);
    const data = await response.json();
    res.json(data);
  } catch (error) {
    console.error('Failed to fetch tags:', error);
    res.status(500).json({ error: 'Failed to fetch tags from Ollama' });
  }
});

// Generate completion (Ollama format)
app.post('/generate', async (req, res) => {
  try {
    const { model, prompt, options } = req.body;

    const response = await fetch(`${OLLAMA_URL}/api/generate`, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({
        model: model || 'llama2',
        prompt: prompt,
        stream: false,
        options: options || {}
      })
    });

    const data = await response.json();
    res.json(data);
  } catch (error) {
    console.error('Generation error:', error);
    res.status(500).json({ error: error.message });
  }
});

// Chat completions (OpenAI format for IDE compatibility)
app.post('/v1/chat/completions', async (req, res) => {
  try {
    const { model, messages, temperature, max_tokens } = req.body;

    // Convert OpenAI messages format to Ollama prompt
    let prompt = '';
    if (Array.isArray(messages)) {
      prompt = messages.map(msg => {
        if (msg.role === 'system') return `System: ${msg.content}\n`;
        if (msg.role === 'user') return `User: ${msg.content}\n`;
        if (msg.role === 'assistant') return `Assistant: ${msg.content}\n`;
        return msg.content;
      }).join('\n');
    }

    const response = await fetch(`${OLLAMA_URL}/api/generate`, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({
        model: model || 'llama2',
        prompt: prompt,
        stream: false,
        options: {
          temperature: temperature || 0.7,
          num_predict: max_tokens || 2000
        }
      })
    });

    const data = await response.json();

    // Convert Ollama response to OpenAI format
    res.json({
      id: 'chatcmpl-' + Date.now(),
      object: 'chat.completion',
      created: Date.now(),
      model: model || 'llama2',
      choices: [{
        index: 0,
        message: {
          role: 'assistant',
          content: data.response || ''
        },
        finish_reason: 'stop'
      }],
      usage: {
        prompt_tokens: 0,
        completion_tokens: 0,
        total_tokens: 0
      }
    });
  } catch (error) {
    console.error('Chat completion error:', error);
    res.status(500).json({ error: error.message });
  }
});

app.listen(PORT, () => {
  console.log(`🎵 Orchestra Server running on http://localhost:${PORT}`);
  console.log(`📡 Proxying to Ollama at ${OLLAMA_URL}`);
});
