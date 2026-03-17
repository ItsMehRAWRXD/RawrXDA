const fs = require('fs');
const text = fs.readFileSync('D:/rawrxd/gui/ide_chatbot.html', 'utf8');
const lines = text.split('\n');

// Check ALL HTML body content (not inside script blocks) for <script tags
const ranges = [
  [197, 6267, 'HTML body section 1 (line 197-6267)'],
  [6328, 8661, 'HTML body section 2 (line 6328-8661)'],
];

for (const [start, end, label] of ranges) {
  const sectionLines = lines.slice(start - 1, end); // 0-indexed
  const section = sectionLines.join('\n');
  const scriptOpens = [...section.matchAll(/<script[\s>]/gi)];
  const scriptCloses = [...section.matchAll(/<\/script/gi)];

  console.log(label + ':');
  if (scriptOpens.length > 0) {
    console.log('  FOUND ' + scriptOpens.length + ' <script opens!');
    scriptOpens.forEach(m => {
      const lineOff = section.substring(0, m.index).split('\n').length;
      const ctx = section.substring(m.index, Math.min(section.length, m.index + 80)).replace(/\n/g, ' ');
      console.log('    Line ~' + (start + lineOff - 1) + ': ' + ctx);
    });
  } else {
    console.log('  No <script opens');
  }
  if (scriptCloses.length > 0) {
    console.log('  FOUND ' + scriptCloses.length + ' </script closes!');
  } else {
    console.log('  No </script closes');
  }
}

// Also check inside attribute values for <script
console.log('\n--- Checking for <script in attribute values ---');
const htmlBody = lines.slice(196, 8661).join('\n'); // All HTML between script blocks
const attrPattern = /(?:onclick|onerror|onload|href|src|title|value|placeholder|content)="[^"]*<script[^"]*"/gi;
const attrMatches = [...htmlBody.matchAll(attrPattern)];
if (attrMatches.length > 0) {
  console.log('FOUND <script in attributes:');
  attrMatches.forEach(m => console.log('  ', m[0].substring(0, 100)));
} else {
  console.log('No <script found in attributes');
}
