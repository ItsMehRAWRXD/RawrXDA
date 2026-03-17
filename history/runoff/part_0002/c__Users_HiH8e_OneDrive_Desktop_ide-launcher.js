#!/usr/bin/env node
/* Minimal cross-platform launcher replacing PowerShell scripts.
   Opens IDEre2.html in default browser and verifies file exists. */
const { exec } = require('child_process');
const { existsSync } = require('fs');
const { join } = require('path');

const ideFile = join(__dirname, 'IDEre2.html');
if (!existsSync(ideFile)) {
  console.error('IDE file not found:', ideFile);
  process.exit(1);
}

function openBrowser(target) {
  const platform = process.platform;
  let cmd;
  if (platform === 'win32') cmd = `start "" "${target}"`;
  else if (platform === 'darwin') cmd = `open "${target}"`;
  else cmd = `xdg-open "${target}"`;
  exec(cmd, (err) => {
    if (err) {
      console.error('Failed to launch browser:', err.message);
      process.exit(1);
    } else {
      console.log('IDE launched ->', target);
    }
  });
}

openBrowser('file:///' + ideFile.replace(/\\/g, '/'));
