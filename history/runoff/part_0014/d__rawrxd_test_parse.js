const fs = require('fs');
const content = fs.readFileSync('D:/rawrxd/gui/ide_chatbot.html', 'utf8');
const lines = content.split('\n');

// Find script block 4 precisely
let scriptStart = -1;
for (let i = 8650; i < 8680; i++) {
  if (lines[i] && lines[i].trim() === '<script>') {
    scriptStart = i + 1;
    break;
  }
}
let scriptEnd = -1;
for (let i = lines.length - 1; i > scriptStart; i--) {
  if (lines[i] && lines[i].trim() === '</script>') {
    scriptEnd = i;
    break;
  }
}

console.log('Script block 4: lines', scriptStart + 1, 'to', scriptEnd + 1);
const scriptContent = lines.slice(scriptStart, scriptEnd).join('\n');
console.log('Script length:', scriptContent.length, 'chars,', scriptEnd - scriptStart, 'lines');

// Standard parse
try {
  new Function(scriptContent);
  console.log('Standard parse: OK');
} catch (e) {
  console.log('Standard parse ERROR:', e.message);
  const m = e.stack.match(/<anonymous>:(\d+)/);
  if (m) {
    const errLine = parseInt(m[1]);
    console.log('Error at script line:', errLine, '(file line ~' + (scriptStart + errLine) + ')');
    console.log('Content:', lines[scriptStart + errLine - 1]);
  }
}

// Check for addTerminalLine — was it deleted?
let found = false;
for (let i = scriptStart; i < scriptEnd; i++) {
  if (lines[i].match(/function\s+addTerminalLine/)) {
    found = true;
    console.log('addTerminalLine found at line', i + 1);
    break;
  }
}
if (!found) console.log('addTerminalLine NOT FOUND in script block!');

// Check if addTerminalLine is called before being defined (it was deleted in Phase 42)
// This would only matter if it's not a function declaration
for (let i = scriptStart; i < Math.min(scriptStart + 100, scriptEnd); i++) {
  if (lines[i].match(/addTerminalLine/)) {
    console.log('addTerminalLine referenced early at line', i + 1, ':', lines[i].trim().substring(0, 100));
  }
}

// Check for any immediately-invoked code at top level that could throw
// Look for function calls at the top-level (not inside function bodies)
let braceDepth = 0;
let topLevelCalls = [];
for (let i = 0; i < scriptContent.split('\n').length; i++) {
  const line = scriptContent.split('\n')[i];
  for (const ch of line) {
    if (ch === '{') braceDepth++;
    if (ch === '}') braceDepth--;
  }
  if (braceDepth === 0) {
    // Check for function calls at top level (outside any function)
    const callMatch = line.match(/^\s+(\w+)\s*\(/);
    if (callMatch && !line.match(/^\s*(function|var|let|const|\/\/|if|else|for|while|switch|return|class|async|try|catch)/)) {
      topLevelCalls.push({ line: i + scriptStart + 1, name: callMatch[1], content: line.trim().substring(0, 100) });
    }
  }
}
console.log('\nTop-level function calls (potential runtime errors):');
topLevelCalls.forEach(c => console.log('  Line', c.line, ':', c.content));
