// src/extension.ts  –  consent-hardened edition
import * as vscode from 'vscode';
import axios from 'axios';

interface AIService { name: string; available: boolean; lastCheck: number; }
interface AIResponse { service: string; message: string; response: string; timestamp: string; }

class MultiAIExtension {
    private outputChannel = vscode.window.createOutputChannel('Multi-AI Assistant');
    private statusBarItem = vscode.window.createStatusBarItem(vscode.StatusBarAlignment.Right, 100);
    private chatPanel: vscode.WebviewPanel | undefined;
    private services = new Map<string, AIService>();

    /* =====  CONSENT GUARD  ===== */
    private writeConsent = false;               // session-only
    private async askWriteConsent(): Promise<boolean> {
        if (this.writeConsent) return true;
        const choice = await vscode.window.showWarningMessage(
            'Multi-AI wants to insert code into your file. Allow once for this session?',
            'Allow Once', 'Deny'
        );
        if (choice === 'Allow Once') { this.writeConsent = true; return true; }
        return false;
    }
    private async insertCode(code: string) {
        const editor = vscode.window.activeTextEditor;
        if (!editor) return;
        await editor.edit(() => editor.insertSnippet(new vscode.SnippetString(code), editor.selection.start));
    }
    /* ============================ */

    constructor() {
        this.statusBarItem.command = 'cursor-multi-ai.askQuestion';
        this.statusBarItem.text = '$(robot) Multi-AI';
        this.statusBarItem.tooltip = 'Click to ask AI a question';
        this.statusBarItem.show();
    }

    private getConfig() { return vscode.workspace.getConfiguration('multiAI'); }

    /* --------------  health / server comms -------------- */
    private async checkServerHealth(): Promise<boolean> {
        try {
            const url = this.getConfig().get('serverUrl', 'http://localhost:3003');
            const to  = this.getConfig().get('timeout', 30000) / 6;
            const { status } = await axios.get(`${url}/health`, { timeout: to });
            return status === 200;
        } catch { return false; }
    }
    private async sendToServer(service: string, msg: string): Promise<string> {
        const url = this.getConfig().get('serverUrl', 'http://localhost:3003');
        const to  = this.getConfig().get('timeout', 30000);
        const res = await axios.post(`${url}/chat/${service}`, { message: msg }, { timeout: to });
        return res.data.response;
    }
    /* ---------------------------------------------------- */

    /* --------------  write-back commands (gated) -------- */
    async addComments()   { await this.writeBackCommand('Comments',   'Add ONLY inline comments'); }
    async optimizeCode()  { await this.writeBackCommand('Optimize',   'Optimize this code'); }
    async generateTests() { await this.writeBackCommand('Tests',      'Generate unit tests'); }

    private async writeBackCommand(title: string, instruction: string) {
        const ed = vscode.window.activeTextEditor;
        if (!ed) return;
        const sel = ed.document.getText(ed.selection);
        if (!sel) { vscode.window.showErrorMessage('Select code first'); return; }

        const lang = ed.document.languageId;
        const msg = `${instruction} for ${lang}. Return ONLY the modified code block, no explanations.\n\n\`\`\`${lang}\n${sel}\n\`\`\``;

        const resp = await this.processCodeRequest(title, msg, sel);
        if (!resp) return;
        const ok = await this.askWriteConsent();
        if (!ok) return;
        await this.insertCode(resp);
    }
    /* ---------------------------------------------------- */

    /* --------------  read-only commands ----------------- */
    async explainCode() {
        const ed = vscode.window.activeTextEditor;
        if (!ed) return;
        const sel = ed.document.getText(ed.selection);
        if (!sel) { vscode.window.showErrorMessage('Select code first'); return; }
        await this.processCodeRequest('Explain', 'Explain this code', sel);
    }
    async debugCode() {
        const ed = vscode.window.activeTextEditor;
        if (!ed) return;
        const sel = ed.document.getText(ed.selection);
        if (!sel) { vscode.window.showErrorMessage('Select code first'); return; }
        await this.processCodeRequest('Debug', 'Help me debug this code', sel);
    }
    /* ---------------------------------------------------- */

    /* --------------  generic ask / UI helpers ----------- */
    async askQuestion() {
        const question = await vscode.window.showInputBox({
            prompt: 'Ask AI a question',
            placeHolder: 'Enter your question here...'
        });
        if (question) {
            await this.processCodeRequest('Question', question, '');
        }
    }

    async askSpecificAI(service: string) {
        const question = await vscode.window.showInputBox({
            prompt: `Ask ${service} a question`,
            placeHolder: 'Enter your question here...'
        });
        if (question) {
            await this.processCodeRequest(`Question to ${service}`, question, '');
        }
    }

    async askAllAIs() {
        const question = await vscode.window.showInputBox({
            prompt: 'Ask all AIs this question',
            placeHolder: 'Enter your question here...'
        });
        if (question) {
            await this.askAllAIsImplementation(question);
        }
    }

