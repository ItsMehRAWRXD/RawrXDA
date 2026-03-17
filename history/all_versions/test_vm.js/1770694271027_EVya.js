const fs = require('fs');
const vm = require('vm');

const content = fs.readFileSync('D:/rawrxd/gui/ide_chatbot.html', 'utf8');
const lines = content.split('\n');
const scriptContent = lines.slice(8662, 23825).join('\n');

// Create a browser-like context
const context = vm.createContext({
  window: {
    location: { protocol: 'file:', origin: 'null', hostname: '' },
    addEventListener: () => {},
    matchMedia: () => ({ matches: false, addListener: () => {} }),
    getComputedStyle: () => new Proxy({}, { get: () => '' }),
    innerWidth: 1920, innerHeight: 1080,
    open: () => {},
    scrollTo: () => {},
    performance: { now: () => Date.now() },
    navigator: { userAgent: 'test', clipboard: { writeText: () => Promise.resolve() } },
    crypto: { getRandomValues: (a) => a },
  },
  document: {
    getElementById: () => null,
    querySelector: () => null,
    querySelectorAll: () => [],
    createElement: (tag) => ({
      style: {}, classList: { add:()=>{}, remove:()=>{}, toggle:()=>{}, contains:()=>false },
      addEventListener: ()=>{}, appendChild: ()=>{}, setAttribute: ()=>{}, getAttribute: ()=>null,
      innerHTML: '', textContent: '', remove: ()=>{}, tagName: tag,
      closest: () => null, querySelector: () => null, querySelectorAll: () => [],
      insertBefore: ()=>{}, replaceChild: ()=>{}, removeChild: ()=>{},
      firstChild: null, lastChild: null, parentNode: null,
    }),
    createTextNode: () => ({}),
    body: { appendChild:()=>{}, classList:{add:()=>{},remove:()=>{}} },
    head: { appendChild:()=>{} },
    documentElement: { style:{}, classList:{add:()=>{},remove:()=>{}} },
    addEventListener: ()=>{},
    createDocumentFragment: () => ({ appendChild:()=>{} }),
    title: 'test', styleSheets: [],
  },
  localStorage: { getItem:()=>null, setItem:()=>{}, removeItem:()=>{} },
  sessionStorage: { getItem:()=>null, setItem:()=>{}, removeItem:()=>{} },
  fetch: () => Promise.resolve({ ok:true, json:()=>Promise.resolve({}), text:()=>Promise.resolve('') }),
  DOMPurify: { sanitize: s => s || '' },
  hljs: { highlightElement:()=>{}, getLanguage:()=>null, listLanguages:()=>[] },
  navigator: { userAgent:'test', clipboard:{ writeText:()=>Promise.resolve() } },
  MutationObserver: class { observe(){} disconnect(){} },
  IntersectionObserver: class { observe(){} disconnect(){} },
  ResizeObserver: class { observe(){} disconnect(){} },
  AbortController: class { constructor(){ this.signal = { aborted: false } } abort(){} },
  AbortSignal: { timeout: () => ({}) },
  FileReader: class { readAsText(){} addEventListener(){} },
  XMLHttpRequest: class { open(){} send(){} setRequestHeader(){} addEventListener(){} },
  URL: URL,
  HTMLElement: class {},
  CustomEvent: class { constructor(t,o){ this.type=t; this.detail=o?.detail } },
  Event: class { constructor(t){ this.type=t } },
  location: { protocol:'file:', origin:'null', hostname:'' },
  DOMParser: class { parseFromString() { return { body: { firstChild: null } } } },
  alert: ()=>{}, confirm: ()=>true, prompt: ()=>'',
  console: console,
  setTimeout: setTimeout, setInterval: setInterval,
  clearTimeout: clearTimeout, clearInterval: clearInterval,
  requestAnimationFrame: ()=>{}, cancelAnimationFrame: ()=>{},
  performance: { now: ()=>Date.now() },
  atob: (s) => Buffer.from(s, 'base64').toString(),
  btoa: (s) => Buffer.from(s).toString('base64'),
  Image: class { set src(v){} },
  Blob: class { constructor(){} },
  FormData: class { append(){} },
  TextEncoder: TextEncoder,
  TextDecoder: TextDecoder,
  structuredClone: structuredClone,
  Proxy: Proxy,
  Map: Map, Set: Set, WeakMap: WeakMap, WeakSet: WeakSet,
  Promise: Promise,
  JSON: JSON,
  Array: Array, Object: Object, String: String, Number: Number,
  Math: Math, Date: Date, RegExp: RegExp, Error: Error,
  TypeError: TypeError, ReferenceError: ReferenceError, SyntaxError: SyntaxError,
  parseInt: parseInt, parseFloat: parseFloat, isNaN: isNaN, isFinite: isFinite,
  encodeURIComponent: encodeURIComponent, decodeURIComponent: decodeURIComponent,
  encodeURI: encodeURI, decodeURI: decodeURI,
  Uint8Array: Uint8Array, Int32Array: Int32Array, Float64Array: Float64Array,
  ArrayBuffer: ArrayBuffer, DataView: DataView,
  Symbol: Symbol,
  Intl: Intl,
  undefined: undefined,
  NaN: NaN, Infinity: Infinity,
});

console.log('Evaluating script block in VM context...');
try {
  const script = new vm.Script(scriptContent, { filename: 'ide_chatbot.html' });
  script.runInContext(context);
  console.log('SUCCESS! Script evaluated without errors.');
  
  // Check if key functions are defined
  const fns = ['switchTerminalMode', 'handleTerminal', 'showExtensionsPanel', 'checkServerStatus', 'showDebug'];
  for (const fn of fns) {
    console.log(`  ${fn}: ${typeof context[fn]}`);
  }
} catch(e) {
  console.log('RUNTIME ERROR:', e.constructor.name + ':', e.message);
  const stack = e.stack.split('\n');
  // Find the line in ide_chatbot.html
  for (const line of stack) {
    if (line.includes('ide_chatbot.html')) {
      console.log('  At:', line.trim());
      const m = line.match(/:(\d+):\d+/);
      if (m) {
        const scriptLine = parseInt(m[1]);
        const fileLine = 8662 + scriptLine;
        console.log('  File line:', fileLine);
        console.log('  Content:', lines[fileLine - 1]?.trim()?.substring(0, 120));
        // Show context
        for (let i = Math.max(0, fileLine - 4); i <= fileLine + 2; i++) {
          const prefix = i === fileLine - 1 ? '>>>' : '   ';
          console.log('  ' + prefix + ' L' + (i+1) + ':', lines[i]?.substring(0, 120));
        }
      }
    }
  }
}
