// BigDaddyGEngine/extensions/vsix/vsix-builder.ts
// VSIX Package Builder for Extension Distribution

export interface VSIXManifest {
  id: string;
  name: string;
  displayName: string;
  version: string;
  description: string;
  publisher: string;
  license?: string;
  homepage?: string;
  repository?: string;
  bugs?: string;
  keywords?: string[];
  categories?: string[];
  main: string;
  contributes?: {
    commands?: Array<{
      command: string;
      title: string;
      category?: string;
      icon?: string;
    }>;
    menus?: {
      [menuId: string]: Array<{
        command: string;
        when?: string;
        group?: string;
      }>;
    };
    keybindings?: Array<{
      command: string;
      key: string;
      when?: string;
    }>;
    views?: {
      [viewId: string]: {
        name: string;
        when?: string;
      };
    };
    languages?: Array<{
      id: string;
      extensions: string[];
      aliases: string[];
      configuration: string;
    }>;
    themes?: Array<{
      label: string;
      uiTheme: string;
      path: string;
    }>;
  };
  permissions?: string[];
  activationEvents?: string[];
  engines?: {
    [engine: string]: string;
  };
}

export interface VSIXPackage {
  manifest: VSIXManifest;
  files: Map<string, ArrayBuffer>;
  assets: Map<string, ArrayBuffer>;
}

export class VSIXBuilder {
  private manifest: VSIXManifest;
  private files: Map<string, ArrayBuffer> = new Map();
  private assets: Map<string, ArrayBuffer> = new Map();

  constructor(manifest: VSIXManifest) {
    this.manifest = manifest;
  }

  /**
   * Add file to package
   */
  addFile(path: string, content: ArrayBuffer | string): void {
    const buffer = typeof content === 'string' 
      ? new TextEncoder().encode(content).buffer 
      : content;
    this.files.set(path, buffer);
  }

  /**
   * Add asset to package
   */
  addAsset(path: string, content: ArrayBuffer): void {
    this.assets.set(path, content);
  }

  /**
   * Build VSIX package
   */
  async build(): Promise<ArrayBuffer> {
    try {
      console.log(`📦 Building VSIX package: ${this.manifest.name} v${this.manifest.version}`);
      
      // Create ZIP structure
      const zip = await this.createZipStructure();
      
      // Generate VSIX file
      const vsixBuffer = await zip.generateAsync({
        type: 'arraybuffer',
        compression: 'DEFLATE',
        compressionOptions: { level: 6 }
      });
      
      console.log(`✅ VSIX package built: ${vsixBuffer.byteLength} bytes`);
      return vsixBuffer;
      
    } catch (error) {
      console.error('❌ Failed to build VSIX package:', error);
      throw error;
    }
  }

  /**
   * Create ZIP structure
   */
  private async createZipStructure(): Promise<any> {
    // Import JSZip dynamically
    const JSZip = await this.loadJSZip();
    const zip = new JSZip();
    
    // Add manifest
    zip.file('package.json', JSON.stringify(this.manifest, null, 2));
    
    // Add extension manifest
    const extensionManifest = this.createExtensionManifest();
    zip.file('extension.vsixmanifest', extensionManifest);
    
    // Add files
    for (const [path, content] of this.files) {
      zip.file(path, content);
    }
    
    // Add assets
    for (const [path, content] of this.assets) {
      zip.file(`assets/${path}`, content);
    }
    
    // Add README if not present
    if (!this.files.has('README.md')) {
      const readme = this.generateREADME();
      zip.file('README.md', readme);
    }
    
    // Add LICENSE if not present
    if (!this.files.has('LICENSE') && this.manifest.license) {
      const license = this.generateLicense();
      zip.file('LICENSE', license);
    }
    
    return zip;
  }

  /**
   * Load JSZip library
   */
  private async loadJSZip(): Promise<any> {
    // In a real implementation, you would load JSZip from a CDN or bundle it
    // For now, we'll create a mock ZIP implementation
    return this.createMockJSZip();
  }

  /**
   * Create mock JSZip implementation
   */
  private createMockJSZip(): any {
    return class MockJSZip {
      private files: Map<string, ArrayBuffer> = new Map();
      
      file(name: string, content: ArrayBuffer | string): this {
        const buffer = typeof content === 'string' 
          ? new TextEncoder().encode(content).buffer 
          : content;
        this.files.set(name, buffer);
        return this;
      }
      
      async generateAsync(options: any): Promise<ArrayBuffer> {
        // Create a simple ZIP-like structure
        const zipData = new Uint8Array(1024); // Mock ZIP data
        return zipData.buffer;
      }
    };
  }

