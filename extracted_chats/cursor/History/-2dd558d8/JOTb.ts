import * as vscode from 'vscode';
import { spawn, exec } from 'child_process';
import * as path from 'path';
import * as fs from 'fs';

let beaconProcess: any = null;
let isActive = false;
let currentModel = 'deepseek-coder:latest';
let chatHistory: Array<{role: string, content: string}> = [];
let agentCapabilities = {
    codeAnalysis: true,
    fileOperations: true,
    projectContext: true,
    multiTurn: true
};

export function activate(context: vscode.ExtensionContext) {
    console.log('BigDaddyG Cursor Chat extension activated');

    // Start cursor chat command
    let startCommand = vscode.commands.registerCommand('bigdaddyg.startCursorChat', () => {
        if (!isActive) {
            startBeacon(context);
            vscode.window.showInformationMessage('Cursor Chat Started - Press Ctrl+Space for AI assist');
        }
    });

    // Stop cursor chat command
    let stopCommand = vscode.commands.registerCommand('bigdaddyg.stopCursorChat', () => {
        if (isActive) {
            stopBeacon();
            vscode.window.showInformationMessage('Cursor Chat Stopped');
        }
    });

    // Model selection command
    let selectModelCommand = vscode.commands.registerCommand('bigdaddyg.selectModel', async () => {
        await selectAIModel();
    });

    // Interactive chat command
    let chatCommand = vscode.commands.registerCommand('bigdaddyg.openChat', () => {
        openChatPanel(context);
    });

    // Trigger assist command
    let assistCommand = vscode.commands.registerCommand('bigdaddyg.triggerAssist', () => {
        if (isActive) {
            triggerAIAssist();
        }
    });

    // Agentic code analysis
    let analyzeCommand = vscode.commands.registerCommand('bigdaddyg.analyzeProject', () => {
        performAgenticAnalysis();
    });

    context.subscriptions.push(startCommand, stopCommand, selectModelCommand, chatCommand, assistCommand, analyzeCommand);

    // Auto-start on activation
    startBeacon(context);
}

async function selectAIModel() {
    const models = await getAvailableModels();
    const selected = await vscode.window.showQuickPick(models, {
        placeHolder: 'Select AI Model'
    });
    
    if (selected) {
        currentModel = selected;
        vscode.window.showInformationMessage(`Model switched to: ${currentModel}`);
    }
}

async function getAvailableModels(): Promise<string[]> {
    return new Promise((resolve) => {
        exec('ollama list', (error, stdout) => {
            if (error) {
                resolve(['deepseek-coder:latest', 'llama3.2:latest', 'codellama:latest']);
                return;
            }
            
            const models = stdout.split('\n')
                .filter(line => line.trim() && !line.startsWith('NAME'))
                .map(line => line.split('\t')[0].trim())
                .filter(model => model);
            
            resolve(models.length > 0 ? models : ['deepseek-coder:latest']);
        });
    });
}

function openChatPanel(context: vscode.ExtensionContext) {
    const panel = vscode.window.createWebviewPanel(
        'cursorChat',
        'Cursor Chat',
        vscode.ViewColumn.Beside,
        {
            enableScripts: true,
            retainContextWhenHidden: true
        }
    );

    panel.webview.html = getChatHTML();
    
    panel.webview.onDidReceiveMessage(async (message) => {
        if (message.command === 'sendMessage') {
            await handleChatMessage(message.text, panel);
        }
    });
}

function getChatHTML(): string {
    return `
    <!DOCTYPE html>
    <html>
    <head>
        <style>
            body { font-family: Arial, sans-serif; padding: 10px; }
            #chat { height: 400px; overflow-y: auto; border: 1px solid #ccc; padding: 10px; margin-bottom: 10px; }
            .message { margin: 5px 0; padding: 5px; border-radius: 5px; }
            .user { background: #e3f2fd; text-align: right; }
            .ai { background: #f3e5f5; }
            #input { width: 80%; padding: 5px; }
            #send { padding: 5px 10px; }
        </style>
    </head>
    <body>
        <div id="chat"></div>
        <input type="text" id="input" placeholder="Ask me anything..." />
        <button id="send">Send</button>
        
        <script>
            const vscode = acquireVsCodeApi();
            const chat = document.getElementById('chat');
            const input = document.getElementById('input');
            const send = document.getElementById('send');
            
            function addMessage(text, isUser) {
                const div = document.createElement('div');
                div.className = 'message ' + (isUser ? 'user' : 'ai');
                div.textContent = text;
                chat.appendChild(div);
                chat.scrollTop = chat.scrollHeight;
            }
            
            function sendMessage() {
                const text = input.value.trim();
                if (text) {
                    addMessage(text, true);
                    vscode.postMessage({ command: 'sendMessage', text });
                    input.value = '';
                }
            }
            
            send.onclick = sendMessage;
            input.onkeypress = (e) => e.key === 'Enter' && sendMessage();
            
            window.addEventListener('message', event => {
                const message = event.data;
                if (message.command === 'aiResponse') {
                    addMessage(message.text, false);
                }
            });
        </script>
    </body>
    </html>`;
}

