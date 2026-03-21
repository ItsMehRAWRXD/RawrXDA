#!/usr/bin/env node
"use strict";

const fs = require("fs");
const path = require("path");

function readText(filePath) {
  return fs.readFileSync(filePath, "utf-8");
}

function writeJson(filePath, data) {
  fs.writeFileSync(filePath, JSON.stringify(data, null, 2), "utf-8");
}

function parseArgs(argv) {
  const args = {
    input: "",
    out: "bundle_decompilation.json",
    maxBodyPreview: 800
  };

  for (let i = 2; i < argv.length; i++) {
    const a = argv[i];
    if ((a === "--input" || a === "-i") && argv[i + 1]) {
      args.input = argv[++i];
    } else if ((a === "--out" || a === "-o") && argv[i + 1]) {
      args.out = argv[++i];
    } else if (a === "--max-preview" && argv[i + 1]) {
      args.maxBodyPreview = Number(argv[++i]) || 800;
    } else if (a === "--help" || a === "-h") {
      printHelpAndExit(0);
    }
  }

  if (!args.input) {
    printHelpAndExit(1);
  }
  return args;
}

function printHelpAndExit(code) {
  console.log("Usage: node scripts/bundle_decompile.js --input <bundle.js> [--out report.json] [--max-preview 800]");
  process.exit(code);
}

function extractModules(bundle) {
  const modules = [];
  // Matches "<id>:(<params>)=>{" and then we manually close balanced braces.
  const headerRegex = /(\d+)\s*:\s*\(([^)]*)\)\s*=>\s*\{/g;
  let headerMatch;

  while ((headerMatch = headerRegex.exec(bundle)) !== null) {
    const id = Number(headerMatch[1]);
    const paramsRaw = headerMatch[2] || "";
    const params = paramsRaw.split(",").map((p) => p.trim()).filter(Boolean);

    const bodyStart = headerMatch.index + headerMatch[0].length;
    let i = bodyStart;
    let depth = 1;
    let inString = false;
    let stringChar = "";
    let escaped = false;

    while (i < bundle.length && depth > 0) {
      const ch = bundle[i];
      if (inString) {
        if (escaped) {
          escaped = false;
        } else if (ch === "\\") {
          escaped = true;
        } else if (ch === stringChar) {
          inString = false;
          stringChar = "";
        }
      } else {
        if (ch === "'" || ch === "\"" || ch === "`") {
          inString = true;
          stringChar = ch;
        } else if (ch === "{") {
          depth++;
        } else if (ch === "}") {
          depth--;
        }
      }
      i++;
    }

    if (depth !== 0) {
      continue;
    }

    const bodyEndExclusive = i - 1;
    const body = bundle.slice(bodyStart, bodyEndExclusive);
    const raw = bundle.slice(headerMatch.index, i);

    modules.push({
      id,
      params,
      body,
      raw,
      start: headerMatch.index,
      end: i
    });
  }

  return modules;
}

function uniqNumbers(nums) {
  return [...new Set(nums)];
}

function parseDependencies(body) {
  return uniqNumbers([...body.matchAll(/\bs\((\d+)\)/g)].map((m) => Number(m[1])));
}

function parseExports(body) {
  // Covers common webpack forms:
  // s.d(e,{foo:()=>x,bar:()=>y})
  // t.d(e,{foo:()=>x})
  const exports = [];
  const exportObjectMatch = body.match(/\b[st]\.d\(\s*e\s*,\s*\{([\s\S]*?)\}\s*\)/);
  if (!exportObjectMatch) return exports;

  for (const m of exportObjectMatch[1].matchAll(/([A-Za-z0-9_$]+)\s*:/g)) {
    exports.push(m[1]);
  }
  return exports;
}

function countPatterns(body) {
  const patterns = {
    classes: /\bclass\s+[A-Za-z_$][\w$]*/g,
    functions: /\bfunction\s+[A-Za-z_$][\w$]*/g,
    arrows: /\([^)]*\)\s*=>/g,
    privateFields: /#[A-Za-z_$][\w$]*/g,
    importsNumeric: /\bs\(\d+\)/g,
    reactHooks: /\buse[A-Z]\w*/g,
    asyncAwait: /\b(async|await)\b/g,
    promises: /\bPromise\b/g
  };

  const out = {};
  for (const [name, regex] of Object.entries(patterns)) {
    const c = (body.match(regex) || []).length;
    if (c > 0) out[name] = c;
  }
  return out;
}

function analyze(bundle, modules, maxBodyPreview) {
  const moduleReports = modules.map((m) => {
    const deps = parseDependencies(m.body);
    const exports = parseExports(m.body);
    const patternCounts = countPatterns(m.body);
    return {
      id: m.id,
      params: m.params,
      bodyLength: m.body.length,
      bodyPreview: m.body.length > maxBodyPreview ? m.body.slice(0, maxBodyPreview) + "...[truncated]" : m.body,
      dependencies: deps,
      exports,
      patterns: patternCounts
    };
  });

  const sourceMap = (bundle.match(/sourceMappingURL=([^\s]+)/) || [null, null])[1];
  const byId = [...moduleReports].sort((a, b) => a.id - b.id);

  return {
    bundleSize: bundle.length,
    moduleCount: modules.length,
    sourceMap,
    modules: byId,
    entryLikeModules: byId.filter((m) => m.exports.length > 0).map((m) => ({ id: m.id, exports: m.exports }))
  };
}

function printSummary(report) {
  console.log("=".repeat(70));
  console.log("PURE JAVASCRIPT BUNDLE DECOMPILATION");
  console.log("=".repeat(70));
  console.log(`Bundle size: ${report.bundleSize.toLocaleString()} bytes`);
  console.log(`Modules found: ${report.moduleCount}`);
  console.log();

  console.log("[Dependency graph]");
  for (const m of report.modules) {
    if (m.dependencies.length) {
      console.log(`Module ${String(m.id).padStart(5)} -> ${JSON.stringify(m.dependencies)}`);
    } else {
      console.log(`Module ${String(m.id).padStart(5)} -> []`);
    }
  }
  console.log();

  console.log("[Entry-like modules]");
  if (!report.entryLikeModules.length) {
    console.log("None detected");
  } else {
    for (const m of report.entryLikeModules) {
      console.log(`Module ${m.id}: exports ${JSON.stringify(m.exports)}`);
    }
  }
  console.log();

  console.log(`[Source map] ${report.sourceMap || "none"}`);
}

function main() {
  const args = parseArgs(process.argv);
  const inputPath = path.resolve(args.input);
  const outPath = path.resolve(args.out);

  const bundle = readText(inputPath);
  const modules = extractModules(bundle);
  const report = analyze(bundle, modules, args.maxBodyPreview);
  writeJson(outPath, report);
  printSummary(report);

  console.log();
  console.log(`[+] Full analysis saved to ${outPath}`);
}

main();

