const fs = require('fs');
const html = fs.readFileSync('D:/rawrxd/gui/ide_chatbot.html', 'utf8');

// Actually parse it the way a browser would: extract each script block
// based on HTML parser rules and try to parse each one
const scriptRegex = /<script\b[^>]*>([\s\S]*?)<\/script>/gi;
let match;
let blockNum = 0;

while ((match = scriptRegex.exec(html)) !== null) {
  blockNum++;
  const js = match[1];
  const startOffset = match.index;
  const lineNum = html.substring(0, startOffset).split('\n').length;
  
  if (!js.trim()) {
    console.log(`Block ${blockNum} (line ~${lineNum}): empty`);
    continue;
  }
  
  try {
    // Use acorn-like parsing with strict checks
    new Function('"use strict";\n' + js);
    console.log(`Block ${blockNum} (line ~${lineNum}, ${js.split('\n').length} lines): OK (strict mode)`);
  } catch (e) {
    console.log(`Block ${blockNum} (line ~${lineNum}): STRICT MODE ERROR: ${e.message}`);
    // Try without strict
    try {
      new Function(js);
      console.log(`  → OK in non-strict mode (sloppy)`);
    } catch (e2) {
      console.log(`  → ALSO FAILS in non-strict: ${e2.message}`);
      
      // Find exact line
      const jsLines = js.split('\n');
      for (let i = 0; i < jsLines.length; i++) {
        try {
          new Function(jsLines.slice(0, i + 1).join('\n'));
        } catch (e3) {
          console.log(`  → Error at JS line ${i + 1} (HTML line ~${lineNum + i}): ${jsLines[i].substring(0, 100)}`);
          break;
        }
      }
    }
  }
}

// Also check: does the entire HTML have any issues with nested script tags?
// Count script opens vs closes
const opens = (html.match(/<script\b/gi) || []).length;
const closes = (html.match(/<\/script>/gi) || []).length;
console.log(`\nScript tag opens: ${opens}, closes: ${closes}`);

// Check for potential encoding issue: BOM?
if (html.charCodeAt(0) === 0xFEFF) {
  console.log('WARNING: File has BOM character');
}

// Check for null bytes
const nullBytes = [];
for (let i = 0; i < Math.min(html.length, 100000); i++) {
  if (html.charCodeAt(i) === 0) nullBytes.push(i);
}
if (nullBytes.length > 0) {
  console.log(`WARNING: Found ${nullBytes.length} null bytes at positions: ${nullBytes.slice(0, 5)}`);
}
