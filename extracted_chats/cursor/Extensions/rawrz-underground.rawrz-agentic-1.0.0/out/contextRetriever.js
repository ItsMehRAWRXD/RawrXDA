"use strict";
var __createBinding = (this && this.__createBinding) || (Object.create ? (function(o, m, k, k2) {
    if (k2 === undefined) k2 = k;
    var desc = Object.getOwnPropertyDescriptor(m, k);
    if (!desc || ("get" in desc ? !m.__esModule : desc.writable || desc.configurable)) {
      desc = { enumerable: true, get: function() { return m[k]; } };
    }
    Object.defineProperty(o, k2, desc);
}) : (function(o, m, k, k2) {
    if (k2 === undefined) k2 = k;
    o[k2] = m[k];
}));
var __setModuleDefault = (this && this.__setModuleDefault) || (Object.create ? (function(o, v) {
    Object.defineProperty(o, "default", { enumerable: true, value: v });
}) : function(o, v) {
    o["default"] = v;
});
var __importStar = (this && this.__importStar) || (function () {
    var ownKeys = function(o) {
        ownKeys = Object.getOwnPropertyNames || function (o) {
            var ar = [];
            for (var k in o) if (Object.prototype.hasOwnProperty.call(o, k)) ar[ar.length] = k;
            return ar;
        };
        return ownKeys(o);
    };
    return function (mod) {
        if (mod && mod.__esModule) return mod;
        var result = {};
        if (mod != null) for (var k = ownKeys(mod), i = 0; i < k.length; i++) if (k[i] !== "default") __createBinding(result, mod, k[i]);
        __setModuleDefault(result, mod);
        return result;
    };
})();
Object.defineProperty(exports, "__esModule", { value: true });
exports.ContextRetriever = void 0;
const vscode = __importStar(require("vscode"));
const fs = __importStar(require("fs"));
const path = __importStar(require("path"));
class ContextRetriever {
    constructor() {
        const config = vscode.workspace.getConfiguration('rawrz.context');
        this.contextEndpoint = config.get('endpoint', 'http://localhost:11440/context');
    }
    async getRelevantContext(code, filePath, languageId) {
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
            const result = await response.json();
            return result.context || '';
        }
        catch (error) {
            console.warn('Context retrieval failed, continuing without context:', error);
            return '';
        }
    }
    async learnWorkspace(workspacePath, progress) {
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
    async collectCodeFiles(rootPath, progress) {
        const files = [];
        const supportedExtensions = ['.js', '.ts', '.py', '.java', '.cpp', '.rs', '.go', '.cs', '.php', '.rb', '.asm', '.ps1'];
        async function scanDirectory(dir) {
            const entries = await fs.promises.readdir(dir, { withFileTypes: true });
            for (const entry of entries) {
                const fullPath = path.join(dir, entry.name);
                if (entry.isDirectory()) {
                    // Skip common excluded directories
                    if (!['node_modules', '.git', 'dist', 'build', 'target', 'bin', 'obj'].includes(entry.name)) {
                        await scanDirectory(fullPath);
                    }
                }
                else {
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
                        }
                        catch (err) {
                            console.warn(`Could not read file ${fullPath}:`, err);
                        }
                    }
                }
            }
        }
        await scanDirectory(rootPath);
        return files;
    }
    async sendLearningBatch(endpoint, files) {
        try {
            await fetch(endpoint, {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ files })
            });
        }
        catch (error) {
            console.warn('Failed to send learning batch:', error);
        }
    }
}
exports.ContextRetriever = ContextRetriever;
//# sourceMappingURL=contextRetriever.js.map