// Test: Check with resources: 'usable' (which tries to load external scripts)
const fs = require('fs');
const { JSDOM, VirtualConsole } = require('jsdom');

const html = fs.readFileSync('D:/rawrxd/gui/ide_chatbot.html', 'utf8');

const virtualConsole = new VirtualConsole();
const errors = [];
virtualConsole.on('error', (...args) => errors.push(args.join(' ')));
virtualConsole.on('warn', () => {});
virtualConsole.on('jsdomError', (e) => errors.push('jsdomError: ' + e.message));

// Test 1: WITHOUT resources (like test 2 - this works)
console.log('=== TEST 1: runScripts only (no resource loading) ===');
const dom1 = new JSDOM(html, {
  url: 'file:///D:/rawrxd/gui/ide_chatbot.html',
  runScripts: 'dangerously',
  pretendToBeVisual: true,
  virtualConsole: virtualConsole,
});
console.log('switchTerminalMode:', typeof dom1.window.switchTerminalMode);
dom1.window.close();

// Test 2: WITH resources: 'usable' (tries to download CDN scripts)
console.log('\n=== TEST 2: runScripts + resources: usable ===');
errors.length = 0;
const dom2 = new JSDOM(html, {
  url: 'file:///D:/rawrxd/gui/ide_chatbot.html',
  runScripts: 'dangerously',
  resources: 'usable',
  pretendToBeVisual: true,
  virtualConsole: virtualConsole,
});

// Wait for external resources to load/fail
setTimeout(() => {
  console.log('switchTerminalMode:', typeof dom2.window.switchTerminalMode);
  if (errors.length > 0) {
    console.log('\nErrors:', errors.length);
    errors.slice(0, 10).forEach(e => console.log(' ', e.substring(0, 200)));
  }
  dom2.window.close();
  
  // Test 3: Check script execution ORDER - does the CDN failure block inline scripts?
  console.log('\n=== TEST 3: Direct script execution test ===');
  const testHtml = `<!DOCTYPE html><html><head>
<script src="https://cdnjs.cloudflare.com/ajax/nonexistent.js" onerror="console.warn('CDN failed')"></script>
<script>
function myFunc() { return 42; }
window._testVar = 'hello';
</script>
</head><body></body></html>`;
  
  const dom3 = new JSDOM(testHtml, {
    url: 'file:///test.html',
    runScripts: 'dangerously',
    pretendToBeVisual: true,
  });
  console.log('myFunc:', typeof dom3.window.myFunc);
  console.log('_testVar:', dom3.window._testVar);
  dom3.window.close();
}, 5000);
