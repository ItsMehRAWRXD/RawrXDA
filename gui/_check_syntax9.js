const fs = require('fs');
const html = fs.readFileSync('D:/rawrxd/gui/ide_chatbot.html', 'utf8');
const lines = html.split('\n');

// Check for </script or <script inside the main script block that could confuse HTML parser
const scriptStart = 8658;
const scriptEnd = 24209;

console.log('=== Checking for script-breaking patterns inside main <script> block ===');
for (let i = scriptStart; i < scriptEnd; i++) {
  const line = lines[i];
  // The HTML parser splits on </script - even inside strings
  if (/<\/script/i.test(line) && i !== scriptEnd - 1) {
    console.log(`  LINE ${i + 1} [CLOSING SCRIPT TAG]: ${line.trim().substring(0, 150)}`);
  }
  // Check for <script that's not in a comment or regex
  if (/<script[\s>]/i.test(line) && !/\/\//.test(line.substring(0, line.search(/<script/i)))) {
    console.log(`  LINE ${i + 1} [OPENING SCRIPT TAG]: ${line.trim().substring(0, 150)}`);
  }
}

// Check for the specific error browser reports - "Unexpected token 'case'"
// This happens when a 'case' keyword appears where an expression or statement is expected
// This can happen if switch body gets cut off by </script being found mid-stream
console.log('\n=== All </script patterns ===');
for (let i = scriptStart; i < scriptEnd; i++) {
  if (/<\/script/i.test(lines[i])) {
    console.log(`  Line ${i + 1}: ${lines[i].trim().substring(0, 150)}`);
  }
}
