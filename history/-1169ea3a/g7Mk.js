#!/usr/bin/env node
/**
 * HTML asset audit for BigDaddyG IDE.
 * Reads an HTML file, lists script/link assets, resolves local paths, reports missing files.
 * Remote (http/https) assets are pinged via HEAD (best-effort) to spot 404s.
 */

const fs = require('fs');
const path = require('path');
const http = require('http');
const https = require('https');

const INPUT = process.argv[2] || path.join(__dirname, '..', 'electron', 'index.html');
const TIMEOUT_MS = 4000;

function readFileSafe(p) {
  try {
    return fs.readFileSync(p, 'utf8');
  } catch (err) {
    console.error(`[audit] ❌ Cannot read file: ${p} -> ${err.message}`);
    process.exit(1);
  }
}

function extractAssets(html) {
  const assets = [];
  const scriptRe = /<script[^>]*\bsrc\s*=\s*"([^"]+)"[^>]*>/gi;
  const linkRe = /<link[^>]*\bhref\s*=\s*"([^"]+)"[^>]*>/gi;

  let m;
  while ((m = scriptRe.exec(html)) !== null) {
    assets.push({ type: 'script', url: m[1] });
  }
  while ((m = linkRe.exec(html)) !== null) {
    assets.push({ type: 'link', url: m[1] });
  }
  return assets;
}

function isRemote(u) {
  return /^https?:\/\//i.test(u) || /^\/\//.test(u);
}

function normalizeLocal(baseDir, u) {
  if (u.startsWith('file://')) return u.replace('file://', '');
  if (u.startsWith('/')) return path.join(baseDir, u); // absolute-ish inside project
  return path.join(baseDir, u);
}

function checkLocal(baseDir, asset) {
  const target = normalizeLocal(baseDir, asset.url);
  const exists = fs.existsSync(target);
  return { ...asset, exists, resolved: target };
}

function headRemote(asset) {
  return new Promise((resolve) => {
    const url = asset.url.startsWith('//') ? `https:${asset.url}` : asset.url;
    const lib = url.startsWith('https') ? https : http;
    const req = lib.request(url, { method: 'HEAD', timeout: TIMEOUT_MS }, (res) => {
      resolve({ ...asset, status: res.statusCode, ok: res.statusCode >= 200 && res.statusCode < 300 });
    });
    req.on('timeout', () => {
      req.destroy();
      resolve({ ...asset, status: 'timeout', ok: false });
    });
    req.on('error', (err) => resolve({ ...asset, status: err.message, ok: false }));
    req.end();
  });
}

async function main() {
  const html = readFileSafe(INPUT);
  const baseDir = path.dirname(INPUT);
  const assets = extractAssets(html);

  const localAssets = assets.filter((a) => !isRemote(a.url)).map((a) => checkLocal(baseDir, a));
  const remoteAssets = assets.filter((a) => isRemote(a.url));

  const remoteResults = await Promise.all(remoteAssets.map(headRemote));

  console.log('[audit] 📄 HTML:', INPUT);
  console.log(`[audit] 🔍 Found assets: ${assets.length} (local ${localAssets.length}, remote ${remoteAssets.length})`);

  const missingLocal = localAssets.filter((a) => !a.exists);
  if (missingLocal.length) {
    console.log('\n[audit] ❌ Missing local assets:');
    missingLocal.forEach((a) => console.log(`  - ${a.type}: ${a.url} -> ${a.resolved}`));
  } else {
    console.log('\n[audit] ✅ All local assets exist');
  }

  const badRemote = remoteResults.filter((r) => !r.ok);
  if (remoteResults.length) {
    console.log('\n[audit] 🌐 Remote assets (status):');
    remoteResults.forEach((r) => console.log(`  - ${r.type}: ${r.url} -> ${r.status}`));
  }
  if (badRemote.length) {
    console.log('\n[audit] ❗ Remote failures:');
    badRemote.forEach((r) => console.log(`  - ${r.type}: ${r.url} -> ${r.status}`));
  }

  console.log('\n[audit] ✅ Completed');
}

main().catch((err) => {
  console.error('[audit] Fatal error', err);
  process.exit(1);
});
