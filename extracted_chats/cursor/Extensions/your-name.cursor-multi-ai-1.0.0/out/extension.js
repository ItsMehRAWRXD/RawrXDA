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
var __importStar = (this && this.__importStar) || function (mod) {
    if (mod && mod.__esModule) return mod;
    var result = {};
    if (mod != null) for (var k in mod) if (k !== "default" && Object.prototype.hasOwnProperty.call(mod, k)) __createBinding(result, mod, k);
    __setModuleDefault(result, mod);
    return result;
};
Object.defineProperty(exports, "__esModule", { value: true });
exports.deactivate = exports.activate = void 0;
const vscode = __importStar(require("vscode"));
const aiService_1 = require("./aiService");
function getSelectedCode() {
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
function activate(context) {
    const aiService = new aiService_1.AIService(context);
    context.subscriptions.push(vscode.commands.registerCommand('cursor-multi-ai.askQuestion', async () => {
        const question = await vscode.window.showInputBox({
            prompt: 'Ask a question'
        });
        if (!question) {
            return;
        }
        try {
            const response = await aiService.askQuestion(question);
            vscode.window.showInformationMessage(response, { modal: true });
        }
        catch (error) {
            vscode.window.showErrorMessage(error?.message ?? String(error));
        }
    }));
    context.subscriptions.push(vscode.commands.registerCommand('cursor-multi-ai.explainCode', async () => {
        const selection = getSelectedCode();
        if (!selection) {
            vscode.window.showWarningMessage('Select some code to explain.');
            return;
        }
        try {
            const response = await aiService.explainCode(selection);
            showOutput('Explanation', response);
        }
        catch (error) {
            vscode.window.showErrorMessage(error?.message ?? String(error));
        }
    }));
    context.subscriptions.push(vscode.commands.registerCommand('cursor-multi-ai.generateTests', async () => {
        const selection = getSelectedCode();
        if (!selection) {
            vscode.window.showWarningMessage('Select some code to generate tests for.');
            return;
        }
        try {
            const response = await aiService.generateTests(selection);
            showOutput('Generated Tests', response);
        }
        catch (error) {
            vscode.window.showErrorMessage(error?.message ?? String(error));
        }
    }));
}
exports.activate = activate;
function deactivate() { }
exports.deactivate = deactivate;
function showOutput(title, content) {
    const channel = vscode.window.createOutputChannel(`Cursor AI Suite ▸ ${title}`);
    channel.show(true);
    channel.appendLine(content);
}
//# sourceMappingURL=extension.js.map