const fs = require('fs');
const path = require('path');

const htmlFilePath = path.resolve('c:\\Users\\HiH8e\\OneDrive\\Desktop\\IDEre2.html');
const html = fs.readFileSync(htmlFilePath, 'utf8');

console.log('🔍 Scanning for JavaScript syntax errors in HTML...\n');

// Extract all script blocks
const scriptRegex = /<script(?:\s+[^>]*)?>([^]*?)<\/script>/gi;
let match;
let scriptIndex = 0;
const errors = [];

while ((match = scriptRegex.exec(html)) !== null) {
    scriptIndex++;
    const scriptContent = match[1];
    const scriptStart = match.index;
    
    // Count line number
    const linesBeforeScript = html.substring(0, scriptStart).split('\n').length;
    
    // Skip empty scripts
    if (scriptContent.trim().length === 0) continue;
    
    // Try to parse the script
    try {
        new Function(scriptContent);
    } catch (error) {
        const errorLine = error.stack ? error.stack.match(/Function:(\d+)/) : null;
        const lineInScript = errorLine ? parseInt(errorLine[1]) : 0;
        const actualLine = linesBeforeScript + lineInScript;
        
        errors.push({
            scriptIndex,
            error: error.message,
            lineInHTML: actualLine,
            lineInScript,
            linesBeforeScript,
            preview: scriptContent.split('\n').slice(Math.max(0, lineInScript - 3), lineInScript + 2).join('\n')
        });
        
        console.log(`❌ Error in <script> block #${scriptIndex}:`);
        console.log(`   Message: ${error.message}`);
        console.log(`   Line in HTML: ~${actualLine}`);
        console.log(`   Line in script: ${lineInScript}`);
        console.log(`   Context:`);
        console.log(scriptContent.split('\n').slice(Math.max(0, lineInScript - 3), lineInScript + 2).map((line, i) => {
            const lineNum = Math.max(0, lineInScript - 3) + i + 1;
            const marker = lineNum === lineInScript ? ' >>> ' : '     ';
            return `${marker}${lineNum}: ${line}`;
        }).join('\n'));
        console.log('\n' + '='.repeat(80) + '\n');
    }
}

if (errors.length === 0) {
    console.log('✅ No JavaScript syntax errors found!\n');
} else {
    console.log(`\n🔴 Found ${errors.length} syntax error(s) in JavaScript code.\n`);
    
    // Save detailed report
    const reportPath = 'c:\\Users\\HiH8e\\OneDrive\\Desktop\\syntax-errors-report.json';
    fs.writeFileSync(reportPath, JSON.stringify(errors, null, 2));
    console.log(`📄 Detailed report saved to: ${reportPath}\n`);
}
