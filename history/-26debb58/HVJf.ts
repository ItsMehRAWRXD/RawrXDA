import * as vscode from 'vscode';
import * as http from 'http';

let bypassEnabled = true;
let cachedModels: string[] = [];
const OLLAMA_ENDPOINT = 'http://localhost:3000';

export function activate(context: vscode.ExtensionContext) {
  console.log('[BigDaddyG] Activating...');
  context.subscriptions.push(
    vscode.commands.registerCommand('bigdaddyg.openChat', () => openChatPanel('open')),
    vscode.commands.registerCommand('bigdaddyg.beaconChat', () => openChatPanel('beacon')),
    vscode.commands.registerCommand('bigdaddyg.toggleBypass', () => {
      bypassEnabled = !bypassEnabled;
      vscode.window.showInformationMessage(`✓ BigDaddyG Bypass ${bypassEnabled ? 'ENABLED' : 'DISABLED'}`);
    }),
    vscode.commands.registerCommand('bigdaddyg.startBypass', () => {
      bypassEnabled = true;
      vscode.window.showInformationMessage('✓ BigDaddyG Bypass ENABLED');
    }),
    vscode.commands.registerCommand('bigdaddyg.stopBypass', () => {
      bypassEnabled = false;
      vscode.window.showInformationMessage('✗ BigDaddyG Bypass DISABLED');
    })
  );
}

function openChatPanel(mode: 'open' | 'beacon') {
  const title = mode === 'beacon' ? 'BigDaddyG: Beacon Chat' : 'BigDaddyG: AI Chat';
  const panel = vscode.window.createWebviewPanel('bigdaddyg', title, vscode.ViewColumn.Beside, { enableScripts: true });
  panel.webview.html = getHtml();

  loadModels().then(models => {
    cachedModels = models;
    console.log('[BigDaddyG] Loaded models:', models);
    panel.webview.postMessage({ type: 'models', models });
  }).catch(err => {
    console.error('[BigDaddyG] Error loading models:', err);
    panel.webview.postMessage({ type: 'error', text: 'Failed to load models: ' + err.message });
  });

  panel.webview.onDidReceiveMessage(async (msg) => {
    console.log('[BigDaddyG] Received message:', msg.type);

    if (msg.type === 'send') {
      const model = msg.model || cachedModels[0] || 'mistral:7b';
      const prompt = msg.prompt;

      console.log('[BigDaddyG] Sending to Ollama:', { model, promptLen: prompt.length });

      try {
        const text = await queryOllama(model, prompt);
        console.log('[BigDaddyG] Got response, length:', text.length);
        panel.webview.postMessage({ type: 'response', text });
      } catch (e: any) {
        console.error('[BigDaddyG] Error:', e.message);
        panel.webview.postMessage({ type: 'error', text: e.message });
      }
    } else if (msg.type === 'refreshModels') {
      try {
        const models = await loadModels();
        cachedModels = models;
        console.log('[BigDaddyG] Refreshed models:', models);
        panel.webview.postMessage({ type: 'models', models });
      } catch (e: any) {
        console.error('[BigDaddyG] Error refreshing models:', e);
        panel.webview.postMessage({ type: 'error', text: 'Failed to load models: ' + e.message });
      }
    }
  });
}

