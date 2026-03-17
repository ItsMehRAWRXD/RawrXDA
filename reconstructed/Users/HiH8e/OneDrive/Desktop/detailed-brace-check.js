const fs = require('fs');

const script = fs.readFileSync('c:\\Users\\HiH8e\\OneDrive\\Desktop\\script-block-18.js', 'utf8');
const lines = script.split('\n');

let braceBalance = 0;
let parenBalance = 0;

console.log('Line-by-line brace analysis:\n');

for (let i = 0; i < lines.length; i++) {
    const line = lines[i];
    const prevBraceBalance = braceBalance;
    const prevParenBalance = parenBalance;
    
    // Count braces (simplistic - doesn't handle comments/strings perfectly)
    const cleanLine = line
        .replace(/\/\/.*$/, '')
        .replace(/\/\*[\s\S]*?\*\//g, '')
        .replace(/'[^']*'/g, "''")
        .replace(/"[^"]*"/g, '""')
        .replace(/`[^`]*`/g, '``');
    
    for (const char of cleanLine) {
        if (char === '{') braceBalance++;
        if (char === '}') braceBalance--;
        if (char === '(') parenBalance++;
        if (char === ')') parenBalance--;
    }
    
    // Report lines where balance changes significantly or goes negative
    if (braceBalance < 0 || parenBalance < 0 || Math.abs(braceBalance - prevBraceBalance) > 1) {
        console.log(`Line ${i + 1}: ${line.substring(0, 80)}`);
        console.log(`  Braces: ${prevBraceBalance} -> ${braceBalance}, Parens: ${prevParenBalance} -> ${parenBalance}`);
    }
}

console.log(`\n Final balance: Braces=${braceBalance}, Parens=${parenBalance}`);
