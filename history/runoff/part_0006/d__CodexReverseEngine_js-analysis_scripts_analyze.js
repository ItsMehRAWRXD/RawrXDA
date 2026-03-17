const fs = require("fs");
const path = require("path");
const prettier = require("prettier");
const parser = require("@babel/parser");
const traverse = require("@babel/traverse").default;

const DEFAULT_EXTS = new Set([".js", ".mjs", ".cjs"]);
const SKIP_DIRS = new Set(["node_modules", ".git", "dist", "build", "out"]);

function parseArgs(argv) {
  const args = { inputs: [], out: "analysis_output", beautify: true };
  for (let i = 2; i < argv.length; i += 1) {
    const arg = argv[i];
    if (arg === "--inputs" || arg === "-i") {
      const value = argv[i + 1] || "";
      args.inputs = value.split(",").map((v) => v.trim()).filter(Boolean);
      i += 1;
      continue;
    }
    if (arg === "--out" || arg === "-o") {
      args.out = argv[i + 1] || args.out;
      i += 1;
      continue;
    }
    if (arg === "--no-beautify") {
      args.beautify = false;
      continue;
    }
    if (!arg.startsWith("-")) {
      args.inputs.push(arg);
    }
  }
  return args;
}

function isFile(p) {
  try {
    return fs.statSync(p).isFile();
  } catch {
    return false;
  }
}

function isDir(p) {
  try {
    return fs.statSync(p).isDirectory();
  } catch {
    return false;
  }
}

function collectFiles(entryPath, collected = []) {
  if (isFile(entryPath)) {
    const ext = path.extname(entryPath).toLowerCase();
    if (DEFAULT_EXTS.has(ext)) collected.push(entryPath);
    return collected;
  }
  if (!isDir(entryPath)) return collected;

  const entries = fs.readdirSync(entryPath, { withFileTypes: true });
  for (const entry of entries) {
    if (entry.isDirectory()) {
      if (SKIP_DIRS.has(entry.name)) continue;
      collectFiles(path.join(entryPath, entry.name), collected);
    } else if (entry.isFile()) {
      const ext = path.extname(entry.name).toLowerCase();
      if (DEFAULT_EXTS.has(ext)) {
        collected.push(path.join(entryPath, entry.name));
      }
    }
  }
  return collected;
}

function ensureDir(dir) {
  if (!fs.existsSync(dir)) fs.mkdirSync(dir, { recursive: true });
}

function resolveInputRoots(inputs) {
  return inputs.map((input) => {
    const resolved = path.resolve(input);
    if (isFile(resolved)) return path.dirname(resolved);
    return resolved;
  });
}

function getBestRoot(filePath, roots) {
  const fileLower = filePath.toLowerCase();
  let best = null;
  for (const root of roots) {
    const rootLower = root.toLowerCase();
    if (fileLower.startsWith(rootLower)) {
      if (!best || rootLower.length > best.toLowerCase().length) {
        best = root;
      }
    }
  }
  return best;
}

function getRootName(rootPath) {
  const base = path.basename(rootPath);
  const parent = path.basename(path.dirname(rootPath));
  if (parent && parent !== base) return `${parent}_${base}`;
  return base || "input";
}

function writeList(outPath, items) {
  const text = Array.from(new Set(items)).sort().join("\n");
  fs.writeFileSync(outPath, text + (text ? "\n" : ""), "utf8");
}

function getCalleeName(node) {
  if (!node) return null;
  if (node.type === "Identifier") return node.name;
  if (node.type === "MemberExpression") {
    const obj = node.object && node.object.name ? node.object.name : null;
    const prop = node.property && node.property.name ? node.property.name : null;
    if (obj && prop) return `${obj}.${prop}`;
  }
  return null;
}

function normalizeName(node, fallback = "<anonymous>") {
  if (!node) return fallback;
  if (node.type === "Identifier") return node.name;
  if (node.type === "StringLiteral") return node.value;
  if (node.type === "NumericLiteral") return String(node.value);
  if (node.key && node.key.name) return node.key.name;
  return fallback;
}

