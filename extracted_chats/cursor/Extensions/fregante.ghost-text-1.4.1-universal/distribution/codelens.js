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
exports.activate = void 0;
const vscode = __importStar(require("vscode"));
const state_js_1 = require("./state.js");
class GhostTextCodeLensProvider {
    constructor() {
        Object.defineProperty(this, "_onDidChangeCodeLenses", {
            enumerable: true,
            configurable: true,
            writable: true,
            value: new vscode.EventEmitter()
        });
        Object.defineProperty(this, "onDidChangeCodeLenses", {
            enumerable: true,
            configurable: true,
            writable: true,
            value: this._onDidChangeCodeLenses.event
        });
    }
    provideCodeLenses(document) {
        if (!state_js_1.documents.has(document.uri.toString())) {
            return [];
        }
        const range = new vscode.Range(0, 0, 0, 0);
        const command = {
            title: '👻 🌕 GhostText connected | Disconnect',
            command: 'ghostText.disconnect',
            arguments: [document.uri.toString()],
        };
        return [new vscode.CodeLens(range, command)];
    }
}
function activate(subscriptions) {
    const codeLensProvider = new GhostTextCodeLensProvider();
    const codeLensDisposable = vscode.languages.registerCodeLensProvider({ pattern: '**/*' }, codeLensProvider);
    subscriptions.push(codeLensDisposable);
    state_js_1.documents.onRemove(() => {
        codeLensProvider._onDidChangeCodeLenses.fire();
    }, null, subscriptions);
}
exports.activate = activate;
//# sourceMappingURL=codelens.js.map