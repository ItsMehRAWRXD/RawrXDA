const fs = require('fs');

const targetPath = process.argv[2] || 'c:\\Users\\HiH8e\\OneDrive\\Desktop\\script-block-18.js';
const script = fs.readFileSync(targetPath, 'utf8');
const lines = script.split('\n');

const braceStack = [];
let errors = [];

for (let i = 0; i < lines.length; i++) {
    const line = lines[i];
    
    // Simple cleaning (won't handle all cases but good enough)
    const cleanLine = line
        .replace(/\/\/.*$/, '')
        .replace(/\/\*[\s\S]*?\*\//g, '')
        .replace(/'[^']*'/g, "''")
        .replace(/"[^"]*"/g, '""')
        .replace(/`[^`]*`/g, '``');
    
    for (let j = 0; j < cleanLine.length; j++) {
        const char = cleanLine[j];
        
        if (char === '{') {
            braceStack.push({ line: i + 1, col: j + 1, text: line.substring(0, 80).trim() });
        } else if (char === '}') {
            if (braceStack.length === 0) {
                errors.push({
                    line: i + 1,
                    col: j + 1,
                    text: line.substring(0, 80).trim(),
                    error: 'Closing brace without matching opening brace'
                });
            } else {
                braceStack.pop();
            }
        }
    }
}

console.log(`=== BRACE MATCHING ANALYSIS (${targetPath}) ===\n`);

if (errors.length > 0) {
    console.log('❌ ERRORS FOUND:\n');
    errors.forEach(err => {
        console.log(`Line ${err.line}, Col ${err.col}:`);
        console.log(`  ${err.text}`);
        console.log(`  Error: ${err.error}\n`);
    });
}

if (braceStack.length > 0) {
    console.log(`❌ UNCLOSED BRACES (${braceStack.length}):\n`);
    braceStack.forEach(brace => {
        console.log(`Line ${brace.line}, Col ${brace.col}:`);
        console.log(`  ${brace.text}\n`);
    });
}

if (errors.length === 0 && braceStack.length === 0) {
    console.log('✅ All braces matched correctly!');
} else {
    console.log(`\nSummary: ${errors.length} unmatched closing braces, ${braceStack.length} unclosed opening braces`);
}
