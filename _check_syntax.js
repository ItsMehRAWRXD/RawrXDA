const fs = require('fs');
const html = fs.readFileSync('D:/rawrxd/gui/ide_chatbot.html', 'utf8');
const lines = html.split('\n');

// Find all switch/case structure issues
let inMainScript = false;
let braceStack = [];
let switchStarts = [];

for (let i = 0; i < lines.length; i++) {
  const t = lines[i].trim();
  if (t === '<script>' && i > 8000) inMainScript = true;
  if (t === '</script>' && inMainScript) { inMainScript = false; continue; }
  if (!inMainScript) continue;

  for (let c = 0; c < t.length; c++) {
    if (t[c] === '{') braceStack.push(i + 1);
    if (t[c] === '}') braceStack.pop();
  }

  if (/^\s*switch\s*\(/.test(lines[i])) {
    switchStarts.push({ line: i + 1, brace: braceStack.length });
  }

  const caseMatch = /^\s*case\s+/.test(lines[i]);
  const defaultMatch = /^\s*default\s*:/.test(lines[i]);
  if (caseMatch || defaultMatch) {
    const lastSwitch = switchStarts[switchStarts.length - 1];
    if (!lastSwitch || braceStack.length <= lastSwitch.brace) {
      console.log('*** OUT-OF-SWITCH case at line ' + (i + 1) + ' (depth=' + braceStack.length + '): ' + t.substring(0, 80));
    }
  }
}

// Also check for duplicate function definitions
const funcDefs = {};
const funcRe = /^\s*(?:async\s+)?function\s+(\w+)\s*\(/;
for (let i = 0; i < lines.length; i++) {
  const m = funcRe.exec(lines[i]);
  if (m) {
    const name = m[1];
    if (funcDefs[name]) {
      console.log('DUPLICATE function: ' + name + ' at lines ' + funcDefs[name] + ' and ' + (i + 1));
    }
    funcDefs[name] = i + 1;
  }
}

console.log('Analysis complete. Final brace depth: ' + braceStack.length);
