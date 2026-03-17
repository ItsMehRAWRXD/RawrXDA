const express = require('express');
const helmet = require('helmet');
const rateLimit = require('express-rate-limit');
const http = require('http');
const { spawn } = require('child_process');

const app = express();
const PORT = 11441;
const DEFAULT_MODEL = 'bigdaddyg:latest';

const MODEL_ALIAS_ENTRIES = [
  ['bigdaddyg:latest', 'bigdaddyg:latest'],
  ['bigdaddyg-latest', 'bigdaddyg:latest'],
  ['bigdaddyg_latest', 'bigdaddyg:latest'],
  ['bigdaddyg latest', 'bigdaddyg:latest'],
  ['bigdaddyg', 'bigdaddyg:latest'],
  ['bigdaddyg:security', 'bigdaddyg:latest'],
  ['bigdaddyg-security', 'bigdaddyg:latest'],
  ['bigdaddyg_security', 'bigdaddyg:latest'],
  ['bigdaddyg security', 'bigdaddyg:latest'],
  ['bigdaddyg:code', 'bigdaddyg:coder'],
  ['bigdaddyg-code', 'bigdaddyg:coder'],
  ['bigdaddyg_code', 'bigdaddyg:coder'],
  ['bigdaddyg coder', 'bigdaddyg:coder'],
  ['bigdaddyg:coder', 'bigdaddyg:coder'],
  ['bigdaddyg:python', 'bigdaddyg:python'],
  ['bigdaddyg:javascript', 'bigdaddyg:javascript'],
  ['bigdaddyg:asm', 'bigdaddyg:asm'],
  ['bigdaddyg-coder', 'bigdaddyg:coder'],
  ['bigdaddyg-python', 'bigdaddyg:python'],
  ['bigdaddyg-javascript', 'bigdaddyg:javascript'],
  ['bigdaddyg-asm', 'bigdaddyg:asm'],
  ['bigdaddyg:security-pro', 'bigdaddyg:latest'],
  ['bigdaddyg-security-pro', 'bigdaddyg:latest'],
  ['BigDaddyG:Latest'.toLowerCase(), 'bigdaddyg:latest'],
  ['BigDaddyG:Security'.toLowerCase(), 'bigdaddyg:latest'],
  ['BigDaddyG Security'.toLowerCase(), 'bigdaddyg:latest'],
  ['BigDaddyG Latest'.toLowerCase(), 'bigdaddyg:latest'],
  ['your-custom-model:latest', 'your-custom-model:latest'],
  ['your-custom-model', 'your-custom-model:latest']
];

const MODEL_ALIAS_MAP = new Map(
  MODEL_ALIAS_ENTRIES.map(([alias, target]) => [alias.toLowerCase(), target])
);

// Security middleware
app.use(helmet({
  contentSecurityPolicy: false,
  crossOriginEmbedderPolicy: false
}));

// Rate limiting
const limiter = rateLimit({
  windowMs: 15 * 60 * 1000, // 15 minutes
  max: 100, // limit each IP to 100 requests per windowMs
  message: 'Too many requests from this IP'
});
app.use('/api/', limiter);

// CORS and JSON parsing
app.use((req, res, next) => {
  res.header('Access-Control-Allow-Origin', '*');
  res.header('Access-Control-Allow-Methods', 'GET, POST, PUT, DELETE, OPTIONS');
  res.header('Access-Control-Allow-Headers', 'Origin, X-Requested-With, Content-Type, Accept, Authorization');
  if (req.method === 'OPTIONS') {
    res.sendStatus(200);
  } else {
    next();
  }
});

app.use(express.json({ limit: '50mb' }));

// BigDaddyG model registry
const BIGDADDYG_MODELS = {
  'bigdaddyg:latest': { name: 'BigDaddyG Latest', size: '4.7 GB', type: 'general' },
  'bigdaddyg:coder': { name: 'BigDaddyG Coder', size: '13B', type: 'coding' },
  'bigdaddyg:python': { name: 'BigDaddyG Python', size: '7B', type: 'python' },
  'bigdaddyg:javascript': { name: 'BigDaddyG JavaScript', size: '7B', type: 'javascript' },
  'bigdaddyg:asm': { name: 'BigDaddyG Assembly', size: '7B', type: 'assembly' },
  'your-custom-model:latest': { name: 'Your Custom Model', size: '4.7 GB', type: 'custom' }
};

function normalizeModelId(model) {
  if (!model) return DEFAULT_MODEL;
  const trimmed = String(model).trim();
  if (!trimmed) return DEFAULT_MODEL;
  const lookupKey = trimmed.toLowerCase();
  if (MODEL_ALIAS_MAP.has(lookupKey)) {
    return MODEL_ALIAS_MAP.get(lookupKey);
  }
  return trimmed;
}