function getHtml(): string {
  const nonce = Date.now().toString();
  return `<!DOCTYPE html>
<html>
<head>
<meta charset="UTF-8" />
<meta http-equiv="Content-Security-Policy" content="default-src 'none'; script-src 'nonce-${nonce}'; style-src 'unsafe-inline';" />
<title>BigDaddyG Chat</title>
<style>
body { 
  font-family: system-ui; 
  color: var(--vscode-editor-foreground); 
  background: var(--vscode-editor-background); 
  padding: 12px;
  margin: 0;
}
.row { 
  display: flex; 
  gap: 8px; 
  margin-bottom: 8px; 
}
select, textarea { 
  font: inherit; 
  background: var(--vscode-input-background); 
  color: var(--vscode-input-foreground); 
  border: 1px solid var(--vscode-input-border); 
  padding: 6px; 
  border-radius: 3px; 
  flex: 1;
}
button { 
  background: var(--vscode-button-background); 
  color: var(--vscode-button-foreground); 
  cursor: pointer; 
  border: none;
  padding: 6px 12px;
  border-radius: 3px;
}
button:hover { 
  background: var(--vscode-button-hoverBackground); 
}
textarea { 
  height: 120px;
  resize: vertical;
  font-family: monospace;
}
#output { 
  background: var(--vscode-editor-background); 
  border: 1px solid var(--vscode-editorWidget-border); 
  padding: 8px; 
  border-radius: 3px; 
  min-height: 150px; 
  margin-top: 8px; 
  white-space: pre-wrap;
  word-wrap: break-word;
  overflow-y: auto;
  max-height: 400px;
  font-family: monospace;
  font-size: 12px;
}
.status { 
  font-size: 11px; 
  color: var(--vscode-descriptionForeground);
  margin-top: 4px;
}
</style>
</head>
<body>
<h3>BigDaddyG Chat</h3>

<div class="row">
  <select id="model"><option>Loading models...</option></select>
  <button onclick="refreshModels()" title="Refresh model list">🔄</button>
</div>

<textarea id="prompt" placeholder="Ask BigDaddyG something..."></textarea>

<button onclick="send()" style="width: 100%; padding: 10px; margin-top: 8px;">📤 Send Message</button>

<div class="status" id="status">Ready</div>
<div id="output">Waiting for input...</div>

<script nonce="${nonce}">
const vscode = acquireVsCodeApi();
let isWaiting = false;

function updateStatus(msg) {
  document.getElementById('status').textContent = msg;
  console.log('[UI] Status:', msg);
}

function send() {
  if (isWaiting) {
    updateStatus('⏳ Already sending...');
    return;
  }
  
  const model = document.getElementById('model').value;
  const prompt = document.getElementById('prompt').value.trim();
  
  if (!prompt) {
    updateStatus('❌ Please enter a message');
    return;
  }
  
  if (!model || model === 'Loading models...') {
    updateStatus('❌ No model selected');
    return;
  }
  
  isWaiting = true;
  updateStatus('⏳ Sending to ' + model + '...');
  document.getElementById('output').textContent = '⏳ Waiting for Ollama response...';
  
  console.log('[UI] Sending:', { model, promptLen: prompt.length });
  vscode.postMessage({ type: 'send', model, prompt });
}

function refreshModels() {
  if (isWaiting) return;
  isWaiting = true;
  updateStatus('🔄 Loading models...');
  document.getElementById('output').textContent = '⏳ Querying Ollama...';
  console.log('[UI] Refreshing models');
  vscode.postMessage({ type: 'refreshModels' });
}

window.addEventListener('message', (e) => {
  const msg = e.data;
  console.log('[UI] Message from ext:', msg.type, msg);
  
  if (msg.type === 'models' && msg.models) {
    const html = msg.models.map(m => '<option>' + m + '</option>').join('');
    document.getElementById('model').innerHTML = html || '<option>No models</option>';
    updateStatus('✓ ' + msg.models.length + ' model(s) available');
    isWaiting = false;
    document.getElementById('output').textContent = '✓ Ready to chat!';
    
  } else if (msg.type === 'response') {
    document.getElementById('output').textContent = msg.text;
    updateStatus('✓ Response received');
    isWaiting = false;
    
  } else if (msg.type === 'error') {
    document.getElementById('output').textContent = '❌ ERROR:\\n' + msg.text;
    updateStatus('❌ ' + msg.text);
    isWaiting = false;
  }
});

setTimeout(() => refreshModels(), 300);
</script>
</body>
</html>`;
}

