#!/usr/bin/env node

/**
 * RawrZ Manifestation Script
 * Merges backend source files with frontend HTML files for complete deployment
 * Creates self-contained deployment packages
 */

const fs = require('fs').promises;
const path = require('path');
const crypto = require('crypto');

class ManifestationEngine {
  constructor() {
    this.rootDir = __dirname;
    this.publicDir = path.join(this.rootDir, 'public');
    this.srcDir = path.join(this.rootDir, 'src');
    this.manifestPath = path.join(this.publicDir, 'manifest.json');
  }

  async loadManifest() {
    try {
      const manifestData = await fs.readFile(this.manifestPath, 'utf8');
      return JSON.parse(manifestData);
    } catch (error) {
      throw new Error(`Failed to load manifest: ${error.message}`);
    }
  }

  async readSourceFile(filePath) {
    const fullPath = path.join(this.rootDir, filePath);
    try {
      return await fs.readFile(fullPath, 'utf8');
    } catch (error) {
      console.warn(`Warning: Could not read ${filePath}: ${error.message}`);
      return `// Error loading ${filePath}: ${error.message}`;
    }
  }

  async mergeBackendWithFrontend(panel, backendFiles) {
    const panelPath = path.join(this.publicDir, panel.file);
    let htmlContent;

    try {
      htmlContent = await fs.readFile(panelPath, 'utf8');
    } catch (error) {
      throw new Error(`Failed to read panel ${panel.file}: ${error.message}`);
    }

    // Collect all backend source code
    let backendCode = `// Merged Backend Code for ${panel.name}\n`;
    backendCode += `// Generated on ${new Date().toISOString()}\n\n`;

    for (const backendFile of backendFiles) {
      const code = await this.readSourceFile(backendFile);
      backendCode += `// === ${backendFile} ===\n`;
      backendCode += code + '\n\n';
    }

    // Create a script tag with the backend code
    const scriptTag = `<script>\n${backendCode}</script>\n`;

    // Insert the backend code before the closing </body> tag
    const mergedHtml = htmlContent.replace(
      /<\/body>/i,
      `${scriptTag}</body>`
    );

    return mergedHtml;
  }

  async createDeploymentPackage(manifest) {
    const timestamp = new Date().toISOString().replace(/[:.]/g, '-');
    const packageDir = path.join(this.rootDir, `deployment-${timestamp}`);

    try {
      await fs.mkdir(packageDir, { recursive: true });

      // Copy all public files
      const publicFiles = await fs.readdir(this.publicDir);
      for (const file of publicFiles) {
        if (file !== 'manifest.json') {
          const srcPath = path.join(this.publicDir, file);
          const destPath = path.join(packageDir, file);
          await fs.copyFile(srcPath, destPath);
        }
      }

      // Copy backend files
      const backendFiles = [
        'api-server.js',
        'rawrz-standalone.js',
        'server.js',
        'package.json',
        'package-lock.json'
      ];

      for (const file of backendFiles) {
        const srcPath = path.join(this.rootDir, file);
        const destPath = path.join(packageDir, file);
        try {
          await fs.copyFile(srcPath, destPath);
        } catch (error) {
          console.warn(`Warning: Could not copy ${file}: ${error.message}`);
        }
      }

      // Create merged HTML files
      const backendSourceFiles = [
        'api-server.js',
        'rawrz-standalone.js',
        'server.js'
      for (const panel of manifest.panels) {
        try {
          const mergedHtml = await this.mergeBackendWithFrontend(panel, backendSourceFiles);
          const mergedPath = path.join(packageDir, `merged-${panel.file}`);
          await fs.writeFile(mergedPath, mergedHtml, 'utf8');
          console.log(`✓ Merged ${panel.file} -> merged-${panel.file}`);
        } catch (error) {
          console.warn(`Warning: Failed to merge ${panel.file}: ${error.message}`);
        }
      }

      // Create deployment manifest
      const deploymentManifest = {
        ...manifest,
        deployment: {
          timestamp: new Date().toISOString(),
          packageDir: packageDir,
          mergedPanels: manifest.panels.map(p => `merged-${p.file}`)
        }
      };

      await fs.writeFile(
        path.join(packageDir, 'deployment-manifest.json'),
        JSON.stringify(deploymentManifest, null, 2),
        'utf8'
      );

      console.log(`\n✓ Deployment package created: ${packageDir}`);
      console.log(`✓ ${manifest.panels.length} panels merged with backend code`);
      console.log(`✓ Ready for deployment`);

      return packageDir;

    } catch (error) {
      throw new Error(`Failed to create deployment package: ${error.message}`);
    }
  }

  async run() {
    try {
      console.log('🔄 Loading manifest...');
      const manifest = await this.loadManifest();

      console.log(`📋 Found ${manifest.panels.length} panels to manifest`);
      console.log('🔄 Creating deployment package...');

      const packageDir = await this.createDeploymentPackage(manifest);

      console.log('\n✅ Manifestation complete!');
      console.log(`📦 Package: ${packageDir}`);
      console.log('🚀 Ready for deployment');

    } catch (error) {
      console.error('❌ Manifestation failed:', error.message);
      process.exit(1);
    }
  }
}

// Run if called directly
if (require.main === module) {
  const engine = new ManifestationEngine();
  engine.run();
}

module.exports = ManifestationEngine;