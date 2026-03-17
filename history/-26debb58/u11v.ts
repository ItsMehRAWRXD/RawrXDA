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
    vscode.commands.registerCommand('bigdaddyg.openChat', () => {
      console.log('[BigDaddyG] Command: openChat triggered');
      openChatPanel('open');
    }),
    vscode.commands.registerCommand('bigdaddyg.beaconChat', () => {
      console.log('[BigDaddyG] Command: beaconChat triggered');
      openChatPanel('beacon');
    }),
    vscode.commands.registerCommand('bigdaddyg.toggleBypass', () => {
      bypassEnabled = !bypassEnabled;
      console.log(`[BigDaddyG] Command: toggleBypass - New state: ${bypassEnabled ? 'ENABLED' : 'DISABLED'}`);
      vscode.window.showInformationMessage(`✓ BigDaddyG Bypass ${bypassEnabled ? 'ENABLED' : 'DISABLED'}`);
    }),
    vscode.commands.registerCommand('bigdaddyg.startBypass', () => {
      bypassEnabled = true;
      console.log('[BigDaddyG] Command: startBypass - Bypass ENABLED');
      vscode.window.showInformationMessage('✓ BigDaddyG Bypass ENABLED');
    }),
    vscode.commands.registerCommand('bigdaddyg.stopBypass', () => {
      bypassEnabled = false;
      console.log('[BigDaddyG] Command: stopBypass - Bypass DISABLED');
      vscode.window.showInformationMessage('✗ BigDaddyG Bypass DISABLED');
    }),
    vscode.commands.registerCommand('bigdaddyg.insertCode', async (code: string) => {
      console.log('[BigDaddyG] Command: insertCode');
      console.log(`[BigDaddyG] Code length: ${code?.length || 0} characters`);
      const editor = vscode.window.activeTextEditor;
      if (!editor) {
        console.warn('[BigDaddyG] insertCode: No active editor');
        vscode.window.showWarningMessage('No active editor to insert code');
        return;
      }
      try {
        await editor.edit(editBuilder => {
          editBuilder.insert(editor.selection.active, code);
        });
        console.log(`[BigDaddyG] insertCode: Successfully inserted at line ${editor.selection.active.line}, char ${editor.selection.active.character}`);
        vscode.window.showInformationMessage('✓ Code inserted');
      } catch (e: any) {
        console.error('[BigDaddyG] insertCode ERROR:', e);
        vscode.window.showErrorMessage('Failed to insert code: ' + e.message);
      }
    }),
    vscode.commands.registerCommand('bigdaddyg.applyDiff', async (fileUri: string, newContent: string) => {
      console.log('[BigDaddyG] Command: applyDiff');
      console.log(`[BigDaddyG] File URI: ${fileUri}`);
      console.log(`[BigDaddyG] Content length: ${newContent?.length || 0} characters`);
      try {
        const uri = vscode.Uri.parse(fileUri);
        await vscode.workspace.fs.writeFile(uri, Buffer.from(newContent, 'utf8'));
        const doc = await vscode.workspace.openTextDocument(uri);
        await vscode.window.showTextDocument(doc);
        console.log(`[BigDaddyG] applyDiff: Successfully applied changes to ${fileUri}`);
        vscode.window.showInformationMessage('✓ Changes applied');
      } catch (e: any) {
        console.error('[BigDaddyG] applyDiff ERROR:', e);
        vscode.window.showErrorMessage('Failed to apply changes: ' + e.message);
      }
    }),
    vscode.commands.registerCommand('bigdaddyg.enableInterceptor', async () => {
      console.log('[BigDaddyG] Command: enableInterceptor triggered');
      const endpoint = await vscode.window.showInputBox({ prompt: 'Local endpoint URL (e.g., http://localhost:3000)', value: interceptorConfig.endpoint });
      if (!endpoint) return;
      const backends = ['ollama-generate', 'ollama-chat', 'openai-chat'];
      const backend = await vscode.window.showQuickPick(backends, { placeHolder: 'Select backend' });
      if (!backend) return;
      const model = await vscode.window.showInputBox({ prompt: 'Model name (e.g., mistral:7b)' });
      if (!model) return;
      const modes = ['ask', 'edit', 'plan'];
      const agentMode = await vscode.window.showQuickPick(modes, { placeHolder: 'Select agent mode' }) as any;
      if (!agentMode) return;
      const ideAccessStr = await vscode.window.showQuickPick(['Yes', 'No'], { placeHolder: 'Enable IDE access for edits?' });
      const ideAccess = ideAccessStr === 'Yes';

      interceptorConfig = { enabled: true, endpoint, backend: backend as any, model, agentMode, ideAccess };
      rebuildInterceptor();
      await gatherWorkspaceContext();
      vscode.window.showInformationMessage(`✓ Request Interceptor ENABLED\nMode: ${agentMode} | IDE Access: ${ideAccess ? 'Yes' : 'No'}`);
      console.log('[BigDaddyG] Interceptor enabled:', interceptorConfig);
    }),
    vscode.commands.registerCommand('bigdaddyg.disableInterceptor', () => {
      interceptorConfig.enabled = false;
      rebuildInterceptor();
      vscode.window.showInformationMessage('✗ Request Interceptor DISABLED');
      console.log('[BigDaddyG] Interceptor disabled');
    })

  );
}

