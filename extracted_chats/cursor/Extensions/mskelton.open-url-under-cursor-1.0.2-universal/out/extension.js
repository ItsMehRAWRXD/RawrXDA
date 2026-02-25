"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.activate = activate;
exports.deactivate = deactivate;
const vscode = require("vscode");
const urlDetection_1 = require("./urlDetection");
function activate(context) {
    const disposable = vscode.commands.registerCommand("open-url.openUrlUnderCursor", async () => {
        const editor = vscode.window.activeTextEditor;
        if (!editor) {
            vscode.window.showErrorMessage("No active editor");
            return;
        }
        // Get the current cursor position
        const position = editor.selection.active;
        // Get the document text and line text
        const document = editor.document;
        const line = document.lineAt(position.line);
        const lineText = line.text;
        // Find a URL at the cursor position (character offset in the line)
        const url = (0, urlDetection_1.findUrlAtPosition)(lineText, position.character);
        if (url) {
            // Open the URL in the default browser
            vscode.env.openExternal(vscode.Uri.parse(url));
        }
        else {
            vscode.window.showInformationMessage("No URL found under cursor");
        }
    });
    context.subscriptions.push(disposable);
}
function deactivate() { }
//# sourceMappingURL=extension.js.map