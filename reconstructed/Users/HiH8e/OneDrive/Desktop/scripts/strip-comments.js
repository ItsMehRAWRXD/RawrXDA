#!/usr/bin/env node
/* Strip // and /* */ /* style comments from .js and .d.ts files (non destructive backup). */
const { join } = require('path');
const { readFileSync, writeFileSync, readdirSync, statSync, mkdirSync } = require('fs');
const root = join(__dirname, '..');
const outDir = join(root, 'stripped');
mkdirSync(outDir, { recursive: true });

function strip(content) {
  // Remove block comments and line comments (rough heuristic)
  return content
    .replace(/\/\*[\s\S]*?\*\//g, '')
    .replace(/(^|\n)\s*\/\/.*(?=\n)/g, '$1')
    .replace(/\n{2,}/g, '\n');
}

function walk(dir) {
  for (const f of readdirSync(dir)) {
    const p = join(dir, f);
    const s = statSync(p);
    if (s.isDirectory()) {
      if (f.startsWith('node_modules')) continue;
      walk(p);
    } else if (p.endsWith('.js') || p.endsWith('.d.ts')) {
      const content = readFileSync(p, 'utf8');
      const stripped = strip(content);
      const rel = p.substring(root.length + 1);
      const target = join(outDir, rel);
      const targetDir = target.substring(0, target.lastIndexOf('/'));
      mkdirSync(targetDir, { recursive: true });
      writeFileSync(target, stripped, 'utf8');
    }
  }
}

walk(root);
console.log('Stripped files written to', outDir);
