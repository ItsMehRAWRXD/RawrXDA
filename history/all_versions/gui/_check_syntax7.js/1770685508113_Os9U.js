const fs = require('fs');
const html = fs.readFileSync('D:/rawrxd/gui/ide_chatbot.html', 'utf8');
const lines = html.split('\n');

// Check the specific area - the second file editor feOnEditorKeydown function 
// starts at 23603. The "Mobile Sidebar Toggle" comment is at 23200.
// Let me check if there's a problem in the transition area.

// Check depth tracking backward from line 23200 to find where the extra { is
console.log('=== Lines 23195-23220 (transition region) ===');
for (let i = 23194; i < 23220; i++) {
  console.log(`${i + 1}: ${lines[i]}`);
}

// Now check: what's the brace depth at line 23199 coming from the top?
let depth = 0;
const scriptStart = 8658;
let lastZeroLine = scriptStart;
let firstProblemLine = -1;

for (let i = scriptStart; i < 23210; i++) {
  const line = lines[i];
  let stripped = line.replace(/\/\/.*$/, '');
  stripped = stripped.replace(/'(?:[^'\\]|\\.)*'/g, '""').replace(/"(?:[^"\\]|\\.)*"/g, '""').replace(/`(?:[^`\\]|\\.)*`/g, '""');

  const prev = depth;
  for (const ch of stripped) {
    if (ch === '{') depth++;
    if (ch === '}') depth--;
  }

  if (depth === 0) lastZeroLine = i + 1;
  if (depth < 0 && firstProblemLine === -1) {
    firstProblemLine = i + 1;
    console.log(`NEGATIVE DEPTH at line ${i + 1}: ${line.trim().substring(0, 100)}`);
  }
}

console.log(`\nDepth at line 23210: ${depth}`);
console.log(`Last zero-depth line before 23210: ${lastZeroLine}`);

// So the problem is: the FIRST copy of file editor (20607-21234) is complete and balanced.
// Then there are more functions. Then the SECOND copy starts at ~23200. 
// But the overall script brace depth is 1 at the end.
// This means somewhere a function is opened but not closed, OR there's an extra {.

// Let me trace from line 23195 onward carefully
console.log('\n=== Depth from 23195 to 23610 ===');
let d = depth;
for (let i = 23194; i < 23610; i++) {
  const line = lines[i];
  let stripped = line.replace(/\/\/.*$/, '');
  stripped = stripped.replace(/'(?:[^'\\]|\\.)*'/g, '""').replace(/"(?:[^"\\]|\\.)*"/g, '""').replace(/`(?:[^`\\]|\\.)*`/g, '""');

  const prev = d;
  for (const ch of stripped) {
    if (ch === '{') d++;
    if (ch === '}') d--;
  }

  if (d !== prev) {
    console.log(`Line ${i + 1} [${prev}→${d}]: ${line.trim().substring(0, 120)}`);
  }
}
console.log(`Depth at end of scan: ${d}`);
