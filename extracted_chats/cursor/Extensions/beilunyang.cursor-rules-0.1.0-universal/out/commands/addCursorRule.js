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
var __awaiter = (this && this.__awaiter) || function (thisArg, _arguments, P, generator) {
    function adopt(value) { return value instanceof P ? value : new P(function (resolve) { resolve(value); }); }
    return new (P || (P = Promise))(function (resolve, reject) {
        function fulfilled(value) { try { step(generator.next(value)); } catch (e) { reject(e); } }
        function rejected(value) { try { step(generator["throw"](value)); } catch (e) { reject(e); } }
        function step(result) { result.done ? resolve(result.value) : adopt(result.value).then(fulfilled, rejected); }
        step((generator = generator.apply(thisArg, _arguments || [])).next());
    });
};
Object.defineProperty(exports, "__esModule", { value: true });
exports.addCursorRuleCommand = void 0;
const vscode = __importStar(require("vscode"));
const githubApi_1 = require("../utils/githubApi");
const path = __importStar(require("path"));
function addCursorRuleCommand() {
    return __awaiter(this, void 0, void 0, function* () {
        try {
            const quickPick = vscode.window.createQuickPick();
            quickPick.placeholder = 'Loading...';
            quickPick.show();
            let rules = [];
            try {
                rules = yield (0, githubApi_1.fetchCursorRulesList)();
            }
            catch (error) {
                vscode.window.showErrorMessage('Error loading rules list.');
                quickPick.hide();
                return;
            }
            const ruleNames = rules.map(rule => rule.name);
            quickPick.items = ruleNames.map(name => ({ label: name }));
            quickPick.placeholder = 'Select a rule file';
            const selected = yield new Promise(resolve => {
                quickPick.onDidAccept(() => {
                    var _a;
                    const selection = (_a = quickPick.selectedItems[0]) === null || _a === void 0 ? void 0 : _a.label;
                    resolve(selection);
                    quickPick.hide();
                });
                quickPick.onDidHide(() => {
                    resolve(undefined);
                });
            });
            if (!selected) {
                vscode.window.showInformationMessage('No rules selected.');
                return;
            }
            const workspaceFolders = vscode.workspace.workspaceFolders;
            if (!workspaceFolders) {
                vscode.window.showErrorMessage('Please open a workspace first.');
                return;
            }
            const workspacePath = workspaceFolders[0].uri.fsPath;
            const filePath = path.join(workspacePath, '.cursorrules');
            yield vscode.window.withProgress({
                location: vscode.ProgressLocation.Notification,
                title: `Downloading ${selected}...`,
                cancellable: false
            }, (progress) => __awaiter(this, void 0, void 0, function* () {
                yield (0, githubApi_1.fetchCursorRuleContent)(selected, filePath, (percent) => {
                    progress.report({ increment: percent });
                });
            }));
            vscode.window.showInformationMessage(`.cursorrules file added to ${workspacePath}`);
        }
        catch (error) {
            vscode.window.showErrorMessage(`Error adding rule file: ${error}`);
        }
    });
}
exports.addCursorRuleCommand = addCursorRuleCommand;
//# sourceMappingURL=addCursorRule.js.map