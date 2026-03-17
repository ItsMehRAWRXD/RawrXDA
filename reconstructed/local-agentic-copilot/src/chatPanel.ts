import * as vscode from 'vscode';
import { AgenticCopilot } from './agenticCopilot';

export class ChatPanel {
  public static currentPanel: ChatPanel | undefined;

  private readonly panel: vscode.WebviewPanel;
  private readonly disposables: vscode.Disposable[] = [];

  private constructor(panel: vscode.WebviewPanel, private readonly agenticCopilot: AgenticCopilot) {
    this.panel = panel;

    this.panel.webview.html = this.getHtmlForWebview(this.panel.webview);

    this.panel.onDidDispose(() => this.dispose(), null, this.disposables);

    this.panel.webview.onDidReceiveMessage(
      async (message) => {
        switch (message?.type) {
          case 'ready': {
            await this.pushModels();
            await this.pushState();
            return;
          }
          case 'refreshModels': {
            await this.pushModels();
            return;
          }
          case 'selectModel': {
            const model = typeof message?.model === 'string' ? message.model : undefined;
            await this.agenticCopilot.setSelectedModel(model);
            await this.pushState();
            return;
          }
          case 'send': {
            const text = typeof message?.text === 'string' ? message.text : '';
            if (!text.trim()) {
              return;
            }
            const model = typeof message?.model === 'string' ? message.model : undefined;
            try {
              const response = await this.agenticCopilot.chat(text, model);
              this.panel.webview.postMessage({ type: 'response', text: response });
            } catch (err: unknown) {
              const msg = err instanceof Error ? err.message : String(err);
              this.panel.webview.postMessage({ type: 'error', message: msg });
            }
            return;
          }
        }
      },
      null,
      this.disposables
    );
  }

  public static createOrShow(agenticCopilot: AgenticCopilot): void {
    const column = vscode.window.activeTextEditor?.viewColumn;

    if (ChatPanel.currentPanel) {
      ChatPanel.currentPanel.panel.reveal(column);
      return;
    }

    const panel = vscode.window.createWebviewPanel(
      'agenticCopilotChat',
      'Agentic Copilot Chat',
      column ?? vscode.ViewColumn.One,
      {
        enableScripts: true,
        retainContextWhenHidden: true
      }
    );

    ChatPanel.currentPanel = new ChatPanel(panel, agenticCopilot);
  }

  public dispose(): void {
    ChatPanel.currentPanel = undefined;

    while (this.disposables.length) {
      const d = this.disposables.pop();
      if (d) {
        d.dispose();
      }
    }
  }

  private async pushModels(): Promise<void> {
    const models = await this.agenticCopilot.getAvailableModels();
    this.panel.webview.postMessage({ type: 'models', models });
  }

  private async pushState(): Promise<void> {
    const state = this.agenticCopilot.getUiState();
    this.panel.webview.postMessage({ type: 'state', state });
  }

