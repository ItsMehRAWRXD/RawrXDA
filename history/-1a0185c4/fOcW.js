const fs = require('fs');
const path = require('path');

const root = path.resolve(__dirname, '../../amazonq-private-ide-extension/amazonq-ide');

function walk(dir) {
  const entries = fs.readdirSync(dir, { withFileTypes: true });
  for (const e of entries) {
    const full = path.join(dir, e.name);
    if (e.isDirectory()) walk(full);
    else if (e.isFile() && full.endsWith('.md')) processFile(full);
  }
}

function processFile(filePath) {
  let original = fs.readFileSync(filePath, 'utf8');
  let s = original;

  // Ensure blank line before headings
  s = s.replace(/([^\n\r])\r?\n(#{1,6} )/g, '$1\n\n$2');

  // Ensure blank line after headings
  s = s.replace(/(#{1,6}[^\n\r]*?)\r?\n([^\n\r#`\-\*\d])/g, '$1\n\n$2');

  // Ensure blank line before fenced code blocks
  s = s.replace(/([^\n\r])\r?\n(```)/g, '$1\n\n$2');

  // Ensure fenced code blocks have a language (use 'text' if missing)
  s = s.replace(/^```\s*$/gm, '```text');

  // Ensure blank line after fenced code blocks
  s = s.replace(/```(?:[^\n\r]*)\r?\n([^\n\r])/g, '```\n\n$1');

  // Ensure blank line before lists (unordered and ordered)
  s = s.replace(/([^\n\r])\r?\n(\s*[-\*+]\s+)/g, '$1\n\n$2');
  s = s.replace(/([^\n\r])\r?\n(\s*\d+\.\s+)/g, '$1\n\n$2');

  if (s !== original) {
    fs.copyFileSync(filePath, filePath + '.bak');
    fs.writeFileSync(filePath, s, 'utf8');
    console.log('Updated', filePath);
  }
}

walk(root);
console.log('Done.');