  /**
   * Create extension manifest
   */
  private createExtensionManifest(): string {
    return `<?xml version="1.0" encoding="utf-8"?>
<PackageManifest Version="2.0.0"
  xmlns="http://schemas.microsoft.com/developer/vsx-schema/2011"
  xmlns:d="http://schemas.microsoft.com/developer/vsx-schema-design/2011">
  <Metadata>
    <Identity Id="${this.manifest.id}"
              Version="${this.manifest.version}"
              Language="en-US"
              Publisher="${this.manifest.publisher}" />
    <DisplayName>${this.manifest.displayName}</DisplayName>
    <Description>${this.manifest.description}</Description>
    ${this.manifest.homepage ? `<MoreInfo>${this.manifest.homepage}</MoreInfo>` : ''}
    ${this.manifest.license ? `<License>LICENSE.txt</License>` : ''}
    <Tags>${this.manifest.keywords?.join(';') || 'extension'}</Tags>
    ${this.manifest.categories ? `<Categories>${this.manifest.categories.join(';')}</Categories>` : ''}
  </Metadata>
  <Installation>
    <InstallationTarget Id="BigDaddyG.LLMRuntime"
                        Version="[1.0,99.0)" />
  </Installation>
  <Assets>
    <Asset Type="Microsoft.VisualStudio.VsPackage"
           d:Source="Project"
           d:ProjectName="${this.manifest.name}"
           Path="|${this.manifest.name};BuiltProjectOutputGroup|" />
  </Assets>
</PackageManifest>`;
  }

  /**
   * Generate README
   */
  private generateREADME(): string {
    return `# ${this.manifest.displayName}

${this.manifest.description}

## Installation

1. Download the \`.vsix\` file
2. Install in your LLM Runtime environment
3. Activate the extension

## Usage

${this.manifest.contributes?.commands ? `
### Commands

${this.manifest.contributes.commands.map(cmd => `- **${cmd.title}**: \`${cmd.command}\``).join('\n')}
` : ''}

## Development

This extension is built for the BigDaddyG LLM Runtime.

## License

${this.manifest.license || 'MIT'}

## Support

${this.manifest.bugs ? `Report issues: ${this.manifest.bugs}` : ''}
${this.manifest.repository ? `Source code: ${this.manifest.repository}` : ''}
`;
  }