function parseFile(source, filename) {
  return parser.parse(source, {
    sourceType: "unambiguous",
    plugins: [
      "jsx",
      "typescript",
      "classProperties",
      "decorators-legacy",
      "dynamicImport",
      "objectRestSpread",
      "optionalChaining",
      "nullishCoalescingOperator",
      "topLevelAwait"
    ]
  });
}

function analyzeFile(source, filePath, results) {
  let ast;
  try {
    ast = parseFile(source, filePath);
  } catch (err) {
    results.errors.push({ file: filePath, error: String(err) });
    return;
  }

  const functionStack = [];

  traverse(ast, {
    FunctionDeclaration(path) {
      const name = path.node.id ? path.node.id.name : "<anonymous>";
      results.functions.push(name);
      functionStack.push(name);
    },
    FunctionExpression: {
      enter(path) {
        let name = "<anonymous>";
        if (path.parent && path.parent.type === "VariableDeclarator") {
          name = normalizeName(path.parent.id, name);
        } else if (path.node.id) {
          name = path.node.id.name;
        }
        results.functions.push(name);
        functionStack.push(name);
      },
      exit() {
        functionStack.pop();
      }
    },
    ArrowFunctionExpression: {
      enter(path) {
        let name = "<anonymous>";
        if (path.parent && path.parent.type === "VariableDeclarator") {
          name = normalizeName(path.parent.id, name);
        }
        results.functions.push(name);
        functionStack.push(name);
      },
      exit() {
        functionStack.pop();
      }
    },
    ClassDeclaration(path) {
      const name = path.node.id ? path.node.id.name : "<anonymous>";
      results.classes.push(name);
    },
    ClassExpression(path) {
      let name = "<anonymous>";
      if (path.parent && path.parent.type === "VariableDeclarator") {
        name = normalizeName(path.parent.id, name);
      } else if (path.node.id) {
        name = path.node.id.name;
      }
      results.classes.push(name);
    },
    ExportNamedDeclaration(path) {
      if (path.node.declaration) {
        if (path.node.declaration.id && path.node.declaration.id.name) {
          results.exports.push(path.node.declaration.id.name);
        }
      }
      if (path.node.specifiers && path.node.specifiers.length) {
        for (const spec of path.node.specifiers) {
          if (spec.exported && spec.exported.name) {
            results.exports.push(spec.exported.name);
          }
        }
      }
    },
    ExportDefaultDeclaration(path) {
      results.exports.push("default");
    },
    AssignmentExpression(path) {
      if (path.node.left && path.node.left.type === "MemberExpression") {
        const left = path.node.left;
        if (left.object && left.object.name === "module" && left.property && left.property.name === "exports") {
          results.exports.push("module.exports");
        }
        if (left.object && left.object.name === "exports" && left.property && left.property.name) {
          results.exports.push(left.property.name);
        }
      }
    },
    ImportDeclaration(path) {
      if (path.node.source && path.node.source.value) {
        results.imports.push(path.node.source.value);
      }
    },
    CallExpression(path) {
      if (path.node.callee && path.node.callee.type === "Identifier" && path.node.callee.name === "require") {
        const arg = path.node.arguments[0];
        if (arg && arg.type === "StringLiteral") {
          results.imports.push(arg.value);
        }
      }
      const currentFn = functionStack[functionStack.length - 1];
      if (currentFn) {
        const calleeName = getCalleeName(path.node.callee);
        if (calleeName) {
          if (!results.callGraph[currentFn]) results.callGraph[currentFn] = new Set();
          results.callGraph[currentFn].add(calleeName);
        }
      }
    },
    StringLiteral(path) {
      const val = path.node.value;
      if (/https?:\/\//i.test(val) || /wss?:\/\//i.test(val)) {
        results.urls.push(val);
      }
      if (/mcp/i.test(val)) {
        results.mcpPatterns.push(val);
        const currentFn = functionStack[functionStack.length - 1];
        if (currentFn) results.mcpEntrypoints.add(currentFn);
      }
    },
    TemplateLiteral(path) {
      const text = path.node.quasis.map((q) => q.value.cooked || "").join("${}");
      if (/https?:\/\//i.test(text) || /wss?:\/\//i.test(text)) {
        results.urls.push(text);
      }
      if (/mcp/i.test(text)) {
        results.mcpPatterns.push(text);
        const currentFn = functionStack[functionStack.length - 1];
        if (currentFn) results.mcpEntrypoints.add(currentFn);
      }
    }
  });
}

