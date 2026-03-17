const fs = require('fs');
const html = fs.readFileSync('D:/rawrxd/gui/ide_chatbot.html', 'utf8');
const lines = html.split('\n');

// Check 1: Find all occurrences of 'case' keyword
console.log('=== All "case " occurrences outside script blocks ===');
// First, identify which lines are inside script blocks
const inScript = new Array(lines.length).fill(false);
let inside = false;
for (let i = 0; i < lines.length; i++) {
  if (/<script\b/i.test(lines[i])) inside = true;
  if (inside) inScript[i] = true;
  if (/<\/script>/i.test(lines[i])) inside = false;
}

// Find 'case' outside scripts (shouldn't happen, but check)
for (let i = 0; i < lines.length; i++) {
  if (!inScript[i] && /\bcase\b/.test(lines[i]) && !lines[i].includes('<!--') && !lines[i].includes('lowercase')) {
    console.log(`  Line ${i + 1}: ${lines[i].trim().substring(0, 120)}`);
  }
}

// Check 2: Look for HTML comments (<!-- -->) inside script blocks
console.log('\n=== HTML comments inside script blocks ===');
inside = false;
for (let i = 0; i < lines.length; i++) {
  if (/<script\b/i.test(lines[i])) inside = true;
  if (inside && /<!--/.test(lines[i]) && !lines[i].includes("'<!--") && !lines[i].includes('"<!--') && !lines[i].includes('//')) {
    console.log(`  Line ${i + 1}: ${lines[i].trim().substring(0, 120)}`);
  }
  if (/<\/script>/i.test(lines[i])) inside = false;
}

// Check 3: Look for unmatched braces in the main script block
console.log('\n=== Brace counting in main script (8658-24209) ===');
let braceDepth = 0;
let minDepth = 0;
let lastZeroLine = 8658;
const mainLines = lines.slice(8658, 24209);
for (let i = 0; i < mainLines.length; i++) {
  const line = mainLines[i];
  // Skip strings (rough approximation)
  const stripped = line.replace(/'[^']*'/g, '').replace(/"[^"]*"/g, '').replace(/\/\/.*$/, '').replace(/`[^`]*`/g, '');
  for (const ch of stripped) {
    if (ch === '{') braceDepth++;
    if (ch === '}') braceDepth--;
  }
  if (braceDepth < minDepth) {
    minDepth = braceDepth;
    console.log(`  Negative brace depth ${braceDepth} at line ${8659 + i}: ${line.trim().substring(0, 100)}`);
  }
  if (braceDepth === 0 && i > 0) lastZeroLine = 8659 + i;
}
console.log(`Final brace depth: ${braceDepth}`);
console.log(`Last line where depth was 0: ${lastZeroLine}`);

// Check 4: Count switch/case structure
console.log('\n=== Switch/case analysis ===');
let switchCount = 0;
let caseOutsideSwitch = [];
let switchDepth = 0;
for (let i = 0; i < mainLines.length; i++) {
  const line = mainLines[i].trim();
  if (/\bswitch\s*\(/.test(line)) {
    switchCount++;
    switchDepth++;
  }
  if (/\bcase\s+/.test(line) && switchDepth === 0) {
    caseOutsideSwitch.push(8659 + i);
  }
}
console.log(`Switch statements: ${switchCount}`);
console.log(`Cases potentially outside switch: ${caseOutsideSwitch.length}`);
if (caseOutsideSwitch.length > 0) {
  console.log('Lines:', caseOutsideSwitch.slice(0, 20));
  for (const ln of caseOutsideSwitch.slice(0, 5)) {
    console.log(`  Line ${ln}: ${lines[ln - 1].trim().substring(0, 120)}`);
  }
}

// Check 5: Check for duplicate function declarations
console.log('\n=== Duplicate function declarations ===');
const funcDecls = {};
for (let i = 0; i < mainLines.length; i++) {
  const m = mainLines[i].match(/^\s*function\s+(\w+)\s*\(/);
  if (m) {
    const name = m[1];
    if (!funcDecls[name]) funcDecls[name] = [];
    funcDecls[name].push(8659 + i);
  }
}
for (const [name, locs] of Object.entries(funcDecls)) {
  if (locs.length > 1) {
    console.log(`  ${name}: declared at lines ${locs.join(', ')}`);
  }
}