function resolveBigDaddyModel(model) {
  const normalized = normalizeModelId(model);
  if (normalized && BIGDADDYG_MODELS[normalized]) {
    return normalized;
  }
  return DEFAULT_MODEL;
}

function normalizeChatMessages(body = {}) {
  const { messages, message, prompt } = body;

  if (Array.isArray(messages) && messages.length > 0) {
    return messages
      .filter(entry => entry && typeof entry.content === 'string' && entry.content.trim().length > 0)
      .map(entry => ({
        role: entry.role && typeof entry.role === 'string' ? entry.role : 'user',
        content: entry.content.trim()
      }));
  }

  const fallbackContent = [message, prompt].find(value => typeof value === 'string' && value.trim().length > 0);
  if (fallbackContent) {
    return [
      {
        role: 'user',
        content: fallbackContent.trim()
      }
    ];
  }

  return [];
}

function getLastUserMessage(messages = []) {
  for (let i = messages.length - 1; i >= 0; i -= 1) {
    const entry = messages[i];
    if (!entry || typeof entry.content !== 'string') {
      continue;
    }
    if (entry.role && entry.role.toLowerCase() === 'assistant') {
      continue;
    }
    return entry;
  }
  return messages.length > 0 ? messages[messages.length - 1] : null;
}

function extractContentFromResponse(raw) {
  if (!raw) {
    return '';
  }

  if (typeof raw === 'string') {
    return raw;
  }

  if (Array.isArray(raw)) {
    const first = raw[0];
    return extractContentFromResponse(first);
  }

  if (typeof raw === 'object') {
    if (raw.response && typeof raw.response === 'string') {
      return raw.response;
    }
    if (raw.reply && typeof raw.reply === 'string') {
      return raw.reply;
    }
    if (raw.message && typeof raw.message === 'object' && typeof raw.message.content === 'string') {
      return raw.message.content;
    }
    if (Array.isArray(raw.choices) && raw.choices.length > 0) {
      const choice = raw.choices[0];
      if (choice) {
        if (typeof choice.text === 'string') {
          return choice.text;
        }
        if (choice.message && typeof choice.message.content === 'string') {
          return choice.message.content;
        }
      }
    }
  }

  return '';
}

function formatChatResponse(raw, fallbackModel) {
  const model = (raw && typeof raw === 'object' && raw.model) ? raw.model : fallbackModel;
  const content = extractContentFromResponse(raw);
  const assistantMessage = {
    role: 'assistant',
    content
  };

  if (raw && typeof raw === 'object') {
    return {
      ...raw,
      model,
      response: content,
      message: raw.message && typeof raw.message === 'object'
        ? { role: raw.message.role || 'assistant', content: raw.message.content || content }
        : assistantMessage,
      choices: Array.isArray(raw.choices) && raw.choices.length > 0
        ? raw.choices
        : [{ index: 0, finish_reason: 'stop', message: assistantMessage, text: content }]
    };
  }

  return {
    model,
    response: content,
    message: assistantMessage,
    choices: [{ index: 0, finish_reason: 'stop', message: assistantMessage, text: content }]
  };
}

function formatCompletionResponse(raw, fallbackModel) {
  const content = extractContentFromResponse(raw);
  const payload = raw && typeof raw === 'object' ? { ...raw } : { raw };

  if (!payload.model) {
    payload.model = fallbackModel || DEFAULT_MODEL;
  }

  payload.response = content;
  payload.reply = payload.reply || content;

  if (!Array.isArray(payload.choices) || payload.choices.length === 0) {
    payload.choices = [
      {
        index: 0,
        finish_reason: 'stop',
        text: content
      }
    ];
  }

  return payload;
}

// Health check
app.get('/health', (req, res) => {
  res.json({
    status: 'healthy',
    timestamp: new Date().toISOString(),
    server: 'Orchestra-BigDaddyG',
    version: '2.0.0'
  });
});

// List models
app.get('/api/models', async (req, res) => {
  try {
    const ollamaModels = await getOllamaModels();
    const models = [
      ...Object.entries(BIGDADDYG_MODELS).map(([id, info]) => ({
        name: id,
        ...info,
        modified_at: new Date().toISOString(),
        size: 0,
        digest: `bigdaddyg-${id}`,
        details: { format: 'gguf', family: 'bigdaddyg' }
      })),
      ...ollamaModels
    ];

    res.json({ models });
  } catch (error) {
    res.status(500).json({ error: error.message });
  }
});

