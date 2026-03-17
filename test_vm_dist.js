const fs = require('fs');
const vm = require('vm');

// Test the DIST version
const content = fs.readFileSync('D:/rawrxd/dist/gui/ide_chatbot.html', 'utf8');
const lines = content.split('\n');
const scriptContent = lines.slice(8658, 24175).join('\n');

const context = vm.createContext({
  window: { location: { protocol: 'file:', origin: 'null', hostname: '' }, addEventListener: () => { }, matchMedia: () => ({ matches: false, addListener: () => { } }), getComputedStyle: () => new Proxy({}, { get: () => '' }), innerWidth: 1920, innerHeight: 1080, open: () => { }, scrollTo: () => { }, performance: { now: () => Date.now() }, navigator: { userAgent: 'test', clipboard: { writeText: () => Promise.resolve() } }, crypto: { getRandomValues: a => a } },
  document: { getElementById: () => null, querySelector: () => null, querySelectorAll: () => [], createElement: () => ({ style: {}, classList: { add: () => { }, remove: () => { }, toggle: () => { }, contains: () => false }, addEventListener: () => { }, appendChild: () => { }, setAttribute: () => { }, getAttribute: () => null, innerHTML: '', textContent: '', remove: () => { }, closest: () => null, querySelector: () => null, querySelectorAll: () => [], insertBefore: () => { }, replaceChild: () => { }, removeChild: () => { }, firstChild: null, lastChild: null, parentNode: null }), createTextNode: () => ({}), body: { appendChild: () => { }, classList: { add: () => { }, remove: () => { } } }, head: { appendChild: () => { } }, documentElement: { style: {}, classList: { add: () => { }, remove: () => { } } }, addEventListener: () => { }, createDocumentFragment: () => ({ appendChild: () => { } }), title: 'test', styleSheets: [] },
  localStorage: { getItem: () => null, setItem: () => { }, removeItem: () => { } }, sessionStorage: { getItem: () => null, setItem: () => { }, removeItem: () => { } },
  fetch: () => Promise.resolve({ ok: true, json: () => Promise.resolve({}), text: () => Promise.resolve('') }),
  DOMPurify: { sanitize: s => s || '' }, hljs: { highlightElement: () => { }, getLanguage: () => null, listLanguages: () => [] },
  navigator: { userAgent: 'test', clipboard: { writeText: () => Promise.resolve() } },
  MutationObserver: class { observe() { } disconnect() { } }, IntersectionObserver: class { observe() { } disconnect() { } }, ResizeObserver: class { observe() { } disconnect() { } },
  AbortController: class { constructor() { this.signal = { aborted: false } } abort() { } }, AbortSignal: { timeout: () => ({}) },
  FileReader: class { readAsText() { } addEventListener() { } }, XMLHttpRequest: class { open() { } send() { } setRequestHeader() { } addEventListener() { } },
  URL, HTMLElement: class { }, CustomEvent: class { constructor(t, o) { this.type = t; this.detail = o?.detail } }, Event: class { constructor(t) { this.type = t } },
  location: { protocol: 'file:', origin: 'null', hostname: '' },
  DOMParser: class { parseFromString() { return { body: { firstChild: null } } } },
  alert: () => { }, confirm: () => true, prompt: () => '', console,
  setTimeout, setInterval, clearTimeout, clearInterval,
  requestAnimationFrame: () => { }, cancelAnimationFrame: () => { },
  performance: { now: () => Date.now() },
  atob: s => Buffer.from(s, 'base64').toString(), btoa: s => Buffer.from(s).toString('base64'),
  Image: class { set src(v) { } }, Blob: class { constructor() { } }, FormData: class { append() { } },
  TextEncoder, TextDecoder, structuredClone, Proxy,
  Map, Set, WeakMap, WeakSet, Promise, JSON, Array, Object, String, Number, Math, Date, RegExp, Error,
  TypeError, ReferenceError, SyntaxError,
  parseInt, parseFloat, isNaN, isFinite,
  encodeURIComponent, decodeURIComponent, encodeURI, decodeURI,
  Uint8Array, Int32Array, Float64Array, ArrayBuffer, DataView, Symbol, Intl,
  undefined, NaN, Infinity,
});

console.log('Testing DIST version...');
try {
  const script = new vm.Script(scriptContent, { filename: 'ide_chatbot_dist.html' });
  script.runInContext(context);
  console.log('DIST: SUCCESS!');
  const fns = ['switchTerminalMode', 'handleTerminal', 'showExtensionsPanel', 'checkServerStatus', 'showDebug', 'addTerminalLine', 'showFileEditorPanel'];
  for (const fn of fns) {
    console.log('  ' + fn + ': ' + typeof context[fn]);
  }
} catch (e) {
  console.log('DIST RUNTIME ERROR:', e.constructor.name + ':', e.message);
  const stack = e.stack.split('\n');
  for (const line of stack) {
    if (line.includes('ide_chatbot')) {
      console.log('  At:', line.trim());
      const m = line.match(/:(\d+):\d+/);
      if (m) {
        const scriptLine = parseInt(m[1]);
        const fileLine = 8658 + scriptLine;
        console.log('  Dist file line:', fileLine);
        console.log('  Content:', lines[fileLine - 1]?.trim()?.substring(0, 120));
      }
    }
  }
}
