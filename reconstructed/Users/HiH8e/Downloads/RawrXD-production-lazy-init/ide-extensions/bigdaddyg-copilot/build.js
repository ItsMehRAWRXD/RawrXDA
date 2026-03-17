const { execSync } = require('child_process');
const fs = require('fs');
const path = require('path');

const srcDir = 'C:\\Users\\HiH8e\\Downloads\\RawrXD-production-lazy-init\\ide-extensions\\bigdaddyg-copilot';
const destDir = 'E:\\Everything\\cursor\\extensions\\bigdaddyg-copilot-1.0.0';

console.log('Building BigDaddyG Extension...\n');

try {
  // Compile TypeScript
  console.log('[1] Compiling TypeScript...');
  process.chdir(srcDir);
  execSync('npx tsc -p ./', { stdio: 'inherit' });
  console.log('✓ Compiled\n');

  // Copy source
  console.log('[2] Copying files to Cursor...');
  execSync(`xcopy "${srcDir}" "${destDir}" /E /I /Y /Q`, { stdio: 'inherit' });
  console.log('✓ Copied\n');

  // Check compiled file
  const jsFile = path.join(destDir, 'out', 'extension.js');
  if (fs.existsSync(jsFile)) {
    const size = fs.statSync(jsFile).size;
    console.log('[3] Extension Ready');
    console.log(`✓ ${jsFile}`);
    console.log(`  Size: ${size} bytes\n`);
    console.log('Next: Restart Cursor and test: Ctrl+Shift+P > BigDaddyG: Open AI Chat');
  } else {
    console.log('ERROR: Compiled file not found');
  }
} catch (err) {
  console.error('ERROR:', err.message);
  process.exit(1);
}