function initializeInterceptor() {
  if (interceptorActive) {
    console.log('[BigDaddyG] initializeInterceptor: Already active, skipping');
    return;
  }
  console.log('[BigDaddyG] initializeInterceptor: Starting interceptor initialization');
  interceptorActive = true;
  rebuildInterceptor();
  console.log('[BigDaddyG] initializeInterceptor: Complete');
}

function rebuildInterceptor() {
  console.log(`[BigDaddyG] rebuildInterceptor: Enabled=${interceptorConfig.enabled}`);

  if (!interceptorConfig.enabled) {
    (http as any).request = httpOriginal;
    (https as any).request = httpsOriginal;
    console.log('[BigDaddyG] rebuildInterceptor: Interceptor disabled, restored original http/https');
    return;
  }

  (http as any).request = ((url: any, options: any, callback?: any) => {
    const isChat = detectChatRequest(url, options);
    console.log(`[BigDaddyG] HTTP Request: ${JSON.stringify({ url: typeof url === 'string' ? url : url?.href, isChat, willIntercept: isChat && interceptorConfig.enabled })}`);
    if (isChat && interceptorConfig.enabled && interceptorConfig.model) {
      console.log('[BigDaddyG] ✓ Intercepting HTTP request to local model');
      return proxyToLocal(interceptorConfig.endpoint, interceptorConfig.backend, interceptorConfig.model, url, options, callback);
    }
    return httpOriginal(url, options, callback);
  }) as any;

  (https as any).request = ((url: any, options: any, callback?: any) => {
    const isChat = detectChatRequest(url, options);
    console.log(`[BigDaddyG] HTTPS Request: ${JSON.stringify({ url: typeof url === 'string' ? url : url?.href, isChat, willIntercept: isChat && interceptorConfig.enabled })}`);
    if (isChat && interceptorConfig.enabled && interceptorConfig.model) {
      console.log('[BigDaddyG] ✓ Intercepting HTTPS request to local model');
      return proxyToLocal(interceptorConfig.endpoint, interceptorConfig.backend, interceptorConfig.model, url, options, callback);
    }
    return httpsOriginal(url, options, callback);
  }) as any;

  console.log('[BigDaddyG] rebuildInterceptor: Interceptor hooks installed successfully');
}

function detectChatRequest(url: any, options: any): boolean {
  const urlStr = typeof url === 'string' ? url : (url?.href || '');
  const methodStr = options?.method || '';
  const pathStr = options?.path || '';
  const fullStr = urlStr + pathStr;

  const chatPatterns = [
    /\/chat|\/message|\/completion|\/generate/i,
    /cursor.*api/i,
    /ai.*model/i
  ];
  const isChatPath = chatPatterns.some(p => p.test(fullStr));
  const isPostPut = /^(POST|PUT)$/i.test(methodStr);

  return isChatPath && isPostPut;
}

