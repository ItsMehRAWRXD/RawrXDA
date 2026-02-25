"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.UIUtils = void 0;
const vscode = require("vscode");
const CodeShareAPI_1 = require("./CodeShareAPI");
class UIUtils {
    static async showErrorView(error) {
        // create webview panel
        const panel = vscode.window.createWebviewPanel('codeshare.view.error', 'CodeShare Error', vscode.ViewColumn.One, {});
        // set html content
        panel.webview.html = `
      <!DOCTYPE html>
      <html lang="en">
      <head>
        <meta charset="UTF-8">
        <meta name="viewport" content="width=device-width, initial-scale=1.0">
        <title>CodeShare Error</title>
      </head>
      <body>
        <p>${error}</p>
      </body>
      </html>
    `;
    }
    static async showCodeShareMessage(shareID) {
        // get share url
        const shareURL = CodeShareAPI_1.CodeShareAPI.getShareURL(shareID);
        // display created message
        const result = await vscode.window.showInformationMessage("Code share created", 'Copy URL', 'Open in Browser');
        // check result -> action
        switch (result?.toLowerCase()) {
            case "copy url": {
                vscode.env.clipboard.writeText(shareURL);
                break;
            }
            case "open in browser": {
                vscode.env.openExternal(vscode.Uri.parse(shareURL));
                break;
            }
        }
    }
}
exports.UIUtils = UIUtils;
//# sourceMappingURL=UIUtils.js.map