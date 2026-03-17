const fs = require('fs');
const acorn = require('acorn');

const content = fs.readFileSync('D:/rawrxd/dist/gui/ide_chatbot.html', 'utf8');
const lines = content.split('\n');

// Find script block 4 in dist
let start = -1, end = -1;
for (let i = 8650; i < 8670; i++) {
  if (lines[i] && lines[i].trim() === '<script>') { start = i + 1; break; }
}
for (let i = lines.length - 1; i > start; i--) {
  if (lines[i] && lines[i].trim() === '</script>') { end = i; break; }
}
console.log('Dist Block 4: lines', start + 1, 'to', end + 1, '(' + (end - start) + ' lines)');

const scriptContent = lines.slice(start, end).join('\n');
console.log('Size:', scriptContent.length, 'bytes');

try {
  acorn.parse(scriptContent, {
    ecmaVersion: 2022,
    sourceType: 'script',
    locations: true,
  });
  console.log('Dist parse: OK');
} catch (e) {
  console.log('DIST PARSE ERROR:', e.message);
  if (e.loc) {
    const errLine = e.loc.line;
    const fileLine = start + errLine;
    console.log('At dist file line:', fileLine);
    console.log('Content:', lines[fileLine - 1]);
    // Context
    for (let i = Math.max(0, errLine - 4); i <= Math.min(end - start - 1, errLine + 3); i++) {
      const prefix = i === errLine - 1 ? '>>>' : '   ';
      console.log(prefix, 'L' + (start + 1 + i) + ':', lines[start + i].substring(0, 120));
    }
  }
}

// Also check the source version with the SAME method
const srcContent = fs.readFileSync('D:/rawrxd/gui/ide_chatbot.html', 'utf8');
const srcLines = srcContent.split('\n');
let srcStart = -1, srcEnd = -1;
for (let i = 8650; i < 8680; i++) {
  if (srcLines[i] && srcLines[i].trim() === '<script>') { srcStart = i + 1; break; }
}
for (let i = srcLines.length - 1; i > srcStart; i--) {
  if (srcLines[i] && srcLines[i].trim() === '</script>') { srcEnd = i; break; }
}
console.log('\nSource Block 4: lines', srcStart + 1, 'to', srcEnd + 1, '(' + (srcEnd - srcStart) + ' lines)');
const srcScript = srcLines.slice(srcStart, srcEnd).join('\n');

try {
  acorn.parse(srcScript, {
    ecmaVersion: 2022,
    sourceType: 'script',
    locations: true,
  });
  console.log('Source parse: OK');
} catch (e) {
  console.log('SOURCE PARSE ERROR:', e.message);
  if (e.loc) {
    const errLine = e.loc.line;
    const fileLine = srcStart + errLine;
    console.log('At source file line:', fileLine);
    console.log('Content:', srcLines[fileLine - 1]);
    for (let i = Math.max(0, errLine - 4); i <= Math.min(srcEnd - srcStart - 1, errLine + 3); i++) {
      const prefix = i === errLine - 1 ? '>>>' : '   ';
      console.log(prefix, 'L' + (srcStart + 1 + i) + ':', srcLines[srcStart + i].substring(0, 120));
    }
  }
}
