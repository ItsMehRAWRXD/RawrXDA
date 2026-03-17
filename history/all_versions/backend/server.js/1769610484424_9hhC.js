// RawrXD Simple HTTP Backend Server
// Provides /models and /ask endpoints for the IDE frontend

const http = require('http');
const fs = require('fs');
const path = require('path');

const PORT = 8080;
const MODEL_DIR = 'D:/OllamaModels';

// Get local GGUF models
function getLocalModels() {
  const models = [];

  // Scan GGUF directory
  try {
    if (fs.existsSync(MODEL_DIR)) {
      const files = fs.readdirSync(MODEL_DIR);
      files.filter(f => f.endsWith('.gguf')).forEach(file => {
        const stats = fs.statSync(path.join(MODEL_DIR, file));
        const sizeGB = (stats.size / (1024 * 1024 * 1024)).toFixed(1);
        models.push({
          name: file.replace('.gguf', ''),
          path: path.join(MODEL_DIR, file),
          type: 'gguf',
          size: `${sizeGB}GB`
        });
      });
    }
  } catch (e) {
    console.error('Error scanning model directory:', e.message);
  }

  // Add Ollama models
  const ollamaModels = [
    'dolphin3:latest', 'gemma3:12b', 'gemma3:latest', 'llama3:latest',
    'qwen3:8b', 'qwen3:4b', 'qwen2.5-coder:latest', 'phi:latest',
    'ministral-3:latest', 'deepseek-coder:latest', 'bigdaddyg:latest',
    'bigdaddyg-agentic:latest', 'bigdaddyg-fast:latest'
  ];

  ollamaModels.forEach(name => {
    models.push({ name, path: name, type: 'ollama', size: 'varies' });
  });

  // Add cloud models
  const cloudModels = [
    'qwen3-coder:480b-cloud', 'deepseek-v3.1:671b-cloud',
    'gpt-oss:120b-cloud', 'kimi-k2-thinking:cloud'
  ];

  cloudModels.forEach(name => {
    models.push({ name, path: name, type: 'cloud', size: 'Cloud' });
  });

  return models;
}

// Generate response based on question
function generateAnswer(question, model, language) {
  const q = (question || '').toLowerCase();

  if (q.includes('swarm')) {
    return `**Swarm Mode Help**

Deploy multiple AI agents for parallel processing:

\`\`\`powershell
# Deploy swarm to directory
.\\swarm_control.ps1 -Operation deploy -TargetDirectory "D:\\target" -SwarmSize 5
\`\`\`

Or use the Swarm panel in the IDE sidebar. Max 40 agents supported.`;
  }

  if (q.includes('model')) {
    return `**Model Management**

Available model types:
- **GGUF**: Local files in D:/OllamaModels/
- **Ollama**: Via Ollama API
- **Cloud**: Remote inference for large models

Use the dropdown to select, or GET /models for the full list.`;
  }

  if (q.includes('benchmark')) {
    return `**Benchmarking**

Current hardware profile:
- GPU: AMD RX 7800 XT (16GB VRAM)
- RAM: 64GB DDR5-5600
- Expected: ~3,158 TPS for 3.8B models

Run benchmarks via the Debug panel or:
\`\`\`powershell
.\\autonomous_finetune_bench.ps1 -Operation benchmark
\`\`\``;
  }

  if (q.includes('hello') || q.includes('hi')) {
    return `Hello! I'm the RawrXD IDE Assistant running on the MASM backend. How can I help you today?`;
  }

  if (q.includes('todo') || q.includes('task')) {
    return `**Task Management**

Add tasks via:
\`\`\`powershell
.\\todo_manager.ps1 -Operation add -TodoText "Your task" -Priority High
\`\`\`

Or use the quick action buttons in the IDE.`;
  }

  return `I'm your RawrXD IDE assistant. I can help with:
- 🐝 Swarm operations and deployment
- 🧠 Model management (GGUF/Ollama/Cloud)
- 📊 Benchmarking and performance
- ✅ Task management
- 🔧 Debugging and diagnostics

What would you like to explore?`;
}

// Create HTTP server
const server = http.createServer((req, res) => {
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

  // GET /models
  if (req.method === 'GET' && pathname === '/models') {
    const models = getLocalModels();
    res.writeHead(200);
    res.end(JSON.stringify({ models }));
    return;
  }

  // GET /health
  if (req.method === 'GET' && pathname === '/health') {
    res.writeHead(200);
    res.end(JSON.stringify({
      status: 'online',
      models: getLocalModels().length,
      uptime: process.uptime()
    }));
    return;
  }

  // POST /ask
  if (req.method === 'POST' && pathname === '/ask') {
    let body = '';
    req.on('data', chunk => body += chunk);
    req.on('end', () => {
      try {
        const data = JSON.parse(body);
        const answer = generateAnswer(data.question, data.model, data.language);
        res.writeHead(200);
        res.end(JSON.stringify({ answer }));
      } catch (e) {
        res.writeHead(400);
        res.end(JSON.stringify({ error: 'Invalid JSON' }));
      }
    });
    return;
  }

  // POST /exec (terminal commands)
  if (req.method === 'POST' && pathname === '/exec') {
    let body = '';
    req.on('data', chunk => body += chunk);
    req.on('end', () => {
      try {
        const data = JSON.parse(body);
        res.writeHead(200);
        res.end(JSON.stringify({
          output: `Executed: ${data.command}`,
          result: 'Command received (simulation mode)'
        }));
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

server.listen(PORT, () => {
  console.log(`
================================================
  RawrXD Node.js Backend Server
  Port: ${PORT}
  Endpoints: GET /models, POST /ask, GET /health
================================================
  
[+] Server listening on http://localhost:${PORT}
[+] Models directory: ${MODEL_DIR}
[+] Ready for frontend connections
`);
});

// Graceful shutdown
process.on('SIGINT', () => {
  console.log('\n[!] Shutting down...');
  server.close(() => process.exit(0));
});
