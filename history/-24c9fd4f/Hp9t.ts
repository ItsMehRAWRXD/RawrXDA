import * as vscode from 'vscode';
import { AgenticCopilot } from './agenticCopilot';

export function activate(context: vscode.ExtensionContext) {
    const agenticCopilot = new AgenticCopilot();

    // Register commands
    const commands = [
        vscode.commands.registerCommand('agentic-copilot.toggleAgenticMode', () => {
            agenticCopilot.toggleAgenticMode();
        }),
        vscode.commands.registerCommand('agentic-copilot.generateCode', () => {
            agenticCopilot.generateCode();
        }),
        vscode.commands.registerCommand('agentic-copilot.explainCode', () => {
            agenticCopilot.explainCode();
        }),
        vscode.commands.registerCommand('agentic-copilot.fixCode', () => {
            agenticCopilot.fixCode();
        }),
        vscode.commands.registerCommand('agentic-copilot.showStatus', () => {
            agenticCopilot.showStatus();
        })
    ];

    // Add all commands to context
    commands.forEach(command => {
        context.subscriptions.push(command);
    });

    // Add agentic copilot to context for disposal
    context.subscriptions.push({
        dispose: () => agenticCopilot.dispose()
    });

    console.log('Local Agentic Copilot extension is now active!');
}

export function deactivate() {}