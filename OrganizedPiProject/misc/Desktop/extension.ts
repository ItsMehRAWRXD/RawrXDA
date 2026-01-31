// extension.ts - VSCode extension with React webview and AI agent integration
import * as vscode from 'vscode';
import * as path from 'path';

interface AgentMessage {
    command: string;
    text?: string;
    feature?: string;
    mode?: 'chat' | 'ask' | 'features';
    context?: any;
}

interface AgentResponse {
    status: 'success' | 'error' | 'awaiting_human_input';
    message: string;
    data?: any;
    requiresReview?: boolean;
}

interface FeedbackData {
    userId: string;
    sessionId: string;
    conversationId: string;
    originalPrompt: string;
    originalResponse: string;
    interaction: 'ACCEPT' | 'REJECT' | 'EDIT' | 'RATE';
    editedResponse?: string;
    rating?: number;
    context?: string;
    reasoning?: string;
}

let currentPanel: vscode.WebviewPanel | undefined = undefined;
let lspClient: any = undefined;
let currentConversation: { prompt: string; response: string; sessionId: string } | undefined = undefined;

export function activate(context: vscode.ExtensionContext) {
    console.log('AI Agent Extension activated');

    // Register command to start AI chat
    context.subscriptions.push(
        vscode.commands.registerCommand('aicli.startChat', () => {
            createOrShowPanel(context);
        })
    );

    // Register command to ask AI a question
    context.subscriptions.push(
        vscode.commands.registerCommand('aicli.askQuestion', () => {
            createOrShowPanel(context, 'ask');
        })
    );

    // Register command to show features
    context.subscriptions.push(
        vscode.commands.registerCommand('aicli.showFeatures', () => {
            createOrShowPanel(context, 'features');
        })
    );

    // Register feedback commands
    context.subscriptions.push(
        vscode.commands.registerCommand('aicli.acceptAiOutput', async () => {
            await submitFeedback('ACCEPT');
        })
    );

    context.subscriptions.push(
        vscode.commands.registerCommand('aicli.rejectAiOutput', async () => {
            await submitFeedback('REJECT');
        })
    );

    context.subscriptions.push(
        vscode.commands.registerCommand('aicli.editAiOutput', async () => {
            await submitFeedback('EDIT');
        })
    );

    context.subscriptions.push(
        vscode.commands.registerCommand('aicli.rateAiOutput', async () => {
            await submitFeedback('RATE');
        })
    );

    // Register command to refactor code
    context.subscriptions.push(
        vscode.commands.registerCommand('aicli.refactorCode', async () => {
            const editor = vscode.window.activeTextEditor;
            if (!editor) {
                vscode.window.showErrorMessage('No active editor found');
                return;
            }

            const selectedText = editor.document.getText(editor.selection);
            const fullText = editor.document.getText();
            const textToRefactor = selectedText || fullText;

            try {
                const response = await sendToAgent({
                    command: 'refactor',
                    text: textToRefactor,
                    context: {
                        filePath: editor.document.uri.fsPath,
                        language: editor.document.languageId,
                        selection: editor.selection
                    }
                });

                if (response.requiresReview) {
                    await showDiffAndAwaitApproval(response.data);
                } else {
                    vscode.window.showInformationMessage('Refactoring completed: ' + response.message);
                }
            } catch (error) {
                vscode.window.showErrorMessage('Refactoring failed: ' + error.message);
            }
        })
    );

    // Register command to generate tests
    context.subscriptions.push(
        vscode.commands.registerCommand('aicli.generateTests', async () => {
            const editor = vscode.window.activeTextEditor;
            if (!editor) {
                vscode.window.showErrorMessage('No active editor found');
                return;
            }

            const code = editor.document.getText();
            try {
                const response = await sendToAgent({
                    command: 'generateTests',
                    text: code,
                    context: {
                        filePath: editor.document.uri.fsPath,
                        language: editor.document.languageId
                    }
                });

                if (response.requiresReview) {
                    await showDiffAndAwaitApproval(response.data);
                } else {
                    // Create a new file with generated tests
                    const testFileName = generateTestFileName(editor.document.fileName);
                    await createTestFile(testFileName, response.data.tests);
                }
            } catch (error) {
                vscode.window.showErrorMessage('Test generation failed: ' + error.message);
            }
        })
    );

    // Register command to analyze security
    context.subscriptions.push(
        vscode.commands.registerCommand('aicli.analyzeSecurity', async () => {
            const editor = vscode.window.activeTextEditor;
            if (!editor) {
                vscode.window.showErrorMessage('No active editor found');
                return;
            }

            const code = editor.document.getText();
            try {
                const response = await sendToAgent({
                    command: 'analyzeSecurity',
                    text: code,
                    context: {
                        filePath: editor.document.uri.fsPath,
                        language: editor.document.languageId
                    }
                });

                // Show security analysis in a new document
                const doc = await vscode.workspace.openTextDocument({
                    content: response.data.analysis,
                    language: 'markdown'
                });
                await vscode.window.showTextDocument(doc);
            } catch (error) {
                vscode.window.showErrorMessage('Security analysis failed: ' + error.message);
            }
        })
    );
}

