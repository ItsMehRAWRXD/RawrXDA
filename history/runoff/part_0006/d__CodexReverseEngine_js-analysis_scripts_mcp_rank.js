const fs = require("fs");
const path = require("path");

const DEFAULT_WINDOW = 40;
const KEYWORDS = [
  "connect",
  "upgrade",
  "http",
  "https",
  "http2",
  "ws",
  "websocket",
  "socket",
  "request",
  "response",
  "server",
  "client",
  "listen",
  "fetch",
  "tls",
  "net"
];

function parseArgs(argv) {
  const args = { baseDir: "D:\\Cursor_Analysis_Results\\combined", window: DEFAULT_WINDOW };
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
    if (!arg.startsWith("-")) {
      args.baseDir = arg;
    }
  }
  return args;
}

function readJson(filePath) {
  return JSON.parse(fs.readFileSync(filePath, "utf8"));
}

function getSnippet(filePath, lineNumber, windowSize) {
  const content = fs.readFileSync(filePath, "utf8");
  const lines = content.split(/\r?\n/);
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
  for (const key of KEYWORDS) {
    if (lower.includes(key)) {
      score += 1;
      hits.push(key);
    }
  }
  return { score, hits: Array.from(new Set(hits)) };
}

function main() {
  const args = parseArgs(process.argv);
  const baseDir = path.resolve(args.baseDir);
  const locationsFile = path.join(baseDir, "mcp_entrypoint_locations.json");
  if (!fs.existsSync(locationsFile)) {
    console.error("Missing location report:", locationsFile);
    process.exit(1);
  }

  const locations = readJson(locationsFile);
  const ranked = [];

  for (const [name, entries] of Object.entries(locations)) {
    for (const entry of entries) {
      if (!entry.line || !entry.file) continue;
      const snippet = getSnippet(entry.file, entry.line, args.window);
      const score = scoreSnippet(snippet.text);
      ranked.push({
        name,
        file: entry.file,
        line: entry.line,
        column: entry.column,
        kind: entry.kind,
        score: score.score,
        hits: score.hits,
        window: { start: snippet.startLine, end: snippet.endLine }
      });
    }
  }

  ranked.sort((a, b) => b.score - a.score || a.name.localeCompare(b.name));

  const outJson = path.join(baseDir, "mcp_entrypoint_ranked.json");
  const outTxt = path.join(baseDir, "mcp_entrypoint_ranked.txt");

  fs.writeFileSync(outJson, JSON.stringify(ranked, null, 2));

  const lines = [];
  lines.push("MCP Entrypoint Transport Ranking");
  lines.push("=");
  for (const item of ranked.slice(0, 50)) {
    lines.push(
      `\n[${item.name}] score=${item.score} hits=${item.hits.join(",")} ` +
      `@ ${item.file}:${item.line}:${item.column} (${item.kind}) ` +
      `[window ${item.window.start}-${item.window.end}]`
    );
  }
  fs.writeFileSync(outTxt, lines.join("\n") + "\n");
  console.log("Ranking written:", outJson, outTxt);
}

if (require.main === module) {
  main();
}
