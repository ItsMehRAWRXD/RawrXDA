// Test: Does jsdom (a real HTML parser) correctly parse the script blocks?
// Does it make the functions available in global scope?
const fs = require('fs');
const { JSDOM } = require('jsdom');

const html = fs.readFileSync('D:/rawrxd/gui/ide_chatbot.html', 'utf8');

console.log('File size:', html.length, 'chars');
console.log('Parsing with jsdom (real HTML parser)...');

// Count script tags the HTML parser finds
const scriptTagMatches = html.match(/<script[^>]*>/gi);
console.log('Script open tags in source:', scriptTagMatches ? scriptTagMatches.length : 0);

const closeScriptMatches = html.match(/<\/script>/gi);
console.log('Script close tags in source:', closeScriptMatches ? closeScriptMatches.length : 0);

// Now try jsdom - but with resources disabled to avoid network calls
// runScripts: 'dangerously' tells jsdom to actually execute scripts
const dom = new JSDOM(html, {
  url: 'file:///D:/rawrxd/gui/ide_chatbot.html',
  runScripts: 'dangerously',
  resources: 'usable',
  pretendToBeVisual: true,
  beforeParse(window) {
    // Stub out fetch/AbortSignal etc.
    window.fetch = async () => ({ ok: false, json: async () => ({}) });
    window.AbortSignal = { timeout: () => ({}) };
    window.AbortController = class { constructor() { this.signal = {} } abort() { } };
  }
});

const window = dom.window;

// Check if key functions are defined
const functionsToCheck = [
  'switchTerminalMode',
  'showExtensionsPanel',
  'showFileEditorPanel',
  'connectBackend',
  'sendToBackend',
  'addMessage',
  'escHtml',
  'formatMessage',
  'logDebug',
  'feOpenFile',
  'feSaveFile',
  'feNewFile',
  'showHybridPanel',
  'showReplayPanel',
  'showBrowserPanel',
];

console.log('\n--- Function availability check ---');
let defined = 0, missing = 0;
for (const fn of functionsToCheck) {
  const exists = typeof window[fn] === 'function';
  console.log(`  ${fn}: ${exists ? '✓ DEFINED' : '✗ NOT DEFINED'}`);
  if (exists) defined++; else missing++;
}
console.log(`\nResult: ${defined} defined, ${missing} missing`);

// Check for errors
const errors = [];
dom.window.addEventListener('error', (e) => errors.push(e.message));

// Check console errors
const origError = dom.window.console.error;
const consoleErrors = [];
dom.window.console.error = (...args) => {
  consoleErrors.push(args.join(' '));
};

setTimeout(() => {
  if (consoleErrors.length > 0) {
    console.log('\nConsole errors:');
    consoleErrors.forEach(e => console.log('  ', e));
  }
  if (errors.length > 0) {
    console.log('\nWindow errors:');
    errors.forEach(e => console.log('  ', e));
  }
  dom.window.close();
}, 2000);
