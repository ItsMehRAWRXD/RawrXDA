// hyper-ide-main.js
// Boots Monaco editor, WASMFS shim, and llama.cpp worker for Beaconism Hyper-IDE (WASM Edition)

require.config({
  paths: {
    vs: 'https://cdnjs.cloudflare.com/ajax/libs/monaco-editor/0.46.0/min/vs'
  }
});

const logOverlay = (msg, color = '#9f9') => {
  const o = document.getElementById('overlay');
  if (!o) return;
  const e = document.createElement('div');
  e.style.color = color;
  e.textContent = msg;
  o.appendChild(e);
  o.scrollTop = o.scrollHeight;
};

['log', 'warn', 'error'].forEach(k => {
  const orig = console[k].bind(console);
  console[k] = (...args) => {
    const msg = args.map(x => typeof x === 'object' ? JSON.stringify(x) : String(x)).join(' ');
    const color = k === 'error' ? '#f66' : k === 'warn' ? '#fd6' : '#9f9';
    logOverlay(`[console:${k}] ${msg}`, color);
    orig(...args);
  };
});

const WASMFS = {
  files: {
    '/home/main.js': '// WASMFS File\nconsole.log("Hello from WASMFS!");\n',
    '/models/llama.gguf': '<<<llama.cpp GGUF model placeholder>>>',
    '/workspace/README.md': '# Workspace linked via FSA.'
  },
  async mount(path, type) {
    logOverlay(`[FS] Mounted ${path} as ${type}`, '#ff0');
  },
  async read(path) {
    logOverlay(`[FS] READ ${path}`, '#ff0');
    return WASMFS.files[path] || '';
  },
  async write(path, content) {
    logOverlay(`[FS] WRITE ${path} (${content.length} bytes)`, '#ff0');
    WASMFS.files[path] = content;
  }
};

let monacoEditor = null;
let currentFileName = '/home/main.js';
let currentLanguage = 'javascript';
let llamaWorker = null;

function detectLanguage(name) {
  const ext = (name.split('.').pop() || '').toLowerCase();
  const map = {
    js: 'javascript', jsx: 'javascript',
    ts: 'typescript', tsx: 'typescript',
    py: 'python', pyw: 'python',
    cpp: 'cpp', cxx: 'cpp', cc: 'cpp', c: 'c', h: 'c', hpp: 'cpp',
    css: 'css', scss: 'css', sass: 'css',
    html: 'html', htm: 'html',
    md: 'markdown', markdown: 'markdown',
    json: 'json', xml: 'xml', yml: 'yaml', yaml: 'yaml',
    sh: 'shell', bash: 'shell', zsh: 'shell',
    rs: 'rust', go: 'go', php: 'php', rb: 'ruby'
  };
  return map[ext] || 'plaintext';
}

function setStatus(state, file = currentFileName, lang = currentLanguage) {
  const s = document.getElementById('status-state');
  const f = document.getElementById('status-file');
  if (s) s.textContent = state;
  if (f) f.textContent = `${file} (${lang})`;
}

async function initOS() {
  await WASMFS.mount('/', 'WASI-ROOT');
  await WASMFS.mount('/home', 'OPFS');
  await WASMFS.mount('/workspace', 'FSA_BRIDGE');
}

function initIDE() {
  return new Promise(resolve => {
    require(['vs/editor/editor.main'], () => {
      const initial = WASMFS.files['/home/main.js'] || '';
      monacoEditor = monaco.editor.create(document.getElementById('code-editor'), {
        value: initial,
        language: 'javascript',
        theme: 'vs-dark',
        automaticLayout: true,
        minimap: { enabled: false }
      });
      resolve(monacoEditor);
    });
  });
}

async function openLocalFile() {
  if (!window.showOpenFilePicker) {
    alert('File System Access API not supported in this browser.');
    return;
  }
  try {
    const [handle] = await window.showOpenFilePicker();
    const file = await handle.getFile();
    const text = await file.text();
    currentFileName = file.name;
    currentLanguage = detectLanguage(file.name);
    if (monacoEditor) {
      monacoEditor.setValue(text);
      monaco.editor.setModelLanguage(monacoEditor.getModel(), currentLanguage);
    }
    setStatus('Editing', currentFileName, currentLanguage);
  } catch (e) {
    if (e.name !== 'AbortError') console.error(e);
  }
}

