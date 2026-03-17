const http = require('http');
const fs = require('fs');

// Fetch the HTML and check if the browser would see all the JS functions
const html = fs.readFileSync('D:/rawrxd/gui/ide_chatbot.html', 'utf8');

// Simulate HTML parser: find where script blocks start and end
// According to HTML spec, inside <script>, parser looks for </script (case-insensitive)
// Let's manually walk through the HTML like a browser would

let pos = 0;
let scriptBlocks = [];
let inScript = false;
let scriptStart = -1;

while (pos < html.length) {
  if (!inScript) {
    // Look for <script
    const idx = html.indexOf('<script', pos);
    if (idx === -1) break;
    // Find the closing >
    const closeTag = html.indexOf('>', idx);
    if (closeTag === -1) break;
    inScript = true;
    scriptStart = closeTag + 1;
    pos = closeTag + 1;
  } else {
    // In script data state: look for </script> (case-insensitive)
    // HTML spec: must match </script followed by space, >, or /
    let searchPos = pos;
    let found = false;
    while (searchPos < html.length) {
      const idx = html.toLowerCase().indexOf('</script', searchPos);
      if (idx === -1) break;
      // Check what follows
      const afterIdx = idx + 8; // length of '</script'
      if (afterIdx < html.length) {
        const nextChar = html[afterIdx];
        if (nextChar === '>' || nextChar === ' ' || nextChar === '/' || nextChar === '\t' || nextChar === '\n' || nextChar === '\r') {
          // This is a real closing tag
          const endTag = html.indexOf('>', afterIdx);
          scriptBlocks.push({
            start: scriptStart,
            end: idx,
            startLine: html.substring(0, scriptStart).split('\n').length,
            endLine: html.substring(0, idx).split('\n').length
          });
          inScript = false;
          pos = endTag + 1;
          found = true;
          break;
        }
      }
      searchPos = idx + 1;
    }
    if (!found) {
      // No closing tag found - rest of document is script
      scriptBlocks.push({
        start: scriptStart,
        end: html.length,
        startLine: html.substring(0, scriptStart).split('\n').length,
        endLine: html.split('\n').length
      });
      break;
    }
  }
}

console.log(`Found ${scriptBlocks.length} script blocks (browser-style parsing):`);
for (const block of scriptBlocks) {
  const content = html.substring(block.start, block.end);
  const lines = content.split('\n').length;
  console.log(`  Lines ${block.startLine}-${block.endLine} (${lines} lines)`);

  // Check if it parses
  if (content.trim()) {
    try {
      new Function(content);
      console.log(`    → Parses OK`);
    } catch (e) {
      console.log(`    → SYNTAX ERROR: ${e.message}`);
      // Find exact line
      const jsLines = content.split('\n');
      for (let i = 0; i < jsLines.length; i++) {
        try { new Function(jsLines.slice(0, i + 1).join('\n')); }
        catch (e2) {
          console.log(`    → Error at script-relative line ${i + 1} (abs ~${block.startLine + i})`);
          console.log(`    → Content: ${jsLines[i].substring(0, 120)}`);
          // Context
          for (let c = Math.max(0, i - 3); c <= Math.min(i + 3, jsLines.length - 1); c++) {
            console.log(`      ${block.startLine + c}${c === i ? ' >>>' : '    '}: ${jsLines[c].substring(0, 120)}`);
          }
          break;
        }
      }
    }
  } else {
    console.log(`    → (empty)`);
  }
}

// Also check: does browser-style parsing find </script inside the main block?
const mainBlock = scriptBlocks[scriptBlocks.length - 1];
if (mainBlock) {
  const mainContent = html.substring(mainBlock.start, mainBlock.end);
  const closingIdx = mainContent.toLowerCase().indexOf('</script');
  if (closingIdx >= 0) {
    const lineOfClose = mainContent.substring(0, closingIdx).split('\n').length;
    console.log(`\nWARNING: Found </script inside main block at relative line ${lineOfClose} (abs ~${mainBlock.startLine + lineOfClose - 1})`);
    const context = mainContent.substring(Math.max(0, closingIdx - 50), closingIdx + 30);
    console.log(`Context: ...${context}...`);
  } else {
    console.log('\nNo stray </script found inside main block - OK');
  }
}
