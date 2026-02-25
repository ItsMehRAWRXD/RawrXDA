// BigDaddyGEngine/builder/core/TypeScriptBuilder.ts
// TypeScript builder that compiles TS to JS and packages it

import { execSync } from "child_process";
import { Packager } from "./Packager";
import { BuilderBase } from "./BuilderBase";

export class TypeScriptBuilder extends BuilderBase {
  async build() {
    console.log("[TypeScript] Compiling TypeScript sources...");
    
    try {
      execSync(`tsc --outDir ${this.out}`, { stdio: "inherit" });
      await this.package();
    } catch (error: any) {
      console.error("[TypeScript] Compilation failed:", error.message);
      throw error;
    }
  }

  async package() {
    console.log("[TypeScript] Packaging compiled output...");
    const packager = new Packager(this.out, "TypeScriptApp");
    await packager.bundle();
  }
}
