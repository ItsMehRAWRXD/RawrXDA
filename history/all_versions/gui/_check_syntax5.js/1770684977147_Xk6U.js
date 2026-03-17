const fs = require('fs');
const html = fs.readFileSync('D:/rawrxd/gui/ide_chatbot.html', 'utf8');
const lines = html.split('\n');

// Focus on the duplicate region. Track brace depth from script start to find where it diverges.
const mainStart = 8658; // 0-indexed line where main <script> starts
const mainEnd = 24209;  // 0-indexed line where </script> is

let braceDepth = 0;
let problemFound = false;

for (let i = mainStart; i < mainEnd; i++) {
  const line = lines[i];
  // Rough strip of strings and comments
  let stripped = line.replace(/\/\/.*$/, '');
  // Very crude string removal
  stripped = stripped.replace(/'(?:[^'\\]|\\.)*'/g, '""').replace(/"(?:[^"\\]|\\.)*"/g, '""').replace(/`(?:[^`\\]|\\.)*`/g, '""');
  
  const prevDepth = braceDepth;
  for (const ch of stripped) {
    if (ch === '{') braceDepth++;
    if (ch === '}') braceDepth--;
  }
  
  // We want to find where a top-level function (depth 0→1) opens but never properly closes
  // Let's just print EVERY line where depth transitions through 0 after line 20000
  if (i >= 19000 && (prevDepth === 0 || braceDepth === 0 || braceDepth < 0)) {
    console.log(`Line ${i + 1} [${prevDepth}→${braceDepth}]: ${line.trim().substring(0, 120)}`);
  }
}

console.log(`\nFinal depth: ${braceDepth}`);