async function saveLocalFile() {
  if (!monacoEditor) return;
  const content = monacoEditor.getValue();
  if (!window.showSaveFilePicker) {
    await WASMFS.write(currentFileName, content);
    setStatus('Saved (WASMFS)', currentFileName, currentLanguage);
    return;
  }
  try {
    const handle = await window.showSaveFilePicker({ suggestedName: currentFileName || 'untitled.txt' });
    const writable = await handle.createWritable();
    await writable.write(content);
    await writable.close();
    setStatus('Saved', currentFileName, currentLanguage);
  } catch (e) {
    if (e.name !== 'AbortError') console.error(e);
  }
}

window.openLocalFile = openLocalFile;
window.saveLocalFile = saveLocalFile;

async function chooseWorkspace() {
  const s = document.getElementById('status-state');
  if (!window.showDirectoryPicker) {
    s.textContent = 'Workspace: OPFS fallback';
    return;
  }
  try {
    const dir = await window.showDirectoryPicker();
    s.textContent = `Workspace: ${dir.name}`;
  } catch (e) {
    if (e.name !== 'AbortError') console.error(e);
  }
}
window.chooseWorkspace = chooseWorkspace;

function initLlamaWorker() {
  llamaWorker = new Worker('./llama-worker.js', { type: 'module' });
  const messages = document.getElementById('chat-messages');

  const appendChat = (html) => {
    if (!messages) return;
    const div = document.createElement('div');
    div.innerHTML = html;
    messages.appendChild(div);
    messages.scrollTop = messages.scrollHeight;
  };

  llamaWorker.onmessage = (ev) => {
    const m = ev.data;
    if (m.type === 'token') {
      appendChat(m.text || '');
    } else if (m.type === 'done') {
      appendChat('<br/>');
    } else if (m.type === 'err') {
      console.error('[llama]', m.msg);
    } else if (m.type === 'ok') {
      console.log('[llama]', m.msg);
    } else if (m.type === 'ready') {
      console.log('[llama] runtime ready');
    } else if (m.type === 'loaded') {
      console.log('[llama] model loaded');
    } else if (m.type === 'sab-ready') {
      console.log('[llama] SAB ready');
    }
  };

  llamaWorker.postMessage({
    cmd: 'init',
    urls: { wasm: './llama.wasm', runtime: './llama.runtime.js' }
  });

  // simple GGUF open button via prompt command !load
  const input = document.getElementById('chat-input');
  const sendBtn = document.getElementById('chat-send-btn');
  const termPanel = document.getElementById('terminal-panel');
  const toggleBtn = document.getElementById('toggle-terminal-btn');
  let visible = false;

  const toggleTerm = () => {
    visible = !visible;
    termPanel.style.display = visible ? 'flex' : 'none';
    toggleBtn.textContent = visible ? 'Hide AI Terminal' : 'Show AI Terminal';
    if (monacoEditor) monacoEditor.layout();
  };

  toggleBtn.addEventListener('click', toggleTerm);

  const handleSend = async () => {
    const text = input.value.trim();
    if (!text) return;
    appendChat(`&gt; ${text}`);
    input.value = '';
    if (text === '!load') {
      if (!window.showOpenFilePicker) {
        appendChat('<br/><span style="color:#f66;">FSA not supported for GGUF load.</span>');
        return;
      }
      const [fh] = await window.showOpenFilePicker({
        types: [{ description: 'GGUF', accept: { 'application/octet-stream': ['.gguf'] } }]
      });
      llamaWorker.postMessage({ cmd: 'load', fileHandle: fh });
      return;
    }
    llamaWorker.postMessage({ cmd: 'prompt', text, opts: { max_tokens: 256, temperature: 0.7, top_p: 0.9 } });
  };

  sendBtn.addEventListener('click', handleSend);
  input.addEventListener('keydown', (e) => {
    if (e.key === 'Enter') handleSend();
  });
}

(async function boot() {
  await initOS();
  await initIDE();
  initLlamaWorker();
  setStatus('Ready', currentFileName, currentLanguage);
})();
