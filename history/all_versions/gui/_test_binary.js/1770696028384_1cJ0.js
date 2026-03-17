const fs = require('fs');
const buf = fs.readFileSync('D:/rawrxd/gui/ide_chatbot.html');
const text = buf.toString('utf8');
const lines = text.split('\n');

// Get byte offset of start of script content (after <script>\n on line 8662)
let startOff = 0;
for (let i = 0; i < 8662; i++) startOff += Buffer.byteLength(lines[i] + '\n');

// Get byte offset of </script> on line 23826 (line 23825 in 0-indexed)
let endOff = 0;
for (let i = 0; i < 23825; i++) endOff += Buffer.byteLength(lines[i] + '\n');

console.log('Script content: bytes', startOff, 'to', endOff, '= ' + (endOff - startOff) + ' bytes');

const scriptBuf = buf.slice(startOff, endOff);

// Search for </scr in the script content
const target = Buffer.from('</scr');
let pos = 0;
const hits = [];
while (pos < scriptBuf.length - 5) {
  const idx = scriptBuf.indexOf(target, pos);
  if (idx === -1) break;
  const after = scriptBuf.slice(idx, Math.min(scriptBuf.length, idx + 12)).toString('utf8').toLowerCase();
  if (after.startsWith('</script')) {
    const lineNum = scriptBuf.slice(0, idx).toString('utf8').split('\n').length + 8662;
    const ctx = scriptBuf.slice(Math.max(0, idx - 30), Math.min(scriptBuf.length, idx + 20)).toString('utf8').replace(/\n/g, '\\n');
    hits.push({ line: lineNum, ctx });
  }
  pos = idx + 1;
}

console.log(hits.length === 0 ? 'CONFIRMED: Zero </script inside script block.' : 'FOUND ' + hits.length);
hits.forEach(h => console.log('  Line ~' + h.line + ': ' + h.ctx));

// Check for null bytes
let nullCount = 0;
for (let i = 0; i < scriptBuf.length; i++) {
  if (scriptBuf[i] === 0) nullCount++;
}
console.log(nullCount === 0 ? 'No null bytes.' : 'WARNING: ' + nullCount + ' null bytes found!');

// Check for BOM or weird leading chars
console.log('First bytes (hex):', scriptBuf.slice(0, 20).toString('hex'));
console.log('First chars:', JSON.stringify(scriptBuf.slice(0, 50).toString('utf8')));
