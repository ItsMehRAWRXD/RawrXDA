import * as vscode from 'vscode';

let bypassEnabled = true;
let cachedModels: string[] = [];
const preferredRawrTags = ['rawr-1024', 'rawr_1024', 'rawr 1024'];
// Ollama endpoint - running on localhost:3000
const OLLAMA_ENDPOINT = 'http://localhost:3000';

export function activate(context: vscode.ExtensionContext) {
  context.subscriptions.push(
    vscode.commands.registerCommand('bigdaddyg.openChat', () => openChatPanel('open')),
    vscode.commands.registerCommand('bigdaddyg.beaconChat', () => openChatPanel('beacon')),
    vscode.commands.registerCommand('bigdaddyg.toggleBypass', () => {
      bypassEnabled = !bypassEnabled;
      vscode.window.showInformationMessage(`BigDaddyG bypass ${bypassEnabled ? 'enabled' : 'disabled'}.`);
    }),
    vscode.commands.registerCommand('bigdaddyg.startBypass', () => {
      bypassEnabled = true;
      vscode.window.showInformationMessage('BigDaddyG bypass enabled.');
    }),
    vscode.commands.registerCommand('bigdaddyg.stopBypass', () => {
      bypassEnabled = false;
      vscode.window.showInformationMessage('BigDaddyG bypass disabled.');
    })
  );
}

function openChatPanel(mode: 'open' | 'beacon') {
  const panel = vscode.window.createWebviewPanel(
    'bigdaddygChat',
    mode === 'beacon' ? 'BigDaddyG Beacon Chat (Local)' : 'BigDaddyG Chat',
    vscode.ViewColumn.Beside,
    { enableScripts: true, retainContextWhenHidden: true }
  );

  panel.webview.html = getHtml(panel);

  // Populate models dynamically from local Ollama
  listModels().then(models => {
    cachedModels = models;
    panel.webview.postMessage({ type: 'models', models });
  }).catch(() => {
    // ignore errors; user can still type custom model
  });

  panel.webview.onDidReceiveMessage(async (msg) => {
    if (msg.type === 'loadModel') {
      const ok = await ensureModel(msg.model);
      panel.webview.postMessage({ type: 'modelLoaded', ok });
    } else if (msg.type === 'send') {
      const model = msg.model;
      const mode = msg.mode as 'Agent' | 'Plan' | 'Ask' | 'Edit';
      const prompt = applyModeToPrompt(mode, msg.prompt, msg.params?.preset);
      try {
        const text = await ollamaGenerate(model, prompt, msg.params);
        panel.webview.postMessage({ type: 'response', text });
      } catch (err: any) {
        panel.webview.postMessage({ type: 'response', text: `Error: ${err?.message || err}` });
      }
    } else if (msg.type === 'refreshModels') {
      const models = await listModels().catch(() => [] as string[]);
      cachedModels = models;
      panel.webview.postMessage({ type: 'models', models });
    }
  });
}