    private async askAllAIsImplementation(question: string) {
        const preferredAI = this.getConfig().get('preferredAI', 'amazonq');
        return vscode.window.withProgress({
            location: vscode.ProgressLocation.Notification,
            title: `Asking all AIs: ${question.substring(0, 50)}...`,
            cancellable: false
        }, async () => {
            try {
                const serverUp = await this.checkServerHealth();
                if (!serverUp) {
                    vscode.window.showErrorMessage('Server unavailable - cannot ask all AIs');
                    return '';
                }

                // Ask all available AI services
                const aiServices = ['amazonq', 'claude', 'chatgpt', 'gemini', 'kimi'];
                const responses = [];

                for (const service of aiServices) {
                    try {
                        const response = await this.sendToServer(service, question);
                        responses.push({ service, response, success: true });
                    } catch (error) {
                        const errorMessage = error instanceof Error ? error.message : String(error);
                        responses.push({ service, response: `Error: ${errorMessage}`, success: false });
                    }
                }

                // Show combined results
                this.showAllResponses(question, responses);
                return 'All AI responses collected';
            } catch (err: any) {
                vscode.window.showErrorMessage(`Ask all AIs failed: ${err.message}`);
                return '';
            }
        });
    }

    private showAllResponses(question: string, responses: any[]) {
        const content = `# All AI Responses\n\n**Question:** ${question}\n\n${responses.map(r =>
            `## ${r.service.toUpperCase()}\n${r.response}\n\n---\n`
        ).join('')}`;

        vscode.workspace.openTextDocument({
            content,
            language: 'markdown'
        }).then(doc => vscode.window.showTextDocument(doc, vscode.ViewColumn.Beside));

        this.outputChannel.appendLine(`=== All AI Responses ===\nQ: ${question}\n${responses.map(r =>
            `${r.service}: ${r.success ? 'SUCCESS' : 'ERROR'}`
        ).join('\n')}\n`);
    }
    /* ---------------------------------------------------- */

    private async processCodeRequest(title: string, message: string, selectedText: string) {
        const preferredAI = this.getConfig().get('preferredAI', 'amazonq');
        return vscode.window.withProgress({
            location: vscode.ProgressLocation.Notification,
            title: `${title} with ${preferredAI}...`,
            cancellable: false
        }, async () => {
            try {
                const serverUp = await this.checkServerHealth();
                if (serverUp) {
                    return await this.sendToServer(preferredAI, message);
                } else {
                    vscode.window.showErrorMessage(`${title} failed: Server unavailable`);
                    return '';
                }
            } catch (err: any) {
                vscode.window.showErrorMessage(`${title} failed: ${err.message}`);
                return '';
            }
        });
    }

    private showResponse(title: string, question: string, response: string) {
        vscode.workspace.openTextDocument({
            content: `# ${title}\n\n**Question:**\n${question}\n\n**Response:**\n${response}`,
            language: 'markdown'
        }).then((doc: vscode.TextDocument) => vscode.window.showTextDocument(doc, vscode.ViewColumn.Beside));
        this.outputChannel.appendLine(`=== ${title} ===\nQ: ${question}\nA: ${response}\n`);
    }

    dispose() {
        this.outputChannel.dispose();
        this.statusBarItem.dispose();
        this.chatPanel?.dispose();
    }
}

/* ---------- boiler-plate activate / deactivate ---------- */
export function activate(ctx: vscode.ExtensionContext) {
    const ext = new MultiAIExtension();
    ctx.subscriptions.push(
        vscode.commands.registerCommand('cursor-multi-ai.askQuestion', () => ext.askQuestion()),
        vscode.commands.registerCommand('cursor-multi-ai.explainCode',   () => ext.explainCode()),
        vscode.commands.registerCommand('cursor-multi-ai.optimizeCode', () => ext.optimizeCode()),
        vscode.commands.registerCommand('cursor-multi-ai.debugCode',    () => ext.debugCode()),
        vscode.commands.registerCommand('cursor-multi-ai.addComments',  () => ext.addComments()),
        vscode.commands.registerCommand('cursor-multi-ai.generateTests',() => ext.generateTests()),
        vscode.commands.registerCommand('cursor-multi-ai.askAmazonQ',   () => ext.askSpecificAI('amazonq')),
        vscode.commands.registerCommand('cursor-multi-ai.askClaude',    () => ext.askSpecificAI('claude')),
        vscode.commands.registerCommand('cursor-multi-ai.askChatGPT',   () => ext.askSpecificAI('chatgpt')),
        vscode.commands.registerCommand('cursor-multi-ai.askGemini',    () => ext.askSpecificAI('gemini')),
        vscode.commands.registerCommand('cursor-multi-ai.askKimi',      () => ext.askSpecificAI('kimi')),
        vscode.commands.registerCommand('cursor-multi-ai.askAll',       () => ext.askAllAIs()),
        ext
    );
}

export function deactivate() {}