import * as vscode from 'vscode';
import * as fs from 'fs';
import * as path from 'path';
import { exec } from 'child_process';

class BigDaddyGChatProvider implements vscode.WebviewViewProvider {
    public static readonly viewType = 'bigdaddyg.chatView';
    private static detachedPanels: Set<vscode.WebviewPanel> = new Set();

    constructor(private readonly _extensionUri: vscode.Uri) {}

    public resolveWebviewView(
        webviewView: vscode.WebviewView,
        context: vscode.WebviewViewResolveContext,
        _token: vscode.CancellationToken
    ) {
        webviewView.webview.options = {
            enableScripts: true,
            localResourceRoots: [this._extensionUri]
        };

        webviewView.webview.html = this._getHtmlForWebview(webviewView.webview);

        webviewView.webview.onDidReceiveMessage(async (data: any) => {
            switch (data.type) {
                case 'chat':
                    try {
                        const response = await this._sendToBigDaddyG(data.message, data.model || 'bigdaddyg-assembly');
                        webviewView.webview.postMessage({
                            type: 'response',
                            content: response
                        });
                    } catch (error) {
                        webviewView.webview.postMessage({
                            type: 'error',
                            content: `Error: ${error instanceof Error ? error.message : 'Unknown error'}`
                        });
                    }
                    break;
                case 'popout':
                    this.createDetachedChatWindow(data.model || 'bigdaddyg-assembly');
                    break;
            }
        });
    }

    public createDetachedChatWindow(model: string = 'bigdaddyg-assembly') {
        const panel = vscode.window.createWebviewPanel(
            'bigdaddyg.detachedChat',
            `BigDaddyG Chat - ${model}`,
            vscode.ViewColumn.Beside,
            {
                enableScripts: true,
                retainContextWhenHidden: true,
                localResourceRoots: [this._extensionUri]
            }
        );

        panel.webview.html = this._getDetachedHtmlForWebview(panel.webview, model);
        BigDaddyGChatProvider.detachedPanels.add(panel);

        panel.webview.onDidReceiveMessage(async (data: any) => {
            switch (data.type) {
                case 'chat':
                    try {
                        const response = await this._sendToBigDaddyG(data.message, data.model || model);
                        panel.webview.postMessage({
                            type: 'response',
                            content: response
                        });
                    } catch (error) {
                        panel.webview.postMessage({
                            type: 'error',
                            content: `Error: ${error instanceof Error ? error.message : 'Unknown error'}`
                        });
                    }
                    break;
                case 'clone':
                    this.createDetachedChatWindow(data.model || model);
                    break;
            }
        });

        panel.onDidDispose(() => {
            BigDaddyGChatProvider.detachedPanels.delete(panel);
        });

        return panel;
    }

