const fs = require('fs');
const os = require('os');
const path = require('path');
const { execSync } = require('child_process');

const filePath = path.join('C:', 'Users', 'HiH8e', 'OneDrive', 'Desktop', 'IDEre2.html');
const content = fs.readFileSync(filePath, 'utf8');
const regex = /<script\b[^>]*>([\s\S]*?)<\/script>/gi;
let match;
let idx = 0;
let failure = null;

while ((match = regex.exec(content)) !== null) {
  idx += 1;
  const scriptCode = match[1].trim();
  if (!scriptCode) continue;
  const tempFile = path.join(os.tmpdir(), `ide_script_${idx}.js`);
  fs.writeFileSync(tempFile, scriptCode, 'utf8');
  try {
    execSync(`node --check "${tempFile}"`, { stdio: 'ignore' });
  } catch (err) {
    failure = { idx, message: err.message.replace(/\(node:.*$/s, '').trim(), tempFile };
    break;
  }
}

if (failure) {
  console.log(`Script #${failure.idx} failed syntax check.`);
  console.log(`Temp file: ${failure.tempFile}`);
  console.log(failure.message);
  process.exit(1);
}

console.log('All script blocks passed syntax check.');
