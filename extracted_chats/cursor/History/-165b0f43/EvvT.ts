// BigDaddyGEngine/builder/core/JavaBuilder.ts
// Java builder that compiles Java source files

import { execSync } from "child_process";
import { Packager } from "./Packager";
import { BuilderBase } from "./BuilderBase";

export class JavaBuilder extends BuilderBase {
  async build() {
    console.log("[Java] Compiling Java sources...");
    
    this.cleanOutput();
    
    try {
      // Use javac to compile Java files
      execSync(`javac -d ${this.out} ${this.src}/*.java`, { stdio: "inherit" });
      await this.package();
    } catch (error: any) {
      console.error("[Java] Compilation failed:", error.message);
      throw error;
    }
  }

  async package() {
    console.log("[Java] Packaging compiled output...");
    const packager = new Packager(this.out, "JavaApp");
    await packager.bundle();
  }
}