    private async _sendToBigDaddyG(message: string, model: string = 'bigdaddyg-assembly'): Promise<string> {
        const fetch = (await import('node-fetch')).default;
        
        const response = await fetch('http://localhost:11441/v1/chat/completions', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json',
                'Authorization': 'Bearer sk-local-bigdaddyg'
            },
            body: JSON.stringify({
                model: model,
                messages: [{ role: 'user', content: message }],
                temperature: 0.7,
                max_tokens: 1000
            })
        });

        if (!response.ok) {
            throw new Error(`BigDaddyG server error: ${response.status}`);
        }

        const data = await response.json() as any;
        return data.choices[0]?.message?.content || 'No response from BigDaddyG';
    }

    private _getHtmlForWebview(webview: vscode.Webview) {
        return `<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>BigDaddyG Chat</title>
    <style>
        body { 
            padding: 10px; 
            font-family: var(--vscode-font-family);
            background: var(--vscode-editor-background);
            color: var(--vscode-editor-foreground);
        }
        .chat-container {
            display: flex;
            flex-direction: column;
            height: 100vh;
        }
        .controls {
            display: flex;
            gap: 5px;
            margin-bottom: 10px;
            align-items: center;
        }
        .model-select {
            flex: 1;
            padding: 4px;
            background: var(--vscode-input-background);
            color: var(--vscode-input-foreground);
            border: 1px solid var(--vscode-input-border);
            border-radius: 4px;
        }
        .popout-button {
            padding: 4px 8px;
            background: var(--vscode-button-secondaryBackground);
            color: var(--vscode-button-secondaryForeground);
            border: none;
            border-radius: 4px;
            cursor: pointer;
            font-size: 12px;
        }
        .popout-button:hover {
            background: var(--vscode-button-secondaryHoverBackground);
        }
        .messages {
            flex: 1;
            overflow-y: auto;
            margin-bottom: 10px;
            padding: 10px;
            border: 1px solid var(--vscode-panel-border);
            border-radius: 4px;
        }
        .message {
            margin-bottom: 10px;
            padding: 8px;
            border-radius: 4px;
        }
        .user-message {
            background: var(--vscode-inputOption-activeBackground);
            text-align: right;
        }
        .bot-message {
            background: var(--vscode-textCodeBlock-background);
        }
        .input-container {
            display: flex;
            gap: 5px;
        }
        .message-input {
            flex: 1;
            padding: 8px;
            border: 1px solid var(--vscode-input-border);
            border-radius: 4px;
            background: var(--vscode-input-background);
            color: var(--vscode-input-foreground);
        }
        .send-button {
            padding: 8px 12px;
            background: var(--vscode-button-background);
            color: var(--vscode-button-foreground);
            border: none;
            border-radius: 4px;
            cursor: pointer;
        }
        .send-button:hover {
            background: var(--vscode-button-hoverBackground);
        }
    </style>
</head>
<body>
    <div class="chat-container">
        <div class="controls">
            <select class="model-select" id="modelSelect">
                <option value="bigdaddyg-assembly">BigDaddyG Assembly</option>
                <option value="bigdaddyg-ensemble">BigDaddyG Ensemble</option>
                <option value="bigdaddyg-pe">BigDaddyG PE</option>
                <option value="bigdaddyg-reverse">BigDaddyG Reverse</option>
                <option value="custom-agentic-coder">Custom Agentic Coder (ASM)</option>
                <option value="your-custom-model">Custom Model</option>
            </select>
            <button class="popout-button" onclick="popOut()">🗗 Pop Out</button>
        </div>
        <div class="messages" id="messages">
            <div class="message bot-message">
                👋 BigDaddyG is ready! Ask me about assembly programming, reverse engineering, or PE analysis.
            </div>
        </div>
        <div class="input-container">
            <input type="text" class="message-input" id="messageInput" placeholder="Ask BigDaddyG..." />
            <button class="send-button" onclick="sendMessage()">Send</button>
        </div>
    </div>

    <script>
        const vscode = acquireVsCodeApi();
        const messagesDiv = document.getElementById('messages');
        const messageInput = document.getElementById('messageInput');
        const modelSelect = document.getElementById('modelSelect');

        function sendMessage() {
            const message = messageInput.value.trim();
            if (!message) return;

            // Add user message
            addMessage(message, 'user');
            messageInput.value = '';

            // Send to extension
            vscode.postMessage({
                type: 'chat',
                message: message,
                model: modelSelect.value
            });
        }

        function popOut() {
            vscode.postMessage({
                type: 'popout',
                model: modelSelect.value
            });
        }

        function addMessage(content, sender) {
            const messageDiv = document.createElement('div');
            messageDiv.className = \`message \${sender}-message\`;
            messageDiv.textContent = content;
            messagesDiv.appendChild(messageDiv);
            messagesDiv.scrollTop = messagesDiv.scrollHeight;
        }

        // Handle messages from extension
        window.addEventListener('message', event => {
            const message = event.data;
            switch (message.type) {
                case 'response':
                    addMessage(message.content, 'bot');
                    break;
                case 'error':
                    addMessage(message.content, 'bot');
                    break;
            }
        });

        // Enter key to send
        messageInput.addEventListener('keypress', function(e) {
            if (e.key === 'Enter') {
                sendMessage();
            }
        });
    </script>
</body>
</html>`;
    }

    private _getDetachedHtmlForWebview(webview: vscode.Webview, model: string) {
        return `<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>BigDaddyG Chat - ${model}</title>
    <style>
        body { 
            padding: 20px; 
            font-family: var(--vscode-font-family);
            background: var(--vscode-editor-background);
            color: var(--vscode-editor-foreground);
            margin: 0;
        }
        .chat-container {
            display: flex;
            flex-direction: column;
            height: calc(100vh - 40px);
            max-width: 1200px;
            margin: 0 auto;
        }
        .header {
            display: flex;
            justify-content: space-between;
            align-items: center;
            margin-bottom: 15px;
            padding: 10px 15px;
            background: var(--vscode-panel-background);
            border-radius: 8px;
            border: 1px solid var(--vscode-panel-border);
        }
        .title {
            font-size: 16px;
            font-weight: bold;
            color: var(--vscode-foreground);
        }
        .controls {
            display: flex;
            gap: 10px;
            align-items: center;
        }
        .model-select {
            padding: 6px 10px;
            background: var(--vscode-input-background);
            color: var(--vscode-input-foreground);
            border: 1px solid var(--vscode-input-border);
            border-radius: 4px;
        }
        .clone-button {
            padding: 6px 12px;
            background: var(--vscode-button-secondaryBackground);
            color: var(--vscode-button-secondaryForeground);
            border: none;
            border-radius: 4px;
            cursor: pointer;
            font-size: 12px;
        }
        .clone-button:hover {
            background: var(--vscode-button-secondaryHoverBackground);
        }
        .messages {
            flex: 1;
            overflow-y: auto;
            margin-bottom: 15px;
            padding: 15px;
            border: 1px solid var(--vscode-panel-border);
            border-radius: 8px;
            background: var(--vscode-panel-background);
        }
        .message {
            margin-bottom: 15px;
            padding: 12px 15px;
            border-radius: 8px;
            line-height: 1.4;
        }
        .user-message {
            background: var(--vscode-inputOption-activeBackground);
            text-align: right;
            margin-left: 20%;
        }
        .bot-message {
            background: var(--vscode-textCodeBlock-background);
            margin-right: 20%;
            white-space: pre-wrap;
        }
        .input-container {
            display: flex;
            gap: 10px;
            padding: 10px;
            background: var(--vscode-panel-background);
            border-radius: 8px;
            border: 1px solid var(--vscode-panel-border);
        }
        .message-input {
            flex: 1;
            padding: 12px 15px;
            border: 1px solid var(--vscode-input-border);
            border-radius: 6px;
            background: var(--vscode-input-background);
            color: var(--vscode-input-foreground);
            font-size: 14px;
        }
        .send-button {
            padding: 12px 20px;
            background: var(--vscode-button-background);
            color: var(--vscode-button-foreground);
            border: none;
            border-radius: 6px;
            cursor: pointer;
            font-size: 14px;
            font-weight: bold;
        }
        .send-button:hover {
            background: var(--vscode-button-hoverBackground);
        }
    </style>
</head>
<body>
    <div class="chat-container">
        <div class="header">
            <div class="title">🤖 BigDaddyG Agent - ${model}</div>
            <div class="controls">
                <select class="model-select" id="modelSelect">
                    <option value="bigdaddyg-assembly" ${model === 'bigdaddyg-assembly' ? 'selected' : ''}>BigDaddyG Assembly</option>
                    <option value="bigdaddyg-ensemble" ${model === 'bigdaddyg-ensemble' ? 'selected' : ''}>BigDaddyG Ensemble</option>
                    <option value="bigdaddyg-pe" ${model === 'bigdaddyg-pe' ? 'selected' : ''}>BigDaddyG PE</option>
                    <option value="bigdaddyg-reverse" ${model === 'bigdaddyg-reverse' ? 'selected' : ''}>BigDaddyG Reverse</option>
                    <option value="custom-agentic-coder" ${model === 'custom-agentic-coder' ? 'selected' : ''}>Custom Agentic Coder (ASM)</option>
                    <option value="your-custom-model" ${model === 'your-custom-model' ? 'selected' : ''}>Custom Model</option>
                </select>
                <button class="clone-button" onclick="cloneWindow()">🗗 Clone</button>
            </div>
        </div>
        <div class="messages" id="messages">
            <div class="message bot-message">
                👋 BigDaddyG ${model} agent is ready! This is a detached chat window that can be moved anywhere on your screen.
            </div>
        </div>
        <div class="input-container">
            <input type="text" class="message-input" id="messageInput" placeholder="Ask BigDaddyG..." />
            <button class="send-button" onclick="sendMessage()">Send</button>
        </div>
    </div>

    <script>
        const vscode = acquireVsCodeApi();
        const messagesDiv = document.getElementById('messages');
        const messageInput = document.getElementById('messageInput');
        const modelSelect = document.getElementById('modelSelect');

        function sendMessage() {
            const message = messageInput.value.trim();
            if (!message) return;

            // Add user message
            addMessage(message, 'user');
            messageInput.value = '';

            // Send to extension
            vscode.postMessage({
                type: 'chat',
                message: message,
                model: modelSelect.value
            });
        }

        function cloneWindow() {
            vscode.postMessage({
                type: 'clone',
                model: modelSelect.value
            });
        }

        function addMessage(content, sender) {
            const messageDiv = document.createElement('div');
            messageDiv.className = \`message \${sender}-message\`;
            messageDiv.textContent = content;
            messagesDiv.appendChild(messageDiv);
            messagesDiv.scrollTop = messagesDiv.scrollHeight;
        }

        // Handle messages from extension
        window.addEventListener('message', event => {
            const message = event.data;
            switch (message.type) {
                case 'response':
                    addMessage(message.content, 'bot');
                    break;
                case 'error':
                    addMessage(message.content, 'bot');
                    break;
            }
        });

        // Enter key to send
        messageInput.addEventListener('keypress', function(e) {
            if (e.key === 'Enter') {
                sendMessage();
            }
        });

        // Focus on input when window loads
        window.addEventListener('load', () => {
            messageInput.focus();
        });
    </script>
</body>
</html>`;
    }
}

