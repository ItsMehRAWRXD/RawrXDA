const fs = require('fs');
const text = fs.readFileSync('D:/rawrxd/gui/ide_chatbot.html', 'utf8');
const lines = text.split('\n');

// The main script block starts at line 8662 (<script>) 
// Content starts at line 8663, ends at line 23825
// </script> is at line 23826

// Extract just the script content (between <script> and </script>)
const contentLines = lines.slice(8662, 23825); // 0-indexed: 8662 = line 8663 to 23824 = line 23825
const content = contentLines.join('\n');
console.log('Script content:', content.length, 'chars,', contentLines.length, 'lines');

// Search for </script (any case) in the content
const re = /<\/script/gi;
let m;
const found = [];
while ((m = re.exec(content)) !== null) {
  const lineOffset = content.substring(0, m.index).split('\n').length;
  const lineNum = 8663 + lineOffset - 1;
  const ctx = content.substring(Math.max(0, m.index - 40), Math.min(content.length, m.index + 30)).replace(/\n/g, '\\n');
  found.push({ line: lineNum, context: ctx });
}

if (found.length === 0) {
  console.log('✓ No </script found inside main script block content');
} else {
  console.log('✗ FOUND', found.length, '</script matches inside script block:');
  found.forEach(f => console.log('  Line ~' + f.line + ': ...' + f.context + '...'));
}

// Also check for HTML parser "script data end tag" triggers:
// The spec says: in script data state, after </ the parser tries to match "script"
// So we need: </ immediately followed by script (case insensitive)
const re2 = /<\/\s*script/gi;
let m2;
const found2 = [];
while ((m2 = re2.exec(content)) !== null) {
  const lineOffset = content.substring(0, m2.index).split('\n').length;
  const lineNum = 8663 + lineOffset - 1;
  const ctx = content.substring(Math.max(0, m2.index - 40), Math.min(content.length, m2.index + 30)).replace(/\n/g, '\\n');
  found2.push({ line: lineNum, context: ctx });
}

if (found2.length === 0) {
  console.log('✓ No </ script patterns found');
} else {
  console.log('Found', found2.length, '</ script patterns:');
  found2.forEach(f => console.log('  Line ~' + f.line + ': ...' + f.context + '...'));
}
