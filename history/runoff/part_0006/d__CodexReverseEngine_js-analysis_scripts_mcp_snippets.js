const fs = require("fs");
const path = require("path");

const DEFAULT_WINDOW = 40;
const TRANSPORT_KEYS = [
  "upgrade",
  "handleupgrade",
  "websocket",
  "ws",
  "http",
  "https",
  "http2",
  "content-length",
  "transfer-encoding",
  "json-rpc",
  "jsonrpc",
  "batch",
  "framing",
  "message",
  "read",
  "write",
  "socket",
  "request",
  "response",
  "client",
  "server",
  "connect",
  "listen"
];

const PRIMARY_KEYS = [
  "upgrade",
  "handleupgrade",
  "content-length",
  "transfer-encoding",
  "json-rpc",
  "jsonrpc",
  "batch",
  "framing",
  "read",
  "write",
  "message"
];

function parseArgs(argv) {
  const args = {
    baseDir: "D:\\Cursor_Analysis_Results\\combined",
    window: DEFAULT_WINDOW,
    limit: 10
  };
  for (let i = 2; i < argv.length; i += 1) {
    const arg = argv[i];
    if (arg === "--base" && argv[i + 1]) {
      args.baseDir = argv[i + 1];
      i += 1;
      continue;
    }
    if (arg === "--window" && argv[i + 1]) {
      args.window = Number(argv[i + 1]) || DEFAULT_WINDOW;
      i += 1;
      continue;
    }
    if (arg === "--limit" && argv[i + 1]) {
      args.limit = Number(argv[i + 1]) || 10;
      i += 1;
      continue;
    }
    if (!arg.startsWith("-")) {
      args.baseDir = arg;
    }
  }
  return args;
}

function readJson(filePath) {
  return JSON.parse(fs.readFileSync(filePath, "utf8"));
}

function getLines(filePath) {
  return fs.readFileSync(filePath, "utf8").split(/\r?\n/);
}

function getLineStartOffsets(lines) {
  const offsets = [0];
  let total = 0;
  for (const line of lines) {
    total += line.length + 1;
    offsets.push(total);
  }
  return offsets;
}

function getSnippet(lines, lineNumber, windowSize) {
  const start = Math.max(0, lineNumber - 1 - windowSize);
  const end = Math.min(lines.length, lineNumber - 1 + windowSize);
  return {
    startLine: start + 1,
    endLine: end,
    text: lines.slice(start, end).join("\n")
  };
}

function scoreSnippet(text) {
  const lower = text.toLowerCase();
  let score = 0;
  const hits = [];
  const primaryHits = [];
  for (const key of TRANSPORT_KEYS) {
    if (lower.includes(key)) {
      score += 1;
      hits.push(key);
    }
  }
  for (const key of PRIMARY_KEYS) {
    if (lower.includes(key)) {
      score += 3;
      primaryHits.push(key);
    }
  }
  return {
    score,
    hits: Array.from(new Set(hits)),
    primaryHits: Array.from(new Set(primaryHits))
  };
}

function toPseudocode(text, limit = 12) {
  const rawLines = text.split(/\r?\n/).map((line) => line.trim());
  const keywordLines = rawLines.filter((line) => {
    const lower = line.toLowerCase();
    return TRANSPORT_KEYS.some((key) => lower.includes(key));
  });
  const selected = (keywordLines.length ? keywordLines : rawLines)
    .filter((line) => line.length > 0)
    .slice(0, limit)
    .map((line) =>
      line
        .replace(/^function\s+/, "void ")
        .replace(/^const\s+/, "auto ")
        .replace(/^let\s+/, "auto ")
        .replace(/^var\s+/, "auto ")
        .replace(/=>/g, "")
        .replace(/===/g, "==")
        .replace(/!==/g, "!=")
    );
  return selected.join("\n");
}

function makeMasmTemplate(rvaHex) {
  return [
    "mov rax, rcx",
    `add rax, ${rvaHex}`,
    "jmp rax"
  ].join("\n");
}

function main() {
  const args = parseArgs(process.argv);
  const baseDir = path.resolve(args.baseDir);
  const rankedFile = path.join(baseDir, "mcp_entrypoint_ranked.json");
  if (!fs.existsSync(rankedFile)) {
    console.error("Missing ranked report:", rankedFile);
    process.exit(1);
  }

  const ranked = readJson(rankedFile);
  const grouped = {};

  const candidates = [];
  for (const item of ranked) {
    if (!item.file) continue;
    const fileData = grouped[item.file];
    if (!fileData) {
      grouped[item.file] = { lines: getLines(item.file) };
    }
    const lines = grouped[item.file].lines;
    const offsets = grouped[item.file].offsets || (grouped[item.file].offsets = getLineStartOffsets(lines));
    const lineIndex = Math.max(1, item.line || 1);
    const column = item.column || 0;
    const byteOffset = (offsets[lineIndex - 1] || 0) + column;
    const snippet = getSnippet(lines, lineIndex, args.window);
    const score = scoreSnippet(snippet.text);
    candidates.push({
      symbol: item.name,
      file: item.file,
      line: item.line,
      column: item.column,
      byte_offset: byteOffset,
      rva_hex: `0x${byteOffset.toString(16)}`,
      hits: score.hits,
      primaryHits: score.primaryHits,
      score: score.score,
      window: { start: snippet.startLine, end: snippet.endLine },
      pseudocode: toPseudocode(snippet.text),
      masm_hook: makeMasmTemplate(`0x${byteOffset.toString(16)}`)
    });
  }

  const filtered = candidates.filter((c) => c.primaryHits.length > 0);
  const sorted = (filtered.length ? filtered : candidates).sort(
    (a, b) => b.score - a.score || a.symbol.localeCompare(b.symbol)
  );
  const report = sorted.slice(0, args.limit).map((item, index) => ({
    rank: index + 1,
    ...item
  }));

  const outJson = path.join(baseDir, "mcp_entrypoint_snippets.json");
  const outTxt = path.join(baseDir, "mcp_entrypoint_snippets.txt");

  fs.writeFileSync(outJson, JSON.stringify(report, null, 2));

  const lines = [];
  lines.push("MCP Entrypoint Snippets (Top Ranked)");
  lines.push("=");
  for (const item of report) {
    lines.push(`\n#${item.rank} ${item.symbol}`);
    lines.push(`Location: ${item.file}:${item.line}:${item.column}`);
    lines.push(`RVA (bundle offset): ${item.rva_hex}`);
    lines.push(`Primary hits: ${item.primaryHits.join(",")}`);
    lines.push(`Hits: ${item.hits.join(",")}`);
    lines.push(`Window: ${item.window.start}-${item.window.end}`);
    lines.push("Pseudocode:");
    lines.push(item.pseudocode || "<empty>");
    lines.push("MASM Hook:");
    lines.push(item.masm_hook);
  }

  fs.writeFileSync(outTxt, lines.join("\n") + "\n");
  console.log("Snippet report written:", outJson, outTxt);
}

if (require.main === module) {
  main();
}
