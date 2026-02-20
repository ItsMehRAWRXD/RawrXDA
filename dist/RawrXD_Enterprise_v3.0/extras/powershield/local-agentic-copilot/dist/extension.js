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
var __importDefault = (this && this.__importDefault) || function (mod) {
    return (mod && mod.__esModule) ? mod : { "default": mod };
};
Object.defineProperty(exports, "__esModule", { value: true });
exports.activate = activate;
exports.deactivate = deactivate;
const vscode = __importStar(require("vscode"));
const axios_1 = __importDefault(require("axios"));
class LocalAgenticCopilot {
    constructor() {
        this.completionProvider = null;
        this.statusBar = vscode.window.createStatusBarItem(vscode.StatusBarAlignment.Right, 100);
        this.config = {
            agenticMode: false,
            ollamaEndpoint: vscode.workspace.getConfiguration('agenticCopilot').get('ollamaEndpoint') || 'http://localhost:11434',
            agenticModel: vscode.workspace.getConfiguration('agenticCopilot').get('agenticModel') || 'cheetah-stealth-agentic:latest',
            standardModel: vscode.workspace.getConfiguration('agenticCopilot').get('standardModel') || 'bigdaddyg-fast:latest',
            temperature: 0.7
        };
    }
    async initialize(context) {
        // Register commands
        context.subscriptions.push(vscode.commands.registerCommand('agentic-copilot.toggleAgenticMode', () => this.toggleAgenticMode()), vscode.commands.registerCommand('agentic-copilot.generateCode', () => this.generateCode()), vscode.commands.registerCommand('agentic-copilot.explainCode', () => this.explainCode()), vscode.commands.registerCommand('agentic-copilot.fixCode', () => this.fixCode()), vscode.commands.registerCommand('agentic-copilot.showStatus', () => this.showStatus()));
        // Register inline completion provider
        if (vscode.workspace.getConfiguration('agenticCopilot').get('enableInlineCompletion')) {
            this.registerInlineCompletionProvider(context);
        }
        // Update status bar
        this.updateStatusBar();
        this.statusBar.show();
        // Listen for config changes
        context.subscriptions.push(vscode.workspace.onDidChangeConfiguration(e => this.onConfigChange(e)));
        vscode.window.showInformationMessage('🚀 Local Agentic Copilot initialized!');
    }
    registerInlineCompletionProvider(context) {
        const provider = {
            provideInlineCompletionItems: async (document, position, context, token) => {
                if (!this.config.agenticMode) {
                    return [];
                }
                const linePrefix = document.lineAt(position).text.substring(0, position.character);
                if (linePrefix.length < 5) {
                    return [];
                }
                try {
                    const completion = await this.getCompletion(linePrefix, document.languageId);
                    if (completion) {
                        return [new vscode.InlineCompletionItem(completion)];
                    }
                }
                catch (error) {
                    console.error('Completion error:', error);
                }
                return [];
            }
        };
        if (this.completionProvider) {
            this.completionProvider.dispose();
        }
        this.completionProvider = vscode.languages.registerInlineCompletionItemProvider({ pattern: '**' }, provider);
        context.subscriptions.push(this.completionProvider);
    }
    async toggleAgenticMode() {
        this.config.agenticMode = !this.config.agenticMode;
        try {
            // Test Ollama connection
            await axios_1.default.get(`${this.config.ollamaEndpoint}/api/tags`, { timeout: 3000 });
        }
        catch (error) {
            vscode.window.showErrorMessage('❌ Ollama is not running. Start Ollama to enable agentic mode.');
            this.config.agenticMode = false;
            return;
        }
        this.updateStatusBar();
        const mode = this.config.agenticMode ? '🚀 AGENTIC' : '⏸️  STANDARD';
        vscode.window.showInformationMessage(`Switched to ${mode} mode`);
    }
    async generateCode() {
        const editor = vscode.window.activeTextEditor;
        if (!editor) {
            vscode.window.showErrorMessage('No active editor');
            return;
        }
        const selection = editor.selection;
        const selectedText = editor.document.getText(selection);
        const prompt = await vscode.window.showInputBox({
            placeHolder: 'Describe the code you want to generate...'
        });
        if (!prompt)
            return;
        const model = this.config.agenticMode ? this.config.agenticModel : this.config.standardModel;
        try {
            vscode.window.showInformationMessage('⏳ Generating code...');
            const response = await axios_1.default.post(`${this.config.ollamaEndpoint}/api/generate`, {
                model: model,
                prompt: `Generate code for: ${prompt}\n\nContext:\n${selectedText}`,
                stream: false
            }, { timeout: 60000 });
            const generatedCode = response.data.response;
            if (selection.isEmpty) {
                editor.edit(editBuilder => {
                    editBuilder.insert(selection.active, generatedCode);
                });
            }
            else {
                editor.edit(editBuilder => {
                    editBuilder.replace(selection, generatedCode);
                });
            }
            vscode.window.showInformationMessage('✅ Code generated!');
        }
        catch (error) {
            const msg = error && typeof error === 'object' && 'message' in error ? String(error.message) : String(error);
            const code = error && typeof error === 'object' && 'code' in error ? error.code : '';
            const isNetwork = msg.includes('fetch failed') || msg.includes('ECONNREFUSED') || msg.includes('ENOTFOUND') || msg.includes('ETIMEDOUT') || code === 'ECONNREFUSED' || code === 'ENOTFOUND' || code === 'ETIMEDOUT';
            const friendly = isNetwork ? `Ollama unreachable at ${this.config.ollamaEndpoint}. Start Ollama and pull model "${this.config.agenticMode ? this.config.agenticModel : this.config.standardModel}".` : `Error: ${msg}`;
            vscode.window.showErrorMessage(friendly);
        }
    }
    async explainCode() {
        const editor = vscode.window.activeTextEditor;
        if (!editor) {
            vscode.window.showErrorMessage('No active editor');
            return;
        }
        const selection = editor.selection;
        const selectedText = editor.document.getText(selection) || editor.document.getText();
        const model = this.config.agenticMode ? this.config.agenticModel : this.config.standardModel;
        try {
            vscode.window.showInformationMessage('⏳ Analyzing code...');
            const response = await axios_1.default.post(`${this.config.ollamaEndpoint}/api/generate`, {
                model: model,
                prompt: `Explain this code in detail:\n\n${selectedText}`,
                stream: false
            }, { timeout: 60000 });
            const explanation = response.data.response;
            const panel = vscode.window.createWebviewPanel('codeExplanation', 'Code Explanation', vscode.ViewColumn.Two, {});
            panel.webview.html = `
                <!DOCTYPE html>
                <html>
                <head>
                    <style>
                        body { font-family: Arial; padding: 20px; line-height: 1.6; }
                        code { background: #f0f0f0; padding: 2px 6px; }
                        pre { background: #f0f0f0; padding: 10px; overflow-x: auto; }
                    </style>
                </head>
                <body>
                    <h2>Code Explanation</h2>
                    <pre>${explanation.replace(/</g, '&lt;').replace(/>/g, '&gt;')}</pre>
                </body>
                </html>
            `;
        }
        catch (error) {
            const msg = error && typeof error === 'object' && 'message' in error ? String(error.message) : String(error);
            const code = error && typeof error === 'object' && 'code' in error ? error.code : '';
            const isNetwork = msg.includes('fetch failed') || msg.includes('ECONNREFUSED') || msg.includes('ENOTFOUND') || msg.includes('ETIMEDOUT') || code === 'ECONNREFUSED' || code === 'ENOTFOUND' || code === 'ETIMEDOUT';
            const friendly = isNetwork
                ? `RawrXD Explanation: Cannot reach Ollama at ${this.config.ollamaEndpoint}. Start Ollama (e.g. \`ollama serve\`) and ensure the model "${model}" is pulled.`
                : `RawrXD Explanation failed: ${msg}`;
            vscode.window.showErrorMessage(friendly);
        }
    }
    async fixCode() {
        const editor = vscode.window.activeTextEditor;
        if (!editor) {
            vscode.window.showErrorMessage('No active editor');
            return;
        }
        const selectedText = editor.document.getText(editor.selection) || editor.document.getText();
        const model = this.config.agenticMode ? this.config.agenticModel : this.config.standardModel;
        try {
            vscode.window.showInformationMessage('⏳ Analyzing code for issues...');
            const response = await axios_1.default.post(`${this.config.ollamaEndpoint}/api/generate`, {
                model: model,
                prompt: `Fix any issues in this code and explain the changes:\n\n${selectedText}`,
                stream: false
            }, { timeout: 60000 });
            const fixedCode = response.data.response;
            const selection = editor.selection;
            editor.edit(editBuilder => {
                if (selection.isEmpty) {
                    editBuilder.insert(selection.active, fixedCode);
                }
                else {
                    editBuilder.replace(selection, fixedCode);
                }
            });
            vscode.window.showInformationMessage('✅ Code fixed!');
        }
        catch (error) {
            const msg = error && typeof error === 'object' && 'message' in error ? String(error.message) : String(error);
            const code = error && typeof error === 'object' && 'code' in error ? error.code : '';
            const isNetwork = msg.includes('fetch failed') || msg.includes('ECONNREFUSED') || msg.includes('ENOTFOUND') || msg.includes('ETIMEDOUT') || code === 'ECONNREFUSED' || code === 'ENOTFOUND' || code === 'ETIMEDOUT';
            const friendly = isNetwork ? `Ollama unreachable at ${this.config.ollamaEndpoint}. Start Ollama to fix code.` : `Error: ${msg}`;
            vscode.window.showErrorMessage(friendly);
        }
    }
    async getCompletion(linePrefix, language) {
        try {
            const model = this.config.agenticMode ? this.config.agenticModel : this.config.standardModel;
            const response = await axios_1.default.post(`${this.config.ollamaEndpoint}/api/generate`, {
                model: model,
                prompt: `Complete this ${language} code (only return the completion, no explanation):\n${linePrefix}`,
                stream: false,
                options: {
                    temperature: this.config.temperature,
                    num_predict: 50
                }
            }, { timeout: 10000 });
            return response.data.response.trim().split('\n')[0];
        }
        catch (error) {
            return null;
        }
    }
    updateStatusBar() {
        const status = this.config.agenticMode ? '🚀 AGENTIC' : '⏸️  STANDARD';
        const color = this.config.agenticMode ? '#00FF00' : '#CCCCCC';
        this.statusBar.text = `${status}`;
        this.statusBar.color = color;
        this.statusBar.command = 'agentic-copilot.toggleAgenticMode';
        this.statusBar.tooltip = 'Click to toggle agentic mode (Ctrl+Shift+A)';
    }
    showStatus() {
        vscode.window.showInformationMessage(this.config.agenticMode
            ? `🚀 Agentic Mode: ACTIVE\nModel: ${this.config.agenticModel}`
            : `⏸️  Standard Mode: ACTIVE\nModel: ${this.config.standardModel}`);
    }
    onConfigChange(e) {
        if (e.affectsConfiguration('agenticCopilot')) {
            const config = vscode.workspace.getConfiguration('agenticCopilot');
            this.config.ollamaEndpoint = config.get('ollamaEndpoint') || this.config.ollamaEndpoint;
            this.config.agenticModel = config.get('agenticModel') || this.config.agenticModel;
            this.config.standardModel = config.get('standardModel') || this.config.standardModel;
        }
    }
}
let copilot;
function activate(context) {
    copilot = new LocalAgenticCopilot();
    copilot.initialize(context);
}
function deactivate() { }
