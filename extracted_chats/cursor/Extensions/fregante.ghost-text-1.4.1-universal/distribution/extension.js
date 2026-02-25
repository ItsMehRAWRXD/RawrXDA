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
var __importDefault = (this && this.__importDefault) || function (mod) {
    return (mod && mod.__esModule) ? mod : { "default": mod };
};
Object.defineProperty(exports, "__esModule", { value: true });
exports.activate = void 0;
const node_util_1 = require("node:util");
const node_os_1 = require("node:os");
const node_child_process_1 = require("node:child_process");
const node_process_1 = __importDefault(require("node:process"));
const vscode = __importStar(require("vscode"));
const filenamify_1 = __importDefault(require("filenamify"));
const codelens = __importStar(require("./codelens.js"));
const state_js_1 = require("./state.js");
const server_js_1 = require("./server.js");
const vscode_js_1 = require("./vscode.js");
/** When the browser sends new content, the editor should not detect this "change" event and echo it */
let updateFromBrowserInProgress = false;
const exec = (0, node_util_1.promisify)(node_child_process_1.execFile);
let context;
const osxFocus = `
	tell application "Visual Studio Code"
		activate
	end tell`;
function bringEditorToFront() {
    if (node_process_1.default.platform === 'darwin') {
        void exec('osascript', ['-e', osxFocus]);
    }
}
async function initView(title, socket) {
    const t = new Date();
    // This string is visible if multiple tabs are open from the same page
    const avoidsOverlappingFiles = `${t.getHours()}-${t.getMinutes()}-${t.getSeconds()}`;
    const filename = `${(0, filenamify_1.default)(title.trim(), { replacement: '-' })}.${getFileExtension()}`;
    const file = vscode.Uri.from({
        scheme: 'untitled',
        path: `${(0, node_os_1.tmpdir)()}/${avoidsOverlappingFiles}/${filename}`,
    });
    const document = await vscode.workspace.openTextDocument(file);
    const editor = await vscode.window.showTextDocument(document, {
        viewColumn: vscode.ViewColumn.Active,
        preview: false,
    });
    bringEditorToFront();
    const uriString = file.toString();
    state_js_1.documents.set(uriString, {
        uri: uriString,
        document,
        editor,
        socket,
    });
    state_js_1.documents.onRemove((removedUriString) => {
        if (uriString === removedUriString) {
            socket.close();
        }
    });
    return { document, editor };
}
function openConnection(socket, request) {
    // Only the background page can connect to this server
    try {
        if (!new URL(request.headers.origin).protocol.endsWith('extension:')) {
            socket.close();
            return;
        }
    }
    catch {
        socket.close();
        return;
    }
    let tab;
    socket.on('close', async () => {
        const { document } = await tab;
        state_js_1.documents.delete(document.uri.toString());
    });
    // Listen for incoming messages on the WebSocket
    // Don't `await` anything before this or else it might come too late
    socket.on('message', async (rawMessage) => {
        const { text, selections, title } = JSON.parse(String(rawMessage));
        tab ?? (tab = initView(title, socket));
        const { document, editor } = await tab;
        // When a message is received, replace the document content with the message
        const edit = new vscode.WorkspaceEdit();
        edit.replace(document.uri, new vscode.Range(0, 0, document.lineCount, 0), text);
        updateFromBrowserInProgress = true;
        await vscode.workspace.applyEdit(edit);
        updateFromBrowserInProgress = false;
        editor.selections = selections.map((selection) => new vscode.Selection(document.positionAt(selection.start), document.positionAt(selection.end)));
    });
}
function getFileExtension() {
    // Use || to set the default or else an empty field will override it
    // eslint-disable-next-line @typescript-eslint/prefer-nullish-coalescing
    return vscode.workspace.getConfiguration('ghostText').get('fileExtension') || 'ghosttext';
}
function mapEditorSelections(document, selections) {
    return selections.map((selection) => ({
        start: document.offsetAt(selection.start),
        end: document.offsetAt(selection.end),
    }));
}
function onDisconnectCommand(uriString = vscode.window.activeTextEditor?.document.uri.toString()) {
    if (uriString) {
        state_js_1.documents.delete(uriString);
    }
}
async function onDocumentClose(closedDocument) {
    // https://github.com/fregante/GhostText-for-VSCode/issues/2
    if (closedDocument.isClosed) {
        state_js_1.documents.delete(closedDocument.uri.toString());
    }
}
async function onLocalSelection(event) {
    const document = event.textEditor.document;
    const field = state_js_1.documents.get(document.uri.toString());
    if (!field) {
        return;
    }
    const content = document.getText();
    const selections = mapEditorSelections(document, field.editor.selections);
    field.socket.send(JSON.stringify({ text: content, selections }));
}
async function onConfigurationChange(event) {
    if (event.affectsConfiguration('ghostText.serverPort')) {
        await (0, server_js_1.startServer)(context.subscriptions, openConnection);
    }
}
async function onLocalEdit(event) {
    if (updateFromBrowserInProgress || event.contentChanges.length === 0) {
        return;
    }
    const document = event.document;
    const field = state_js_1.documents.get(document.uri.toString());
    if (!field) {
        return;
    }
    const content = document.getText();
    const selections = mapEditorSelections(document, field.editor.selections);
    field.socket.send(JSON.stringify({ text: content, selections }));
}
function registerListeners(subscriptions) {
    const setup = [null, subscriptions];
    codelens.activate(subscriptions);
    // Watch for changes to the HTTP port option
    // This event is already debounced
    vscode.workspace.onDidChangeConfiguration(onConfigurationChange, ...setup);
    vscode.workspace.onDidCloseTextDocument(onDocumentClose, ...setup);
    vscode.window.onDidChangeTextEditorSelection(onLocalSelection, ...setup);
    vscode.workspace.onDidChangeTextDocument(onLocalEdit, ...setup);
    (0, vscode_js_1.registerCommand)('ghostText.disconnect', onDisconnectCommand, subscriptions);
    (0, vscode_js_1.registerCommand)('ghostText.stopServer', server_js_1.stopServer, subscriptions);
    (0, vscode_js_1.registerCommand)('ghostText.startServer', async () => {
        await (0, server_js_1.startServer)(subscriptions, openConnection);
    }, subscriptions);
}
async function activate(_context) {
    // Set global
    context = _context;
    const { subscriptions } = context;
    // Listen to commands before starting the server
    registerListeners(subscriptions);
    try {
        await (0, server_js_1.startServer)(subscriptions, openConnection);
    }
    catch (error) {
        if (error instanceof server_js_1.Eaddrinuse) {
            return;
        }
        throw error;
    }
    subscriptions.push({
        dispose() {
            state_js_1.documents.clear();
        },
    });
}
exports.activate = activate;
//# sourceMappingURL=extension.js.map