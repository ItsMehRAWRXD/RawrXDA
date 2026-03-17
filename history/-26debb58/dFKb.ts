import * as vscode from 'vscode';
import * as http from 'http';
import * as https from 'https';
import * as path from 'path';

let bypassEnabled = true;
let cachedModels: string[] = [];
let interceptorConfig = {
  enabled: false,
  endpoint: 'http://localhost:3000',
  backend: 'ollama-chat' as 'ollama-generate' | 'ollama-chat' | 'openai-chat',
  model: '',
  agentMode: 'ask' as 'ask' | 'edit' | 'plan',
  ideAccess: false
};
let httpOriginal = http.request;
let httpsOriginal = https.request;
let interceptorActive = false;
let contextCache = { code: '', files: '', workspace: '' };

export function activate(context: vscode.ExtensionContext) {
  console.log('[BigDaddyG] ========================================');
  console.log('[BigDaddyG] ACTIVATION STARTING');
  console.log('[BigDaddyG] Extension Version: 1.0.0 (Cursor Parity)');
  console.log('[BigDaddyG] ========================================');

  initializeInterceptor();

  context.subscriptions.push(
    vscode.commands.registerCommand('bigdaddyg-copilot.openChat', () => {
      console.log('[BigDaddyG] Command: openChat triggered');
      openChatPanel('open');
    }),
    vscode.commands.registerCommand('bigdaddyg-copilot.beaconChat', () => {
      console.log('[BigDaddyG] Command: beaconChat triggered');
      openChatPanel('beacon');
    }),
    vscode.commands.registerCommand('bigdaddyg-copilot.toggleBypass', () => {
      bypassEnabled = !bypassEnabled;
      console.log(`[BigDaddyG] Command: toggleBypass - New state: ${bypassEnabled ? 'ENABLED' : 'DISABLED'}`);
      vscode.window.showInformationMessage(`✓ BigDaddyG Bypass ${bypassEnabled ? 'ENABLED' : 'DISABLED'}`);
    }),
    vscode.commands.registerCommand('bigdaddyg-copilot.startBypass', () => {
      bypassEnabled = true;
      console.log('[BigDaddyG] Command: startBypass - Bypass ENABLED');
      vscode.window.showInformationMessage('✓ BigDaddyG Bypass ENABLED');
    }),
    vscode.commands.registerCommand('bigdaddyg-copilot.stopBypass', () => {
      bypassEnabled = false;
      console.log('[BigDaddyG] Command: stopBypass - Bypass DISABLED');
      vscode.window.showInformationMessage('✗ BigDaddyG Bypass DISABLED');
    }),
    vscode.commands.registerCommand('bigdaddyg-copilot.insertCode', async (code: string) => {
      console.log('[BigDaddyG] Command: insertCode');
      const editor = vscode.window.activeTextEditor;
      if (!editor) {
        vscode.window.showWarningMessage('No active editor');
        return;
      }
      try {
        await editor.edit(editBuilder => {
          editBuilder.insert(editor.selection.active, code);
        });
        vscode.window.showInformationMessage('✓ Code inserted');
      } catch (e: any) {
        vscode.window.showErrorMessage('Insert error: ' + e.message);
      }
    }),
    vscode.commands.registerCommand('bigdaddyg-copilot.applyDiff', async (fileUri: string, newContent: string) => {
      console.log('[BigDaddyG] Command: applyDiff');
      try {
        const uri = vscode.Uri.parse(fileUri);
        await vscode.workspace.fs.writeFile(uri, Buffer.from(newContent, 'utf8'));
        const doc = await vscode.workspace.openTextDocument(uri);
        await vscode.window.showTextDocument(doc);
        vscode.window.showInformationMessage('✓ Changes applied');
      } catch (e: any) {
        vscode.window.showErrorMessage('Apply error: ' + e.message);
      }
    }),
    vscode.commands.registerCommand('bigdaddyg-copilot.enableInterceptor', async () => {
      const endpoint = await vscode.window.showInputBox({ prompt: 'Local endpoint URL (e.g., http://localhost:3000)', value: interceptorConfig.endpoint });
      if (!endpoint) return;
      const backends = ['ollama-generate', 'ollama-chat', 'openai-chat'];
      const backend = await vscode.window.showQuickPick(backends, { placeHolder: 'Select backend' });
      if (!backend) return;
      const model = await vscode.window.showInputBox({ prompt: 'Model name' });
      if (!model) return;
      const modes = ['ask', 'edit', 'plan'];
      const agentMode = await vscode.window.showQuickPick(modes, { placeHolder: 'Select mode' }) as any;
      if (!agentMode) return;
      const ideAccessStr = await vscode.window.showQuickPick(['Yes', 'No'], { placeHolder: 'IDE Access?' });
      const ideAccess = ideAccessStr === 'Yes';

      interceptorConfig = { enabled: true, endpoint, backend: backend as any, model, agentMode, ideAccess };
      rebuildInterceptor();
      await gatherWorkspaceContext();
      vscode.window.showInformationMessage(`✓ Interceptor ENABLED [${agentMode}]`);
    }),
    vscode.commands.registerCommand('bigdaddyg-copilot.disableInterceptor', () => {
      interceptorConfig.enabled = false;
      rebuildInterceptor();
      vscode.window.showInformationMessage('✗ Interceptor DISABLED');
    })
  );
}