function proxyToLocal(endpoint: string, backend: string, model: string, url: any, options: any, callback?: any): http.ClientRequest {
  const origUrl = typeof url === 'string' ? url : (url?.href || '');
  console.log('[BigDaddyG] Proxying to local:', { endpoint, backend, model, origUrl });

  // Intercept response for edit/plan handling
  const wrappedCallback = (res: any) => {
    const origEmit = res.emit.bind(res);
    res.emit = ((event: string, ...args: any[]) => {
      if (event === 'data' && args[0]) {
        const chunk = args[0];
        const text = chunk.toString();
        if (interceptorConfig.ideAccess && interceptorConfig.agentMode === 'edit') {
          handleEditDirective(text).catch(e => console.warn('[BigDaddyG] Edit handling error:', e.message));
        }
      }
      return origEmit(event, ...args);
    }) as any;
    if (callback) {
      callback(res);
    }
  };

  const req = http.request(endpoint, { method: 'POST', path: urlForBackend(backend), headers: { 'Content-Type': 'application/json' } }, wrappedCallback);

  const origWrite = req.write.bind(req);
  req.write = ((data: any, ...args: any[]) => {
    if (typeof data === 'string' || Buffer.isBuffer(data)) {
      try {
        const bodyObj = JSON.parse(data.toString());
        const transformed = transformRequestBody(bodyObj, backend, model);
        console.log('[BigDaddyG] Transformed request:', { original: bodyObj, transformed });
        return origWrite(JSON.stringify(transformed), ...args);
      } catch (e: any) {
        console.warn('[BigDaddyG] Transform error:', e.message);
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
      return [
        'System: You are an IDE coding agent. Keep explanations short.',
        'After your brief reply, output a single line beginning with AGENT_EDIT: followed by JSON.',
        'JSON schema: {"file":"relative/path","content":"...","find":"...","replace":"..."}',
        'Do not wrap JSON in backticks. Keep it on one line.'
      ].join('\n');
    }
    if (mode === 'plan') {
      return [
        'System: You are a planning agent. Provide a concise plan.',
        'After your plan, output a single line beginning with AGENT_PLAN: followed by JSON.',
        'No backticks; one-line JSON only.'
      ].join('\n');
    }
    return 'System: You are a helpful coding assistant. Answer succinctly and accurately.';
  }

  function addContextToPrompt(prompt: string): string {
    if (!interceptorConfig.ideAccess) return prompt;
    const ctx = `\n\n[Workspace Context]\nFiles: ${contextCache.files}\nActive Code:\n${contextCache.code}`;
    return prompt + ctx;
  }

  function addContextToMessages(messages: any[]): any[] {
    if (!interceptorConfig.ideAccess || messages.length === 0) return messages;
    const lastMsg = messages[messages.length - 1];
    if (lastMsg?.role === 'user') {
      const ctx = `\n\n[Workspace Context]\nFiles: ${contextCache.files}\nActive Code:\n${contextCache.code}`;
      return [...messages.slice(0, -1), { ...lastMsg, content: lastMsg.content + ctx }];
    }
    return messages;
  }

  async function gatherWorkspaceContext(): Promise<void> {
    try {
      const editor = vscode.window.activeTextEditor;
      if (editor) {
        contextCache.code = editor.document.getText().slice(0, 500);
      }
      const workspaceFolders = vscode.workspace.workspaceFolders;
      if (workspaceFolders) {
        contextCache.workspace = workspaceFolders.map(f => f.name).join(', ');
        contextCache.files = workspaceFolders.map(f => f.uri.fsPath).join(', ').slice(0, 200);
      }
    } catch (e: any) {
      console.warn('[BigDaddyG] Context gather failed:', e.message);
    }
  }

  function extractPromptFromMessages(messages: any[]): string {
    if (!Array.isArray(messages)) return '';
    const lastMsg = messages[messages.length - 1];
    return (lastMsg?.content || lastMsg?.text || '').toString();
  }

  function openChatPanel(mode: 'open' | 'beacon') {
    const title = mode === 'beacon' ? 'BigDaddyG: Beacon Chat' : 'BigDaddyG: AI Chat';
    const panel = vscode.window.createWebviewPanel('bigdaddyg', title, vscode.ViewColumn.Beside, { enableScripts: true });
    panel.webview.html = getHtml();

    // Models are loaded on demand from the webview with provided endpoint/backend (no defaults).

    panel.webview.onDidReceiveMessage(async (msg) => {
      console.log('[BigDaddyG] Received message:', msg.type);

      if (msg.type === 'send') {
        const endpoint: string = (msg.endpoint || '').trim();
        const backend: 'ollama-generate' | 'ollama-chat' | 'openai-chat' = msg.backend;
        const useCustom: boolean = !!msg.useCustom;
        const customModel: string = (msg.customModel || '').trim();
        const selectedModel: string = (msg.model || '').trim();
        const model = useCustom ? customModel : selectedModel;
        const prompt = msg.prompt as string;
        const agentMode: 'ask' | 'edit' | 'plan' = (msg.mode || 'ask');
        const ideAccess: boolean = !!msg.ideAccess;
        const rawPrompt: boolean = !!msg.rawPrompt;
        const preface: string = (msg.preface || '').toString();

        if (!endpoint) {
          panel.webview.postMessage({ type: 'error', text: 'Endpoint URL is required' });
          return;
        }
        if (!backend) {
          panel.webview.postMessage({ type: 'error', text: 'Select a backend type' });
          return;
        }
        if (!model) {
          panel.webview.postMessage({ type: 'error', text: 'Select or enter a model id' });
          return;
        }

        console.log('[BigDaddyG] Sending:', { endpoint, backend, model, promptLen: prompt.length, agentMode, ideAccess, rawPrompt });

        try {
          const fullPrompt = rawPrompt ? prompt : (preface ? (preface + "\n\n" + prompt) : prompt);
          panel.webview.postMessage({ type: 'responseStart' });
          let collected = '';

          await streamQuerySingle(endpoint, backend, model, fullPrompt, (chunk) => {
            collected += chunk;
            panel.webview.postMessage({ type: 'responseChunk', text: chunk });
          });

          console.log('[BigDaddyG] Stream complete, total length:', collected.length);
          if (ideAccess && agentMode === 'edit') {
            try {
              const applied = await tryApplyAgentEdit(collected);
              if (applied) {
                vscode.window.showInformationMessage('✓ Applied AGENT_EDIT to workspace');
              }
            } catch (e: any) {
              vscode.window.showWarningMessage('Agent edit failed: ' + e.message);
            }
          }
          panel.webview.postMessage({ type: 'responseEnd' });
        } catch (e: any) {
          console.error('[BigDaddyG] Error:', e.message);
          panel.webview.postMessage({ type: 'error', text: e.message });
        }
      } else if (msg.type === 'refreshModels') {
        try {
          const endpoint: string = (msg.endpoint || '').trim();
          const backend: 'ollama-generate' | 'ollama-chat' | 'openai-chat' = msg.backend;
          if (!endpoint) throw new Error('Endpoint URL is required');
          if (!backend) throw new Error('Select a backend type');
          const models = await loadModels(endpoint, backend);
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
  <input id="endpoint" placeholder="Endpoint URL (e.g. http://localhost:3000)" style="flex:2" />
  <select id="backend" title="Backend type" style="flex:1">
    <option value="">Select backend</option>
    <option value="ollama-generate">Ollama Generate (/api/generate)</option>
    <option value="ollama-chat">Ollama Chat (/api/chat)</option>
    <option value="openai-chat">OpenAI Chat (/v1/chat/completions)</option>
  </select>
  <button onclick="refreshModels()" title="Refresh model list">🔄</button>
</div>

<div class="row">
  <select id="model"><option>No models loaded</option></select>
  <input id="customModel" placeholder="Custom model id" />
  <label style="display:flex;align-items:center;gap:4px;user-select:none;">
    <input type="checkbox" id="useCustom" /> Use custom
  </label>
  <select id="agentMode" title="Agent mode">
    <option value="ask">Ask</option>
    <option value="edit">Edit</option>
    <option value="plan">Plan</option>
  </select>
  <label style="display:flex;align-items:center;gap:4px;user-select:none;">
    <input type="checkbox" id="ideAccess" /> IDE Access
  </label>
</div>

<textarea id="prompt" placeholder="Ask BigDaddyG something..."></textarea>

<div class="row">
  <label style="display:flex;align-items:center;gap:4px;user-select:none;">
    <input type="checkbox" id="rawPrompt" checked /> Raw Prompt (no preface)
  </label>
  <input id="preface" placeholder="Optional system preface (used only if Raw is unchecked)" style="flex:2" />
  <span style="flex:1"></span>
  <span style="flex:1"></span>
</div>

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
  
  const endpoint = document.getElementById('endpoint').value.trim();
  const backend = document.getElementById('backend').value;
  const modelSel = document.getElementById('model').value;
  const customModel = document.getElementById('customModel').value.trim();
  const useCustom = document.getElementById('useCustom').checked;
  const model = useCustom ? customModel : modelSel;
  const prompt = document.getElementById('prompt').value.trim();
  const agentMode = document.getElementById('agentMode').value;
  const ideAccess = document.getElementById('ideAccess').checked;
  const rawPrompt = document.getElementById('rawPrompt').checked;
  const preface = document.getElementById('preface').value;
  
  if (!prompt) {
    updateStatus('❌ Please enter a message');
    return;
  }
  
  if (!endpoint) {
    updateStatus('❌ Endpoint URL required');
    return;
  }
  if (!backend) {
    updateStatus('❌ Select a backend type');
    return;
  }
  if (!model) {
    updateStatus('❌ Select or enter a model id');
    return;
  }
  
  isWaiting = true;
  updateStatus('⏳ Sending to ' + model + ' [' + agentMode + ']...');
  document.getElementById('output').textContent = '';
  
  console.log('[UI] Sending:', { endpoint, backend, model, promptLen: prompt.length, agentMode, ideAccess, rawPrompt });
  vscode.postMessage({ type: 'send', endpoint, backend, model, useCustom, customModel, prompt, mode: agentMode, ideAccess, rawPrompt, preface });
}

function refreshModels() {
  if (isWaiting) return;
  isWaiting = true;
  updateStatus('🔄 Loading models...');
  document.getElementById('output').textContent = '⏳ Querying models...';
  const endpoint = document.getElementById('endpoint').value.trim();
  const backend = document.getElementById('backend').value;
  if (!endpoint || !backend) {
    updateStatus('❌ Provide endpoint and select backend');
    isWaiting = false;
    return;
  }
  console.log('[UI] Refreshing models', { endpoint, backend });
  vscode.postMessage({ type: 'refreshModels', endpoint, backend });
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
    // legacy full response path (not used in streaming mode)
    document.getElementById('output').textContent = msg.text;
    updateStatus('✓ Response received');
    isWaiting = false;
    
  } else if (msg.type === 'error') {
    document.getElementById('output').textContent = '❌ ERROR:\\n' + msg.text;
    updateStatus('❌ ' + msg.text);
    isWaiting = false;
  } else if (msg.type === 'responseStart') {
    document.getElementById('output').textContent = '';
    updateStatus('↘ Streaming response...');
    isWaiting = true;
  } else if (msg.type === 'responseChunk') {
    const out = document.getElementById('output');
    out.textContent += msg.text;
    out.scrollTop = out.scrollHeight;
  } else if (msg.type === 'responseEnd') {
    updateStatus('✓ Response complete');
    isWaiting = false;
  }
});

// No auto-refresh: user provides endpoint and backend explicitly.
</script>
</body>
</html>`;
  }

  // Streaming helpers
  async function streamQuerySingle(endpoint: string, backend: 'ollama-generate' | 'ollama-chat' | 'openai-chat', model: string, prompt: string, onChunk: (t: string) => void): Promise<void> {
    if (backend === 'ollama-generate') {
      await streamEndpoint(endpoint, '/api/generate', { model, prompt, stream: true }, onChunk, parseOllamaGenerateLine);
      return;
    }
    if (backend === 'ollama-chat') {
      await streamEndpoint(endpoint, '/api/chat', { model, messages: [{ role: 'user', content: prompt }], stream: true }, onChunk, parseOllamaChatLine);
      return;
    }
    if (backend === 'openai-chat') {
      await streamEndpoint(endpoint, '/v1/chat/completions', { model, messages: [{ role: 'user', content: prompt }], stream: true }, onChunk, parseOpenAIStreamLine);
      return;
    }
    throw new Error('Unsupported backend');
  }

  type LineParser = (line: string) => { text?: string, done?: boolean } | undefined;

  function parseOllamaGenerateLine(line: string) {
    if (!line) return;
    const s = line.startsWith('data:') ? line.replace(/^data:\s*/, '') : line;
    if (s === '[DONE]') return { done: true };
    try {
      const obj = JSON.parse(s);
      if (obj.done) return { done: true };
      if (typeof obj.response === 'string' && obj.response) return { text: obj.response };
    } catch { /* ignore */ }
  }

  function parseOllamaChatLine(line: string) {
    if (!line) return;
    const s = line.startsWith('data:') ? line.replace(/^data:\s*/, '') : line;
    if (s === '[DONE]') return { done: true };
    try {
      const obj = JSON.parse(s);
      if (obj.done) return { done: true };
      const t = obj?.message?.content;
      if (typeof t === 'string' && t) return { text: t };
    } catch { /* ignore */ }
  }

  function parseOpenAIStreamLine(line: string) {
    if (!line) return;
    const s = line.startsWith('data:') ? line.replace(/^data:\s*/, '') : line;
    if (s === '[DONE]') return { done: true };
    try {
      const obj = JSON.parse(s);
      const delta = obj?.choices?.[0]?.delta?.content || obj?.choices?.[0]?.message?.content;
      if (typeof delta === 'string' && delta) return { text: delta };
    } catch { /* ignore */ }
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
          'Accept': 'text/event-stream',
          'Connection': 'keep-alive',
          'Content-Length': Buffer.byteLength(bodyStr)
        }
      };
      console.log('[HTTP] STREAM POST', options.hostname + ':' + options.port + options.path);
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
          // flush remaining buffer
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

  function buildAgentPreface(mode: 'ask' | 'edit' | 'plan'): string {
    if (mode === 'edit') {
      return [
        'System: You are an IDE coding agent. Keep explanations short.',
        'After your brief reply, output a single line beginning with AGENT_EDIT: followed by JSON.',
        'JSON schema: {"file":"relative/path/from/workspaceRoot","content":"optional full file content","find":"optional exact string to find","replace":"optional replacement string","replaceAll":true}',
        'Do not wrap JSON in backticks or extra text. Keep it on one line.'
      ].join('\n');
    }
    if (mode === 'plan') {
      return [
        'System: You are a planning agent. Provide a concise plan.',
        'After your plan, output a single line beginning with AGENT_PLAN: followed by JSON {"steps":["..."]}.',
        'No backticks; one-line JSON only.'
      ].join('\n');
    }
    return 'System: You are a helpful AI. Answer succinctly and accurately.';
  }

  async function tryApplyAgentEdit(text: string): Promise<boolean> {
    const m = /AGENT_EDIT:\s*(\{[\s\S]*?\})/i.exec(text);
    if (!m) return false;
    let payload: any;
    try { payload = JSON.parse(m[1]); } catch { throw new Error('Invalid AGENT_EDIT JSON'); }

    const file = String(payload.file || '').trim();
    if (!file) throw new Error('AGENT_EDIT missing "file"');

    const root = vscode.workspace.workspaceFolders?.[0]?.uri;
    if (!root) throw new Error('No workspace root available');

    const segments = file.split(/[\\\/]+/).filter(Boolean);
    const target = vscode.Uri.joinPath(root, ...segments);

    const rootFs = root.fsPath.replace(/\\/g, '/').toLowerCase();
    const tgtFs = target.fsPath.replace(/\\/g, '/').toLowerCase();
    if (!tgtFs.startsWith(rootFs)) throw new Error('Target file is outside workspace');

    let newContent: string | undefined;
    if (typeof payload.content === 'string') {
      newContent = payload.content;
    } else if (typeof payload.find === 'string' && typeof payload.replace === 'string') {
      try {
        const bytes = await vscode.workspace.fs.readFile(target);
        const cur = Buffer.from(bytes).toString('utf8');
        const replaceAll = !!payload.replaceAll;
        if (replaceAll) {
          newContent = cur.split(payload.find).join(payload.replace);
        } else {
          newContent = cur.replace(payload.find, payload.replace);
        }
      } catch (e: any) {
        throw new Error('Read failed: ' + e.message);
      }
    } else {
      throw new Error('AGENT_EDIT requires either "content" or "find" + "replace"');
    }

    try {
      await vscode.workspace.fs.writeFile(target, Buffer.from(newContent!, 'utf8'));
      const doc = await vscode.workspace.openTextDocument(target);
      await vscode.window.showTextDocument(doc, { preview: false });
      return true;
    } catch (e: any) {
      throw new Error('Write failed: ' + e.message);
    }
  }

  // Generic JSON HTTP helper (no hardcoded base, no fallback)
  function httpJsonBase(base: string, method: 'GET' | 'POST', path: string, payload?: any, timeoutMs = 15000): Promise<{ status: number, json: any, raw: string }> {
    return new Promise((resolve, reject) => {
      const url = new URL(`${base}${path}`);
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

      console.log('[HTTP] ' + method, options.hostname + ':' + options.port + options.path);

      const req = http.request(options, (res) => {
        let data = '';
        res.on('data', chunk => data += chunk);
        res.on('end', () => {
          const status = res.statusCode || 0;
          try {
            const parsed = JSON.parse(data);
            resolve({ status, json: parsed, raw: data });
          } catch {
            // Try last JSON line if body contained SSE-like lines
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
      } catch { }
    }
    if (out.trim()) return out;
    return undefined;
  }

  function extractText(obj: any, raw?: string): string | undefined {
    const t = basicExtract(obj);
    if (t && t.trim()) return t;
    return extractFromRaw(raw);
  }

  async function queryOnce(base: string, backend: 'ollama-generate' | 'ollama-chat' | 'openai-chat', model: string, prompt: string): Promise<string> {
    if (backend === 'ollama-generate') {
      const gen = await httpJsonBase(base, 'POST', '/api/generate', { model, prompt, stream: false });
      if (gen.status !== 200) throw new Error(`HTTP ${gen.status}`);
      const text = extractText(gen.json, gen.raw) || '';
      return text;
    }
    if (backend === 'ollama-chat') {
      const chat = await httpJsonBase(base, 'POST', '/api/chat', { model, messages: [{ role: 'user', content: prompt }], stream: false });
      if (chat.status !== 200) throw new Error(`HTTP ${chat.status}`);
      const text = extractText(chat.json, chat.raw) || '';
      return text;
    }
    if (backend === 'openai-chat') {
      const openai = await httpJsonBase(base, 'POST', '/v1/chat/completions', { model, messages: [{ role: 'user', content: prompt }], stream: false });
      if (openai.status !== 200) throw new Error(`HTTP ${openai.status}`);
      const text = extractText(openai.json, openai.raw) || '';
      return text;
    }
    throw new Error('Unsupported backend');
  }

  async function loadModels(base: string, backend: 'ollama-generate' | 'ollama-chat' | 'openai-chat'): Promise<string[]> {
    if (backend === 'ollama-generate' || backend === 'ollama-chat') {
      const tags = await httpJsonBase(base, 'GET', '/api/tags');
      if (tags.status !== 200) return [];
      const models = (tags.json?.models || []).map((m: any) => m.name).filter(Boolean);
      return Array.isArray(models) ? models : [];
    }
    if (backend === 'openai-chat') {
      const m = await httpJsonBase(base, 'GET', '/v1/models');
      if (m.status !== 200) return [];
      const models = Array.isArray(m.json?.data) ? m.json.data.map((x: any) => x.id).filter(Boolean) : [];
      return models;
    }
    return [];
  }

  export function deactivate() { }