export function deactivate() {
    if (lspClient) {
        lspClient.stop();
    }
}

function createOrShowPanel(context: vscode.ExtensionContext, initialMode: 'chat' | 'ask' | 'features' = 'chat') {
    if (currentPanel) {
        currentPanel.reveal(vscode.ViewColumn.Beside);
        // Send mode change message to existing panel
        currentPanel.webview.postMessage({ command: 'setMode', mode: initialMode });
        return;
    }

    currentPanel = vscode.window.createWebviewPanel(
        'aicliChat',
        'AI Agent Assistant',
        vscode.ViewColumn.Beside,
        {
            enableScripts: true,
            retainContextWhenHidden: true,
            localResourceRoots: [
                vscode.Uri.joinPath(context.extensionUri, 'webview-ui', 'dist')
            ]
        }
    );

    currentPanel.webview.html = getWebviewContent(currentPanel.webview, context.extensionUri, initialMode);

    // Handle messages from the webview
    currentPanel.webview.onDidReceiveMessage(async (message: AgentMessage) => {
        try {
            const response = await handleWebviewMessage(message);
            currentPanel!.webview.postMessage(response);
        } catch (error) {
            currentPanel!.webview.postMessage({
                status: 'error',
                message: error.message
            });
        }
    });

    currentPanel.onDidDispose(() => {
        currentPanel = undefined;
    });
}

async function handleWebviewMessage(message: AgentMessage): Promise<AgentResponse> {
    switch (message.command) {
        case 'askAgent':
        case 'chatAgent':
            return await sendToAgent({
                command: message.command,
                text: message.text,
                mode: message.mode
            });

        case 'executeFeature':
            return await executeFeature(message.feature!);

        case 'requestHumanApproval':
            return await handleHumanApproval(message);

        case 'provideFeedback':
            return await handleFeedback(message);

        default:
            throw new Error(`Unknown command: ${message.command}`);
    }
}

async function sendToAgent(message: AgentMessage): Promise<AgentResponse> {
    // In a real implementation, this would communicate with the Java backend
    // For now, we'll simulate the communication
    
    try {
        // Simulate API call to Java backend
        const response = await simulateAgentCall(message);
        return response;
    } catch (error) {
        return {
            status: 'error',
            message: `Agent communication failed: ${error.message}`
        };
    }
}