// Generic JSON HTTP helper with simple timeout and SSE last-line fallback parsing
function httpJson(method: 'GET' | 'POST', path: string, payload?: any, timeoutMs = 15000): Promise<{ status: number, json: any, raw: string }> {
  return new Promise((resolve, reject) => {
    const url = new URL(`${OLLAMA_ENDPOINT}${path}`);
    const bodyStr = payload ? JSON.stringify(payload) : undefined;

    const options: http.RequestOptions = {
      hostname: url.hostname,
      port: parseInt(url.port) || 3000,
      path: url.pathname + (url.search || ''),
      method,
      headers: {
        'Content-Type': 'application/json',
        ...(bodyStr ? { 'Content-Length': Buffer.byteLength(bodyStr) } : {})
      }
    };

    console.log('[Ollama] ' + method, options.hostname + ':' + options.port + options.path);

    const req = http.request(options, (res) => {
      let data = '';
      res.on('data', chunk => data += chunk);
      res.on('end', () => {
        const status = res.statusCode || 0;
        try {
          const parsed = JSON.parse(data);
          resolve({ status, json: parsed, raw: data });
        } catch {
          // Some proxies stream SSE; try last JSON line if prefixed by "data: "
          const lines = data.split(/\r?\n/).map(l => l.trim()).filter(Boolean);
          const last = lines.reverse().find(l => l.startsWith('data:'));
          if (last) {
            try {
              const s = last.replace(/^data:\s*/, '');
              const parsed = JSON.parse(s);
              resolve({ status, json: parsed, raw: data });
              return;
            } catch { }
          }
          resolve({ status, json: undefined, raw: data });
        }
      });
    });

    req.on('error', e => {
      reject(new Error('HTTP error: ' + e.message));
    });

    req.setTimeout(timeoutMs, () => {
      try { req.destroy(new Error('Request timeout')); } catch { }
    });

    if (bodyStr) req.write(bodyStr);
    req.end();
  });
}

function basicExtract(obj: any): string | undefined {
  if (!obj) return undefined;
  if (typeof obj === 'string') return obj;
  if (typeof obj.response === 'string' && obj.response.trim()) return obj.response;
  if (obj.message && typeof obj.message.content === 'string' && obj.message.content.trim()) return obj.message.content;
  if (obj.message && Array.isArray(obj.message.content)) {
    const joined = obj.message.content.map((p: any) => (typeof p === 'string' ? p : (p?.text || p?.content || ''))).join('');
    if (joined.trim()) return joined;
  }
  if (Array.isArray(obj.choices) && obj.choices.length > 0) {
    const c = obj.choices[0];
    if (typeof c?.message?.content === 'string' && c.message.content.trim()) return c.message.content;
    if (typeof c?.delta?.content === 'string' && c.delta.content.trim()) return c.delta.content;
    if (typeof c?.text === 'string' && c.text.trim()) return c.text;
  }
  if (typeof obj.content === 'string' && obj.content.trim()) return obj.content;
  return undefined;
}

