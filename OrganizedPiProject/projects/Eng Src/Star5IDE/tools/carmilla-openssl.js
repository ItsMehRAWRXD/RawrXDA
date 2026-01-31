#!/usr/bin/env node
"use strict";

const { execFileSync } = require("child_process");
const fs = require("fs");
const os = require("os");
const path = require("path");

function assert(condition, message) {
  if (!condition) {
    console.error(`Error: ${message}`);
    process.exit(1);
  }
}

function makeTempDir() {
  return fs.mkdtempSync(path.join(os.tmpdir(), "carmilla-"));
}

function writeFile(filePath, data) {
  fs.writeFileSync(filePath, data);
}

function readFile(filePath) {
  return fs.readFileSync(filePath);
}

function removeDir(dirPath) {
  try {
    fs.rmSync(dirPath, { recursive: true, force: true });
  } catch {}
}

function runOpenSSL(args, inputBuffer) {
  // Prefer execFileSync to avoid shell interpretation
  return execFileSync("openssl", args, { input: inputBuffer });
}

function encryptBufferAes256Cbc(plainBuffer, passphrase) {
  // Use PBKDF2 + salt; output is binary. We'll base64 encode for transport.
  const out = runOpenSSL(["enc", "-aes-256-cbc", "-salt", "-pbkdf2", "-pass", `pass:${passphrase}`], plainBuffer);
  return Buffer.from(out).toString("base64");
}

function decryptToBufferAes256Cbc(encryptedBase64, passphrase) {
  const bin = Buffer.from(encryptedBase64.trim(), "base64");
  const out = runOpenSSL(["enc", "-d", "-aes-256-cbc", "-salt", "-pbkdf2", "-pass", `pass:${passphrase}`], bin);
  return Buffer.from(out);
}

function repack(encryptedBase64, meta) {
  const pkg = { meta: { method: "openssl", timestamp: new Date().toISOString(), ...meta }, data: encryptedBase64 };
  return Buffer.from(JSON.stringify(pkg), "utf8").toString("base64");
}

function unpack(packagedBase64) {
  const json = Buffer.from(packagedBase64.trim(), "base64").toString("utf8");
  const obj = JSON.parse(json);
  return obj;
}

function readStdinSync() {
  const chunks = [];
  const fd = 0; // stdin
  try {
    // Best-effort read all available stdin
    const stat = fs.fstatSync(fd);
    if (stat.size === 0) {
      // Stream input of unknown size
      const data = fs.readFileSync(0);
      return data;
    }
  } catch {}
  return fs.readFileSync(0);
}

function parseArgs(argv) {
  const args = { _: [] };
  for (let i = 2; i < argv.length; i++) {
    const a = argv[i];
    if (a === "--in") { args.in = argv[++i]; continue; }
    if (a === "--out") { args.out = argv[++i]; continue; }
    if (a === "--pass" || a === "--passphrase") { args.pass = argv[++i]; continue; }
    if (a === "--hint") { args.hint = argv[++i]; continue; }
    if (a === "--meta") { args.meta = argv[++i]; continue; }
    args._.push(a);
  }
  return args;
}

function main() {
  const args = parseArgs(process.argv);
  const cmd = args._[0];
  if (!cmd || ["encrypt", "decrypt", "repack", "unpack"].indexOf(cmd) === -1) {
    console.log(
      "Usage:\n" +
      "  carmilla-openssl encrypt --pass <pass> [--in file|-] [--out file|-] [--hint <text>]\n" +
      "  carmilla-openssl decrypt --pass <pass> [--in file|-] [--out file|-]\n" +
      "  carmilla-openssl repack [--in file|-] [--out file|-] [--meta '{" +
        "\"hint\":\"...\",\"userId\":\"...\"}']\n" +
      "  carmilla-openssl unpack [--in file|-] [--out file|-]" 
    );
    process.exit(1);
  }

  if (cmd === "encrypt") {
    assert(args.pass, "--pass is required");
    let input;
    if (args.in && args.in !== "-") {
      input = readFile(args.in);
    } else {
      input = readStdinSync();
    }
    const encB64 = encryptBufferAes256Cbc(Buffer.from(input), args.pass);
    const packaged = repack(encB64, args.hint ? { passphrase_hint: args.hint } : {});
    if (args.out && args.out !== "-") {
      writeFile(args.out, packaged);
    } else {
      process.stdout.write(packaged);
    }
    return;
  }

  if (cmd === "decrypt") {
    assert(args.pass, "--pass is required");
    let input;
    if (args.in && args.in !== "-") {
      input = readFile(args.in).toString("utf8");
    } else {
      input = readStdinSync().toString("utf8");
    }
    // Accept either packaged or raw base64
    let encryptedBase64 = input.trim();
    try {
      const maybePkg = unpack(encryptedBase64);
      if (maybePkg && maybePkg.data) {
        encryptedBase64 = String(maybePkg.data);
      }
    } catch {}
    const plain = decryptToBufferAes256Cbc(encryptedBase64, args.pass);
    if (args.out && args.out !== "-") {
      writeFile(args.out, plain);
    } else {
      process.stdout.write(plain);
    }
    return;
  }

  if (cmd === "repack") {
    let input;
    if (args.in && args.in !== "-") {
      input = readFile(args.in).toString("utf8").trim();
    } else {
      input = readStdinSync().toString("utf8").trim();
    }
    let meta = {};
    if (args.meta) {
      try { meta = JSON.parse(args.meta); } catch { assert(false, "--meta must be valid JSON"); }
    }
    const packaged = repack(input, meta);
    if (args.out && args.out !== "-") {
      writeFile(args.out, packaged);
    } else {
      process.stdout.write(packaged);
    }
    return;
  }

  if (cmd === "unpack") {
    let input;
    if (args.in && args.in !== "-") {
      input = readFile(args.in).toString("utf8").trim();
    } else {
      input = readStdinSync().toString("utf8").trim();
    }
    const obj = unpack(input);
    const pretty = JSON.stringify(obj, null, 2);
    if (args.out && args.out !== "-") {
      writeFile(args.out, pretty + "\n");
    } else {
      process.stdout.write(pretty + "\n");
    }
    return;
  }
}

if (require.main === module) {
  main();
}