// Generate completion
app.post('/api/generate', async (req, res) => {
  const { model, prompt, stream = false } = req.body || {};
  const normalizedModel = normalizeModelId(model || DEFAULT_MODEL);
  const resolvedModel = resolveBigDaddyModel(normalizedModel);

  if (!model || !prompt) {
    return res.status(400).json({ error: 'Model and prompt required' });
  }

  try {
    if (BIGDADDYG_MODELS[resolvedModel]) {
      // BigDaddyG model processing
      const response = await processBigDaddyGRequest(resolvedModel, prompt, stream);
      if (stream) {
        res.setHeader('Content-Type', 'application/x-ndjson');
        response.forEach(chunk => res.write(JSON.stringify(chunk) + '\n'));
        res.end();
      } else {
        res.json(response);
      }
    } else {
      // Forward to Ollama
      const response = await forwardToOllama('/api/generate', {
        ...req.body,
        model: normalizedModel
      });
      res.json(response);
    }
  } catch (error) {
    res.status(500).json({ error: error.message });
  }
});

// Chat completion
app.post('/api/chat', async (req, res) => {
  const body = req.body || {};
  const stream = Boolean(body.stream);
  const requestedModel = normalizeModelId(body.model);
  const normalizedMessages = normalizeChatMessages(body);

  if (normalizedMessages.length === 0) {
    return res.status(400).json({ error: 'No user message provided' });
  }

  const bigDaddyModel = resolveBigDaddyModel(requestedModel);
  const targetModel = BIGDADDYG_MODELS[bigDaddyModel] ? bigDaddyModel : (requestedModel || bigDaddyModel);

  try {
    if (BIGDADDYG_MODELS[bigDaddyModel]) {
      const response = await processBigDaddyGChat(bigDaddyModel, normalizedMessages, stream);

      if (stream) {
        res.setHeader('Content-Type', 'application/x-ndjson');
        response.forEach(chunk => res.write(JSON.stringify(chunk) + '\n'));
        return res.end();
      }

      return res.json(formatChatResponse(response, bigDaddyModel));
    }

    const upstreamBody = {
      ...body,
      model: targetModel,
      messages: normalizedMessages
    };

    const upstreamResponse = await forwardToOllama('/api/chat', upstreamBody);
    return res.json(formatChatResponse(upstreamResponse, targetModel));
  } catch (error) {
    console.error('[Orchestra] /api/chat error:', error);
    return res.status(500).json({ error: error.message || 'Chat request failed' });
  }
});

/* ----------  stubs the IDE currently calls but we hadn’t defined  ---------- */

// Autocomplete fallback (IDE calls /api/query)
app.post('/api/query', async (req, res) => {
  const { prompt } = req.body || {};
  const requestedModel = normalizeModelId((req.body && req.body.model) || DEFAULT_MODEL);

  if (!prompt || typeof prompt !== 'string' || !prompt.trim()) {
    return res.status(400).json({ error: 'missing prompt' });
  }

  try {
    const resolvedModel = resolveBigDaddyModel(requestedModel);
    const reply = await processBigDaddyGRequest(resolvedModel, prompt, false);
    return res.json(formatCompletionResponse(reply, resolvedModel));
  } catch (error) {
    console.error('[Orchestra] /api/query error:', error);
    return res.status(500).json({ error: error.message || 'Autocomplete request failed' });
  }
});

// Model-discovery (IDE calls /api/models/list)
app.get('/api/models/list', async (_req, res) => {
  try {
    const ollamaModels = await getOllamaModels();
    const bigDaddyModels = Object.entries(BIGDADDYG_MODELS).map(([id, info]) => ({
      id,
      name: info.name,
      owned_by: 'BigDaddyG',
      permission: [],
      size: info.size || 'unknown',
      provider: 'BigDaddyG'
    }));

    const formattedOllama = ollamaModels.map(m => ({
      id: m.name,
      name: m.name,
      owned_by: 'BigDaddyG', // Changed from 'Ollama' - user made these models
      permission: [],
      size: m.size || m.parameter_size || 'unknown',
      provider: 'BigDaddyG' // Changed from 'Ollama' - user owns these
    }));

    const allModels = [...bigDaddyModels, ...formattedOllama];

    res.json({
      models: allModels,
      ollama: formattedOllama,
      bigdaddyg: bigDaddyModels,
      total: allModels.length
    });
  } catch (e) {
    res.status(500).json({ error: e.message });
  }
});

