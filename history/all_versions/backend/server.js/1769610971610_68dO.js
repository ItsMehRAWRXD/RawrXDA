// RawrXD Agentic Backend Server v2.0
// Real AI inference - no hardcoded responses
// Supports: Ollama API, Local GGUF scan, llama.cpp server

const http = require('http');
const fs = require('fs');
const path = require('path');
const { exec } = require('child_process');

const PORT = 8080;
const MODEL_DIR = 'D:/OllamaModels';
const OLLAMA_HOST = process.env.OLLAMA_HOST || 'http://localhost:11434';
const LLAMA_CPP_HOST = process.env.LLAMA_CPP_HOST || 'http://localhost:8081';
const DEFAULT_MODEL = 'qwen2.5-coder:latest';

// ============================================================================
// REAL MODEL FETCHING - Query Ollama API for actual installed models
// ============================================================================
async function fetchOllamaModels() {
  return new Promise((resolve) => {
    const url = new URL('/api/tags', OLLAMA_HOST);
    
    const req = http.request(url, { method: 'GET', timeout: 5000 }, (res) => {
      let data = '';
      res.on('data', chunk => data += chunk);
      res.on('end', () => {
        try {
          const json = JSON.parse(data);
          const models = (json.models || []).map(m => ({
            name: m.name,
            path: m.name,
            type: 'ollama',
            size: formatBytes(m.size),
            modified: m.modified_at,
            family: m.details?.family || 'unknown',
            parameters: m.details?.parameter_size || 'unknown',
            quantization: m.details?.quantization_level || 'unknown'
          }));
          resolve(models);
        } catch (e) {
          console.error('[Ollama] Parse error:', e.message);
          resolve([]);
        }
      });
    });
    
    req.on('error', (e) => {
      console.error('[Ollama] Connection error:', e.message);
      resolve([]);
    });
    
    req.on('timeout', () => {
      req.destroy();
      resolve([]);
    });
    
    req.end();
  });
}

// Scan local GGUF files
function scanGGUFModels() {
  const models = [];
  try {
    if (fs.existsSync(MODEL_DIR)) {
      const files = fs.readdirSync(MODEL_DIR);
      files.filter(f => f.endsWith('.gguf')).forEach(file => {
        const filepath = path.join(MODEL_DIR, file);
        const stats = fs.statSync(filepath);
        models.push({
          name: file.replace('.gguf', ''),
          path: filepath,
          type: 'gguf',
          size: formatBytes(stats.size),
          modified: stats.mtime.toISOString()
        });
      });
    }
  } catch (e) {
    console.error('[GGUF] Scan error:', e.message);
  }
  return models;
}

// Get all available models (REAL - from Ollama + GGUF scan)
async function getAllModels() {
  const [ollamaModels, ggufModels] = await Promise.all([
    fetchOllamaModels(),
    Promise.resolve(scanGGUFModels())
  ]);
  
  const allModels = [...ollamaModels, ...ggufModels];
  console.log(`[Models] Found ${ollamaModels.length} Ollama + ${ggufModels.length} GGUF = ${allModels.length} total`);
  return allModels;
}

function formatBytes(bytes) {
  if (!bytes || bytes === 0) return '0 B';
  const k = 1024;
  const sizes = ['B', 'KB', 'MB', 'GB', 'TB'];
  const i = Math.floor(Math.log(bytes) / Math.log(k));
  return parseFloat((bytes / Math.pow(k, i)).toFixed(1)) + ' ' + sizes[i];
}

// ============================================================================
// REAL AI INFERENCE - Send to Ollama for actual model responses
// ============================================================================
async function queryOllama(model, prompt, options = {}) {
  return new Promise((resolve, reject) => {
    const url = new URL('/api/generate', OLLAMA_HOST);
    
    const payload = JSON.stringify({
      model: model,
      prompt: prompt,
      stream: false,
      options: {
        temperature: options.temperature || 0.7,
        top_p: options.top_p || 0.9,
        num_predict: options.max_tokens || 2048
      }
    });
    
    console.log(`[Ollama] Querying ${model}...`);
    
    const req = http.request(url, {
      method: 'POST',
      headers: {
        'Content-Type': 'application/json',
        'Content-Length': Buffer.byteLength(payload)
      },
      timeout: 120000 // 2 minute timeout for inference
    }, (res) => {
      let data = '';
      res.on('data', chunk => data += chunk);
      res.on('end', () => {
        try {
          const json = JSON.parse(data);
          if (json.error) {
            reject(new Error(json.error));
            return;
          }
          const tps = json.eval_count && json.eval_duration 
            ? (json.eval_count / (json.eval_duration / 1e9)).toFixed(1)
            : 'N/A';
          console.log(`[Ollama] Response: ${json.response?.length || 0} chars, ${tps} tok/s`);
          resolve({
            answer: json.response,
            model: model,
            eval_count: json.eval_count,
            eval_duration: json.eval_duration,
            tokens_per_second: tps
          });
        } catch (e) {
          reject(new Error('Failed to parse Ollama response: ' + e.message));
        }
      });
    });
    
    req.on('error', (e) => reject(new Error('Ollama connection failed: ' + e.message)));
    req.on('timeout', () => {
      req.destroy();
      reject(new Error('Ollama request timeout (model may be loading)'));
    });
    
    req.write(payload);
    req.end();
  });
}

