const fs = require('fs');
const html = fs.readFileSync('D:/rawrxd/gui/ide_chatbot.html', 'utf8');
const lines = html.split('\n');

// Check all script blocks
const blocks = [];
let inScript = false;
let blockStart = -1;
for (let i = 0; i < lines.length; i++) {
  const line = lines[i];
  if (/<script\b/i.test(line) && !inScript) {
    inScript = true;
    blockStart = i;
  } else if (/<\/script>/i.test(line) && inScript) {
    blocks.push({ start: blockStart + 1, end: i + 1, startIdx: blockStart, endIdx: i });
    inScript = false;
  }
  // Handle <script on same line as </script>
  if (/<script\b/i.test(line) && /<\/script>/i.test(line)) {
    // inline script, already handled above
  }
}

console.log(`Found ${blocks.length} script blocks:`);
for (const b of blocks) {
  const jsLines = lines.slice(b.startIdx + 1, b.endIdx);
  const js = jsLines.join('\n').trim();
  if (!js) {
    console.log(`  Block lines ${b.start}-${b.end}: (empty)`);
    continue;
  }
  try {
    new Function(js);
    console.log(`  Block lines ${b.start}-${b.end}: OK (${jsLines.length} lines)`);
  } catch (e) {
    console.log(`  Block lines ${b.start}-${b.end}: SYNTAX ERROR: ${e.message}`);
    // Find the exact line
    for (let i = 0; i < jsLines.length; i++) {
      const chunk = jsLines.slice(0, i + 1).join('\n');
      try { new Function(chunk); } catch (e2) {
        console.log(`    Error at absolute line ${b.startIdx + 2 + i}: ${jsLines[i].substring(0, 120)}`);
        for (let c = Math.max(0, i - 5); c <= Math.min(i + 5, jsLines.length - 1); c++) {
          console.log(`    ${b.startIdx + 2 + c}: ${c === i ? '>>>' : '   '} ${jsLines[c].substring(0, 140)}`);
        }
        break;
      }
    }
  }
}

// Also check for UNCLOSED script blocks (script tags inside other script blocks)
console.log('\nChecking for script tags inside script content...');
for (const b of blocks) {
  const jsLines = lines.slice(b.startIdx + 1, b.endIdx);
  for (let i = 0; i < jsLines.length; i++) {
    if (/<script\b/i.test(jsLines[i]) && !jsLines[i].includes('replace') && !jsLines[i].includes("'<script") && !jsLines[i].includes('"<script') && !jsLines[i].includes('/<script')) {
      console.log(`  WARNING: <script tag inside script block at absolute line ${b.startIdx + 2 + i}: ${jsLines[i].substring(0, 120)}`);
    }
  }
}
