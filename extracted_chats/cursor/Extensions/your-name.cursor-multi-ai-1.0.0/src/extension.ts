import * as vscode from 'vscode';
import { AIService } from './aiService';

function getSelectedCode(): string | undefined {
    const editor = vscode.window.activeTextEditor;
    if (!editor) {
        return undefined;
    }

    const selection = editor.selection;
    if (selection.isEmpty) {
        return undefined;
    }

    return editor.document.getText(selection);
}

export function activate(context: vscode.ExtensionContext) {
    const aiService = new AIService(context);

    context.subscriptions.push(
        vscode.commands.registerCommand('cursor-multi-ai.askQuestion', async () => {
            const question = await vscode.window.showInputBox({
                prompt: 'Ask a question'
            });

            if (!question) {
                return;
            }

            try {
                const response = await aiService.askQuestion(question);
                vscode.window.showInformationMessage(response, { modal: true });
            } catch (error: any) {
                vscode.window.showErrorMessage(error?.message ?? String(error));
            }
        })
    );

    context.subscriptions.push(
        vscode.commands.registerCommand('cursor-multi-ai.explainCode', async () => {
            const selection = getSelectedCode();
            if (!selection) {
                vscode.window.showWarningMessage('Select some code to explain.');
                return;
            }

            try {
                const response = await aiService.explainCode(selection);
                showOutput('Explanation', response);
            } catch (error: any) {
                vscode.window.showErrorMessage(error?.message ?? String(error));
            }
        })
    );

    context.subscriptions.push(
        vscode.commands.registerCommand('cursor-multi-ai.generateTests', async () => {
            const selection = getSelectedCode();
            if (!selection) {
                vscode.window.showWarningMessage('Select some code to generate tests for.');
                return;
            }

            try {
                const response = await aiService.generateTests(selection);
                showOutput('Generated Tests', response);
            } catch (error: any) {
                vscode.window.showErrorMessage(error?.message ?? String(error));
            }
        })
    );
}

export function deactivate() {}

function showOutput(title: string, content: string) {
    const channel = vscode.window.createOutputChannel(`Cursor AI Suite ▸ ${title}`);
    channel.show(true);
    channel.appendLine(content);
}