// ===== WORKSPACE CONTEXT =====

async function gatherWorkspaceContext(): Promise<void> {
  try {
    const editor = vscode.window.activeTextEditor;
    if (editor) {
      contextCache.code = editor.document.getText().slice(0, 500);
    }
    const folders = vscode.workspace.workspaceFolders;
    if (folders) {
      contextCache.workspace = folders.map(f => f.name).join(', ');
      contextCache.files = folders.map(f => f.uri.fsPath).join(', ').slice(0, 200);
    }
  } catch (e: any) {
    console.warn('[BigDaddyG] Context gather failed:', e.message);
  }
}

// ===== INTERCEPTOR CORE =====

function initializeInterceptor() {
  if (interceptorActive) return;
  interceptorActive = true;
  rebuildInterceptor();
}

function rebuildInterceptor() {
  if (!interceptorConfig.enabled) {
    (http as any).request = httpOriginal;
    (https as any).request = httpsOriginal;
    return;
  }

  (http as any).request = ((url: any, options: any, callback?: any) => {
    if (detectChatRequest(url, options) && interceptorConfig.enabled && interceptorConfig.model) {
      return proxyToLocal(interceptorConfig.endpoint, interceptorConfig.backend, interceptorConfig.model, url, options, callback);
    }
    return httpOriginal(url, options, callback);
  }) as any;

  (https as any).request = ((url: any, options: any, callback?: any) => {
    if (detectChatRequest(url, options) && interceptorConfig.enabled && interceptorConfig.model) {
      return proxyToLocal(interceptorConfig.endpoint, interceptorConfig.backend, interceptorConfig.model, url, options, callback);
    }
    return httpsOriginal(url, options, callback);
  }) as any;
}

function detectChatRequest(url: any, options: any): boolean {
  const urlStr = typeof url === 'string' ? url : (url?.href || '');
  const methodStr = options?.method || '';
  const pathStr = options?.path || '';
  const fullStr = urlStr + pathStr;
  const isChatPath = /\/chat|\/message|\/completion|\/generate|cursor.*api|ai.*model/i.test(fullStr);
  return isChatPath && /^(POST|PUT)$/i.test(methodStr);
}

