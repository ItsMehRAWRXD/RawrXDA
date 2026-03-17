const fs = require('fs');
const html = fs.readFileSync('D:/rawrxd/gui/ide_chatbot.html', 'utf8');
const lines = html.split('\n');

// Find the main big script block (starts after line 8000)
let mainStart = -1, mainEnd = -1;
for (let i = 0; i < lines.length; i++) {
  if (i > 8000 && /<script\b/i.test(lines[i]) && mainStart < 0) mainStart = i;
  if (i > 8000 && /<\/script>/i.test(lines[i])) mainEnd = i;
}
console.log(`Main script block: lines ${mainStart + 1} to ${mainEnd + 1}`);

// Extract JS content
const jsLines = lines.slice(mainStart + 1, mainEnd);
const js = jsLines.join('\n');

// Try to parse
try {
  new Function(js);
  console.log('Main script block: PARSES OK');
} catch (e) {
  console.log('SYNTAX ERROR:', e.message);
  
  // Try to narrow down by binary search
  function tryParse(code) {
    try { new Function(code); return true; } catch(e) { return false; }
  }
  
  // Find the approximate error line by checking progressively
  let lo = 0, hi = jsLines.length;
  // Check first half, second half, etc.
  const mid = Math.floor(hi / 2);
  
  // Instead, find the line by trying to parse increasing chunks
  let lastGood = 0;
  for (let step = 1000; step >= 100; step = Math.floor(step / 2)) {
    for (let i = lastGood; i < jsLines.length; i += step) {
      const chunk = jsLines.slice(0, i).join('\n');
      if (!tryParse(chunk)) {
        console.log(`Error somewhere between line ${lastGood} and ${i} of script block (absolute: ${mainStart + 1 + lastGood} to ${mainStart + 1 + i})`);
        lastGood = Math.max(0, i - step);
        break;
      }
      lastGood = i;
    }
  }
  
  // Final fine-grained search
  for (let i = lastGood; i < Math.min(lastGood + 200, jsLines.length); i++) {
    const chunk = jsLines.slice(0, i + 1).join('\n');
    if (!tryParse(chunk)) {
      console.log(`ERROR at script-relative line ${i + 1}, absolute line ${mainStart + 2 + i}`);
      console.log(`Line content: ${jsLines[i]}`);
      // Show context
      for (let c = Math.max(0, i - 5); c <= Math.min(i + 5, jsLines.length - 1); c++) {
        console.log(`  ${mainStart + 2 + c}: ${c === i ? '>>>' : '   '} ${jsLines[c]}`);
      }
      break;
    }
  }
}
