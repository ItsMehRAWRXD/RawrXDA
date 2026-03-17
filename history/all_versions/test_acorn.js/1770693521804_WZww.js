const fs = require('fs');
const acorn = require('acorn');

const content = fs.readFileSync('D:/rawrxd/gui/ide_chatbot.html', 'utf8');
const lines = content.split('\n');

// Extract block 4
const scriptContent = lines.slice(8662, 23825).join('\n');

console.log('Parsing with acorn (ecmaVersion 2022, sourceType script)...');
try {
  acorn.parse(scriptContent, {
    ecmaVersion: 2022,
    sourceType: 'script',
    locations: true,
    allowAwaitOutsideFunction: false,
  });
  console.log('Parse OK!');
} catch(e) {
  console.log('PARSE ERROR:', e.message);
  if (e.loc) {
    const errScriptLine = e.loc.line;
    const errFileLine = 8662 + errScriptLine;
    console.log('At script line:', errScriptLine, '(file line:', errFileLine, ')');
    console.log('Column:', e.loc.column);
    const errContent = lines[errFileLine - 1];
    console.log('Content:', errContent);
    // Show surrounding lines
    for (let i = Math.max(0, errScriptLine - 3); i <= Math.min(scriptContent.split('\n').length - 1, errScriptLine + 2); i++) {
      const prefix = i === errScriptLine - 1 ? '>>> ' : '    ';
      console.log(prefix + 'L' + (8663 + i) + ': ' + scriptContent.split('\n')[i]);
    }
  }
}