async function analyze(inputs, outDir, beautify) {
  const outputDir = path.resolve(outDir);
  ensureDir(outputDir);
  const beautifiedDir = path.join(outputDir, "beautified");
  ensureDir(beautifiedDir);

  const inputRoots = resolveInputRoots(inputs);

  const results = {
    functions: [],
    classes: [],
    exports: [],
    imports: [],
    urls: [],
    mcpPatterns: [],
    mcpEntrypoints: new Set(),
    callGraph: {},
    errors: []
  };

  const files = inputs.flatMap((p) => collectFiles(path.resolve(p)));
  for (const file of files) {
    const raw = fs.readFileSync(file, "utf8");
    let formatted = raw;
    if (beautify) {
      try {
        formatted = await prettier.format(raw, { parser: "babel" });
      } catch (err) {
        results.errors.push({ file, error: `prettier: ${String(err)}` });
      }
    }

    const root = getBestRoot(file, inputRoots) || process.cwd();
    const relPath = path.relative(root, file);
    const rootName = getRootName(root);
    const beautifiedPath = path.join(beautifiedDir, rootName, relPath);
    ensureDir(path.dirname(beautifiedPath));
    fs.writeFileSync(beautifiedPath, formatted, "utf8");

    analyzeFile(formatted, file, results);
  }

  writeList(path.join(outputDir, "functions.txt"), results.functions);
  writeList(path.join(outputDir, "classes.txt"), results.classes);
  writeList(path.join(outputDir, "exports.txt"), results.exports);
  writeList(path.join(outputDir, "imports.txt"), results.imports);
  writeList(path.join(outputDir, "urls.txt"), results.urls);
  writeList(path.join(outputDir, "mcp_patterns.txt"), results.mcpPatterns);
  writeList(path.join(outputDir, "mcp_entrypoints.txt"), Array.from(results.mcpEntrypoints));

  const callGraphJson = {};
  for (const [fn, callees] of Object.entries(results.callGraph)) {
    callGraphJson[fn] = Array.from(callees).sort();
  }

  const apiMap = {
    summary: {
      files: files.length,
      functions: new Set(results.functions).size,
      classes: new Set(results.classes).size,
      exports: new Set(results.exports).size,
      imports: new Set(results.imports).size,
      urls: new Set(results.urls).size,
      mcp_patterns: new Set(results.mcpPatterns).size,
      mcp_entrypoints: results.mcpEntrypoints.size,
      errors: results.errors.length
    },
    exports: Array.from(new Set(results.exports)).sort(),
    imports: Array.from(new Set(results.imports)).sort(),
    urls: Array.from(new Set(results.urls)).sort(),
    mcp_entrypoints: Array.from(results.mcpEntrypoints).sort()
  };

  fs.writeFileSync(path.join(outputDir, "call_graph.json"), JSON.stringify(callGraphJson, null, 2));
  fs.writeFileSync(path.join(outputDir, "api_map.json"), JSON.stringify(apiMap, null, 2));
  fs.writeFileSync(path.join(outputDir, "summary.json"), JSON.stringify(apiMap.summary, null, 2));
  if (results.errors.length) {
    fs.writeFileSync(path.join(outputDir, "errors.json"), JSON.stringify(results.errors, null, 2));
  }

  return apiMap.summary;
}

async function main() {
  const args = parseArgs(process.argv);
  if (!args.inputs.length) {
    console.error("Usage: node scripts/analyze.js --inputs <fileOrDir[,fileOrDir]> --out <dir>");
    process.exit(1);
  }
  const summary = await analyze(args.inputs, args.out, args.beautify);
  console.log("Analysis complete:", summary);
}

if (require.main === module) {
  main();
}
