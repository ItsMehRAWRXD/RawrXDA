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
Object.defineProperty(exports, "__esModule", { value: true });
exports.activate = activate;
exports.deactivate = deactivate;
const vscode = __importStar(require("vscode"));
const contextRetriever_1 = require("./contextRetriever");
const agentOrchestrator_1 = require("./agentOrchestrator");
function activate(context) {
    const contextRetriever = new contextRetriever_1.ContextRetriever();
    const agentOrchestrator = new agentOrchestrator_1.AgentOrchestrator();
    // Generate Code Command
    const generateCommand = vscode.commands.registerCommand('rawrz.generate', async () => {
        const editor = vscode.window.activeTextEditor;
        if (!editor)
            return;
        const selection = editor.document.getText(editor.selection);
        const languageId = editor.document.languageId;
        const filePath = editor.document.fileName;
        try {
            // Get relevant context from codebase
            let context = '';
            const config = vscode.workspace.getConfiguration('rawrz.context');
            if (config.get('enabled', true)) {
                context = await contextRetriever.getRelevantContext(selection, filePath, languageId);
            }
            // Orchestrate AI agents
            const completion = await agentOrchestrator.generateCode(selection, languageId, context, 'generate');
            // Apply with user confirmation
            const apply = await vscode.window.showInformationMessage('Apply generated code?', 'Apply', 'Preview', 'Cancel');
            if (apply === 'Apply') {
                const edit = new vscode.WorkspaceEdit();
                edit.replace(editor.document.uri, editor.selection, completion);
                await vscode.workspace.applyEdit(edit);
                vscode.window.showInformationMessage('RawrZ: Code applied successfully!');
            }
            else if (apply === 'Preview') {
                const doc = await vscode.workspace.openTextDocument({
                    content: `// RawrZ Generated Code\n${completion}`,
                    language: languageId
                });
                await vscode.window.showTextDocument(doc, { preview: true });
            }
        }
        catch (error) {
            vscode.window.showErrorMessage(`RawrZ Generation Failed: ${error.message}`);
        }
    });
    // Explain Code Command
    const explainCommand = vscode.commands.registerCommand('rawrz.explain', async () => {
        const editor = vscode.window.activeTextEditor;
        if (!editor)
            return;
        const selection = editor.document.getText(editor.selection);
        const languageId = editor.document.languageId;
        try {
            const explanation = await agentOrchestrator.explainCode(selection, languageId);
            const doc = await vscode.workspace.openTextDocument({
                content: `# RawrZ Code Explanation\n\n${explanation}`,
                language: 'markdown'
            });
            await vscode.window.showTextDocument(doc, { preview: true });
        }
        catch (error) {
            vscode.window.showErrorMessage(`RawrZ Explanation Failed: ${error.message}`);
        }
    });
    // Refactor Code Command
    const refactorCommand = vscode.commands.registerCommand('rawrz.refactor', async () => {
        const editor = vscode.window.activeTextEditor;
        if (!editor)
            return;
        const selection = editor.document.getText(editor.selection);
        const languageId = editor.document.languageId;
        const filePath = editor.document.fileName;
        try {
            let context = '';
            const config = vscode.workspace.getConfiguration('rawrz.context');
            if (config.get('enabled', true)) {
                context = await contextRetriever.getRelevantContext(selection, filePath, languageId);
            }
            const refactored = await agentOrchestrator.refactorCode(selection, languageId, context);
            const apply = await vscode.window.showInformationMessage('Apply refactored code?', 'Apply', 'Preview', 'Cancel');
            if (apply === 'Apply') {
                const edit = new vscode.WorkspaceEdit();
                edit.replace(editor.document.uri, editor.selection, refactored);
                await vscode.workspace.applyEdit(edit);
                vscode.window.showInformationMessage('RawrZ: Code refactored successfully!');
            }
            else if (apply === 'Preview') {
                const doc = await vscode.workspace.openTextDocument({
                    content: `// RawrZ Refactored Code\n${refactored}`,
                    language: languageId
                });
                await vscode.window.showTextDocument(doc, { preview: true });
            }
        }
        catch (error) {
            vscode.window.showErrorMessage(`RawrZ Refactoring Failed: ${error.message}`);
        }
    });
    // Learn Workspace Command
    const learnCommand = vscode.commands.registerCommand('rawrz.learnWorkspace', async () => {
        const workspaceFolders = vscode.workspace.workspaceFolders;
        if (!workspaceFolders) {
            vscode.window.showErrorMessage('No workspace folder open');
            return;
        }
        try {
            await vscode.window.withProgress({
                location: vscode.ProgressLocation.Notification,
                title: 'RawrZ: Learning from workspace...',
                cancellable: false
            }, async (progress) => {
                await contextRetriever.learnWorkspace(workspaceFolders[0].uri.fsPath, progress);
            });
            vscode.window.showInformationMessage('RawrZ: Workspace learning complete!');
        }
        catch (error) {
            vscode.window.showErrorMessage(`Learning failed: ${error.message}`);
        }
    });
    // Toggle Agentic Mode
    const toggleCommand = vscode.commands.registerCommand('rawrz.toggleAgenticMode', async () => {
        const config = vscode.workspace.getConfiguration('rawrz.agentic');
        const currentMode = config.get('mode', 'collaborative');
        const modes = [
            { label: '🤖 Autonomous', description: 'AI applies edits directly', mode: 'autonomous' },
            { label: '👥 Collaborative', description: 'AI suggests, you approve', mode: 'collaborative' },
            { label: '🔒 Paranoid', description: 'Preview only, no writes', mode: 'paranoid' }
        ];
        const selected = await vscode.window.showQuickPick(modes, {
            placeHolder: `Current mode: ${currentMode}`
        });
        if (selected) {
            await config.update('mode', selected.mode, vscode.ConfigurationTarget.Global);
            vscode.window.showInformationMessage(`RawrZ mode: ${selected.mode}`);
        }
    });
    context.subscriptions.push(generateCommand, explainCommand, refactorCommand, learnCommand, toggleCommand);
    // Show activation message
    vscode.window.showInformationMessage('RawrZ Agentic AI is now active! Use right-click context menu or Command Palette.');
}
function deactivate() { }
//# sourceMappingURL=extension.js.map