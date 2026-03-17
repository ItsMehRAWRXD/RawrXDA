const fs = require("fs");
const path = require("path");

function readLines(filePath) {
  return fs
    .readFileSync(filePath, "utf8")
    .split(/\r?\n/)
    .map((l) => l.trim())
    .filter(Boolean);
}

function loadJson(filePath) {
  return JSON.parse(fs.readFileSync(filePath, "utf8"));
}

function scoreTransportCandidate(name) {
  const lower = name.toLowerCase();
  const keywords = [
    "http",
    "https",
    "fetch",
    "request",
    "upgrade",
    "websocket",
    "ws",
    "socket",
    "net",
    "http2",
    "tls",
    "connect",
    "listen",
    "server",
    "client"
  ];
  let score = 0;
  for (const key of keywords) {
    if (lower.includes(key)) score += 1;
  }
  return score;
}

function buildReport(entrypoints, callGraph) {
  const report = {
    generated_at: new Date().toISOString(),
    entrypoint_count: entrypoints.length,
    entrypoints: []
  };

  for (const entrypoint of entrypoints) {
    const callees = callGraph[entrypoint] || [];
    const scored = callees
      .map((name) => ({ name, score: scoreTransportCandidate(name) }))
      .filter((item) => item.score > 0)
      .sort((a, b) => b.score - a.score || a.name.localeCompare(b.name));

    report.entrypoints.push({
      name: entrypoint,
      callee_count: callees.length,
      callees: callees.slice().sort(),
      transport_candidates: scored.slice(0, 25)
    });
  }

  return report;
}

function main() {
  const baseDir = process.argv[2] || "D:\\Cursor_Analysis_Results\\combined";
  const entryFile = path.join(baseDir, "mcp_entrypoints.txt");
  const graphFile = path.join(baseDir, "call_graph.json");

  if (!fs.existsSync(entryFile) || !fs.existsSync(graphFile)) {
    console.error("Missing inputs. Expected:", entryFile, "and", graphFile);
    process.exit(1);
  }

  const entrypoints = readLines(entryFile);
  const callGraph = loadJson(graphFile);
  const report = buildReport(entrypoints, callGraph);

  const outJson = path.join(baseDir, "mcp_entrypoint_report.json");
  const outTxt = path.join(baseDir, "mcp_entrypoint_report.txt");

  fs.writeFileSync(outJson, JSON.stringify(report, null, 2));

  const lines = [];
  lines.push(`MCP Entrypoint Report (${report.generated_at})`);
  lines.push("=");
  for (const entry of report.entrypoints) {
    lines.push(`\n[${entry.name}]`);
    lines.push(`Callees: ${entry.callee_count}`);
    if (entry.transport_candidates.length) {
      lines.push("Likely transport handlers:");
      for (const cand of entry.transport_candidates) {
        lines.push(`  - ${cand.name} (score ${cand.score})`);
      }
    } else {
      lines.push("Likely transport handlers: none detected");
    }
  }

  fs.writeFileSync(outTxt, lines.join("\n") + "\n");
  console.log("Report written:", outJson, outTxt);
}

if (require.main === module) {
  main();
}
