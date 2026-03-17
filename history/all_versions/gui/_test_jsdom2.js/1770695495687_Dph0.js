// Improved jsdom test - find what's going wrong
const fs = require('fs');
const { JSDOM } = require('jsdom');

const html = fs.readFileSync('D:/rawrxd/gui/ide_chatbot.html', 'utf8');

// Parse with jsdom, capture console output
const virtualConsole = new (require('jsdom').VirtualConsole)();
const jsdomErrors = [];
const jsdomWarns = [];
const jsdomLogs = [];

virtualConsole.on('error', (...args) => jsdomErrors.push(args.join(' ')));
virtualConsole.on('warn', (...args) => jsdomWarns.push(args.join(' ')));
virtualConsole.on('log', (...args) => jsdomLogs.push(args.join(' ')));
virtualConsole.on('info', (...args) => jsdomLogs.push(args.join(' ')));

try {
  const dom = new JSDOM(html, {
    url: 'file:///D:/rawrxd/gui/ide_chatbot.html',
    runScripts: 'dangerously',
    pretendToBeVisual: true,
    virtualConsole: virtualConsole,
  });

  const window = dom.window;
  const document = window.document;

  // Count script elements the DOM parser found
  const scripts = document.querySelectorAll('script');
  console.log('Script elements in DOM:', scripts.length);
  scripts.forEach((s, i) => {
    const src = s.getAttribute('src');
    const len = s.textContent.length;
    console.log(`  Script ${i + 1}: src=${src || 'inline'}, length=${len} chars, type=${s.type || 'default'}`);
  });

  // Check function availability
  const fns = ['switchTerminalMode', 'showExtensionsPanel', 'connectBackend',
    'sendToBackend', 'addMessage', 'escHtml', 'logDebug',
    'feOpenFile', 'feSaveFile', 'State'];
  console.log('\nFunction/var availability:');
  fns.forEach(fn => {
    const type = typeof window[fn];
    console.log(`  ${fn}: ${type}${type === 'undefined' ? ' ✗' : ' ✓'}`);
  });

  // Show errors
  if (jsdomErrors.length > 0) {
    console.log('\n=== JSDOM ERRORS (' + jsdomErrors.length + ') ===');
    jsdomErrors.slice(0, 20).forEach(e => console.log('  ERR:', e.substring(0, 200)));
  }
  if (jsdomWarns.length > 0) {
    console.log('\n=== JSDOM WARNINGS (' + jsdomWarns.length + ') ===');
    jsdomWarns.slice(0, 10).forEach(w => console.log('  WARN:', w.substring(0, 200)));
  }

  dom.window.close();
} catch (e) {
  console.error('JSDOM PARSE ERROR:', e.message);
  console.error(e.stack);
}
