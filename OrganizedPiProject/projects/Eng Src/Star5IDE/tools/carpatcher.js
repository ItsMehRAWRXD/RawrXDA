#!/usr/bin/env node
"use strict";

// In-memory patcher that replaces lines containing only `Car();` with injected code.
// Usage:
//   carpatcher js <target.js> --patch "console.log('Hello')" --patch "..." [--run]
//   carpatcher ts <target.ts> --patch "..." [--tsc "ts-node/register"] [--run]
// Without --run, writes the patched output to stdout.

const fs = require("fs");
const os = require("os");
const path = require("path");
const { spawnSync } = require("child_process");

function parseArgs(argv) {
  const args = { _: [] };
  for (let i = 2; i < argv.length; i++) {
    const a = argv[i];
    if (a === "--patch") { (args.patches ||= []).push(argv[++i] || ""); continue; }
    if (a === "--patch-file") { (args.patchFiles ||= []).push(argv[++i] || ""); continue; }
    if (a === "--run") { args.run = true; continue; }
    if (a === "--tsc") { args.tsc = argv[++i]; continue; }
    args._.push(a);
  }
  return args;
}

function patchSource(source, patches) {
  const lines = source.split(/\r?\n/);
  let patchIndex = 0;
  const out = lines.map(line => {
    if (/^\s*Car\(\);\s*$/.test(line)) {
      const injection = patches && patchIndex < patches.length ? patches[patchIndex++] : "";
      return injection;
    }
    return line;
  });
  return { code: out.join("\n"), used: patchIndex };
}

function main() {
  const args = parseArgs(process.argv);
  const lang = args._[0]; // 'js' or 'ts'
  const target = args._[1];
  if (!lang || !target || !fs.existsSync(target)) {
    console.log(
      "Usage:\n" +
      "  carpatcher js <target.js> --patch 'code' [--patch 'code2'] [--run]\n" +
      "  carpatcher ts <target.ts> --patch 'code' [--tsc ts-node/register] [--run]"
    );
    process.exit(1);
  }

  const source = fs.readFileSync(target, "utf8");
  const patches = [];
  // Prefer patch files first (multi-line), then inline --patch entries
  if (Array.isArray(args.patchFiles)) {
    for (const f of args.patchFiles) {
      if (!f) continue;
      const body = fs.readFileSync(f, "utf8");
      patches.push(body);
    }
  }
  if (Array.isArray(args.patches)) patches.push(...args.patches);
  const { code, used } = patchSource(source, patches);

  if (!args.run) {
    process.stdout.write(code);
    return;
  }

  // Write to temp and execute with Node
  const tmpDir = fs.mkdtempSync(path.join(os.tmpdir(), "carpatch-"));
  const ext = lang === "ts" ? ".ts" : ".js";
  const tmp = path.join(tmpDir, path.basename(target).replace(/\.(t|j)sx?$/, ext));
  fs.writeFileSync(tmp, code);

  let cmd = process.execPath; // node
  let cmdArgs = [tmp];
  if (lang === "ts") {
    // require ts-node/register if provided
    if (args.tsc) {
      cmdArgs = ["-r", args.tsc, tmp];
    } else {
      // Best-effort: try ts-node/register/transpile-only if available
      cmdArgs = ["-r", "ts-node/register/transpile-only", tmp];
    }
  }

  const result = spawnSync(cmd, cmdArgs, { stdio: "inherit" });
  process.exit(result.status || 0);
}

if (require.main === module) {
  main();
}