function proxyToLocal(endpoint: string, backend: string, model: string, url: any, options: any, callback?: any): http.ClientRequest {
  const wrappedCallback = (res: any) => {
    const origEmit = res.emit.bind(res);
    res.emit = ((event: string, ...args: any[]) => {
      if (event === 'data' && args[0]) {
        const text = args[0].toString();
        if (interceptorConfig.ideAccess && interceptorConfig.agentMode === 'edit') {
          handleEditDirective(text).catch(e => console.warn('[BigDaddyG] Edit error:', e.message));
        }
      }
      return origEmit(event, ...args);
    }) as any;
    if (callback) callback(res);
  };

  const req = http.request(endpoint, { method: 'POST', path: urlForBackend(backend), headers: { 'Content-Type': 'application/json' } }, wrappedCallback);

  const origWrite = req.write.bind(req);
  req.write = ((data: any, ...args: any[]) => {
    if (typeof data === 'string' || Buffer.isBuffer(data)) {
      try {
        const bodyObj = JSON.parse(data.toString());
        const transformed = transformRequestBody(bodyObj, backend, model);
        return origWrite(JSON.stringify(transformed), ...args);
      } catch (e: any) {
        return origWrite(data, ...args);
      }
    }
    return origWrite(data, ...args);
  }) as any;

  return req;
}

async function handleEditDirective(text: string): Promise<void> {
  const m = /AGENT_EDIT:\s*(\{[\s\S]*?\})/i.exec(text);
  if (!m) return;
  try {
    const payload = JSON.parse(m[1]);
    const file = String(payload.file || '').trim();
    if (!file) return;

    const root = vscode.workspace.workspaceFolders?.[0]?.uri;
    if (!root) return;

    const segments = file.split(/[\\\/]+/).filter(Boolean);
    const target = vscode.Uri.joinPath(root, ...segments);

    const rootFs = root.fsPath.replace(/\\/g, '/').toLowerCase();
    const tgtFs = target.fsPath.replace(/\\/g, '/').toLowerCase();
    if (!tgtFs.startsWith(rootFs)) return;

    let newContent: string | undefined;
    if (typeof payload.content === 'string') {
      newContent = payload.content;
    } else if (typeof payload.find === 'string' && typeof payload.replace === 'string') {
      const bytes = await vscode.workspace.fs.readFile(target);
      const cur = Buffer.from(bytes).toString('utf8');
      newContent = cur.split(payload.find).join(payload.replace);
    }

    if (newContent) {
      await vscode.workspace.fs.writeFile(target, Buffer.from(newContent, 'utf8'));
      const doc = await vscode.workspace.openTextDocument(target);
      await vscode.window.showTextDocument(doc, { preview: false });
      console.log('[BigDaddyG] Applied AGENT_EDIT:', file);
    }
  } catch (e: any) {
    console.warn('[BigDaddyG] Edit directive failed:', e.message);
  }
}

function urlForBackend(backend: string): string {
  if (backend === 'ollama-generate') return '/api/generate';
  if (backend === 'ollama-chat') return '/api/chat';
  if (backend === 'openai-chat') return '/v1/chat/completions';
  return '/api/chat';
}

function transformRequestBody(body: any, backend: string, model: string): any {
  const prompt = body.prompt || extractPromptFromMessages(body.messages);
  const preface = buildAgentPreface(interceptorConfig.agentMode);
  const fullPrompt = preface ? (preface + '\n\n' + prompt) : prompt;
  const finalPrompt = addContextToPrompt(fullPrompt);

  if (backend === 'ollama-generate') {
    return { model, prompt: finalPrompt, stream: false };
  } else if (backend === 'ollama-chat') {
    const messages = body.messages ? addContextToMessages(body.messages) : [{ role: 'user', content: finalPrompt }];
    return { model, messages, stream: false };
  } else if (backend === 'openai-chat') {
    const messages = body.messages ? addContextToMessages(body.messages) : [{ role: 'user', content: finalPrompt }];
    return { model, messages, stream: false };
  }
  return body;
}

