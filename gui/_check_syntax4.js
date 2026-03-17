const fs = require('fs');
const html = fs.readFileSync('D:/rawrxd/gui/ide_chatbot.html', 'utf8');
const lines = html.split('\n');

// Track brace depth through the main script block and find where it goes wrong
const mainLines = lines.slice(8658, 24209);
let braceDepth = 0;
let parenDepth = 0;
let bracketDepth = 0;

// Track depth at start of each function/switch/if
const depthLog = [];

for (let i = 0; i < mainLines.length; i++) {
  const line = mainLines[i];
  const absLine = 8659 + i;

  // Very rough: strip strings and comments
  let stripped = '';
  let inSingleQ = false, inDoubleQ = false, inTemplate = false, inLineComment = false;
  for (let j = 0; j < line.length; j++) {
    const ch = line[j];
    const next = line[j + 1] || '';
    if (inLineComment) break;
    if (!inSingleQ && !inDoubleQ && !inTemplate && ch === '/' && next === '/') break;
    if (!inSingleQ && !inDoubleQ && !inTemplate && ch === "'") { inSingleQ = true; continue; }
    if (inSingleQ && ch === "'" && line[j - 1] !== '\\') { inSingleQ = false; continue; }
    if (!inSingleQ && !inDoubleQ && !inTemplate && ch === '"') { inDoubleQ = true; continue; }
    if (inDoubleQ && ch === '"' && line[j - 1] !== '\\') { inDoubleQ = false; continue; }
    if (!inSingleQ && !inDoubleQ && !inTemplate && ch === '`') { inTemplate = true; continue; }
    if (inTemplate && ch === '`' && line[j - 1] !== '\\') { inTemplate = false; continue; }
    if (!inSingleQ && !inDoubleQ && !inTemplate) stripped += ch;
  }

  const prevDepth = braceDepth;
  for (const ch of stripped) {
    if (ch === '{') braceDepth++;
    if (ch === '}') braceDepth--;
  }

  // Log significant changes
  if (braceDepth !== prevDepth) {
    const delta = braceDepth - prevDepth;
    if (Math.abs(delta) >= 2 || braceDepth <= 1) {
      depthLog.push({ line: absLine, depth: braceDepth, delta, text: line.trim().substring(0, 100) });
    }
  }

  // Log function declarations with their depth
  if (/^\s*function\s+\w+/.test(line) || /^\s*(async\s+)?function\s/.test(line)) {
    depthLog.push({ line: absLine, depth: braceDepth, delta: 0, text: 'FUNC: ' + line.trim().substring(0, 80) });
  }
}

// Print interesting entries - focus on depth 0 and 1 transitions
console.log('=== Brace depth transitions (showing depth 0-2 region) ===');
for (const entry of depthLog) {
  if (entry.depth <= 2 || entry.text.startsWith('FUNC')) {
    console.log(`  Line ${entry.line} [depth=${entry.depth} Δ=${entry.delta}]: ${entry.text}`);
  }
}

// Also specifically find where the extra { is by checking if depth goes to 2 when it should be 1
console.log('\n=== Looking for the extra opening brace ===');
// In a well-structured file at the top scope, after a function closes, depth returns to 0.
// If depth is 1 when it should be 0, there's an extra {.
// Let's find the first place where depth stays at 1 after what should be a top-level function close.

let prevWasZero = true;
for (let i = 0; i < mainLines.length; i++) {
  const line = mainLines[i];
  let stripped = line.replace(/'[^']*'/g, '').replace(/"[^"]*"/g, '').replace(/`[^`]*`/g, '').replace(/\/\/.*$/, '');
  let d = 0;
  for (const ch of stripped) { if (ch === '{') d++; if (ch === '}') d--; }
  braceDepth += 0; // just tracking manually above
}

// More targeted: find the specific area where the problem is
// The file editor functions are duplicated. Let's see what's around line 20618 vs 23222
console.log('\n=== Around first showFileEditorPanel (line 20618) ===');
for (let i = 20613; i <= 20630; i++) {
  console.log(`  ${i}: ${lines[i - 1].substring(0, 120)}`);
}

console.log('\n=== Around second showFileEditorPanel (line 23222) ===');
for (let i = 23217; i <= 23240; i++) {
  console.log(`  ${i}: ${lines[i - 1].substring(0, 120)}`);
}

// Check what's between the two copies - around line 21200-21300 
console.log('\n=== End of first file editor block (around 21164-21250) ===');
for (let i = 21160; i <= 21260; i++) {
  console.log(`  ${i}: ${lines[i - 1].substring(0, 120)}`);
}
