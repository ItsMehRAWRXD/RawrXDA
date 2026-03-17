const fs = require('fs');
const html = fs.readFileSync('D:/rawrxd/gui/ide_chatbot.html', 'utf8');
const lines = html.split('\n');

// Track depth from line 23600 to end of script (should be around 24209)
let depth = 0;
const start = 23600;
const end = 24210;

for (let i = start; i < Math.min(end, lines.length); i++) {
  const line = lines[i];
  let stripped = line.replace(/\/\/.*$/, '');
  stripped = stripped.replace(/'(?:[^'\\]|\\.)*'/g, '""').replace(/"(?:[^"\\]|\\.)*"/g, '""').replace(/`(?:[^`\\]|\\.)*`/g, '""');
  
  const prev = depth;
  for (const ch of stripped) {
    if (ch === '{') depth++;
    if (ch === '}') depth--;
  }
  
  if (depth !== prev) {
    console.log(`Line ${i + 1} [${prev}→${depth}]: ${line.substring(0, 120)}`);
  }
}
console.log(`\nFinal depth from line ${start + 1}: ${depth}`);

// Now also verify: if we remove lines 23200-24100 (the second copy),
// does the file parse cleanly?
console.log('\n=== Testing removal of second file editor copy ===');
const mainStart = 8658;
const mainEnd = lines.findIndex((l, i) => i > 23000 && /<\/script>/i.test(l));
console.log(`Script ends at line ${mainEnd + 1}`);

// Test: remove lines 23200-24100 (second file editor)
const jsLines = lines.slice(mainStart + 1, mainEnd);
console.log(`Total script lines: ${jsLines.length}`);

// Find where second file editor starts and ends
const secondFeStart = lines.findIndex((l, i) => i > 23195 && l.includes('AGENTIC FILE EDITOR'));
const secondFeEnd = lines.findIndex((l, i) => i > secondFeStart + 10 && l.trim() === '/* ── Mobile Sidebar Toggle ── */' && i > 23500);
console.log(`Second file editor comment at line ${secondFeStart + 1}`);

// Find where the Mobile Sidebar section is after the second FE
for (let i = 23600; i < Math.min(24100, lines.length); i++) {
  if (lines[i].includes('Mobile Sidebar') || lines[i].includes('DOMContentLoaded') || lines[i].includes('</script>')) {
    console.log(`Marker at line ${i + 1}: ${lines[i].trim().substring(0, 80)}`);
  }
}