function getHtml(panel: vscode.WebviewPanel): string {
  const nonce = String(Date.now());
  const options = `<option value="Auto">Auto</option>`; // dynamic models will be populated at runtime
  return `<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8" />
<meta http-equiv="Content-Security-Policy" content="default-src 'none'; img-src ${panel.webview.cspSource} https:; script-src 'nonce-${nonce}'; style-src ${panel.webview.cspSource} 'unsafe-inline';" />
<meta name="viewport" content="width=device-width, initial-scale=1.0" />
<title>BigDaddyG Chat</title>
<style>
  body { font-family: var(--vscode-font-family); color: var(--vscode-editor-foreground); background: var(--vscode-editor-background); padding: 10px; }
  .row { display: flex; gap: 8px; align-items: center; }
  select, input[type=range], button, textarea { width: 100%; }
  textarea { height: 140px; }
  #out { white-space: pre-wrap; border: 1px solid var(--vscode-editorWidget-border); padding: 8px; min-height: 160px; }
  .small { width: auto; }
</style>
</head>
<body>
  <div class="row">
    <label>Model</label>
    <select id="model">${options}</select>
    <input id="customModel" type="text" placeholder="or type any model tag (e.g., codellama:7b)" />
    <button id="refresh" class="small">Refresh</button>
    <button id="load" class="small">Load</button>
  </div>
  <div class="row">
    <label class="small">Mode</label>
    <select id="mode">
      <option>Agent</option>
      <option>Plan</option>
      <option>Ask</option>
      <option>Edit</option>
    </select>
  </div>
  <div class="row">
    <label class="small">Temp</label><input id="temp" type="range" min="0" max="2" step="0.1" value="0.7" />
    <label class="small">Max Tokens</label><input id="maxtokens" type="range" min="100" max="8192" step="100" value="2048" />
    <label class="small">Top P</label><input id="topp" type="range" min="0" max="1" step="0.05" value="0.9" />
  </div>
  <div class="row">
    <select id="preset">
      <option>Assembly Expert</option>
      <option>General Coding</option>
      <option>Debug Assistant</option>
      <option>Code Optimizer</option>
      <option>Security Analyst</option>
    </select>
    <label><input id="deep" type="checkbox" /> Deep thinking</label>
  </div>
  <textarea id="prompt" placeholder="Type your request..."></textarea>
  <div class="row">
    <button id="send">Send</button>
  </div>
  <div id="out"></div>
<script nonce="${nonce}">
  const vscode = acquireVsCodeApi();
  const modelSel = document.getElementById('model');
  const modeSel = document.getElementById('mode');
  const promptEl = document.getElementById('prompt');
  const out = document.getElementById('out');
  const customModelEl = document.getElementById('customModel');
  const refreshBtn = document.getElementById('refresh');
  document.getElementById('send').addEventListener('click', () => {
    const typed = (customModelEl.value || '').trim();
    const selected = modelSel.value;
    const model = typed ? typed : selected;
    const mode = modeSel.value;
    const params = {
      temperature: Number(document.getElementById('temp').value),
      top_p: Number(document.getElementById('topp').value),
      max_tokens: Number(document.getElementById('maxtokens').value),
      deep: document.getElementById('deep').checked,
      preset: document.getElementById('preset').value
    };
    vscode.postMessage({ type: 'send', model, mode, prompt: promptEl.value, params });
  });
  document.getElementById('load').addEventListener('click', () => {
    const typed = (customModelEl.value || '').trim();
    const selected = modelSel.value;
    const model = typed ? typed : selected;
    vscode.postMessage({ type: 'loadModel', model });
  });
  refreshBtn.addEventListener('click', () => {
    vscode.postMessage({ type: 'refreshModels' });
  });
  window.addEventListener('message', (ev) => {
    const msg = ev.data;
    if (msg.type === 'response') {
      out.textContent += "\n" + msg.text + " ";
    } else if (msg.type === 'modelLoaded') {
      out.textContent += "\nModel loaded: " + (msg.ok ? 'OK' : 'Failed') + " ";
    } else if (msg.type === 'models') {
      const sel = document.getElementById('model');
      const models = Array.isArray(msg.models) ? msg.models : [];
      // reset options: keep Auto as first
      sel.innerHTML = '<option value="Auto">Auto</option>' + models.map(m => '<option value="' + m + '">' + m + '</option>').join('');
}
  });
</script>
  </body>
  </html>`;
}

async function ensureModel(model: string): Promise<boolean> {
  const resolved = resolveModel(model);
  try {
    const res = await (globalThis as any).fetch(`${OLLAMA_ENDPOINT}/api/pull`, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ name: resolved })
    } as any);
    return res.ok;
  } catch {
    return false;
  }
}
function resolveModel(model: string): string {
  if (!model || model === 'Auto') {
    // Prefer rawr 1024 family if present
    const rawr = cachedModels.find(m => preferredRawrTags.some(tag => m.toLowerCase().includes(tag)));
    if (rawr) return rawr;
    // Else pick first installed or sensible default
    return cachedModels[0] || 'codellama:7b';
  }
  // Use user-provided exact tag/name (no hardcoded mapping)
  return model;
}

async function ollamaGenerate(model: string, prompt: string, params: any): Promise<string> {
  const resolved = resolveModel(model);
  const body = {
    model: resolved,
    prompt,
    options: {
      temperature: params?.temperature ?? 0.7,
      top_p: params?.top_p ?? 0.9,
      num_predict: params?.max_tokens ?? 2048
    }
  };
  const res = await (globalThis as any).fetch(`${OLLAMA_ENDPOINT}/api/generate`, {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify(body)
  } as any);
  if (!res.ok) throw new Error(`Ollama error ${res.status}`);
  const text = await res.text();
  return `Resolved model: ${resolved}\n${text}`;
}

export function deactivate() { }

function applyModeToPrompt(mode: 'Agent' | 'Plan' | 'Ask' | 'Edit', prompt: string, preset?: string): string {
  const basePreset = preset ? `[Preset: ${preset}] ` : '';
  switch (mode) {
    case 'Agent':
      return `${basePreset}You are an IDE Agent. Think step-by-step, propose actions, and respond concisely. If editing code, prefer unified diffs.\nUser: ${prompt}`;
    case 'Plan':
      return `${basePreset}Produce a high-quality plan (5-10 steps) tailored to the workspace. Use bullets, no fluff.\nTask: ${prompt}`;
    case 'Edit':
      return `${basePreset}Apply changes to code. Return a unified diff or clear patch instructions aligned to the repo style.\nChange request: ${prompt}`;
    case 'Ask':
    default:
      return `${basePreset}${prompt}`;
  }
}

async function listModels(): Promise<string[]> {
  try {
    const res = await (globalThis as any).fetch(`${OLLAMA_ENDPOINT}/api/tags`, { method: 'GET' } as any);
    if (!res.ok) return [];
    const json = await res.json();
    const arr = Array.isArray(json?.models) ? json.models : [];
    const names = arr.map((m: any) => m?.name).filter((n: any) => typeof n === 'string');
    return names;
  } catch {
    return [];
  }
}
