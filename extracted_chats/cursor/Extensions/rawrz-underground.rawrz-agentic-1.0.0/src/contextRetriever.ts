import * as vscode from 'vscode';
import * as fs from 'fs';
import * as path from 'path';

export class ContextRetriever {
    private contextEndpoint: string;

    constructor() {
        const config = vscode.workspace.getConfiguration('rawrz.context');
        this.contextEndpoint = config.get('endpoint', 'http://localhost:11440/context');
    }

    async getRelevantContext(code: string, filePath: string, languageId: string): Promise<string> {
        try {
            const response = await fetch(this.contextEndpoint, {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({
                    query: code,
                    currentFile: filePath,
                    language: languageId,
                    maxResults: 5
                })
            });

            if (!response.ok) {
                throw new Error(`Context server error: ${response.statusText}`);
            }

            const result = await response.json() as { context: string };
            return result.context || '';
        } catch (error) {
            console.warn('Context retrieval failed, continuing without context:', error);
            return '';
        }
    }

    async learnWorkspace(workspacePath: string, progress?: vscode.Progress<{ message?: string; increment?: number }>): Promise<void> {
        const learnEndpoint = this.contextEndpoint.replace('/context', '/learn');
        
        // Collect all code files
        const codeFiles = await this.collectCodeFiles(workspacePath, progress);
        
        // Send in batches
        const batchSize = 50;
        for (let i = 0; i < codeFiles.length; i += batchSize) {
            const batch = codeFiles.slice(i, i + batchSize);
            
            if (progress) {
                progress.report({ 
                    message: `Learning batch ${i / batchSize + 1}/${Math.ceil(codeFiles.length / batchSize)}` 
                });
            }

            await this.sendLearningBatch(learnEndpoint, batch);
        }
    }

    private async collectCodeFiles(rootPath: string, progress?: vscode.Progress<{ message?: string; increment?: number }>): Promise<string[]> {
        const files: string[] = [];
        const supportedExtensions = ['.js', '.ts', '.py', '.java', '.cpp', '.rs', '.go', '.cs', '.php', '.rb', '.asm', '.ps1'];
        
        async function scanDirectory(dir: string): Promise<void> {
            const entries = await fs.promises.readdir(dir, { withFileTypes: true });
            
            for (const entry of entries) {
                const fullPath = path.join(dir, entry.name);
                
                if (entry.isDirectory()) {
                    // Skip common excluded directories
                    if (!['node_modules', '.git', 'dist', 'build', 'target', 'bin', 'obj'].includes(entry.name)) {
                        await scanDirectory(fullPath);
                    }
                } else {
                    const ext = path.extname(entry.name).toLowerCase();
                    if (supportedExtensions.includes(ext)) {
                        try {
                            const stats = await fs.promises.stat(fullPath);
                            if (stats.size <= 1024 * 1024) { // 1MB max
                                const content = await fs.promises.readFile(fullPath, 'utf-8');
                                files.push(JSON.stringify({
                                    path: fullPath,
                                    content: content,
                                    language: ext.slice(1),
                                    size: stats.size
                                }));
                            }
                        } catch (err) {
                            console.warn(`Could not read file ${fullPath}:`, err);
                        }
                    }
                }
            }
        }

        await scanDirectory(rootPath);
        return files;
    }

    private async sendLearningBatch(endpoint: string, files: string[]): Promise<void> {
        try {
            await fetch(endpoint, {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ files })
            });
        } catch (error) {
            console.warn('Failed to send learning batch:', error);
        }
    }
}