async function handleChatMessage(userMessage: string, panel: vscode.WebviewPanel) {
    chatHistory.push({ role: 'user', content: userMessage });
    
    const context = await gatherProjectContext();
    const prompt = buildAgenticPrompt(userMessage, context);
    
    exec(`ollama run ${currentModel} "${prompt}"`, (error, stdout) => {
        if (error) {
            panel.webview.postMessage({
                command: 'aiResponse',
                text: `Error: ${error.message}`
            });
            return;
        }
        
        const response = stdout.trim();
        chatHistory.push({ role: 'assistant', content: response });
        
        panel.webview.postMessage({
            command: 'aiResponse',
            text: response
        });
    });
}

function buildAgenticPrompt(userMessage: string, context: any): string {
    const systemPrompt = `You are an agentic AI assistant with the following capabilities:
- Code analysis and suggestions
- File operations and project understanding
- Multi-turn conversation with context
- Proactive problem-solving

Project Context:
${JSON.stringify(context, null, 2)}

Chat History:
${chatHistory.slice(-5).map(msg => `${msg.role}: ${msg.content}`).join('\n')}

User: ${userMessage}

Provide a helpful, contextual response:`;
    
    return systemPrompt.replace(/"/g, '\"');
}

async function gatherProjectContext(): Promise<any> {
    const editor = vscode.window.activeTextEditor;
    const workspaceFolder = vscode.workspace.workspaceFolders?.[0];
    
    return {
        currentFile: editor?.document.fileName,
        language: editor?.document.languageId,
        selection: editor?.document.getText(editor.selection),
        workspaceRoot: workspaceFolder?.uri.fsPath,
        openFiles: vscode.workspace.textDocuments.map(doc => doc.fileName)
    };
}

function performAgenticAnalysis() {
    const editor = vscode.window.activeTextEditor;
    if (!editor) {
        vscode.window.showErrorMessage('No active file to analyze');
        return;
    }
    
    const code = editor.document.getText();
    const fileName = path.basename(editor.document.fileName);
    
    const analysisPrompt = `Perform a comprehensive agentic analysis of this ${editor.document.languageId} file:

File: ${fileName}

${code}

Provide:
1. Code quality assessment
2. Security vulnerabilities
3. Performance optimizations
4. Architectural suggestions
5. Refactoring opportunities`;
    
    exec(`ollama run ${currentModel} "${analysisPrompt.replace(/"/g, '\"')}"`, (error, stdout) => {
        if (error) {
            vscode.window.showErrorMessage(`Analysis Error: ${error.message}`);
            return;
        }
        
        const outputChannel = vscode.window.createOutputChannel('Agentic Analysis');
        outputChannel.clear();
        outputChannel.appendLine('=== AGENTIC CODE ANALYSIS ===');
        outputChannel.appendLine(`File: ${fileName}`);
        outputChannel.appendLine(`Model: ${currentModel}`);
        outputChannel.appendLine('\n' + stdout);
        outputChannel.show();
    });
}

function startBeacon(context: vscode.ExtensionContext) {
    const beaconPath = path.join(context.extensionPath, 'out', 'beacon.exe');
    
    try {
        beaconProcess = spawn(beaconPath, [], {
            detached: false,
            stdio: 'pipe'
        });

        beaconProcess.on('error', (err: any) => {
            vscode.window.showErrorMessage(`Beacon failed to start: ${err.message}`);
        });

        isActive = true;
    } catch (error) {
        vscode.window.showErrorMessage(`Failed to start beacon: ${error}`);
    }
}

function stopBeacon() {
    if (beaconProcess) {
        beaconProcess.kill();
        beaconProcess = null;
    }
    isActive = false;
}

function triggerAIAssist() {
    const editor = vscode.window.activeTextEditor;
    if (!editor) return;

    const selection = editor.selection;
    const text = editor.document.getText(selection);
    const context = text || editor.document.getText();

    const prompt = `Analyze this code and provide suggestions:\n${context}`;
    
    exec(`ollama run ${currentModel} "${prompt}"`, (error, stdout, stderr) => {
        if (error) {
            vscode.window.showErrorMessage(`AI Error: ${error.message}`);
            return;
        }

        if (stdout) {
            vscode.window.showInformationMessage(stdout.substring(0, 200) + '...');
            
            const outputChannel = vscode.window.createOutputChannel('Cursor Chat');
            outputChannel.appendLine('AI Response:');
            outputChannel.appendLine(stdout);
            outputChannel.show();
        }
    });
}

export function deactivate() {
    stopBeacon();
}