function extractFromRaw(raw?: string): string | undefined {
  if (!raw || !raw.trim()) return undefined;
  const lines = raw.split(/\r?\n/).map(l => l.trim()).filter(Boolean);
  let acc = '';
  let lastObj: any = undefined;
  for (const line of lines) {
    const s = line.startsWith('data:') ? line.replace(/^data:\s*/, '') : line;
    if (!(s.startsWith('{') && s.endsWith('}'))) continue;
    try {
      const o = JSON.parse(s);
      lastObj = o;
      if (typeof o.response === 'string') {
        acc += o.response;
        continue;
      }
      if (o.message && typeof o.message.content === 'string') {
        acc += o.message.content;
        continue;
      }
      if (Array.isArray(o.choices) && o.choices.length > 0) {
        const piece = o.choices[0]?.delta?.content || o.choices[0]?.message?.content || o.choices[0]?.text;
        if (typeof piece === 'string') acc += piece;
      }
    } catch {
      // ignore line parse
    }
  }
  if (acc.trim()) return acc;
  if (lastObj) {
    const t = basicExtract(lastObj);
    if (t && t.trim()) return t;
  }
  // Regex fallback for "response":"..."
  let out = '';
  const re = /\"response\"\s*:\s*\"((?:\\.|[^\"\\])*)\"/g;
  let m: RegExpExecArray | null;
  while ((m = re.exec(raw)) !== null) {
    try {
      out += m[1].replace(/\\n/g, '\n').replace(/\\\"/g, '"').replace(/\\/g, '\\');
    } catch {}
  }
  if (out.trim()) return out;
  return undefined;
}

function extractText(obj: any, raw?: string): string | undefined {
  const t = basicExtract(obj);
  if (t && t.trim()) return t;
  return extractFromRaw(raw);
}

async function queryOllama(model: string, prompt: string): Promise<string> {
  // 1) Ollama generate
  try {
    const gen = await httpJson('POST', '/api/generate', { model, prompt, stream: false });
    if (gen.status === 200) {
      const text = extractText(gen.json, gen.raw);
      if (text && text.trim()) {
        console.log('[Ollama] /api/generate ok, len:', text.length);
        return text;
      }
      console.warn('[Ollama] /api/generate returned empty/unknown schema');
    } else {
      console.warn('[Ollama] /api/generate HTTP', gen.status);
    }
  } catch (e: any) {
    console.warn('[Ollama] /api/generate failed:', e.message);
  }

  // 2) Ollama chat
  try {
    const chat = await httpJson('POST', '/api/chat', {
      model,
      messages: [{ role: 'user', content: prompt }],
      stream: false
    });
    if (chat.status === 200) {
      const text = extractText(chat.json, chat.raw);
      if (text && text.trim()) {
        console.log('[Ollama] /api/chat ok, len:', text.length);
        return text;
      }
      console.warn('[Ollama] /api/chat returned empty/unknown schema');
    } else {
      console.warn('[Ollama] /api/chat HTTP', chat.status);
    }
  } catch (e: any) {
    console.warn('[Ollama] /api/chat failed:', e.message);
  }

  // 3) OpenAI-compatible chat completions
  try {
    const openai = await httpJson('POST', '/v1/chat/completions', {
      model,
      messages: [{ role: 'user', content: prompt }],
      stream: false
    });
    if (openai.status === 200) {
      const text = extractText(openai.json, openai.raw);
      if (text && text.trim()) {
        console.log('[Ollama] /v1/chat/completions ok, len:', text.length);
        return text;
      }
      console.warn('[Ollama] /v1/chat/completions returned empty/unknown schema');
    } else {
      console.warn('[Ollama] /v1/chat/completions HTTP', openai.status);
    }
  } catch (e: any) {
    console.warn('[Ollama] /v1/chat/completions failed:', e.message);
  }

  throw new Error('No response from local AI (tried 3 endpoints)');
}

async function loadModels(): Promise<string[]> {
  // Primary: Ollama /api/tags
  try {
    const tags = await httpJson('GET', '/api/tags');
    if (tags.status === 200 && tags.json) {
      const models = (tags.json.models || []).map((m: any) => m.name).filter(Boolean);
      if (models.length) {
        console.log('[Ollama] Models (/api/tags):', models);
        return models;
      }
    }
  } catch (e: any) {
    console.warn('[Ollama] /api/tags failed:', e.message);
  }

  // Fallback: OpenAI-compatible /v1/models
  try {
    const m = await httpJson('GET', '/v1/models');
    if (m.status === 200 && m.json) {
      const models = Array.isArray(m.json.data) ? m.json.data.map((x: any) => x.id).filter(Boolean) : [];
      if (models.length) {
        console.log('[Ollama] Models (/v1/models):', models);
        return models;
      }
    }
  } catch (e: any) {
    console.warn('[Ollama] /v1/models failed:', e.message);
  }

  console.warn('[Ollama] Falling back to default model list');
  return ['mistral:7b'];
}

export function deactivate() { }
