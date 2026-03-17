const fs = require('fs');
const html = fs.readFileSync('D:/rawrxd/gui/ide_chatbot.html', 'utf8');
const lines = html.split('\n');

// Binary search: find where the cumulative brace depth first stays at 1
// Start from line 8658 (script start), step through
const scriptStart = 8658;
let depth = 0;

// Find the LAST line where depth returns to 0, before it permanently stays >= 1
let lastZeroLine = scriptStart;
let firstPermanentNonZero = -1;

for (let i = scriptStart; i < 23200; i++) {
  const line = lines[i];
  let stripped = line.replace(/\/\/.*$/, '');
  stripped = stripped.replace(/'(?:[^'\\]|\\.)*'/g, '""').replace(/"(?:[^"\\]|\\.)*"/g, '""').replace(/`(?:[^`\\]|\\.)*`/g, '""');
  
  for (const ch of stripped) {
    if (ch === '{') depth++;
    if (ch === '}') depth--;
  }
  
  if (depth === 0) {
    lastZeroLine = i + 1;
  }
}

console.log(`Last zero-depth line (before 23200): ${lastZeroLine}`);
console.log(`Depth at line 23200: ${depth}`);

// Now show lines around the last zero-depth line
console.log(`\n=== Lines around ${lastZeroLine} ===`);
for (let i = lastZeroLine - 5; i < lastZeroLine + 25; i++) {
  // Recalculate depth up to this point
  let d = 0;
  for (let j = scriptStart; j <= i; j++) {
    let s = lines[j].replace(/\/\/.*$/, '');
    s = s.replace(/'(?:[^'\\]|\\.)*'/g, '""').replace(/"(?:[^"\\]|\\.)*"/g, '""').replace(/`(?:[^`\\]|\\.)*`/g, '""');
    for (const ch of s) { if (ch === '{') d++; if (ch === '}') d--; }
  }
  const marker = d === 0 ? '  [=0]' : d === 1 ? ' [=1!]' : `  [=${d}]`;
  console.log(`${i + 1}${marker}: ${lines[i].substring(0, 120)}`);
}
