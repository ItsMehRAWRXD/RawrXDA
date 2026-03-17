const fs = require('fs');
const path = require('path');

const htmlFilePath = path.resolve('c:\\Users\\HiH8e\\OneDrive\\Desktop\\IDEre2.html');
let html = fs.readFileSync(htmlFilePath, 'utf8');

console.log('🔧 Removing duplicate initK2 function...\n');

// The duplicate initK2 function to remove is between these markers
const startMarker = '    // ============================================================================\n    // 9. Initialize K2 on Load\n    // ============================================================================\n    async function initK2() {';
const endMarker = '    }\n    \n})();\n</script>\n\n<!-- K2 Beaconism Integration -->';

const startIndex = html.indexOf(startMarker);
if (startIndex === -1) {
    console.log('❌ Could not find start marker');
    process.exit(1);
}

const searchFrom = startIndex + startMarker.length;
const endIndex = html.indexOf(endMarker, searchFrom);
if (endIndex === -1) {
    console.log('❌ Could not find end marker');
    process.exit(1);
}

console.log(`Found duplicate initK2 at position ${startIndex} to ${endIndex}`);

// Replace the entire duplicate section
const newText = '    }\n    \n})();\n</script>\n\n<!-- K2 Beaconism Integration -->';
html = html.substring(0, startIndex) + newText + html.substring(endIndex + endMarker.length);

// Create backup
const backupPath = htmlFilePath.replace('.html', '-backup-syntax-fix.html');
fs.writeFileSync(backupPath, fs.readFileSync(htmlFilePath, 'utf8'));
console.log(`📦 Backup created: ${path.basename(backupPath)}`);

// Save
fs.writeFileSync(htmlFilePath, html, 'utf8');
console.log(`✅ Removed duplicate initK2 function`);
console.log(`📝 File saved`);
