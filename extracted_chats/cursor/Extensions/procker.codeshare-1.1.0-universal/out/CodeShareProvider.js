"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.ShareItem = exports.CodeShareProvider = void 0;
const vscode = require("vscode");
const moment = require("moment");
const CodeShareAPI_1 = require("./CodeShareAPI");
class CodeShareProvider {
    constructor() {
        this._onDidChangeTreeData = new vscode.EventEmitter();
        this.onDidChangeTreeData = this._onDidChangeTreeData.event;
    }
    refresh() {
        this._onDidChangeTreeData.fire();
    }
    getTreeItem(element) {
        return element;
    }
    getChildren(element) {
        const items = [];
        const shareHistory = CodeShareAPI_1.CodeShareAPI.getShareHistory();
        for (let i = shareHistory.length - 1; i >= 0; i--) {
            const jsonShare = shareHistory[i];
            let icon = "";
            if (jsonShare.type.toLowerCase() === "normal") {
                icon = "📄";
            }
            else if (jsonShare.type.toLowerCase() === "password") {
                icon = "📑";
            }
            items.push(new ShareItem(jsonShare.shareID, jsonShare.date, icon, vscode.TreeItemCollapsibleState.None));
        }
        return Promise.resolve(items);
    }
}
exports.CodeShareProvider = CodeShareProvider;
class ShareItem extends vscode.TreeItem {
    constructor(shareID, date, icon, collapsibleState, command) {
        super(icon + " " + shareID, collapsibleState);
        this.shareID = shareID;
        this.date = date;
        this.icon = icon;
        this.collapsibleState = collapsibleState;
        this.command = command;
        this.formatedDate = moment(date).format('MMMM Do YYYY, hh:mm:ss a');
        this.tooltip = `${this.shareID} - ${this.formatedDate}`;
        this.description = this.formatedDate;
    }
}
exports.ShareItem = ShareItem;
//# sourceMappingURL=CodeShareProvider.js.map