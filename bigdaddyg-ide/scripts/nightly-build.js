#!/usr/bin/env node
/**
 * Nightly build script: build React app then run electron-builder.
 * Optionally set version suffix via env NIGHTLY_SUFFIX (e.g. "nightly.20260219").
 */
const { execSync } = require('child_process');
const path = require('path');

const PREFIX = '[nightly-build]';

const root = path.join(__dirname, '..');
const suffix = process.env.NIGHTLY_SUFFIX || '';

process.chdir(root);

try {
  console.log(`${PREFIX} Building React app (npm run build)...`);
  execSync('npm run build', { stdio: 'inherit' });

  if (suffix) {
    console.log(`${PREFIX} Setting package.json version suffix:`, suffix);
    const pkgPath = path.join(root, 'package.json');
    const pkg = require(pkgPath);
    const base = pkg.version.replace(/-.*$/, '');
    pkg.version = `${base}-${suffix}`;
    require('fs').writeFileSync(pkgPath, JSON.stringify(pkg, null, 2) + '\n');
  }

  console.log(`${PREFIX} Running electron-builder...`);
  execSync('npx electron-builder', { stdio: 'inherit' });

  console.log(`${PREFIX} Complete.`);
} catch (e) {
  const msg = e && e.message ? e.message : String(e);
  console.error(`${PREFIX} FAILED:`, msg);
  console.error(`${PREFIX} Next: run the failing command from bigdaddyg-ide in a clean shell (often \`npm ci\` then \`npm run build\` or \`npx electron-builder\` alone) and fix the first error above.`);
  process.exit(typeof e.status === 'number' ? e.status : 1);
}
