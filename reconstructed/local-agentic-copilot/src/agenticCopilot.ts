import * as vscode from 'vscode';
import axios from 'axios';

interface OllamaResponse {
    response: string;
    done: boolean;
}

interface OllamaRequest {
    model: string;
    prompt: string;
    stream?: boolean;
    options?: {
        temperature?: number;
        top_p?: number;
        top_k?: number;
    };
}

export class AgenticCopilot {
    private statusBarItem: vscode.StatusBarItem;
    private isAgenticMode: boolean = false;
    private config: vscode.WorkspaceConfiguration;
    private selectedModel: string | undefined;

    constructor(private readonly context: vscode.ExtensionContext) {
        this.statusBarItem = vscode.window.createStatusBarItem(vscode.StatusBarAlignment.Right, 100);
        this.config = vscode.workspace.getConfiguration('agenticCopilot');
        this.selectedModel = this.context.globalState.get<string>('agenticCopilot.selectedModel');
        this.updateStatusBar();
        this.statusBarItem.show();
    }

    public async toggleAgenticMode(): Promise<void> {
        this.isAgenticMode = !this.isAgenticMode;
        this.updateStatusBar();
        
        const message = this.isAgenticMode 
            ? '🚀 Agentic Mode Enabled - Autonomous reasoning active'
            : '⏸️ Standard Mode Enabled - Direct suggestions active';
        
        vscode.window.showInformationMessage(message);
    }

    public async generateCode(): Promise<void> {
        const editor = vscode.window.activeTextEditor;
        if (!editor) {
            vscode.window.showErrorMessage('No active editor found');
            return;
        }

        const selection = editor.selection;
        const selectedText = editor.document.getText(selection);
        
        const prompt = await vscode.window.showInputBox({
            prompt: 'Enter your code generation prompt',
            placeHolder: 'e.g., Create a function that sorts an array'
        });

        if (!prompt) {
            return;
        }

        const fullPrompt = selectedText 
            ? `Context: ${selectedText}\n\n${prompt}`
            : prompt;

        await this.sendRequest(fullPrompt, 'generate');
    }

    public async explainCode(): Promise<void> {
        const editor = vscode.window.activeTextEditor;
        if (!editor) {
            vscode.window.showErrorMessage('No active editor found');
            return;
        }

        const selection = editor.selection;
        const selectedText = editor.document.getText(selection);

        if (!selectedText.trim()) {
            vscode.window.showErrorMessage('No code selected');
            return;
        }

        const prompt = `Explain this code:\n\n${selectedText}`;
        await this.sendRequest(prompt, 'explain');
    }

    public async fixCode(): Promise<void> {
        const editor = vscode.window.activeTextEditor;
        if (!editor) {
            vscode.window.showErrorMessage('No active editor found');
            return;
        }

        const selection = editor.selection;
        const selectedText = editor.document.getText(selection);

        if (!selectedText.trim()) {
            vscode.window.showErrorMessage('No code selected');
            return;
        }

        const prompt = `Fix any issues in this code:\n\n${selectedText}`;
        await this.sendRequest(prompt, 'fix');
    }

    public showStatus(): void {
        const status = this.isAgenticMode ? '🚀 AGENTIC' : '⏸️ STANDARD';
        const model = this.getActiveModel();
        vscode.window.showInformationMessage(`Agentic Copilot Status: ${status} | Model: ${model}`);
    }

    public getUiState(): { isAgenticMode: boolean; selectedModel?: string } {
        return {
            isAgenticMode: this.isAgenticMode,
            selectedModel: this.selectedModel
        };
    }

    public async setSelectedModel(model: string | undefined): Promise<void> {
        const trimmed = model?.trim();
        this.selectedModel = trimmed ? trimmed : undefined;
        await this.context.globalState.update('agenticCopilot.selectedModel', this.selectedModel);
        this.updateStatusBar();
    }

    public async getAvailableModels(): Promise<string[]> {
        const endpoint = this.config.get('ollamaEndpoint', 'http://localhost:11434');
        try {
            const response = await this.retryRequest(() => axios.get(`${endpoint}/api/tags`));
            const models = (response.data?.models ?? []) as Array<{ name?: string }>;
            return models
                .map((m) => (typeof m.name === 'string' ? m.name : ''))
                .filter(Boolean);
        } catch (error) {
            console.error('Failed to fetch models:', error);
            return [];
        }
    }

    public async chat(text: string, modelOverride?: string): Promise<string> {
        const prompt = text;
        const response = await this.sendRequestForText(prompt, modelOverride);
        return response;
    }

    private getActiveModel(): string {
        if (this.selectedModel) {
            return this.selectedModel;
        }

        return this.isAgenticMode
            ? this.config.get('agenticModel', 'cheetah-stealth-agentic:latest')
            : this.config.get('standardModel', 'bigdaddyg-fast:latest');
    }

    private getTemperature(): number {
        return this.isAgenticMode ? 0.9 : 0.7;
    }

