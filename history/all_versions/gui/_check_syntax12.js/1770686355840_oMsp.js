const fs = require('fs');
const html = fs.readFileSync('D:/rawrxd/gui/ide_chatbot.html', 'utf8');
const lines = html.split('\n');

// Find ALL lines with <script
console.log('=== All lines containing <script ===');
for (let i = 0; i < lines.length; i++) {
  if (/<script\b/i.test(lines[i])) {
    console.log(`  Line ${i + 1}: ${lines[i].trim().substring(0, 150)}`);
  }
}

console.log('\n=== All lines containing </script ===');
for (let i = 0; i < lines.length; i++) {
  if (/<\/script>/i.test(lines[i])) {
    console.log(`  Line ${i + 1}: ${lines[i].trim().substring(0, 150)}`);
  }
}