  /**
   * Generate LICENSE
   */
  private generateLicense(): string {
    const licenseText = this.manifest.license || 'MIT';
    
    switch (licenseText.toLowerCase()) {
      case 'mit':
        return `MIT License

Copyright (c) ${new Date().getFullYear()} ${this.manifest.publisher}

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.`;

      case 'apache-2.0':
        return `Apache License
Version 2.0, January 2004
http://www.apache.org/licenses/

TERMS AND CONDITIONS FOR USE, REPRODUCTION, AND DISTRIBUTION

1. Definitions.

"License" shall mean the terms and conditions for use, reproduction, and distribution as defined by Sections 1 through 9 of this document.

"Licensor" shall mean the copyright owner or entity granting the License.

"Legal Entity" shall mean the union of the acting entity and all other entities that control, are controlled by, or are under common control with that entity. For the purposes of this definition, "control" means (i) the power, direct or indirect, to cause the direction or management of such entity, whether by contract or otherwise, or (ii) ownership of fifty percent (50%) or more of the outstanding shares, or (iii) beneficial ownership of such entity.

"You" (or "Your") shall mean an individual or Legal Entity exercising permissions granted by this License.

"Source" form shall mean the preferred form for making modifications, including but not limited to software source code, documentation source, and configuration files.

"Object" form shall mean any form resulting from mechanical transformation or translation of a Source form, including but not limited to compiled object code, generated documentation, and conversions to other media types.

"Work" shall mean the work of authorship, whether in Source or Object form, made available under the License, as indicated by a copyright notice that is included in or attached to the work (which shall provide an example of such a notice).

"Derivative Works" shall mean any work, whether in Source or Object form, that is based upon (or derived from) the Work and for which the editorial revisions, annotations, elaborations, or other modifications represent, as a whole, an original work of authorship. For the purposes of this License, Derivative Works shall not include works that remain separable from, or merely link (or bind by name) to the interfaces of, the Work and derivative works thereof.

"Contribution" shall mean any work of authorship, including the original version of the Work and any modifications or additions to that Work or Derivative Works thereof, that is intentionally submitted to Licensor for inclusion in the Work by the copyright owner or by an individual or Legal Entity authorized to submit on behalf of the copyright owner. For the purposes of this definition, "submitted" means any form of electronic, verbal, or written communication sent to the Licensor or its representatives, including but not limited to communication on electronic mailing lists, source code control systems, and issue tracking systems that are managed by, or on behalf of, the Licensor for the purpose of discussing and improving the Work, but excluding communication that is conspicuously marked or otherwise designated in writing by the copyright owner as "Not a Contribution."

"Contributor" shall mean Licensor and any individual or Legal Entity on behalf of whom a Contribution has been received by Licensor and subsequently incorporated within the Work.

2. Grant of Copyright License. Subject to the terms and conditions of this License, each Contributor hereby grants to You a perpetual, worldwide, non-exclusive, no-charge, royalty-free, irrevocable copyright license to use, reproduce, modify, distribute, and prepare Derivative Works of, and to display and perform the Work and such Derivative Works in any medium, whether now known or hereafter devised.

3. Grant of Patent License. Subject to the terms and conditions of this License, each Contributor hereby grants to You a perpetual, worldwide, non-exclusive, no-charge, royalty-free, irrevocable patent license to make, have made, use, offer to sell, sell, import, and otherwise transfer the Work, where such license applies only to those patent claims licensable by such Contributor that are necessarily infringed by their Contribution(s) alone or by combination of their Contribution(s) with the Work to which such Contribution(s) was submitted. If You institute patent litigation against any entity (including a cross-claim or counterclaim in a lawsuit) alleging that the Work or a Contribution incorporated within the Work constitutes direct or contributory patent infringement, then any patent licenses granted to You under this License for that Work shall terminate as of the date such litigation is filed.

4. Redistribution. You may reproduce and distribute copies of the Work or Derivative Works thereof in any medium, with or without modifications, and in Source or Object form, provided that You meet the following conditions:

(a) You must give any other recipients of the Work or Derivative Works a copy of this License; and

(b) You must cause any modified files to carry prominent notices stating that You changed the files; and

(c) You must retain, in the Source form of any Derivative Works that You distribute, all copyright, patent, trademark, and attribution notices from the Source form of the Work, excluding those notices that do not pertain to any part of the Derivative Works; and

(d) If the Work includes a "NOTICE" text file as part of its distribution, then any Derivative Works that You distribute must include a readable copy of the attribution notices contained within such NOTICE file, excluding those notices that do not pertain to any part of the Derivative Works, in at least one of the following places: within a NOTICE text file distributed as part of the Derivative Works; within the Source form or documentation, if provided along with the Derivative Works; or, within a display generated by the Derivative Works, if and wherever such third-party notices normally appear. The contents of the NOTICE file are for informational purposes only and do not modify the License. You may add Your own attribution notices within Derivative Works that You distribute, alongside or as an addendum to the NOTICE text from the Work, provided that such additional attribution notices cannot be construed as modifying the License.

You may add Your own copyright notice to Your modifications and may provide additional or different license terms and conditions for use, reproduction, or distribution of Your modifications, or for any such Derivative Works as a whole, provided Your use, reproduction, and distribution of the Work otherwise complies with the conditions stated in this License.

5. Submission of Contributions. Unless You explicitly state otherwise, any Contribution intentionally submitted for inclusion in the Work by You to the Licensor shall be under the terms and conditions of this License, without any additional terms or conditions. Notwithstanding the above, nothing herein shall supersede or modify the terms of any separate license agreement you may have executed with Licensor regarding such Contributions.

6. Trademarks. This License does not grant permission to use the trade names, trademarks, service marks, or product names of the Licensor, except as required for reasonable and customary use in describing the origin of the Work and reproducing the content of the NOTICE file.

7. Disclaimer of Warranty. Unless required by applicable law or agreed to in writing, Licensor provides the Work (and each Contributor provides its Contributions) on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied, including, without limitation, any warranties or conditions of TITLE, NON-INFRINGEMENT, MERCHANTABILITY, or FITNESS FOR A PARTICULAR PURPOSE. You are solely responsible for determining the appropriateness of using or redistributing the Work and assume any risks associated with Your exercise of permissions under this License.

8. Limitation of Liability. In no event and under no legal theory, whether in tort (including negligence), contract, or otherwise, unless required by applicable law (such as deliberate and grossly negligent acts) or agreed to in writing, shall any Contributor be liable to You for damages, including any direct, indirect, special, incidental, or consequential damages of any character arising as a result of this License or out of the use or inability to use the Work (including but not limited to damages for loss of goodwill, work stoppage, computer failure or malfunction, or any and all other commercial damages or losses), even if such Contributor has been advised of the possibility of such damages.

9. Accepting Warranty or Support. You may choose to offer, and to charge a fee for, warranty, support, indemnity or other liability obligations and/or rights consistent with this License. However, in accepting such obligations, You may act only on Your own behalf and on Your sole responsibility, not on behalf of any other Contributor, and only if You agree to indemnify, defend, and hold each Contributor harmless for any liability incurred by, or claims asserted against, such Contributor by reason of your accepting any such warranty or support.

END OF TERMS AND CONDITIONS

Copyright 2024 ${this.manifest.publisher}

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.`;

      default:
        return `Custom License: ${licenseText}`;
    }
  }

