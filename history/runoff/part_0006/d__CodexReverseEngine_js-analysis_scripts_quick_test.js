const path = require("path");
const { spawnSync } = require("child_process");

const sample = path.join(__dirname, "..", "examples", "sample.js");
const outDir = path.join(__dirname, "..", "analysis_output_sample");

const result = spawnSync("node", [path.join(__dirname, "analyze.js"), "--inputs", sample, "--out", outDir], {
  stdio: "inherit"
});

process.exit(result.status || 0);
