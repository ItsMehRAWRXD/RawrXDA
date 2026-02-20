import * as vscode from 'vscode';
import axios from 'axios';

interface AgenticConfig {
  agenticMode: boolean;
  ollamaEndpoint: string;
  agenticModel: string;
  standardModel: string;
  temperature: number;
}

class LocalAgenticCopilot {
  private statusBar: vscode.StatusBarItem;
  private config: AgenticConfig;
  private completionProvider: vscode.Disposable | null = null;

  constructor() {
    this.statusBar = vscode.window.createStatusBarItem(vscode.StatusBarAlignment.Right, 100);
    this.config = {
      agenticMode: false,
      ollamaEndpoint: vscode.workspace.getConfiguration('agenticCopilot').get('ollamaEndpoint') || 'http://localhost:11434',
      agenticModel: vscode.workspace.getConfiguration('agenticCopilot').get('agenticModel') || 'cheetah-stealth-agentic:latest',
      standardModel: vscode.workspace.getConfiguration('agenticCopilot').get('standardModel') || 'bigdaddyg-fast:latest',
      temperature: 0.7
    };
  }

  async initialize(context: vscode.ExtensionContext) {
    // Register commands
    context.subscriptions.push(
      vscode.commands.registerCommand('agentic-copilot.toggleAgenticMode', () => this.toggleAgenticMode()),
      vscode.commands.registerCommand('agentic-copilot.generateCode', () => this.generateCode()),
      vscode.commands.registerCommand('agentic-copilot.explainCode', () => this.explainCode()),
      vscode.commands.registerCommand('agentic-copilot.fixCode', () => this.fixCode()),
      vscode.commands.registerCommand('agentic-copilot.showStatus', () => this.showStatus())
    );

    // Register inline completion provider
    if (vscode.workspace.getConfiguration('agenticCopilot').get('enableInlineCompletion')) {
      this.registerInlineCompletionProvider(context);
    }

    // Update status bar
    this.updateStatusBar();
    this.statusBar.show();

    // Listen for config changes
    context.subscriptions.push(
      vscode.workspace.onDidChangeConfiguration(e => this.onConfigChange(e))
    );

    vscode.window.showInformationMessage('🚀 Local Agentic Copilot initialized!');
  }