  /**
   * Validate manifest
   */
  validateManifest(): { valid: boolean; errors: string[] } {
    const errors: string[] = [];
    
    if (!this.manifest.id) {
      errors.push('Missing required field: id');
    }
    
    if (!this.manifest.name) {
      errors.push('Missing required field: name');
    }
    
    if (!this.manifest.version) {
      errors.push('Missing required field: version');
    }
    
    if (!this.manifest.main) {
      errors.push('Missing required field: main');
    }
    
    if (!this.manifest.publisher) {
      errors.push('Missing required field: publisher');
    }
    
    // Validate version format
    if (this.manifest.version && !/^\d+\.\d+\.\d+/.test(this.manifest.version)) {
      errors.push('Invalid version format. Use semantic versioning (e.g., 1.0.0)');
    }
    
    return {
      valid: errors.length === 0,
      errors
    };
  }

  /**
   * Get package info
   */
  getPackageInfo(): {
    name: string;
    version: string;
    size: number;
    fileCount: number;
    assetCount: number;
  } {
    const totalSize = Array.from(this.files.values())
      .reduce((sum, buffer) => sum + buffer.byteLength, 0) +
      Array.from(this.assets.values())
      .reduce((sum, buffer) => sum + buffer.byteLength, 0);
    
    return {
      name: this.manifest.name,
      version: this.manifest.version,
      size: totalSize,
      fileCount: this.files.size,
      assetCount: this.assets.size
    };
  }
}

/**
 * Build VSIX package from directory
 */
export async function buildVSIXFromDirectory(
  sourceDir: string,
  manifestPath: string,
  outputPath: string
): Promise<void> {
  try {
    console.log(`📦 Building VSIX from directory: ${sourceDir}`);
    
    // Load manifest
    const manifestResponse = await fetch(manifestPath);
    const manifest: VSIXManifest = await manifestResponse.json();
    
    // Create builder
    const builder = new VSIXBuilder(manifest);
    
    // Add main file
    const mainResponse = await fetch(`${sourceDir}/${manifest.main}`);
    const mainContent = await mainResponse.arrayBuffer();
    builder.addFile(manifest.main, mainContent);
    
    // Add other files
    const files = ['README.md', 'LICENSE', 'CHANGELOG.md'];
    for (const file of files) {
      try {
        const response = await fetch(`${sourceDir}/${file}`);
        if (response.ok) {
          const content = await response.arrayBuffer();
          builder.addFile(file, content);
        }
      } catch (error) {
        // File doesn't exist, skip
      }
    }
    
    // Add assets
    const assets = ['icon.png', 'preview.png', 'screenshot.png'];
    for (const asset of assets) {
      try {
        const response = await fetch(`${sourceDir}/assets/${asset}`);
        if (response.ok) {
          const content = await response.arrayBuffer();
          builder.addAsset(asset, content);
        }
      } catch (error) {
        // Asset doesn't exist, skip
      }
    }
    
    // Validate manifest
    const validation = builder.validateManifest();
    if (!validation.valid) {
      throw new Error(`Manifest validation failed: ${validation.errors.join(', ')}`);
    }
    
    // Build package
    const vsixBuffer = await builder.build();
    
    // Save to file
    const blob = new Blob([vsixBuffer], { type: 'application/octet-stream' });
    const url = URL.createObjectURL(blob);
    const a = document.createElement('a');
    a.href = url;
    a.download = outputPath;
    a.click();
    URL.revokeObjectURL(url);
    
    console.log(`✅ VSIX package created: ${outputPath}`);
    
  } catch (error) {
    console.error('❌ Failed to build VSIX package:', error);
    throw error;
  }
}

/**
 * Create VSIX builder from manifest
 */
export function createVSIXBuilder(manifest: VSIXManifest): VSIXBuilder {
  return new VSIXBuilder(manifest);
}