export function activate(context: vscode.ExtensionContext) {
    console.log('BigDaddyG Assembly IDE is now active!');

    // Create Project Command
    let createProject = vscode.commands.registerCommand('bigdaddyg.createProject', async () => {
        const workspaceFolders = vscode.workspace.workspaceFolders;
        if (!workspaceFolders) {
            vscode.window.showErrorMessage('Please open a workspace first');
            return;
        }

        const projectName = await vscode.window.showInputBox({
            prompt: 'Enter project name',
            value: 'BigDaddyGProject'
        });

        if (projectName) {
            const projectPath = path.join(workspaceFolders[0].uri.fsPath, projectName);
            
            // Create project structure
            fs.mkdirSync(projectPath, { recursive: true });
            fs.mkdirSync(path.join(projectPath, 'src'), { recursive: true });
            fs.mkdirSync(path.join(projectPath, 'build'), { recursive: true });
            
            // Create main.asm template
            const mainAsmTemplate = `; BigDaddyG Assembly Project: ${projectName}
.386
.model flat, stdcall
option casemap:none

include \\masm32\\include\\windows.inc
include \\masm32\\include\\kernel32.inc
include \\masm32\\include\\user32.inc

includelib \\masm32\\lib\\kernel32.lib
includelib \\masm32\\lib\\user32.lib

.data
    szMsg       db "Hello from ${projectName}!", 0
    szTitle     db "BigDaddyG ASM", 0

.code
start:
    invoke MessageBox, 0, addr szMsg, addr szTitle, MB_OK
    invoke ExitProcess, 0
end start`;

            fs.writeFileSync(path.join(projectPath, 'src', 'main.asm'), mainAsmTemplate);
            
            // Create build script
            const buildScript = `@echo off
echo Building ${projectName}...
ml /c /coff src\\main.asm
link /subsystem:windows src\\main.obj
echo Build complete!
pause`;

            fs.writeFileSync(path.join(projectPath, 'build.bat'), buildScript);
            
            // Open the project
            const uri = vscode.Uri.file(path.join(projectPath, 'src', 'main.asm'));
            vscode.window.showTextDocument(uri);
            
            vscode.window.showInformationMessage(`BigDaddyG ASM project '${projectName}' created successfully!`);
        }
    });

    // Build Project Command
    let buildProject = vscode.commands.registerCommand('bigdaddyg.buildProject', () => {
        const activeEditor = vscode.window.activeTextEditor;
        if (!activeEditor) {
            vscode.window.showErrorMessage('No active editor');
            return;
        }

        const workspaceFolder = vscode.workspace.getWorkspaceFolder(activeEditor.document.uri);
        if (!workspaceFolder) {
            vscode.window.showErrorMessage('File not in workspace');
            return;
        }

        const terminal = vscode.window.createTerminal('BigDaddyG Build');
        terminal.show();
        
        // Save current file first
        activeEditor.document.save();
        
        // Build command
        const buildCmd = `cd "${workspaceFolder.uri.fsPath}" && build.bat`;
        terminal.sendText(buildCmd);
        
        vscode.window.showInformationMessage('Building Assembly project...');
    });

    // Run Project Command
    let runProject = vscode.commands.registerCommand('bigdaddyg.runProject', () => {
        const activeEditor = vscode.window.activeTextEditor;
        if (!activeEditor) {
            vscode.window.showErrorMessage('No active editor');
            return;
        }

        const workspaceFolder = vscode.workspace.getWorkspaceFolder(activeEditor.document.uri);
        if (!workspaceFolder) {
            vscode.window.showErrorMessage('File not in workspace');
            return;
        }

        const exePath = path.join(workspaceFolder.uri.fsPath, 'main.exe');
        if (fs.existsSync(exePath)) {
            exec(`"${exePath}"`, (error, stdout, stderr) => {
                if (error) {
                    vscode.window.showErrorMessage(`Execution error: ${error.message}`);
                    return;
                }
                if (stderr) {
                    vscode.window.showWarningMessage(`Warning: ${stderr}`);
                }
                vscode.window.showInformationMessage('Program executed successfully!');
            });
        } else {
            vscode.window.showErrorMessage('Executable not found. Build the project first.');
        }
    });

    // Assembly Language Provider
    const asmProvider = vscode.languages.registerCompletionItemProvider('asm', {
        provideCompletionItems(document: vscode.TextDocument, position: vscode.Position) {
            const completions: vscode.CompletionItem[] = [];
            
            // Assembly instructions
            const instructions = ['mov', 'add', 'sub', 'mul', 'div', 'cmp', 'jmp', 'call', 'ret', 'push', 'pop', 'invoke'];
            instructions.forEach(inst => {
                const completion = new vscode.CompletionItem(inst, vscode.CompletionItemKind.Keyword);
                completion.detail = `Assembly instruction: ${inst}`;
                completions.push(completion);
            });
            
            // Registers
            const registers = ['eax', 'ebx', 'ecx', 'edx', 'esi', 'edi', 'esp', 'ebp'];
            registers.forEach(reg => {
                const completion = new vscode.CompletionItem(reg, vscode.CompletionItemKind.Variable);
                completion.detail = `32-bit register: ${reg}`;
                completions.push(completion);
            });
            
            return completions;
        }
    });

    // Register Chat Provider
    const chatProvider = new BigDaddyGChatProvider(context.extensionUri);
    context.subscriptions.push(
        vscode.window.registerWebviewViewProvider(BigDaddyGChatProvider.viewType, chatProvider)
    );

    // Open Chat Command
    let openChat = vscode.commands.registerCommand('bigdaddyg.openChat', () => {
        chatProvider.createDetachedChatWindow('bigdaddyg-assembly');
        vscode.window.showInformationMessage('BigDaddyG chat window opened!');
    });

    // Launch Multi-Agent Session Command
    let launchMultiAgent = vscode.commands.registerCommand('bigdaddyg.launchMultiAgent', async () => {
        const agentCount = await vscode.window.showInputBox({
            prompt: 'How many BigDaddyG agents to launch?',
            value: '3',
            validateInput: (value) => {
                const num = parseInt(value);
                if (isNaN(num) || num < 1 || num > 10) {
                    return 'Please enter a number between 1 and 10';
                }
                return null;
            }
        });

        if (agentCount) {
            const count = parseInt(agentCount);
            const models = ['bigdaddyg-assembly', 'bigdaddyg-pe', 'bigdaddyg-reverse', 'custom-agentic-coder', 'bigdaddyg-ensemble'];
            
            for (let i = 0; i < count; i++) {
                const model = models[i % models.length];
                setTimeout(() => {
                    chatProvider.createDetachedChatWindow(model);
                }, i * 500); // Stagger creation
            }
            
            vscode.window.showInformationMessage(`Launched ${count} BigDaddyG agents for assembly development!`);
        }
    });

    // Generate Assembly Code Command
    let generateAssembly = vscode.commands.registerCommand('bigdaddyg.generateAssembly', async () => {
        const activeEditor = vscode.window.activeTextEditor;
        if (!activeEditor) {
            vscode.window.showErrorMessage('No active editor');
            return;
        }

        const selection = activeEditor.selection;
        const selectedText = activeEditor.document.getText(selection);
        
        const prompt = await vscode.window.showInputBox({
            prompt: 'Describe the assembly code you want to generate',
            value: selectedText ? `Convert this to assembly: ${selectedText}` : 'Generate assembly code for...'
        });

        if (prompt) {
            // Use the custom agentic coder for assembly generation
            try {
                const fetch = (await import('node-fetch')).default;
                const response = await fetch('http://localhost:11441/v1/chat/completions', {
                    method: 'POST',
                    headers: {
                        'Content-Type': 'application/json',
                        'Authorization': 'Bearer sk-local-bigdaddyg'
                    },
                    body: JSON.stringify({
                        model: 'custom-agentic-coder',
                        messages: [{ 
                            role: 'system', 
                            content: 'You are an expert assembly programmer. Generate efficient, well-commented assembly code for x86-64 architecture. Use MASM syntax.' 
                        }, { 
                            role: 'user', 
                            content: prompt 
                        }],
                        temperature: 0.2,
                        max_tokens: 2000
                    })
                });

                if (response.ok) {
                    const data = await response.json() as any;
                    const generatedCode = data.choices[0]?.message?.content || 'No code generated';
                    
                    // Insert the generated code
                    const edit = new vscode.WorkspaceEdit();
                    if (selectedText) {
                        edit.replace(activeEditor.document.uri, selection, generatedCode);
                    } else {
                        edit.insert(activeEditor.document.uri, activeEditor.selection.active, generatedCode);
                    }
                    vscode.workspace.applyEdit(edit);
                    
                    vscode.window.showInformationMessage('Assembly code generated successfully!');
                } else {
                    vscode.window.showErrorMessage(`Generation failed: ${response.status}`);
                }
            } catch (error) {
                vscode.window.showErrorMessage(`Error: ${error instanceof Error ? error.message : 'Unknown error'}`);
            }
        }
    });

    context.subscriptions.push(createProject, buildProject, runProject, asmProvider, openChat, launchMultiAgent, generateAssembly);
}

export function deactivate() {}
