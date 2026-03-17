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

async function queryOllama(model: string, prompt: string): Promise<string> {
  return new Promise((resolve, reject) => {
    const url = new URL(`${OLLAMA_ENDPOINT}/api/generate`);
    const requestData = { model, prompt, stream: false };
    const bodyStr = JSON.stringify(requestData);

    const options = {
      hostname: url.hostname,
      port: parseInt(url.port) || 3000,
      path: '/api/generate',
      method: 'POST',
      headers: {
        'Content-Type': 'application/json',
        'Content-Length': Buffer.byteLength(bodyStr)
      }
    };

    console.log('[Ollama] POST', options.hostname + ':' + options.port + options.path);

    const req = http.request(options, (res) => {
      let data = '';
      res.on('data', chunk => data += chunk);
      res.on('end', () => {
        try {
          if (res.statusCode !== 200) {
            reject(new Error(`HTTP ${res.statusCode}`));
            return;
          }
          const json = JSON.parse(data);
          const response = json.response || '';
          console.log('[Ollama] Response:', response.length, 'chars');
          resolve(response);
        } catch (e: any) {
          reject(new Error('Parse error: ' + e.message));
        }
      });
    });

    req.on('error', e => {
      console.error('[Ollama] Error:', e.message);
      reject(new Error('Ollama connection failed: ' + e.message));
    });

    req.write(bodyStr);
    req.end();
  });
}

async function loadModels(): Promise<string[]> {
  return new Promise((resolve) => {
    const url = new URL(`${OLLAMA_ENDPOINT}/api/tags`);

    const options = {
      hostname: url.hostname,
      port: parseInt(url.port) || 3000,
      path: '/api/tags',
      method: 'GET'
    };

    console.log('[Ollama] GET', options.hostname + ':' + options.port + options.path);

    const req = http.request(options, (res) => {
      let data = '';
      res.on('data', chunk => data += chunk);
      res.on('end', () => {
        try {
          if (res.statusCode === 200) {
            const json = JSON.parse(data);
            const models = (json.models || []).map((m: any) => m.name);
            console.log('[Ollama] Models:', models);
            resolve(models.length > 0 ? models : ['mistral:7b']);
          } else {
            resolve(['mistral:7b']);
          }
        } catch {
          resolve(['mistral:7b']);
        }
      });
    });

    req.on('error', e => {
      console.warn('[Ollama] Connection error:', e.message);
      resolve(['mistral:7b']);
    });

    req.end();
  });
}

export function deactivate() { }