// BigDaddyG processing functions
async function processBigDaddyGRequest(model, prompt, stream) {
  const resolvedModel = resolveBigDaddyModel(model);
  const modelInfo = BIGDADDYG_MODELS[resolvedModel];
  const response = generateBigDaddyGResponse(prompt, modelInfo, resolvedModel);

  if (stream) {
    return response.split(' ').map((word, i, arr) => ({
      model: resolvedModel,
      created_at: new Date().toISOString(),
      response: word + (i < arr.length - 1 ? ' ' : ''),
      done: i === arr.length - 1
    }));
  }

  return {
    model: resolvedModel,
    created_at: new Date().toISOString(),
    response,
    reply: response,
    done: true,
    context: [],
    total_duration: 1000000000,
    load_duration: 100000000,
    prompt_eval_count: prompt.length,
    eval_count: response.length,
    eval_duration: 900000000,
    choices: [
      {
        index: 0,
        finish_reason: 'stop',
        text: response
      }
    ]
  };
}

async function processBigDaddyGChat(model, messages = [], stream) {
  const resolvedModel = resolveBigDaddyModel(model);
  const lastMessage = getLastUserMessage(messages);
  const prompt = lastMessage && typeof lastMessage.content === 'string' ? lastMessage.content : '';
  const modelInfo = BIGDADDYG_MODELS[resolvedModel];
  const response = generateBigDaddyGResponse(prompt, modelInfo, resolvedModel);

  if (stream) {
    return response.split(' ').map((word, i, arr) => ({
      model: resolvedModel,
      created_at: new Date().toISOString(),
      message: {
        role: 'assistant',
        content: word + (i < arr.length - 1 ? ' ' : '')
      },
      done: i === arr.length - 1
    }));
  }

  const assistantMessage = {
    role: 'assistant',
    content: response
  };

  return {
    model: resolvedModel,
    created_at: new Date().toISOString(),
    message: assistantMessage,
    response,
    reply: response,
    done: true,
    choices: [
      {
        index: 0,
        finish_reason: 'stop',
        message: assistantMessage,
        text: response
      }
    ]
  };
}

function generateBigDaddyGResponse(prompt = '', modelInfo, modelKey) {
  const fallbackInfo = BIGDADDYG_MODELS[DEFAULT_MODEL];
  const info = modelInfo || fallbackInfo || { name: modelKey || DEFAULT_MODEL, type: 'general' };

  const responses = {
    coding: [
      "Here's an optimized solution using modern best practices:",
      "Let me refactor this code for better performance:",
      "I'll implement this with proper error handling:"
    ],
    python: [
      "Here's a Pythonic approach to solve this:",
      "Let me write this using Python best practices:",
      "I'll implement this with proper type hints:"
    ],
    javascript: [
      "Here's a modern JavaScript solution:",
      "Let me implement this using ES6+ features:",
      "I'll write this with proper async/await handling:"
    ],
    general: [
      "Based on my analysis, here's the solution:",
      "Let me break this down step by step:",
      "Here's a comprehensive approach:"
    ]
  };

  const typeResponses = responses[info.type] || responses.general;
  const baseResponse = typeResponses[Math.floor(Math.random() * typeResponses.length)];
  const sanitizedPrompt = typeof prompt === 'string' ? prompt : '';

  return `${baseResponse}\n\n\`\`\`\n// BigDaddyG ${info.name} response\n// Processing: ${sanitizedPrompt.substring(0, 50)}...\nconsole.log("BigDaddyG is processing your request...");\n\`\`\`\n\nThis is a simulated response from ${info.name}. In a full implementation, this would connect to the actual BigDaddyG model inference engine.`;
}

// Ollama integration
async function getOllamaModels() {
  try {
    const response = await fetch('http://localhost:11434/api/tags');
    if (!response.ok) throw new Error('Ollama not available');
    const data = await response.json();
    return data.models || [];
  } catch (error) {
    return [];
  }
}

async function forwardToOllama(endpoint, body = {}) {
  const normalizedBody = {
    ...body,
    model: body && body.model ? normalizeModelId(body.model) : body.model
  };

  const response = await fetch(`http://localhost:11434${endpoint}`, {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify(normalizedBody)
  });

  if (!response.ok) {
    throw new Error(`Ollama error: ${response.status}`);
  }

  return response.json();
}

// Start server
const server = http.createServer(app);

server.listen(PORT, () => {
  console.log(`🎼 Orchestra-BigDaddyG Server running on port ${PORT}`);
  console.log(`🔒 Security middleware enabled`);
  console.log(`⚡ Rate limiting active`);
  console.log(`🤖 BigDaddyG models loaded: ${Object.keys(BIGDADDYG_MODELS).length}`);
});

server.on('error', (error) => {
  if (error.code === 'EADDRINUSE') {
    console.log(`⚠️ Port ${PORT} already in use - server may already be running`);
  } else {
    console.error('Server error:', error);
  }
});

module.exports = { app, server };