function buildAgentPreface(mode: 'ask' | 'edit' | 'plan'): string {
  if (mode === 'edit') {
    return 'You are a coding agent. Keep explanations short.\nAfter your reply, output AGENT_EDIT: {"file":"path","content":"..."} on one line (no backticks).';
  }
  if (mode === 'plan') {
    return 'You are a planning agent. Provide a concise plan.\nAfter your plan, output AGENT_PLAN: {"steps":["..."]} on one line.';
  }
  return 'You are a helpful coding assistant. Answer succinctly.';
}

function addContextToPrompt(prompt: string): string {
  if (!interceptorConfig.ideAccess) return prompt;
  return prompt + `\n\n[Context]\nFiles: ${contextCache.files}\nCode: ${contextCache.code}`;
}

function addContextToMessages(messages: any[]): any[] {
  if (!interceptorConfig.ideAccess || messages.length === 0) return messages;
  const lastMsg = messages[messages.length - 1];
  if (lastMsg?.role === 'user') {
    return [...messages.slice(0, -1), { ...lastMsg, content: lastMsg.content + `\n\n[Context]\nFiles: ${contextCache.files}\nCode: ${contextCache.code}` }];
  }
  return messages;
}

function extractPromptFromMessages(messages: any[]): string {
  if (!Array.isArray(messages)) return '';
  const lastMsg = messages[messages.length - 1];
  return (lastMsg?.content || lastMsg?.text || '').toString();
}

// ===== CHAT PANEL UI =====