async function simulateAgentCall(message: AgentMessage): Promise<AgentResponse> {
    // Simulate processing time
    await new Promise(resolve => setTimeout(resolve, 1000));

    // Simulate different responses based on command
    switch (message.command) {
        case 'askAgent':
            return {
                status: 'success',
                message: `AI Response to "${message.text}": This is a simulated response for your question. In a real implementation, this would come from your Java backend.`
            };

        case 'chatAgent':
            return {
                status: 'success',
                message: `AI Chat Response: I understand you want to chat about "${message.text}". How can I help you further?`
            };

        case 'refactor':
            return {
                status: 'awaiting_human_input',
                message: 'Code refactoring completed. Please review the proposed changes.',
                requiresReview: true,
                data: {
                    original: message.text,
                    refactored: `// Refactored version of your code\n${message.text}\n// Added improvements and optimizations`,
                    filePath: message.context?.filePath
                }
            };

        case 'generateTests':
            return {
                status: 'success',
                message: 'Test generation completed.',
                data: {
                    tests: `// Generated unit tests\nimport org.junit.jupiter.api.Test;\nimport static org.junit.jupiter.api.Assertions.*;\n\nclass GeneratedTests {\n    @Test\n    void testGenerated() {\n        // Test implementation\n    }\n}`
                }
            };

        case 'analyzeSecurity':
            return {
                status: 'success',
                message: 'Security analysis completed.',
                data: {
                    analysis: `# Security Analysis Report\n\n## Findings\n- No critical vulnerabilities found\n- Consider adding input validation\n- Review authentication mechanisms`
                }
            };

        default:
            return {
                status: 'success',
                message: `Executed ${message.command} successfully.`
            };
    }
}

async function executeFeature(featureName: string): Promise<AgentResponse> {
    switch (featureName) {
        case 'Refactor Code':
            await vscode.commands.executeCommand('aicli.refactorCode');
            return { status: 'success', message: 'Refactoring initiated' };

        case 'Generate Tests':
            await vscode.commands.executeCommand('aicli.generateTests');
            return { status: 'success', message: 'Test generation initiated' };

        case 'Analyze Security':
            await vscode.commands.executeCommand('aicli.analyzeSecurity');
            return { status: 'success', message: 'Security analysis initiated' };

        default:
            return { status: 'error', message: `Unknown feature: ${featureName}` };
    }
}

async function handleHumanApproval(message: AgentMessage): Promise<AgentResponse> {
    // This would communicate with the Java backend to handle approval workflow
    return {
        status: 'success',
        message: 'Approval processed'
    };
}

async function handleFeedback(message: AgentMessage): Promise<AgentResponse> {
    // This would send feedback to the Java backend for learning
    return {
        status: 'success',
        message: 'Feedback recorded'
    };
}

async function showDiffAndAwaitApproval(data: any): Promise<void> {
    const originalUri = vscode.Uri.file(data.filePath);
    const proposedUri = vscode.Uri.parse(`aicli-proposed:${data.filePath}`);

    // Create temporary document with proposed changes
    const doc = await vscode.workspace.openTextDocument({
        content: data.refactored,
        language: 'plaintext'
    });

    // Show diff
    await vscode.commands.executeCommand('vscode.diff', originalUri, proposedUri, 'AI Proposed Changes');

    // Show approval dialog
    const choice = await vscode.window.showQuickPick(
        ['Accept Changes', 'Reject Changes', 'Edit Manually'],
        { placeHolder: 'Review the proposed changes' }
    );

    if (choice === 'Accept Changes') {
        // Apply changes to the original file
        const editor = vscode.window.activeTextEditor;
        if (editor) {
            const edit = new vscode.WorkspaceEdit();
            edit.replace(editor.document.uri, editor.document.validateRange(new vscode.Range(0, 0, editor.document.lineCount, 0)), data.refactored);
            await vscode.workspace.applyEdit(edit);
        }
        vscode.window.showInformationMessage('Changes applied successfully');
    } else if (choice === 'Edit Manually') {
        vscode.window.showInformationMessage('Opening editor for manual review');
    }
}

function generateTestFileName(originalFileName: string): string {
    const ext = path.extname(originalFileName);
    const baseName = path.basename(originalFileName, ext);
    return path.join(path.dirname(originalFileName), `${baseName}Test${ext}`);
}

async function createTestFile(fileName: string, content: string): Promise<void> {
    const uri = vscode.Uri.file(fileName);
    await vscode.workspace.fs.writeFile(uri, Buffer.from(content, 'utf8'));
    const doc = await vscode.workspace.openTextDocument(uri);
    await vscode.window.showTextDocument(doc);
}

/**
 * Submit feedback to the backend
 */
