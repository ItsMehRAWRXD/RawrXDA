#!/usr/bin/env node
/**
 * Nightly build script: build React app then run electron-builder.
 * Optionally set version suffix via env NIGHTLY_SUFFIX (e.g. "nightly.20260219").
 */
const { execSync } = require('child_process');
const path = require('path');

const root = path.join(__dirname, '..');
const suffix = process.env.NIGHTLY_SUFFIX || '';

process.chdir(root);

console.log('Building React app...');
execSync('npm run build', { stdio: 'inherit' });

if (suffix) {
  console.log('Setting version suffix:', suffix);
  const pkgPath = path.join(root, 'package.json');
  const pkg = require(pkgPath);
  const base = pkg.version.replace(/-.*$/, '');
  pkg.version = `${base}-${suffix}`;
  require('fs').writeFileSync(pkgPath, JSON.stringify(pkg, null, 2) + '\n');
}

console.log('Running electron-builder...');
execSync('npx electron-builder', { stdio: 'inherit' });

console.log('Nightly build complete.');
