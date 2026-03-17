// Definitive test: simulate the browser executing the script block
// and check for ANY error during evaluation
const fs = require('fs');
const vm = require('vm');

const html = fs.readFileSync('D:/rawrxd/gui/ide_chatbot.html', 'utf8');
const lines = html.split('\n');

// Extract script content (lines 8663-23825, which is between <script> and </script>)
const scriptContent = lines.slice(8662, 23825).join('\n');
console.log('Script content:', scriptContent.length, 'chars');

// Create a browser-like global context
const context = vm.createContext({
  // DOM stubs
  document: {
    getElementById: (id) => ({
      textContent: '', style: {}, value: '', innerHTML: '',
      classList: { add: () => { }, remove: () => { }, contains: () => false, toggle: () => { } },
      addEventListener: () => { }, removeEventListener: () => { },
      setAttribute: () => { }, getAttribute: () => null, removeAttribute: () => { },
      appendChild: () => { }, removeChild: () => { }, insertBefore: () => { },
      querySelector: () => null, querySelectorAll: () => ({ forEach: () => { }, length: 0 }),
      focus: () => { }, blur: () => { }, click: () => { }, remove: () => { },
      children: { length: 0 }, checked: false, disabled: false,
      offsetWidth: 800, offsetHeight: 600, scrollTop: 0, scrollHeight: 0,
      firstChild: null, parentNode: null, nextSibling: null,
      getBoundingClientRect: () => ({ top: 0, left: 0, width: 800, height: 600, right: 800, bottom: 600 }),
    }),
    querySelector: () => null,
    querySelectorAll: () => ({ forEach: () => { }, length: 0 }),
    createElement: (tag) => ({
      tagName: tag, style: {}, className: '', innerHTML: '', textContent: '',
      classList: { add: () => { }, remove: () => { } },
      setAttribute: () => { }, getAttribute: () => null,
      appendChild: () => { }, addEventListener: () => { },
    }),
    createDocumentFragment: () => ({
      appendChild: () => { }, children: { length: 0 },
    }),
    addEventListener: () => { },
    createTextNode: (t) => ({ textContent: t }),
    body: {
      classList: { add: () => { }, remove: () => { } },
      style: {},
      appendChild: () => { },
    },
    title: '',
  },
  window: {
    location: { protocol: 'http:', origin: 'http://localhost:11435', href: 'http://localhost:11435/gui' },
    innerWidth: 1920, innerHeight: 1080,
    addEventListener: () => { },
    removeEventListener: () => { },
    matchMedia: () => ({ matches: false, addEventListener: () => { } }),
    requestAnimationFrame: (cb) => setTimeout(cb, 16),
    getComputedStyle: () => ({}),
    open: () => { },
  },
  navigator: { userAgent: 'test', clipboard: { writeText: () => Promise.resolve() } },
  location: { protocol: 'http:', origin: 'http://localhost:11435', href: 'http://localhost:11435/gui' },
  console: {
    log: (...a) => { }, warn: (...a) => { }, error: (...a) => { console.log('[script error]', ...a); },
    info: (...a) => { }, debug: (...a) => { },
  },
  fetch: async () => ({ ok: false, json: async () => ({}), text: async () => '', status: 500 }),
  setTimeout: (fn, ms) => 0,
  setInterval: (fn, ms) => 0,
  clearTimeout: () => { },
  clearInterval: () => { },
  localStorage: {
    getItem: () => null, setItem: () => { }, removeItem: () => { },
  },
  DOMPurify: { sanitize: (t) => t },
  hljs: { highlightElement: () => { }, getLanguage: () => null, highlight: () => ({ value: '' }), listLanguages: () => [] },
  AbortController: class { constructor() { this.signal = {} } abort() { } },
  AbortSignal: { timeout: () => ({}) },
  FileReader: class { readAsText() { } addEventListener() { } },
  Blob: class { constructor() { } },
  URL: { createObjectURL: () => 'blob:', revokeObjectURL: () => { } },
  FormData: class { },
  Headers: class { },
  Request: class { },
  Response: class { },
  ReadableStream: class { },
  TextDecoder: class { decode() { return ''; } },
  TextEncoder: class { encode() { return new Uint8Array(); } },
  btoa: (s) => Buffer.from(s).toString('base64'),
  atob: (s) => Buffer.from(s, 'base64').toString(),
  performance: { now: () => Date.now() },
  alert: () => { },
  confirm: () => true,
  prompt: () => '',
  getSelection: () => null,
  MutationObserver: class { observe() { } disconnect() { } },
  ResizeObserver: class { observe() { } disconnect() { } },
  IntersectionObserver: class { observe() { } disconnect() { } },
  requestIdleCallback: (fn) => fn(),
  structuredClone: (o) => JSON.parse(JSON.stringify(o)),
});

// Make 'window' self-referential
context.window.window = context.window;
context.self = context.window;

try {
  const script = new vm.Script(scriptContent, { filename: 'ide_chatbot_block4.js' });
  script.runInContext(context);
  console.log('\n=== SCRIPT EXECUTED WITHOUT ERRORS ===\n');

  // Check which functions are defined in context
  const fns = [
    'switchTerminalMode', 'showExtensionsPanel', 'connectBackend',
    'sendToBackend', 'addMessage', 'escHtml', 'logDebug',
    'feOpenFile', 'feSaveFile', 'showBrowserPanel', 'formatMessage',
    'showFileEditorPanel', 'showHybridPanel', 'showReplayPanel',
    'clearTerminal', 'toggleTerminal', 'handleTerminal',
  ];

  let defined = 0, missing = 0;
  for (const fn of fns) {
    const t = typeof context[fn];
    if (t === 'function') { defined++; }
    else { missing++; console.log('  MISSING: ' + fn + ' (type: ' + t + ')'); }
  }
  console.log('Functions: ' + defined + ' defined, ' + missing + ' missing');

} catch (e) {
  console.error('\n=== SCRIPT EXECUTION ERROR ===');
  console.error('Type:', e.constructor.name);
  console.error('Message:', e.message);
  console.error('Stack:', e.stack);

  // Try to find the exact line
  if (e.stack) {
    const lineMatch = e.stack.match(/ide_chatbot_block4\.js:(\d+)/);
    if (lineMatch) {
      const errorLine = parseInt(lineMatch[1]);
      const actualLine = errorLine + 8662;
      console.log('\nError at script line ' + errorLine + ' (file line ~' + actualLine + ')');
      console.log('Context:');
      const slines = scriptContent.split('\n');
      for (let i = Math.max(0, errorLine - 3); i < Math.min(slines.length, errorLine + 3); i++) {
        const marker = i === errorLine - 1 ? '>>>' : '   ';
        console.log(marker + ' L' + (i + 1) + ': ' + slines[i]);
      }
    }
  }
}
