"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.deactivate = exports.activate = exports.codeShareProvider = exports.extensionFolder = void 0;
const vscode = require("vscode");
const CodeShareAPI_1 = require("./CodeShareAPI");
const CodeShareProvider_1 = require("./CodeShareProvider");
const Constants_1 = require("./Constants");
const UIUtils_1 = require("./UIUtils");
const fs = require("fs");
exports.codeShareProvider = new CodeShareProvider_1.CodeShareProvider();
// this method is called when your extension is activated
function activate(context) {
    // create extension folder
    exports.extensionFolder = context.globalStorageUri.path.substring(1, context.globalStorageUri.path.length);
    if (!fs.existsSync(exports.extensionFolder)) {
        fs.mkdirSync(exports.extensionFolder);
    }
    //#region register tree data provider
    vscode.window.registerTreeDataProvider('codeshareShares', exports.codeShareProvider);
    //#endregion
    //#region website command
    const websiteCommand = vscode.commands.registerCommand('codeshare.website', async () => {
        vscode.env.openExternal(vscode.Uri.parse(Constants_1.Constants.appURL));
    });
    context.subscriptions.push(websiteCommand);
    //#endregion
    //#region share code command
    const shareCommand = vscode.commands.registerCommand('codeshare.share', async () => {
        const currentEditor = vscode.window.activeTextEditor;
        if (!currentEditor) {
            return;
        }
        const currentLanguage = currentEditor.document.languageId;
        const selection = currentEditor.selection;
        let selectedText = currentEditor.document.getText(selection);
        // get whole text if nothing selected
        if (selectedText === '') {
            selectedText = currentEditor.document.getText();
        }
        const shareID = await vscode.window.withProgress({
            location: vscode.ProgressLocation.Notification,
            title: "Generating code share...",
        }, async (progress, token) => {
            const response = await CodeShareAPI_1.CodeShareAPI.createShare(selectedText, currentLanguage, undefined, true);
            if (response === undefined) {
                return;
            }
            const jsonResponse = JSON.parse(response.body);
            const shareID = jsonResponse.data.id;
            return new Promise(resolve => {
                resolve(shareID);
            });
        });
        UIUtils_1.UIUtils.showCodeShareMessage(shareID);
    });
    context.subscriptions.push(shareCommand);
    //#endregion
    //#region share code with password command
    const shareWithPasswordCommand = vscode.commands.registerCommand('codeshare.shareWithPassword', async () => {
        const currentEditor = vscode.window.activeTextEditor;
        if (!currentEditor) {
            return;
        }
        const password = await vscode.window.showInputBox({
            password: true,
            placeHolder: 'enter password for code share'
        });
        // check if password is filled
        if (password === undefined || password === '') {
            return;
        }
        const currentLanguage = currentEditor.document.languageId;
        const selection = currentEditor.selection;
        let selectedText = currentEditor.document.getText(selection);
        // get whole text if nothing selected
        if (selectedText === '') {
            selectedText = currentEditor.document.getText();
        }
        const shareID = await vscode.window.withProgress({
            location: vscode.ProgressLocation.Notification,
            title: "Generating code share...",
        }, async (progress, token) => {
            const response = await CodeShareAPI_1.CodeShareAPI.createShare(selectedText, currentLanguage, password, true);
            if (response === undefined) {
                return;
            }
            const jsonResponse = JSON.parse(response.body);
            const shareID = jsonResponse.data.id;
            return new Promise(resolve => {
                resolve(shareID);
            });
        });
        UIUtils_1.UIUtils.showCodeShareMessage(shareID);
    });
    context.subscriptions.push(shareWithPasswordCommand);
    //#endregion
    //#region view: open share in browser
    const openShareInBrowserCommand = vscode.commands.registerCommand('codeshare.view.openShare', async (contextItem) => {
        if (contextItem === null) {
            return;
        }
        vscode.env.openExternal(vscode.Uri.parse(CodeShareAPI_1.CodeShareAPI.getShareURL(contextItem.shareID)));
    });
    context.subscriptions.push(openShareInBrowserCommand);
    //#endregion
    //#region view: copy share
    const copyShareCommand = vscode.commands.registerCommand('codeshare.view.copyShare', async (contextItem) => {
        if (contextItem === null) {
            return;
        }
        vscode.env.clipboard.writeText(CodeShareAPI_1.CodeShareAPI.getShareURL(contextItem.shareID));
    });
    context.subscriptions.push(copyShareCommand);
    //#endregion
    //#region view: remove share
    const removeShareCommand = vscode.commands.registerCommand('codeshare.view.removeShare', async (contextItem) => {
        if (contextItem === null) {
            return;
        }
        CodeShareAPI_1.CodeShareAPI.removeShareHistoryItem(contextItem.shareID);
        exports.codeShareProvider.refresh();
    });
    context.subscriptions.push(removeShareCommand);
    //#endregion
}
exports.activate = activate;
// this method is called when your extension is deactivated
function deactivate() { }
exports.deactivate = deactivate;
//# sourceMappingURL=extension.js.map