import * as vscode from 'vscode';
import * as fs from 'fs';
import * as path from 'path';

export class DesktopSearchEnhancer {
  static async enhanceDesktopSearch(): Promise<void> {
    const desktopPath = path.join(require('os').homedir(), 'Desktop');
    
    // Create enhanced search index
    const searchIndex = await this.createUnlimitedIndex(desktopPath);
    
    // Register search provider
    vscode.workspace.registerFileSearchProvider('desktop', {
      provideFileSearchResults: async (query, options, token) => {
        const results: vscode.Uri[] = [];
        
        for (const file of searchIndex) {
          if (file.toLowerCase().includes(query.pattern.toLowerCase())) {
            results.push(vscode.Uri.file(file));
          }
          
          if (results.length > 100000) break; // Unlimited results
        }
        
        return results;
      }
    });
    
    vscode.window.showInformationMessage('Desktop search enhanced with unlimited capabilities!');
  }

  private static async createUnlimitedIndex(dir: string): Promise<string[]> {
    const files: string[] = [];
    
    const scan = async (currentDir: string): Promise<void> => {
      try {
        const items = fs.readdirSync(currentDir, { withFileTypes: true });
        
        for (const item of items) {
          const fullPath = path.join(currentDir, item.name);
          
          if (item.isDirectory()) {
            await scan(fullPath); // Unlimited depth
          } else {
            files.push(fullPath);
          }
        }
      } catch (e) {
        // Continue on errors
      }
    };
    
    await scan(dir);
    return files;
  }

  static enhanceWorkspaceSearch(): void {
    const config = vscode.workspace.getConfiguration();
    
    // Remove all search limitations
    config.update('search.exclude', {}, vscode.ConfigurationTarget.Workspace);
    config.update('files.exclude', {}, vscode.ConfigurationTarget.Workspace);
    config.update('search.useParentIgnoreFiles', false, vscode.ConfigurationTarget.Workspace);
    config.update('search.useIgnoreFiles', false, vscode.ConfigurationTarget.Workspace);
    
    vscode.window.showInformationMessage('Workspace search limits removed!');
  }
}