    private async retryRequest<T>(fn: () => Promise<T>, maxRetries: number = 3): Promise<T> {
        let lastError: any;
        for (let i = 0; i < maxRetries; i++) {
            try {
                return await fn();
            } catch (error: any) {
                lastError = error;
                const status = error.response?.status;
                if (status === 429 || (status >= 500 && status <= 599)) {
                    const delay = Math.pow(2, i) * 1000;
                    console.warn(`Request failed with ${status}. Retrying in ${delay}ms (attempt ${i + 1}/${maxRetries})...`);
                    await new Promise(resolve => setTimeout(resolve, delay));
                    continue;
                }
                throw error;
            }
        }
        throw lastError;
    }

    private async sendRequest(prompt: string, action: string): Promise<void> {
        try {
            const model = this.getActiveModel();
            const temperature = this.getTemperature();

            const request: OllamaRequest = {
                model: model,
                prompt: prompt,
                stream: false,
                options: {
                    temperature: temperature
                }
            };

            const endpoint = this.config.get('ollamaEndpoint', 'http://localhost:11434');
            const response = await this.retryRequest(() => axios.post(`${endpoint}/api/generate`, request));
            
            const result = response.data as OllamaResponse;
            
            if (result.done && result.response) {
                await this.handleResponse(result.response, action);
            } else {
                vscode.window.showErrorMessage('No response received from Ollama');
            }
        } catch (error: any) {
            console.error('Ollama API error:', error);
            const status = error.response?.status;
            const message = status === 429 
                ? 'Rate limit exceeded. Please try again later.' 
                : 'Failed to connect to Ollama. Make sure Ollama is running on localhost:11434';
            vscode.window.showErrorMessage(message);
        }
    }

    private async sendRequestForText(prompt: string, modelOverride?: string): Promise<string> {
        const model = modelOverride?.trim() ? modelOverride.trim() : this.getActiveModel();
        const temperature = this.getTemperature();

        const request: OllamaRequest = {
            model,
            prompt,
            stream: false,
            options: {
                temperature
            }
        };

        const endpoint = this.config.get('ollamaEndpoint', 'http://localhost:11434');
        const response = await this.retryRequest(() => axios.post(`${endpoint}/api/generate`, request));
        const result = response.data as OllamaResponse;

        if (result?.done && result?.response) {
            return result.response;
        }
        throw new Error('No response received from Ollama');
    }

    private async handleResponse(response: string, action: string): Promise<void> {
        const editor = vscode.window.activeTextEditor;
        if (!editor) {
            return;
        }

        switch (action) {
            case 'generate':
                await this.insertGeneratedCode(editor, response);
                break;
            case 'explain':
                await this.showExplanation(response);
                break;
            case 'fix':
                await this.applyFix(editor, response);
                break;
        }
    }

    private async insertGeneratedCode(editor: vscode.TextEditor, code: string): Promise<void> {
        const selection = editor.selection;
        
        await editor.edit(editBuilder => {
            if (selection.isEmpty) {
                // Insert at cursor position
                editBuilder.insert(selection.active, code);
            } else {
                // Replace selection
                editBuilder.replace(selection, code);
            }
        });

        if (this.config.get('autoFormat', true)) {
            await vscode.commands.executeCommand('editor.action.formatDocument');
        }

        vscode.window.showInformationMessage('Code generated successfully');
    }

    private async showExplanation(explanation: string): Promise<void> {
        const panel = vscode.window.createWebviewPanel(
            'agenticExplanation',
            'Code Explanation',
            vscode.ViewColumn.Beside,
            {}
        );

        panel.webview.html = `
            <!DOCTYPE html>
            <html>
            <head>
                <style>
                    body { 
                        font-family: var(--vscode-font-family); 
                        padding: 20px; 
                        color: var(--vscode-foreground);
                        background-color: var(--vscode-editor-background);
                    }
                    pre { 
                        white-space: pre-wrap; 
                        background: var(--vscode-textCodeBlock-background);
                        padding: 15px;
                        border-radius: 5px;
                    }
                </style>
            </head>
            <body>
                <h2>Code Explanation</h2>
                <pre>${explanation}</pre>
            </body>
            </html>
        `;
    }

    private async applyFix(editor: vscode.TextEditor, fix: string): Promise<void> {
        const selection = editor.selection;
        
        await editor.edit(editBuilder => {
            editBuilder.replace(selection, fix);
        });

        if (this.config.get('autoFormat', true)) {
            await vscode.commands.executeCommand('editor.action.formatDocument');
        }

        vscode.window.showInformationMessage('Code fixed successfully');
    }

    private updateStatusBar(): void {
        const status = this.isAgenticMode ? '🚀 AGENTIC' : '⏸️ STANDARD';
        const color = this.isAgenticMode ? 'green' : 'gray';
        const model = this.getActiveModel();
        
        this.statusBarItem.text = status;
        this.statusBarItem.color = color;
        this.statusBarItem.tooltip = `Click to toggle agentic mode (Currently: ${status})\nModel: ${model}`;
        this.statusBarItem.command = 'agentic-copilot.toggleAgenticMode';
    }

    public dispose(): void {
        this.statusBarItem.dispose();
    }
}