  private getHtmlForWebview(webview: vscode.Webview): string {
    return `<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8" />
  <meta name="viewport" content="width=device-width, initial-scale=1.0" />
  <title>Agentic Copilot CLI</title>
  <style>
    :root {
      color-scheme: light dark;
    }
    body {
      font-family: var(--vscode-font-family);
      color: var(--vscode-foreground);
      background: var(--vscode-editor-background);
      margin: 0;
      padding: 12px;
      display: flex;
      flex-direction: column;
      gap: 10px;
      height: 100vh;
      box-sizing: border-box;
    }
    .header {
      display: flex;
      align-items: center;
      justify-content: space-between;
      gap: 8px;
      flex-wrap: wrap;
    }
    .title {
      display: flex;
      align-items: center;
      gap: 8px;
      font-weight: 600;
      letter-spacing: 0.2px;
    }
    .chip {
      border: 1px solid var(--vscode-input-border);
      background: var(--vscode-input-background);
      color: var(--vscode-foreground);
      border-radius: 999px;
      padding: 4px 10px;
      font-size: 12px;
      line-height: 1;
      display: inline-flex;
      align-items: center;
      gap: 6px;
    }
    .chip .dot {
      width: 10px;
      height: 10px;
      border-radius: 50%;
      background: var(--vscode-editorWidget-border);
      display: inline-block;
    }
    .chip.live .dot {
      background: #2ecc71;
    }
    .chip.idle .dot {
      background: #f1c40f;
    }
    .chip.error .dot {
      background: #e74c3c;
    }
    .toolbar {
      display: flex;
      gap: 8px;
      align-items: center;
      flex-wrap: wrap;
    }
    select, button, textarea {
      font-family: var(--vscode-font-family);
      color: var(--vscode-input-foreground);
      background: var(--vscode-input-background);
      border: 1px solid var(--vscode-input-border);
      border-radius: 4px;
    }
    button {
      padding: 6px 10px;
      cursor: pointer;
      transition: background 0.1s ease, border-color 0.1s ease;
    }
    button:hover:enabled {
      border-color: var(--vscode-focusBorder);
    }
    button:disabled {
      opacity: 0.6;
      cursor: default;
    }
    select {
      padding: 6px 10px;
      min-width: 260px;
      max-width: 100%;
    }
    .panel {
      border: 1px solid var(--vscode-input-border);
      border-radius: 8px;
      background: var(--vscode-editor-background);
      box-shadow: 0 6px 16px rgba(0, 0, 0, 0.15);
      display: flex;
      flex-direction: column;
      min-height: 0;
      height: 100%;
    }
    .shortcuts {
      display: flex;
      gap: 10px;
      flex-wrap: wrap;
      font-size: 12px;
      opacity: 0.85;
    }
    .shortcut {
      background: var(--vscode-editorWidget-background);
      border: 1px dashed var(--vscode-input-border);
      border-radius: 6px;
      padding: 6px 8px;
    }
    .console {
      flex: 1;
      overflow: hidden;
      display: flex;
      flex-direction: column;
      gap: 8px;
      padding: 10px;
    }
    .history {
      flex: 1;
      overflow: auto;
      border: 1px solid var(--vscode-input-border);
      border-radius: 6px;
      background: var(--vscode-textCodeBlock-background);
      padding: 10px;
      display: flex;
      flex-direction: column;
      gap: 10px;
    }
    .entry {
      border: 1px solid var(--vscode-input-border);
      border-radius: 6px;
      padding: 8px 10px;
      background: var(--vscode-editorWidget-background);
    }
    .entry.assistant {
      background: var(--vscode-textCodeBlock-background);
    }
    .entry.error {
      border-color: #e74c3c;
    }
    .meta {
      display: flex;
      justify-content: space-between;
      align-items: center;
      gap: 6px;
      font-size: 11px;
      opacity: 0.85;
      flex-wrap: wrap;
    }
    .meta strong {
      font-weight: 600;
    }
    .content {
      margin-top: 6px;
      white-space: pre-wrap;
      word-break: break-word;
      font-size: 13px;
      line-height: 1.4;
    }
    .composer {
      display: grid;
      grid-template-columns: 1fr auto;
      gap: 8px;
      align-items: stretch;
    }
    textarea {
      width: 100%;
      min-height: 90px;
      max-height: 160px;
      padding: 10px;
      resize: vertical;
    }
    .actions {
      display: flex;
      flex-direction: column;
      gap: 6px;
      width: 140px;
    }
    .actions button {
      width: 100%;
    }
    .footer {
      display: flex;
      justify-content: space-between;
      align-items: center;
      gap: 8px;
      flex-wrap: wrap;
      font-size: 12px;
      padding: 10px;
      border-top: 1px solid var(--vscode-input-border);
      background: var(--vscode-editorWidget-background);
      border-radius: 0 0 8px 8px;
    }
    .pill {
      padding: 4px 10px;
      border-radius: 999px;
      border: 1px solid var(--vscode-input-border);
      background: var(--vscode-input-background);
    }
    .status-text {
      font-size: 12px;
      opacity: 0.9;
    }
  </style>
</head>
<body>
  <div class="header">
    <div class="title">
      <span class="chip" id="modeChip"><span class="dot"></span><span id="modeLabel">MODE</span></span>
      <div>Agentic Copilot CLI</div>
    </div>
    <div class="chip live" id="statusChip"><span class="dot"></span><span id="statusLabel">Idle</span></div>
  </div>

  <div class="toolbar">
    <label for="model">Model</label>
    <select id="model" disabled>
      <option>Loading models…</option>
    </select>
    <button id="refresh" type="button">Refresh</button>
    <button id="clear" type="button">Clear History</button>
  </div>

  <div class="shortcuts">
    <div class="shortcut">Enter: run • Shift+Enter: newline</div>
    <div class="shortcut">Tab: recall last • ↑/↓: browse history</div>
    <div class="shortcut">Ctrl/Cmd+Enter: run without leaving</div>
  </div>

  <div class="panel">
    <div class="console">
      <div id="history" class="history"></div>
      <div class="composer">
        <textarea id="input" placeholder="Type a command or prompt; use /help for quick actions."></textarea>
        <div class="actions">
          <button id="send" type="button">Run</button>
          <button id="reuseLast" type="button">Reuse Last</button>
          <button id="copyLast" type="button">Copy Last Response</button>
        </div>
      </div>
    </div>
    <div class="footer">
      <div class="status-text" id="metrics">Waiting for first run…</div>
      <div class="pill" id="hint">Model selection applies to extension requests.</div>
    </div>
  </div>

  <script>
    const vscode = acquireVsCodeApi();

    const els = {
      model: document.getElementById('model'),
      refresh: document.getElementById('refresh'),
      clear: document.getElementById('clear'),
      history: document.getElementById('history'),
      input: document.getElementById('input'),
      send: document.getElementById('send'),
      reuseLast: document.getElementById('reuseLast'),
      copyLast: document.getElementById('copyLast'),
      modeChip: document.getElementById('modeChip'),
      modeLabel: document.getElementById('modeLabel'),
      statusChip: document.getElementById('statusChip'),
      statusLabel: document.getElementById('statusLabel'),
      metrics: document.getElementById('metrics'),
    };

    let currentModel = undefined;
    let pendingAssistantId = undefined;
    let pendingStartedAt = 0;

    const commandHistory = [];
    let commandCursor = -1;

    const historyEntries = [];

    function renderHistory() {
      els.history.innerHTML = '';
      for (const entry of historyEntries) {
        const wrapper = document.createElement('div');
        wrapper.className = 'entry ' + entry.role + (entry.error ? ' error' : '');

        const meta = document.createElement('div');
        meta.className = 'meta';
        const left = document.createElement('div');
        left.textContent = entry.role.toUpperCase() + ' • ' + entry.time;
        const right = document.createElement('div');
        const detail = [];
        if (entry.model) detail.push(entry.model);
        if (entry.latencyMs !== undefined) detail.push(String(entry.latencyMs) + ' ms');
        if (entry.status) detail.push(entry.status);
        right.textContent = detail.join(' • ');
        meta.appendChild(left);
        meta.appendChild(right);

        const content = document.createElement('div');
        content.className = 'content';
        content.textContent = entry.text;

        wrapper.appendChild(meta);
        wrapper.appendChild(content);
        els.history.appendChild(wrapper);
      }
      els.history.scrollTop = els.history.scrollHeight;
    }

    function setStatus(label, state) {
      els.statusLabel.textContent = label;
      els.statusChip.classList.remove('live', 'idle', 'error');
      els.statusChip.classList.add(state);
    }

    function addHistory(role, text, extras = {}) {
      const entry = {
        id: crypto.randomUUID(),
        role,
        text,
        time: new Date().toLocaleTimeString(),
        ...extras,
      };
      historyEntries.push(entry);
      renderHistory();
      return entry.id;
    }

    function setModels(models) {
      els.model.innerHTML = '';
      if (!models || models.length === 0) {
        const opt = document.createElement('option');
        opt.textContent = 'No models found';
        opt.value = '';
        els.model.appendChild(opt);
        els.model.disabled = true;
        return;
      }

      for (const m of models) {
        const opt = document.createElement('option');
        opt.value = m;
        opt.textContent = m;
        els.model.appendChild(opt);
      }

      els.model.disabled = false;
      if (currentModel && models.includes(currentModel)) {
        els.model.value = currentModel;
      } else {
        currentModel = els.model.value;
        vscode.postMessage({ type: 'selectModel', model: currentModel });
      }
    }

    function setState(state) {
      if (state && state.selectedModel) {
        currentModel = state.selectedModel;
        if (!els.model.disabled) {
          els.model.value = currentModel;
        }
      }
      if (state && typeof state.isAgenticMode === 'boolean') {
        const label = state.isAgenticMode ? 'Agentic' : 'Standard';
        els.modeLabel.textContent = label + ' Mode';
        els.modeChip.classList.toggle('live', state.isAgenticMode);
        els.modeChip.classList.toggle('idle', !state.isAgenticMode);
      }
    }

    function updateMetrics(text) {
      els.metrics.textContent = text;
    }

    function resetInput() {
      els.input.value = '';
      commandCursor = -1;
    }

    function setBusy(isBusy) {
      els.send.disabled = isBusy;
      els.model.disabled = isBusy;
      els.reuseLast.disabled = isBusy;
      els.copyLast.disabled = isBusy;
    }

    function reuseLastCommand() {
      if (commandHistory.length === 0) return;
      els.input.value = commandHistory[commandHistory.length - 1];
      els.input.focus();
    }

    function copyLastResponse() {
      const lastAssistant = [...historyEntries].reverse().find((e) => e.role === 'assistant');
      if (!lastAssistant) return;
      navigator.clipboard.writeText(lastAssistant.text).catch(() => {});
    }

    async function send() {
      const text = els.input.value;
      if (!text.trim()) return;

      commandHistory.push(text);
      resetInput();
      setBusy(true);
      setStatus('Running', 'idle');

      addHistory('user', text, { model: currentModel });
      pendingAssistantId = addHistory('assistant', '…', { status: 'pending' });
      pendingStartedAt = performance.now();

      vscode.postMessage({ type: 'send', text, model: currentModel });
    }

    function updateAssistant(text, isError) {
      const latency = Math.round(performance.now() - pendingStartedAt);
      for (const entry of historyEntries) {
        if (entry.id === pendingAssistantId) {
          entry.text = text;
          entry.error = !!isError;
          entry.status = 'done';
          entry.latencyMs = latency;
          entry.model = currentModel;
          break;
        }
      }
      renderHistory();
      updateMetrics('Last run • Model: ' + (currentModel || 'n/a') + ' • Latency: ' + latency + ' ms');
      pendingAssistantId = undefined;
      pendingStartedAt = 0;
    }

    els.refresh.addEventListener('click', () => {
      vscode.postMessage({ type: 'refreshModels' });
    });

    els.clear.addEventListener('click', () => {
      historyEntries.splice(0, historyEntries.length);
      renderHistory();
      updateMetrics('History cleared. Ready.');
    });

    els.reuseLast.addEventListener('click', reuseLastCommand);
    els.copyLast.addEventListener('click', copyLastResponse);

    els.model.addEventListener('change', () => {
      currentModel = els.model.value;
      vscode.postMessage({ type: 'selectModel', model: currentModel });
    });

    els.send.addEventListener('click', send);

    els.input.addEventListener('keydown', (e) => {
      if (e.key === 'Enter' && !e.shiftKey && !e.altKey && !e.metaKey && !e.ctrlKey) {
        e.preventDefault();
        send();
        return;
      }
      if (e.key === 'Enter' && (e.ctrlKey || e.metaKey)) {
        e.preventDefault();
        send();
        return;
      }
      if (e.key === 'Tab') {
        e.preventDefault();
        reuseLastCommand();
        return;
      }
      if (e.key === 'ArrowUp' && !e.shiftKey && !e.altKey && !e.metaKey && !e.ctrlKey) {
        e.preventDefault();
        if (commandHistory.length === 0) return;
        if (commandCursor < commandHistory.length - 1) {
          commandCursor += 1;
        }
        els.input.value = commandHistory[commandHistory.length - 1 - commandCursor];
        els.input.setSelectionRange(els.input.value.length, els.input.value.length);
        return;
      }
      if (e.key === 'ArrowDown' && !e.shiftKey && !e.altKey && !e.metaKey && !e.ctrlKey) {
        e.preventDefault();
        if (commandCursor > 0) {
          commandCursor -= 1;
          els.input.value = commandHistory[commandHistory.length - 1 - commandCursor];
        } else {
          commandCursor = -1;
          els.input.value = '';
        }
        els.input.setSelectionRange(els.input.value.length, els.input.value.length);
      }
    });

    window.addEventListener('message', (event) => {
      const msg = event.data;
      if (!msg || !msg.type) return;

      switch (msg.type) {
        case 'models':
          setModels(msg.models);
          return;
        case 'state':
          setState(msg.state);
          return;
        case 'response': {
          setBusy(false);
          setStatus('Ready', 'live');
          updateAssistant(msg.text, false);
          return;
        }
        case 'error': {
          setBusy(false);
          setStatus('Error', 'error');
          const text = 'Error: ' + (msg.message || 'Unknown error');
          updateAssistant(text, true);
          return;
        }
      }
    });

    setStatus('Idle', 'idle');
    updateMetrics('Waiting for first run…');
    vscode.postMessage({ type: 'ready' });
  </script>
</body>
</html>`;
  }
}
