const vscode = require('vscode');
const { spawn } = require('child_process');

function activate(context) {
    console.log('Simple AI Integration activated');

    // Ask AI command
    const askCommand = vscode.commands.registerCommand('simple-ai.ask', async () => {
        const prompt = await vscode.window.showInputBox({
            prompt: 'Ask your custom AI',
            placeHolder: 'Enter your question...'
        });

        if (prompt) {
            await callAI(prompt);
        }
    });

    // Explain code command
    const explainCommand = vscode.commands.registerCommand('simple-ai.explain', async () => {
        const editor = vscode.window.activeTextEditor;
        if (editor) {
            const selection = editor.document.getText(editor.selection);
            if (selection) {
                await callAI(`Explain this code:\n\n${selection}`);
            } else {
                vscode.window.showWarningMessage('Please select some code to explain');
            }
        }
    });

    // Optimize code command
    const optimizeCommand = vscode.commands.registerCommand('simple-ai.optimize', async () => {
        const editor = vscode.window.activeTextEditor;
        if (editor) {
            const selection = editor.document.getText(editor.selection);
            if (selection) {
                await callAI(`Optimize this code:\n\n${selection}`);
            } else {
                vscode.window.showWarningMessage('Please select some code to optimize');
            }
        }
    });

    context.subscriptions.push(askCommand, explainCommand, optimizeCommand);
}

async function callAI(prompt) {
    return new Promise((resolve, reject) => {
        vscode.window.showInformationMessage('Asking your custom AI...');

        const process = spawn('ollama', ['run', 'your-custom-model'], {
            stdio: ['pipe', 'pipe', 'pipe']
        });

        let output = '';
        let error = '';

        process.stdout.on('data', (data) => {
            output += data.toString();
        });

        process.stderr.on('data', (data) => {
            error += data.toString();
        });

        process.on('close', (code) => {
            if (code === 0) {
                // Show response in a new document
                const doc = vscode.workspace.openTextDocument({
                    content: `AI Response:\n\n${output.trim()}`,
                    language: 'markdown'
                });
                doc.then(d => vscode.window.showTextDocument(d));
                resolve(output.trim());
            } else {
                vscode.window.showErrorMessage(`AI Error: ${error}`);
                reject(new Error(error));
            }
        });

        process.on('error', (err) => {
            vscode.window.showErrorMessage(`Failed to start AI: ${err.message}`);
            reject(err);
        });

        // Send prompt
        process.stdin.write(prompt + '\n');
        process.stdin.end();
    });
}

function deactivate() {}

module.exports = {
    activate,
    deactivate
};
