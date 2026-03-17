const fs = require('fs');
const html = fs.readFileSync('D:/rawrxd/gui/ide_chatbot.html', 'utf8');
const lines = html.split('\n');

// Check for invisible/unusual characters around the switch/case area
// The switch starts around line 15096
console.log('=== Finding switch statement ===');
let switchLine = -1;
for (let i = 8658; i < 24209; i++) {
  if (/\bswitch\s*\(\s*cmd\b/.test(lines[i]) || /\bswitch\s*\(\s*args\[0\]/.test(lines[i]) || 
      (/\bswitch\s*\(/.test(lines[i]) && i > 15000 && i < 16000)) {
    console.log(`  Switch at line ${i + 1}: ${lines[i].trim()}`);
    switchLine = i;
  }
}

// Check for all switch statements in the file
console.log('\n=== ALL switch statements ===');
for (let i = 8658; i < 24209; i++) {
  if (/\bswitch\s*\(/.test(lines[i])) {
    console.log(`  Line ${i + 1}: ${lines[i].trim().substring(0, 100)}`);
  }
}

// Check for Unicode/invisible chars near case statements
console.log('\n=== Checking for invisible characters near "case" keywords ===');
for (let i = 8658; i < 24209; i++) {
  if (/\bcase\b/.test(lines[i])) {
    const line = lines[i];
    for (let j = 0; j < line.length; j++) {
      const code = line.charCodeAt(j);
      if (code > 127 || (code < 32 && code !== 9)) {
        console.log(`  Line ${i + 1}, col ${j}: char U+${code.toString(16).padStart(4, '0')} in: ${line.trim().substring(0, 80)}`);
        break;
      }
    }
  }
}

// The real issue: check every line in the file for non-ASCII in the script block
console.log('\n=== Non-ASCII characters in main script block ===');
let nonAsciiCount = 0;
for (let i = 8658; i < 24209; i++) {
  const line = lines[i];
  for (let j = 0; j < line.length; j++) {
    const code = line.charCodeAt(j);
    if (code > 127) {
      nonAsciiCount++;
      if (nonAsciiCount <= 20) {
        console.log(`  Line ${i + 1}, col ${j}: U+${code.toString(16).padStart(4, '0')} '${line.charAt(j)}' in: ${line.trim().substring(0, 80)}`);
      }
    }
  }
}
console.log(`Total non-ASCII chars in script: ${nonAsciiCount}`);

// Check what browser actually gets at the lines where errors are reported
// Error says "line 5641" - but file has changed. Let me check what the browser might see
// as line 5641 from the START of the HTML
console.log('\n=== Browser-reported error lines ===');
console.log(`Line 5641: ${lines[5640] ? lines[5640].substring(0, 120) : 'OUT OF RANGE'}`);
console.log(`Line 5636: ${lines[5635] ? lines[5635].substring(0, 120) : 'OUT OF RANGE'}`);
console.log(`Line 5631: ${lines[5630] ? lines[5630].substring(0, 120) : 'OUT OF RANGE'}`);
console.log(`Line 5627: ${lines[5626] ? lines[5626].substring(0, 120) : 'OUT OF RANGE'}`);