  private registerInlineCompletionProvider(context: vscode.ExtensionContext) {
    const provider: vscode.InlineCompletionItemProvider = {
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
        } catch (error) {
          console.error('Completion error:', error);
        }

        return [];
      }
    };

    if (this.completionProvider) {
      this.completionProvider.dispose();
    }

    this.completionProvider = vscode.languages.registerInlineCompletionItemProvider(
      { pattern: '**' },
      provider
    );
    context.subscriptions.push(this.completionProvider);
  }

  private async toggleAgenticMode() {
    this.config.agenticMode = !this.config.agenticMode;

    try {
      // Test Ollama connection
      await axios.get(`${this.config.ollamaEndpoint}/api/tags`, { timeout: 3000 });
    } catch (error) {
      vscode.window.showErrorMessage('❌ Ollama is not running. Start Ollama to enable agentic mode.');
      this.config.agenticMode = false;
      return;
    }

    this.updateStatusBar();

    const mode = this.config.agenticMode ? '🚀 AGENTIC' : '⏸️  STANDARD';
    vscode.window.showInformationMessage(`Switched to ${mode} mode`);
  }

  private async generateCode() {
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

    if (!prompt) return;

    const model = this.config.agenticMode ? this.config.agenticModel : this.config.standardModel;

    try {
      vscode.window.showInformationMessage('⏳ Generating code...');

      const response = await axios.post(`${this.config.ollamaEndpoint}/api/generate`, {
        model: model,
        prompt: `Generate code for: ${prompt}\n\nContext:\n${selectedText}`,
        stream: false
      }, { timeout: 60000 });

      const generatedCode = response.data.response;

      if (selection.isEmpty) {
        editor.edit(editBuilder => {
          editBuilder.insert(selection.active, generatedCode);
        });
      } else {
        editor.edit(editBuilder => {
          editBuilder.replace(selection, generatedCode);
        });
      }

      vscode.window.showInformationMessage('✅ Code generated!');
    } catch (error: unknown) {
      const msg = error && typeof error === 'object' && 'message' in error ? String((error as Error).message) : String(error);
      const code = error && typeof error === 'object' && 'code' in error ? (error as NodeJS.ErrnoException).code : '';
      const isNetwork = msg.includes('fetch failed') || msg.includes('ECONNREFUSED') || msg.includes('ENOTFOUND') || msg.includes('ETIMEDOUT') || code === 'ECONNREFUSED' || code === 'ENOTFOUND' || code === 'ETIMEDOUT';
      const friendly = isNetwork ? `Ollama unreachable at ${this.config.ollamaEndpoint}. Start Ollama and pull model "${this.config.agenticMode ? this.config.agenticModel : this.config.standardModel}".` : `Error: ${msg}`;
      vscode.window.showErrorMessage(friendly);
    }
  }

  private async explainCode() {
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

      const response = await axios.post(`${this.config.ollamaEndpoint}/api/generate`, {
        model: model,
        prompt: `Explain this code in detail:\n\n${selectedText}`,
        stream: false
      }, { timeout: 60000 });

      const explanation = response.data.response;

      const panel = vscode.window.createWebviewPanel(
        'codeExplanation',
        'Code Explanation',
        vscode.ViewColumn.Two,
        {}
      );

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
    } catch (error: unknown) {
      const msg = error && typeof error === 'object' && 'message' in error ? String((error as Error).message) : String(error);
      const code = error && typeof error === 'object' && 'code' in error ? (error as NodeJS.ErrnoException).code : '';
      const isNetwork = msg.includes('fetch failed') || msg.includes('ECONNREFUSED') || msg.includes('ENOTFOUND') || msg.includes('ETIMEDOUT') || code === 'ECONNREFUSED' || code === 'ENOTFOUND' || code === 'ETIMEDOUT';
      const friendly = isNetwork
        ? `RawrXD Explanation: Cannot reach Ollama at ${this.config.ollamaEndpoint}. Start Ollama (e.g. \`ollama serve\`) and ensure the model "${model}" is pulled.`
        : `RawrXD Explanation failed: ${msg}`;
      vscode.window.showErrorMessage(friendly);
    }
  }

  private async fixCode() {
    const editor = vscode.window.activeTextEditor;
    if (!editor) {
      vscode.window.showErrorMessage('No active editor');
      return;
    }

    const selectedText = editor.document.getText(editor.selection) || editor.document.getText();

    const model = this.config.agenticMode ? this.config.agenticModel : this.config.standardModel;

    try {
      vscode.window.showInformationMessage('⏳ Analyzing code for issues...');

      const response = await axios.post(`${this.config.ollamaEndpoint}/api/generate`, {
        model: model,
        prompt: `Fix any issues in this code and explain the changes:\n\n${selectedText}`,
        stream: false
      }, { timeout: 60000 });

      const fixedCode = response.data.response;
      const selection = editor.selection;

      editor.edit(editBuilder => {
        if (selection.isEmpty) {
          editBuilder.insert(selection.active, fixedCode);
        } else {
          editBuilder.replace(selection, fixedCode);
        }
      });

      vscode.window.showInformationMessage('✅ Code fixed!');
    } catch (error: unknown) {
      const msg = error && typeof error === 'object' && 'message' in error ? String((error as Error).message) : String(error);
      const code = error && typeof error === 'object' && 'code' in error ? (error as NodeJS.ErrnoException).code : '';
      const isNetwork = msg.includes('fetch failed') || msg.includes('ECONNREFUSED') || msg.includes('ENOTFOUND') || msg.includes('ETIMEDOUT') || code === 'ECONNREFUSED' || code === 'ENOTFOUND' || code === 'ETIMEDOUT';
      const friendly = isNetwork ? `Ollama unreachable at ${this.config.ollamaEndpoint}. Start Ollama to fix code.` : `Error: ${msg}`;
      vscode.window.showErrorMessage(friendly);
    }
  }

  private async getCompletion(linePrefix: string, language: string): Promise<string | null> {
    try {
      const model = this.config.agenticMode ? this.config.agenticModel : this.config.standardModel;

      const response = await axios.post(`${this.config.ollamaEndpoint}/api/generate`, {
        model: model,
        prompt: `Complete this ${language} code (only return the completion, no explanation):\n${linePrefix}`,
        stream: false,
        options: {
          temperature: this.config.temperature,
          num_predict: 50
        }
      }, { timeout: 10000 });

      return response.data.response.trim().split('\n')[0];
    } catch (error) {
      return null;
    }
  }

  private updateStatusBar() {
    const status = this.config.agenticMode ? '🚀 AGENTIC' : '⏸️  STANDARD';
    const color = this.config.agenticMode ? '#00FF00' : '#CCCCCC';

    this.statusBar.text = `${status}`;
    this.statusBar.color = color;
    this.statusBar.command = 'agentic-copilot.toggleAgenticMode';
    this.statusBar.tooltip = 'Click to toggle agentic mode (Ctrl+Shift+A)';
  }

  private showStatus() {
    vscode.window.showInformationMessage(
      this.config.agenticMode
        ? `🚀 Agentic Mode: ACTIVE\nModel: ${this.config.agenticModel}`
        : `⏸️  Standard Mode: ACTIVE\nModel: ${this.config.standardModel}`
    );
  }

  private onConfigChange(e: vscode.ConfigurationChangeEvent) {
    if (e.affectsConfiguration('agenticCopilot')) {
      const config = vscode.workspace.getConfiguration('agenticCopilot');
      this.config.ollamaEndpoint = config.get('ollamaEndpoint') || this.config.ollamaEndpoint;
      this.config.agenticModel = config.get('agenticModel') || this.config.agenticModel;
      this.config.standardModel = config.get('standardModel') || this.config.standardModel;
    }
  }
}

let copilot: LocalAgenticCopilot;

export function activate(context: vscode.ExtensionContext) {
  copilot = new LocalAgenticCopilot();
  copilot.initialize(context);
}

export function deactivate() { }
