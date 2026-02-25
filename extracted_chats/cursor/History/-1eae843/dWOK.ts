// BigDaddyGEngine/builder/core/WebBuilder.ts
// Web builder that packages HTML/CSS/JS as standalone executable

import fs from "fs";
import path from "path";
import { Packager } from "./Packager";
import { BuilderBase } from "./BuilderBase";

export class WebBuilder extends BuilderBase {
  async build() {
    console.log("[Web] Copying web assets...");
    
    this.cleanOutput();
    
    // Copy all files from source to output
    this.copyRecursive(this.src, this.out);
    
    await this.package();
  }

  async package() {
    console.log("[Web] Packaging web application...");
    const packager = new Packager(this.out, "WebApp");
    await packager.bundle();
  }
}