async function submitFeedback(interaction: 'ACCEPT' | 'REJECT' | 'EDIT' | 'RATE'): Promise<void> {
    if (!currentConversation) {
        vscode.window.showWarningMessage('No conversation to provide feedback on');
        return;
    }

    try {
        let editedResponse: string | undefined;
        let rating: number | undefined;
        let reasoning: string | undefined;

        // Handle different interaction types
        switch (interaction) {
            case 'EDIT':
                editedResponse = await vscode.window.showInputBox({
                    prompt: 'Enter your edited version of the AI response',
                    value: currentConversation.response
                });
                if (!editedResponse) return;
                break;

            case 'RATE':
                const ratingInput = await vscode.window.showInputBox({
                    prompt: 'Rate the AI response (1-5)',
                    validateInput: (value) => {
                        const num = parseInt(value);
                        if (isNaN(num) || num < 1 || num > 5) {
                            return 'Please enter a number between 1 and 5';
                        }
                        return null;
                    }
                });
                if (!ratingInput) return;
                rating = parseInt(ratingInput);
                break;

            case 'REJECT':
                reasoning = await vscode.window.showInputBox({
                    prompt: 'Why are you rejecting this response? (optional)'
                });
                break;
        }

        // Create feedback data
        const feedbackData: FeedbackData = {
            userId: 'vscode_user', // In a real app, this would be the actual user ID
            sessionId: currentConversation.sessionId,
            conversationId: currentConversation.sessionId,
            originalPrompt: currentConversation.prompt,
            originalResponse: currentConversation.response,
            interaction: interaction,
            editedResponse: editedResponse,
            rating: rating,
            context: 'VSCode Extension',
            reasoning: reasoning
        };

        // Send feedback to backend
        await sendFeedbackToBackend(feedbackData);

        vscode.window.showInformationMessage(`Feedback submitted: ${interaction}`);

    } catch (error) {
        vscode.window.showErrorMessage(`Failed to submit feedback: ${error}`);
    }
}

/**
 * Send feedback data to the backend
 */
async function sendFeedbackToBackend(feedbackData: FeedbackData): Promise<void> {
    const backendUrl = 'http://localhost:8080/feedback';
    
    const response = await fetch(backendUrl, {
        method: 'POST',
        headers: {
            'Content-Type': 'application/json',
        },
        body: JSON.stringify(feedbackData)
    });

    if (!response.ok) {
        throw new Error(`HTTP ${response.status}: ${response.statusText}`);
    }
}

/**
 * Store current conversation for feedback
 */
function storeCurrentConversation(prompt: string, response: string): void {
    currentConversation = {
        prompt: prompt,
        response: response,
        sessionId: `session_${Date.now()}`
    };
}

function getWebviewContent(webview: vscode.Webview, extensionUri: vscode.Uri, initialMode: string = 'chat'): string {
    // In a real implementation, these would point to your built React app
    const scriptUri = webview.asWebviewUri(vscode.Uri.joinPath(extensionUri, 'webview-ui', 'dist', 'main.js'));
    const styleUri = webview.asWebviewUri(vscode.Uri.joinPath(extensionUri, 'webview-ui', 'dist', 'main.css'));

    return `<!DOCTYPE html>
    <html lang="en">
    <head>
        <meta charset="UTF-8">
        <meta name="viewport" content="width=device-width, initial-scale=1.0">
        <title>AI Agent Assistant</title>
        <link href="${styleUri}" rel="stylesheet">
        <style>
            body {
                margin: 0;
                padding: 0;
                font-family: var(--vscode-font-family);
                background-color: var(--vscode-editor-background);
                color: var(--vscode-editor-foreground);
            }
            .loading {
                display: flex;
                justify-content: center;
                align-items: center;
                height: 100vh;
                font-size: 18px;
            }
        </style>
    </head>
    <body>
        <div id="root">
            <div class="loading">Loading AI Agent Interface...</div>
        </div>
        <script>
            window.vscode = acquireVsCodeApi();
            window.initialMode = '${initialMode}';
        </script>
        <script src="${scriptUri}"></script>
    </body>
    </html>`;
}