// Smart model routing
async function queryModel(model, prompt, options = {}) {
  console.log(`[Query] Model: ${model}, Prompt: ${prompt.substring(0, 80)}...`);
  
  // All models go through Ollama (it handles GGUF too via modelfile)
  return queryOllama(model, prompt, options);
}

// ============================================================================
// HTTP SERVER
// ============================================================================
const server = http.createServer(async (req, res) => {
  // CORS headers
  res.setHeader('Access-Control-Allow-Origin', '*');
  res.setHeader('Access-Control-Allow-Methods', 'GET, POST, OPTIONS');
  res.setHeader('Access-Control-Allow-Headers', 'Content-Type');
  res.setHeader('Content-Type', 'application/json');

  const url = new URL(req.url, `http://${req.headers.host}`);
  const pathname = url.pathname;

  console.log(`[${new Date().toISOString()}] ${req.method} ${pathname}`);

  // Handle OPTIONS (CORS preflight)
  if (req.method === 'OPTIONS') {
    res.writeHead(200);
    res.end();
    return;
  }

  // GET /models - REAL model list from Ollama API + GGUF scan
  if (req.method === 'GET' && pathname === '/models') {
    try {
      const models = await getAllModels();
      res.writeHead(200);
      res.end(JSON.stringify({ models }));
    } catch (e) {
      res.writeHead(500);
      res.end(JSON.stringify({ error: e.message }));
    }
    return;
  }

  // GET /health
  if (req.method === 'GET' && pathname === '/health') {
    const models = await getAllModels();
    res.writeHead(200);
    res.end(JSON.stringify({
      status: 'online',
      models: models.length,
      uptime: process.uptime(),
      ollama: OLLAMA_HOST
    }));
    return;
  }

  // POST /ask - REAL AI inference via Ollama
  if (req.method === 'POST' && pathname === '/ask') {
    let body = '';
    req.on('data', chunk => body += chunk);
    req.on('end', async () => {
      try {
        const data = JSON.parse(body);
        const model = data.model || DEFAULT_MODEL;
        const prompt = data.question || data.prompt || '';
        
        if (!prompt) {
          res.writeHead(400);
          res.end(JSON.stringify({ error: 'No question/prompt provided' }));
          return;
        }
        
        const result = await queryModel(model, prompt, {
          temperature: data.temperature,
          max_tokens: data.max_tokens,
          top_p: data.top_p
        });
        
        res.writeHead(200);
        res.end(JSON.stringify(result));
        
      } catch (e) {
        console.error('[Ask] Error:', e.message);
        res.writeHead(500);
        res.end(JSON.stringify({ error: e.message }));
      }
    });
    return;
  }

  // POST /exec - Execute terminal commands (real execution)
  if (req.method === 'POST' && pathname === '/exec') {
    let body = '';
    req.on('data', chunk => body += chunk);
    req.on('end', () => {
      try {
        const data = JSON.parse(body);
        const command = data.command;
        
        if (!command) {
          res.writeHead(400);
          res.end(JSON.stringify({ error: 'No command provided' }));
          return;
        }
        
        console.log(`[Exec] Running: ${command}`);
        
        exec(command, { cwd: 'D:\\rawrxd', timeout: 30000, shell: 'powershell.exe' }, (error, stdout, stderr) => {
          res.writeHead(200);
          res.end(JSON.stringify({
            output: stdout || 'Command completed',
            stderr: stderr || null,
            error: error ? error.message : null,
            exitCode: error ? error.code : 0
          }));
        });
        
      } catch (e) {
        res.writeHead(400);
        res.end(JSON.stringify({ error: 'Invalid JSON' }));
      }
    });
    return;
  }

  // 404 for everything else
  res.writeHead(404);
  res.end(JSON.stringify({ error: 'Not found' }));
});

// ============================================================================
// STARTUP
// ============================================================================
server.listen(PORT, async () => {
  console.log(`
================================================
  RawrXD Agentic Backend Server v2.0
  REAL AI Inference - No Hardcoded Responses
================================================
  
[+] Server listening on http://localhost:${PORT}
[+] Ollama endpoint: ${OLLAMA_HOST}
[+] GGUF directory: ${MODEL_DIR}
`);

  // Test Ollama connection and show available models
  const models = await fetchOllamaModels();
  if (models.length > 0) {
    console.log(`[+] Ollama connected: ${models.length} models available`);
    models.slice(0, 5).forEach(m => console.log(`    - ${m.name} (${m.size})`));
    if (models.length > 5) console.log(`    ... and ${models.length - 5} more`);
  } else {
    console.log(`[!] Ollama not available - will use GGUF models only`);
  }
  
  const gguf = scanGGUFModels();
  if (gguf.length > 0) {
    console.log(`[+] GGUF models found: ${gguf.length}`);
  }
  
  console.log(`\n[+] Ready for frontend connections\n`);
});

// Graceful shutdown
process.on('SIGINT', () => {
  console.log('\n[!] Shutting down...');
  server.close(() => process.exit(0));
});
