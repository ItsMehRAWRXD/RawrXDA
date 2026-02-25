// BigDaddyGEngine/builder/runtime/NodeEmbedder.ts
// Embeds Node runtime with the application

import fs from "fs";
import path from "path";
import { execSync } from "child_process";

export class NodeEmbedder {
  static async embed(sourceDir: string, outputDir: string) {
    console.log("[NodeEmbedder] Embedding Node runtime...");

    const nodeBin = this.findNodeBinary();
    const entryFile = path.join(outputDir, "run.js");
    const nodeCopy = path.join(outputDir, "node_runtime");

    // Copy Node binary
    fs.copyFileSync(nodeBin, nodeCopy);
    fs.chmodSync(nodeCopy, 0o755);

    // Create launcher script
    const launcherCode = `console.log('Launching embedded app...');\nrequire('./${path.relative(outputDir, sourceDir)}');`;
    fs.writeFileSync(entryFile, launcherCode);

    console.log("[NodeEmbedder] Node runtime embedded successfully");
  }

  private static findNodeBinary(): string {
    try {
      return execSync("which node").toString().trim();
    } catch {
      throw new Error("Node binary not found. Ensure Node is in PATH.");
    }
  }
}