function openChatPanel(mode: 'open' | 'beacon') {
  const title = mode === 'beacon' ? 'BigDaddyG: Beacon Chat' : 'BigDaddyG: AI Chat';
  const panel = vscode.window.createWebviewPanel('bigdaddyg', title, vscode.ViewColumn.Beside, { enableScripts: true });
  panel.webview.html = getHtml();

  panel.webview.onDidReceiveMessage(async (msg) => {
    if (msg.type === 'send') {
      const endpoint: string = (msg.endpoint || '').trim();
      const backend: 'ollama-generate' | 'ollama-chat' | 'openai-chat' = msg.backend;
      const model = msg.useCustom ? (msg.customModel || '').trim() : (msg.model || '').trim();
      const prompt = msg.prompt as string;

      if (!endpoint || !backend || !model) {
        panel.webview.postMessage({ type: 'error', text: 'Endpoint, backend, and model required' });
        return;
      }

      try {
        panel.webview.postMessage({ type: 'responseStart' });
        let collected = '';

        await streamQuerySingle(endpoint, backend, model, prompt, (chunk) => {
          collected += chunk;
          panel.webview.postMessage({ type: 'responseChunk', text: chunk });
        });

        if (msg.ideAccess && msg.mode === 'edit') {
          try {
            const applied = await tryApplyAgentEdit(collected);
            if (applied) {
              vscode.window.showInformationMessage('✓ Applied AGENT_EDIT');
            }
          } catch (e: any) {
            vscode.window.showWarningMessage('Edit failed: ' + e.message);
          }
        }
        panel.webview.postMessage({ type: 'responseEnd' });
      } catch (e: any) {
        panel.webview.postMessage({ type: 'error', text: e.message });
      }
    } else if (msg.type === 'refreshModels') {
      try {
        const endpoint: string = (msg.endpoint || '').trim();
        const backend: 'ollama-generate' | 'ollama-chat' | 'openai-chat' = msg.backend;
        if (!endpoint || !backend) throw new Error('Endpoint and backend required');
        const models = await loadModels(endpoint, backend);
        panel.webview.postMessage({ type: 'models', models });
      } catch (e: any) {
        panel.webview.postMessage({ type: 'error', text: 'Load models error: ' + e.message });
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
body { font-family: system-ui; color: var(--vscode-editor-foreground); background: var(--vscode-editor-background); padding: 12px; margin: 0; }
.row { display: flex; gap: 8px; margin-bottom: 8px; }
input, select, textarea { font: inherit; background: var(--vscode-input-background); color: var(--vscode-input-foreground); border: 1px solid var(--vscode-input-border); padding: 6px; border-radius: 3px; flex: 1; }
button { background: var(--vscode-button-background); color: var(--vscode-button-foreground); cursor: pointer; border: none; padding: 6px 12px; border-radius: 3px; }
button:hover { background: var(--vscode-button-hoverBackground); }
textarea { height: 120px; resize: vertical; font-family: monospace; }
#output { background: var(--vscode-editor-background); border: 1px solid var(--vscode-editorWidget-border); padding: 8px; border-radius: 3px; min-height: 150px; margin-top: 8px; white-space: pre-wrap; word-wrap: break-word; overflow-y: auto; max-height: 400px; font-family: monospace; font-size: 12px; }
.status { font-size: 11px; color: var(--vscode-descriptionForeground); margin-top: 4px; }
</style>
</head>
<body>
<h3>BigDaddyG Chat</h3>
<div class="row">
  <input id="endpoint" placeholder="Endpoint URL (e.g. http://localhost:3000)" style="flex:2" />
  <select id="backend" style="flex:1">
    <option value="">Backend</option>
    <option value="ollama-generate">Ollama Generate</option>
    <option value="ollama-chat">Ollama Chat</option>
    <option value="openai-chat">OpenAI Chat</option>
  </select>
  <button onclick="refreshModels()">🔄</button>
</div>
<div class="row">
  <select id="model"><option>Models</option></select>
  <input id="customModel" placeholder="Custom model" />
  <label style="display:flex;align-items:center;gap:4px;">
    <input type="checkbox" id="useCustom" /> Custom
  </label>
  <select id="agentMode">
    <option value="ask">Ask</option>
    <option value="edit">Edit</option>
    <option value="plan">Plan</option>
  </select>
  <label style="display:flex;align-items:center;gap:4px;">
    <input type="checkbox" id="ideAccess" /> IDE
  </label>
</div>
<textarea id="prompt" placeholder="Your message..."></textarea>
<button onclick="send()" style="width: 100%; padding: 10px; margin-top: 8px;">📤 Send</button>
<div class="status" id="status">Ready</div>
<div id="output">Waiting for input...</div>

<script nonce="${nonce}">
const vscode = acquireVsCodeApi();
let isWaiting = false;

function send() {
  if (isWaiting) return;
  const endpoint = document.getElementById('endpoint').value.trim();
  const backend = document.getElementById('backend').value;
  const model = document.getElementById('useCustom').checked ? document.getElementById('customModel').value.trim() : document.getElementById('model').value;
  const prompt = document.getElementById('prompt').value.trim();
  
  if (!endpoint || !backend || !model || !prompt) {
    document.getElementById('status').textContent = '❌ Fill all fields';
    return;
  }
  
  isWaiting = true;
  document.getElementById('status').textContent = '⏳ Sending...';
  document.getElementById('output').textContent = '';
  
  vscode.postMessage({
    type: 'send',
    endpoint, backend, model, prompt,
    useCustom: document.getElementById('useCustom').checked,
    customModel: document.getElementById('customModel').value,
    mode: document.getElementById('agentMode').value,
    ideAccess: document.getElementById('ideAccess').checked
  });
}

function refreshModels() {
  if (isWaiting) return;
  const endpoint = document.getElementById('endpoint').value.trim();
  const backend = document.getElementById('backend').value;
  if (!endpoint || !backend) {
    document.getElementById('status').textContent = '❌ Need endpoint & backend';
    return;
  }
  isWaiting = true;
  document.getElementById('status').textContent = '🔄 Loading...';
  vscode.postMessage({ type: 'refreshModels', endpoint, backend });
}

window.addEventListener('message', (e) => {
  const msg = e.data;
  if (msg.type === 'models' && msg.models) {
    const html = msg.models.map(m => '<option>' + m + '</option>').join('');
    document.getElementById('model').innerHTML = html || '<option>No models</option>';
    document.getElementById('status').textContent = '✓ ' + msg.models.length + ' model(s)';
    isWaiting = false;
  } else if (msg.type === 'error') {
    document.getElementById('output').textContent = '❌ ' + msg.text;
    document.getElementById('status').textContent = '❌ Error';
    isWaiting = false;
  } else if (msg.type === 'responseStart') {
    document.getElementById('output').textContent = '';
    document.getElementById('status').textContent = '↘ Streaming...';
  } else if (msg.type === 'responseChunk') {
    const out = document.getElementById('output');
    out.textContent += msg.text;
    out.scrollTop = out.scrollHeight;
  } else if (msg.type === 'responseEnd') {
    document.getElementById('status').textContent = '✓ Done';
    isWaiting = false;
  }
});
</script>
</body>
</html>`;
}

// ===== STREAMING & HTTP =====

type LineParser = (line: string) => { text?: string, done?: boolean } | undefined;

async function streamQuerySingle(endpoint: string, backend: 'ollama-generate' | 'ollama-chat' | 'openai-chat', model: string, prompt: string, onChunk: (t: string) => void): Promise<void> {
  if (backend === 'ollama-generate') {
    await streamEndpoint(endpoint, '/api/generate', { model, prompt, stream: true }, onChunk, parseOllamaGenerateLine);
  } else if (backend === 'ollama-chat') {
    await streamEndpoint(endpoint, '/api/chat', { model, messages: [{ role: 'user', content: prompt }], stream: true }, onChunk, parseOllamaChatLine);
  } else if (backend === 'openai-chat') {
    await streamEndpoint(endpoint, '/v1/chat/completions', { model, messages: [{ role: 'user', content: prompt }], stream: true }, onChunk, parseOpenAIStreamLine);
  } else {
    throw new Error('Unsupported backend');
  }
}

function parseOllamaGenerateLine(line: string) {
  if (!line) return;
  const s = line.startsWith('data:') ? line.replace(/^data:\s*/, '') : line;
  if (s === '[DONE]') return { done: true };
  try {
    const obj = JSON.parse(s);
    if (obj.done) return { done: true };
    if (typeof obj.response === 'string') return { text: obj.response };
  } catch { }
}

function parseOllamaChatLine(line: string) {
  if (!line) return;
  const s = line.startsWith('data:') ? line.replace(/^data:\s*/, '') : line;
  if (s === '[DONE]') return { done: true };
  try {
    const obj = JSON.parse(s);
    if (obj.done) return { done: true };
    if (typeof obj?.message?.content === 'string') return { text: obj.message.content };
  } catch { }
}

function parseOpenAIStreamLine(line: string) {
  if (!line) return;
  const s = line.startsWith('data:') ? line.replace(/^data:\s*/, '') : line;
  if (s === '[DONE]') return { done: true };
  try {
    const obj = JSON.parse(s);
    const delta = obj?.choices?.[0]?.delta?.content;
    if (typeof delta === 'string') return { text: delta };
  } catch { }
}

function streamEndpoint(base: string, pathname: string, payload: any, onChunk: (t: string) => void, parseLine: LineParser): Promise<void> {
  return new Promise((resolve, reject) => {
    const url = new URL(`${base}${pathname}`);
    const bodyStr = JSON.stringify(payload);
    const options: http.RequestOptions = {
      hostname: url.hostname,
      port: parseInt(url.port) || 3000,
      path: url.pathname + (url.search || ''),
      method: 'POST',
      headers: {
        'Content-Type': 'application/json',
        'Content-Length': Buffer.byteLength(bodyStr)
      }
    };

    const req = http.request(options, (res) => {
      if ((res.statusCode || 0) >= 400) {
        reject(new Error(`HTTP ${res.statusCode}`));
        return;
      }
      let buf = '';
      res.setEncoding('utf8');
      res.on('data', (chunk: string) => {
        buf += chunk;
        let idx: number;
        while ((idx = buf.indexOf('\n')) >= 0) {
          const line = buf.slice(0, idx).trim();
          buf = buf.slice(idx + 1);
          const parsed = parseLine(line);
          if (!parsed) continue;
          if (parsed.text) onChunk(parsed.text);
          if (parsed.done) {
            resolve();
            try { req.destroy(); } catch { }
            return;
          }
        }
      });
      res.on('end', () => {
        const line = buf.trim();
        if (line) {
          const parsed = parseLine(line);
          if (parsed?.text) onChunk(parsed.text);
        }
        resolve();
      });
    });

    req.on('error', (e) => reject(new Error(e.message)));
    req.write(bodyStr);
    req.end();
  });
}

async function loadModels(base: string, backend: 'ollama-generate' | 'ollama-chat' | 'openai-chat'): Promise<string[]> {
  if (backend === 'ollama-generate' || backend === 'ollama-chat') {
    const tags = await httpJsonBase(base, 'GET', '/api/tags');
    if (tags.status !== 200) return [];
    const models = (tags.json?.models || []).map((m: any) => m.name).filter(Boolean);
    return models;
  }
  if (backend === 'openai-chat') {
    const m = await httpJsonBase(base, 'GET', '/v1/models');
    if (m.status !== 200) return [];
    return (m.json?.data || []).map((x: any) => x.id).filter(Boolean);
  }
  return [];
}

async function tryApplyAgentEdit(text: string): Promise<boolean> {
  const m = /AGENT_EDIT:\s*(\{[\s\S]*?\})/i.exec(text);
  if (!m) return false;
  const payload = JSON.parse(m[1]);
  const file = String(payload.file || '').trim();
  if (!file) return false;

  const root = vscode.workspace.workspaceFolders?.[0]?.uri;
  if (!root) return false;

  const target = vscode.Uri.joinPath(root, ...file.split(/[\\\/]+/).filter(Boolean));
  const rootFs = root.fsPath.replace(/\\/g, '/').toLowerCase();
  const tgtFs = target.fsPath.replace(/\\/g, '/').toLowerCase();
  if (!tgtFs.startsWith(rootFs)) return false;

  let newContent: string | undefined;
  if (typeof payload.content === 'string') {
    newContent = payload.content;
  } else if (typeof payload.find === 'string' && typeof payload.replace === 'string') {
    const bytes = await vscode.workspace.fs.readFile(target);
    const cur = Buffer.from(bytes).toString('utf8');
    newContent = cur.split(payload.find).join(payload.replace);
  }

  if (newContent) {
    await vscode.workspace.fs.writeFile(target, Buffer.from(newContent, 'utf8'));
    await vscode.window.showTextDocument(await vscode.workspace.openTextDocument(target));
    return true;
  }
  return false;
}

function httpJsonBase(base: string, method: 'GET' | 'POST', path: string, payload?: any): Promise<{ status: number, json: any }> {
  return new Promise((resolve, reject) => {
    const url = new URL(`${base}${path}`);
    const bodyStr = payload ? JSON.stringify(payload) : undefined;
    const options: http.RequestOptions = {
      hostname: url.hostname,
      port: parseInt(url.port) || 3000,
      path: url.pathname + (url.search || ''),
      method,
      headers: { 'Content-Type': 'application/json', ...(bodyStr ? { 'Content-Length': Buffer.byteLength(bodyStr) } : {}) }
    };

    const req = http.request(options, (res) => {
      let data = '';
      res.on('data', chunk => data += chunk);
      res.on('end', () => {
        try {
          const parsed = JSON.parse(data);
          resolve({ status: res.statusCode || 0, json: parsed });
        } catch {
          resolve({ status: res.statusCode || 0, json: undefined });
        }
      });
    });

    req.on('error', e => reject(e));
    req.setTimeout(10000, () => req.destroy(new Error('timeout')));
    if (bodyStr) req.write(bodyStr);
    req.end();
  });
}

export function deactivate() { }
