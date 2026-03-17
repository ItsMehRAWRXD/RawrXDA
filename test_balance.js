const fs = require('fs');
const content = fs.readFileSync('D:/rawrxd/gui/ide_chatbot.html', 'utf8');
const lines = content.split('\n');

// Block 4: lines 8663 to 23826 (1-indexed), so 8662 to 23825 (0-indexed)
const scriptLines = lines.slice(8662, 23825);

// Track brace, bracket, paren balance
let braces = 0, brackets = 0, parens = 0;
let inString = false;
let stringChar = '';
let inTemplate = false;
let templateDepth = 0;
let inLineComment = false;
let inBlockComment = false;
let prevChar = '';
let escaping = false;

for (let lineIdx = 0; lineIdx < scriptLines.length; lineIdx++) {
  const line = scriptLines[lineIdx];
  inLineComment = false;

  for (let i = 0; i < line.length; i++) {
    const ch = line[i];
    const next = i + 1 < line.length ? line[i + 1] : '';

    if (escaping) {
      escaping = false;
      prevChar = ch;
      continue;
    }

    if (ch === '\\' && (inString || inTemplate)) {
      escaping = true;
      prevChar = ch;
      continue;
    }

    if (inBlockComment) {
      if (ch === '*' && next === '/') {
        inBlockComment = false;
        i++; // skip /
      }
      prevChar = ch;
      continue;
    }

    if (inLineComment) {
      prevChar = ch;
      continue;
    }

    if (inString) {
      if (ch === stringChar) {
        inString = false;
      }
      prevChar = ch;
      continue;
    }

    if (inTemplate) {
      if (ch === '`') {
        inTemplate = false;
        templateDepth--;
      } else if (ch === '$' && next === '{') {
        templateDepth++;
        i++;
      }
      prevChar = ch;
      continue;
    }

    // Not in any string/comment
    if (ch === '/' && next === '/') {
      inLineComment = true;
      prevChar = ch;
      continue;
    }
    if (ch === '/' && next === '*') {
      inBlockComment = true;
      i++;
      prevChar = ch;
      continue;
    }
    if (ch === "'" || ch === '"') {
      inString = true;
      stringChar = ch;
      prevChar = ch;
      continue;
    }
    if (ch === '`') {
      inTemplate = true;
      templateDepth++;
      prevChar = ch;
      continue;
    }

    if (ch === '{') braces++;
    else if (ch === '}') braces--;
    else if (ch === '[') brackets++;
    else if (ch === ']') brackets--;
    else if (ch === '(') parens++;
    else if (ch === ')') parens--;

    if (braces < 0 || brackets < 0 || parens < 0) {
      console.log('NEGATIVE balance at line', lineIdx + 8663, ':', ch);
      console.log('  braces:', braces, 'brackets:', brackets, 'parens:', parens);
      console.log('  Content:', line.substring(0, 120));
    }

    prevChar = ch;
  }
}

console.log('Final balance:');
console.log('  Braces {}: ', braces);
console.log('  Brackets []:', brackets);
console.log('  Parens ():', parens);
console.log('  In string:', inString);
console.log('  In block comment:', inBlockComment);
console.log('  In template:', inTemplate);

if (braces !== 0 || brackets !== 0 || parens !== 0) {
  console.log('\nIMBALANCE DETECTED! This could cause the script to fail.');
}
