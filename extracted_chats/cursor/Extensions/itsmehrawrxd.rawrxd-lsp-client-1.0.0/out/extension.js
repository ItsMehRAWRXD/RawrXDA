"use strict";
// ============================================================================
// extension.ts — RawrXD LSP Client Extension
// ============================================================================
// Connects VS Code to the RawrXD LSP server (RawrXD_IDE_unified.exe --lsp)
// and exposes all 9 custom rawrxd/* JSON-RPC methods as IDE commands.
//
// Custom Methods:
//   rawrxd/hotpatch/list         — enumerate active patches across all layers
//   rawrxd/hotpatch/apply        — apply a patch via UnifiedHotpatchManager
//   rawrxd/hotpatch/revert       — revert a previously applied patch
//   rawrxd/hotpatch/diagnostics  — get hotpatch conflict/validation diagnostics
//   rawrxd/gguf/modelInfo        — query GGUF model metadata
//   rawrxd/gguf/tensorList       — enumerate tensors in a loaded model
//   rawrxd/gguf/validate         — run structural validation on a GGUF file
//   rawrxd/workspace/symbols     — get hotpatch-aware symbol table
//   rawrxd/workspace/stats       — get hotpatch manager statistics
//
// Notifications (server → client):
//   rawrxd/hotpatch/event        — hotpatch applied/reverted/failed
//   rawrxd/gguf/loadProgress     — model loading progress
//   rawrxd/diagnostics/refresh   — diagnostic set changed
//
// Copyright (c) 2025-2026 RawrXD Project — All rights reserved.
// ============================================================================
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
const path = __importStar(require("path"));
const node_1 = require("vscode-languageclient/node");
// ---------------------------------------------------------------------------
// Protocol Constants — mirrors lsp_bridge_protocol.hpp
// ---------------------------------------------------------------------------
const METHOD = {
    HOTPATCH_LIST: 'rawrxd/hotpatch/list',
    HOTPATCH_APPLY: 'rawrxd/hotpatch/apply',
    HOTPATCH_REVERT: 'rawrxd/hotpatch/revert',
    HOTPATCH_DIAGNOSTICS: 'rawrxd/hotpatch/diagnostics',
    GGUF_MODEL_INFO: 'rawrxd/gguf/modelInfo',
    GGUF_TENSOR_LIST: 'rawrxd/gguf/tensorList',
    GGUF_VALIDATE: 'rawrxd/gguf/validate',
    WORKSPACE_SYMBOLS: 'rawrxd/workspace/symbols',
    WORKSPACE_STATS: 'rawrxd/workspace/stats',
};
const NOTIFY = {
    HOTPATCH_EVENT: 'rawrxd/hotpatch/event',
    GGUF_LOAD_PROGRESS: 'rawrxd/gguf/loadProgress',
    DIAGNOSTIC_REFRESH: 'rawrxd/diagnostics/refresh',
};
// ---------------------------------------------------------------------------
// Type Definitions — mirrors C++ structs from lsp_bridge_protocol.hpp
// ---------------------------------------------------------------------------
/** HotpatchLayer enum */
var HotpatchLayer;
(function (HotpatchLayer) {
    HotpatchLayer[HotpatchLayer["Memory"] = 0] = "Memory";
    HotpatchLayer[HotpatchLayer["Byte"] = 1] = "Byte";
    HotpatchLayer[HotpatchLayer["Server"] = 2] = "Server";
    HotpatchLayer[HotpatchLayer["All"] = 255] = "All";
})(HotpatchLayer || (HotpatchLayer = {}));
/** HotpatchSymbolKind enum */
var HotpatchSymbolKind;
(function (HotpatchSymbolKind) {
    HotpatchSymbolKind[HotpatchSymbolKind["MemoryPatch"] = 0] = "MemoryPatch";
    HotpatchSymbolKind[HotpatchSymbolKind["BytePatch"] = 1] = "BytePatch";
    HotpatchSymbolKind[HotpatchSymbolKind["ServerPatch"] = 2] = "ServerPatch";
    HotpatchSymbolKind[HotpatchSymbolKind["GGUFTensor"] = 3] = "GGUFTensor";
    HotpatchSymbolKind[HotpatchSymbolKind["GGUFMetadata"] = 4] = "GGUFMetadata";
    HotpatchSymbolKind[HotpatchSymbolKind["PatchPreset"] = 5] = "PatchPreset";
    HotpatchSymbolKind[HotpatchSymbolKind["InjectionPoint"] = 6] = "InjectionPoint";
    HotpatchSymbolKind[HotpatchSymbolKind["ProxyRewrite"] = 7] = "ProxyRewrite";
})(HotpatchSymbolKind || (HotpatchSymbolKind = {}));
/** HotpatchDiagSeverity — mirrors LSP DiagnosticSeverity */
var HotpatchDiagSeverity;
(function (HotpatchDiagSeverity) {
    HotpatchDiagSeverity[HotpatchDiagSeverity["Error"] = 1] = "Error";
    HotpatchDiagSeverity[HotpatchDiagSeverity["Warning"] = 2] = "Warning";
    HotpatchDiagSeverity[HotpatchDiagSeverity["Information"] = 3] = "Information";
    HotpatchDiagSeverity[HotpatchDiagSeverity["Hint"] = 4] = "Hint";
})(HotpatchDiagSeverity || (HotpatchDiagSeverity = {}));
// ---------------------------------------------------------------------------
// Module-level state
// ---------------------------------------------------------------------------
let client;
let outputChannel;
let diagnosticCollection;
let statusBarItem;
let hotpatchViewProvider;
let ggufViewProvider;
let statsViewProvider;
// ---------------------------------------------------------------------------
// Extension Activation
// ---------------------------------------------------------------------------
async function activate(context) {
    outputChannel = vscode.window.createOutputChannel('RawrXD LSP', { log: true });
    diagnosticCollection = vscode.languages.createDiagnosticCollection('rawrxd');
    statusBarItem = vscode.window.createStatusBarItem(vscode.StatusBarAlignment.Right, 50);
    statusBarItem.text = '$(beaker) RawrXD';
    statusBarItem.tooltip = 'RawrXD LSP Server';
    statusBarItem.command = 'rawrxd.workspace.stats';
    context.subscriptions.push(outputChannel, diagnosticCollection, statusBarItem);
    // ---- Tree View Providers ----
    hotpatchViewProvider = new HotpatchTreeProvider();
    ggufViewProvider = new GGUFTreeProvider();
    statsViewProvider = new StatsTreeProvider();
    context.subscriptions.push(vscode.window.registerTreeDataProvider('rawrxd.hotpatchView', hotpatchViewProvider), vscode.window.registerTreeDataProvider('rawrxd.ggufView', ggufViewProvider), vscode.window.registerTreeDataProvider('rawrxd.statsView', statsViewProvider));
    // ---- Register Commands ----
    context.subscriptions.push(vscode.commands.registerCommand('rawrxd.hotpatch.list', cmdHotpatchList), vscode.commands.registerCommand('rawrxd.hotpatch.apply', cmdHotpatchApply), vscode.commands.registerCommand('rawrxd.hotpatch.revert', cmdHotpatchRevert), vscode.commands.registerCommand('rawrxd.hotpatch.diagnostics', cmdHotpatchDiagnostics), vscode.commands.registerCommand('rawrxd.gguf.modelInfo', cmdGGUFModelInfo), vscode.commands.registerCommand('rawrxd.gguf.tensorList', cmdGGUFTensorList), vscode.commands.registerCommand('rawrxd.gguf.validate', cmdGGUFValidate), vscode.commands.registerCommand('rawrxd.workspace.symbols', cmdWorkspaceSymbols), vscode.commands.registerCommand('rawrxd.workspace.stats', cmdWorkspaceStats), vscode.commands.registerCommand('rawrxd.hotpatch.refreshView', () => hotpatchViewProvider.refresh()), vscode.commands.registerCommand('rawrxd.gguf.refreshView', () => ggufViewProvider.refresh()));
    // ---- Start Language Client ----
    await startClient(context);
    statusBarItem.show();
    outputChannel.appendLine('[RawrXD] Extension activated');
}
// ---------------------------------------------------------------------------
// Extension Deactivation
// ---------------------------------------------------------------------------
async function deactivate() {
    if (client) {
        await client.stop();
        client = undefined;
    }
}
// ---------------------------------------------------------------------------
// Language Client Lifecycle
// ---------------------------------------------------------------------------
async function startClient(context) {
    const config = vscode.workspace.getConfiguration('rawrxd');
    const serverPath = resolveServerPath(config.get('server.path', ''));
    const serverArgs = config.get('server.args', ['--lsp']);
    if (!serverPath) {
        const msg = 'RawrXD LSP server binary not found. Set rawrxd.server.path in settings.';
        outputChannel.appendLine(`[RawrXD] ERROR: ${msg}`);
        vscode.window.showWarningMessage(msg);
        updateStatusBar('error');
        return;
    }
    outputChannel.appendLine(`[RawrXD] Server: ${serverPath}`);
    outputChannel.appendLine(`[RawrXD] Args: ${serverArgs.join(' ')}`);
    const serverOptions = {
        run: { command: serverPath, args: serverArgs, transport: node_1.TransportKind.stdio },
        debug: { command: serverPath, args: [...serverArgs, '--debug'], transport: node_1.TransportKind.stdio },
    };
    const clientOptions = {
        documentSelector: [
            { scheme: 'file', language: 'asm' },
            { scheme: 'file', language: 'masm' },
            { scheme: 'file', pattern: '**/*.gguf' },
            { scheme: 'file', pattern: '**/*.asm' },
            { scheme: 'file', pattern: '**/*.cpp' },
            { scheme: 'file', pattern: '**/*.hpp' },
        ],
        outputChannel,
        traceOutputChannel: outputChannel,
        initializationOptions: {
            hotpatchEnabled: true,
            ggufEnabled: true,
            asmAcceleratorsEnabled: true,
        },
    };
    client = new node_1.LanguageClient('rawrxd-lsp', 'RawrXD LSP Server', serverOptions, clientOptions);
    // ---- State Change Handler ----
    client.onDidChangeState((event) => {
        switch (event.newState) {
            case node_1.State.Running:
                updateStatusBar('running');
                outputChannel.appendLine('[RawrXD] Server is running');
                break;
            case node_1.State.Stopped:
                updateStatusBar('stopped');
                outputChannel.appendLine('[RawrXD] Server stopped');
                break;
            case node_1.State.Starting:
                updateStatusBar('starting');
                break;
        }
    });
    // ---- Start and wait for ready ----
    await client.start();
    // ---- Register Notification Handlers ----
    registerNotificationHandlers(client);
    // ---- Initial data fetch ----
    hotpatchViewProvider.refresh();
    ggufViewProvider.refresh();
    statsViewProvider.refresh();
}
// ---------------------------------------------------------------------------
// Server Path Resolution
// ---------------------------------------------------------------------------
function resolveServerPath(configPath) {
    if (configPath && configPath.length > 0) {
        return configPath;
    }
    // Check workspace-local paths
    const workspaceFolders = vscode.workspace.workspaceFolders;
    if (workspaceFolders) {
        for (const folder of workspaceFolders) {
            const candidates = [
                path.join(folder.uri.fsPath, 'RawrXD_IDE_unified.exe'),
                path.join(folder.uri.fsPath, 'build', 'RawrXD_IDE_unified.exe'),
                path.join(folder.uri.fsPath, 'bin', 'RawrXD_IDE_unified.exe'),
                path.join(folder.uri.fsPath, 'RawrXD.exe'),
                path.join(folder.uri.fsPath, 'build', 'Release', 'RawrXD.exe'),
            ];
            for (const candidate of candidates) {
                // LanguageClient will validate existence; return first likely path
                try {
                    const fs = require('fs');
                    if (fs.existsSync(candidate)) {
                        return candidate;
                    }
                }
                catch {
                    // continue
                }
            }
        }
    }
    // Fall back to PATH
    return 'RawrXD_IDE_unified.exe';
}
// ---------------------------------------------------------------------------
// Status Bar
// ---------------------------------------------------------------------------
function updateStatusBar(state) {
    switch (state) {
        case 'running':
            statusBarItem.text = '$(beaker) RawrXD';
            statusBarItem.backgroundColor = undefined;
            statusBarItem.tooltip = 'RawrXD LSP Server — Running';
            break;
        case 'stopped':
            statusBarItem.text = '$(beaker) RawrXD (Stopped)';
            statusBarItem.backgroundColor = new vscode.ThemeColor('statusBarItem.warningBackground');
            statusBarItem.tooltip = 'RawrXD LSP Server — Stopped';
            break;
        case 'error':
            statusBarItem.text = '$(error) RawrXD';
            statusBarItem.backgroundColor = new vscode.ThemeColor('statusBarItem.errorBackground');
            statusBarItem.tooltip = 'RawrXD LSP Server — Error';
            break;
        case 'starting':
            statusBarItem.text = '$(loading~spin) RawrXD';
            statusBarItem.tooltip = 'RawrXD LSP Server — Starting...';
            break;
    }
}
// ---------------------------------------------------------------------------
// Notification Handlers (server → client)
// ---------------------------------------------------------------------------
function registerNotificationHandlers(lc) {
    // rawrxd/hotpatch/event — hotpatch state changes
    lc.onNotification(NOTIFY.HOTPATCH_EVENT, (params) => {
        outputChannel.appendLine(`[HotpatchEvent] ${params.type}: ${params.patchName} (layer=${layerName(params.layer)}) — ${params.detail}`);
        switch (params.type) {
            case 'applied':
                vscode.window.showInformationMessage(`Hotpatch applied: ${params.patchName} (${layerName(params.layer)})`);
                break;
            case 'reverted':
                vscode.window.showInformationMessage(`Hotpatch reverted: ${params.patchName}`);
                break;
            case 'failed':
                vscode.window.showErrorMessage(`Hotpatch failed: ${params.patchName} — ${params.detail}`);
                break;
            case 'conflicted':
                vscode.window.showWarningMessage(`Hotpatch conflict: ${params.patchName} — ${params.detail}`);
                break;
        }
        // Refresh views on any state change
        hotpatchViewProvider.refresh();
        statsViewProvider.refresh();
        // Auto-refresh diagnostics if configured
        const autoRefresh = vscode.workspace.getConfiguration('rawrxd').get('diagnostics.autoRefresh', true);
        if (autoRefresh) {
            void cmdHotpatchDiagnostics();
        }
    });
    // rawrxd/gguf/loadProgress — model loading progress
    lc.onNotification(NOTIFY.GGUF_LOAD_PROGRESS, (params) => {
        outputChannel.appendLine(`[GGUF] ${params.phase}: ${params.percent}% — ${params.detail} (${path.basename(params.filePath)})`);
        statusBarItem.text = `$(loading~spin) GGUF ${params.percent}%`;
        if (params.percent >= 100) {
            updateStatusBar('running');
            ggufViewProvider.refresh();
        }
    });
    // rawrxd/diagnostics/refresh — server pushed new diagnostics
    lc.onNotification(NOTIFY.DIAGNOSTIC_REFRESH, (_params) => {
        outputChannel.appendLine('[RawrXD] Diagnostic refresh requested by server');
        void cmdHotpatchDiagnostics();
    });
}
// ===========================================================================
// COMMAND IMPLEMENTATIONS — 9 JSON-RPC Method Wrappers
// ===========================================================================
// ---------------------------------------------------------------------------
// rawrxd/hotpatch/list
// ---------------------------------------------------------------------------
async function cmdHotpatchList() {
    if (!ensureClient()) {
        return;
    }
    try {
        const patches = await client.sendRequest(METHOD.HOTPATCH_LIST, {
            layer: HotpatchLayer.All,
        });
        hotpatchViewProvider.setPatches(patches);
        outputChannel.appendLine(`[HotpatchList] ${patches.length} patches found`);
        if (patches.length === 0) {
            vscode.window.showInformationMessage('No active hotpatches.');
            return;
        }
        // Quick pick for inspection
        const items = patches.map(p => ({
            label: `${p.active ? '$(check)' : '$(circle-outline)'} ${p.name}`,
            description: `${layerName(p.layer)} — ${kindName(p.kind)}`,
            detail: `${p.detail} | addr=${p.address} size=${p.size}`,
            patch: p,
        }));
        const selected = await vscode.window.showQuickPick(items, {
            title: 'Active Hotpatches',
            placeHolder: 'Select a hotpatch to view details',
        });
        if (selected) {
            showPatchDetail(selected.patch);
        }
    }
    catch (err) {
        handleRequestError('hotpatch/list', err);
    }
}
// ---------------------------------------------------------------------------
// rawrxd/hotpatch/apply
// ---------------------------------------------------------------------------
async function cmdHotpatchApply() {
    if (!ensureClient()) {
        return;
    }
    // Prompt for layer selection
    const layerPick = await vscode.window.showQuickPick([
        { label: 'Memory', description: 'Direct RAM patching (VirtualProtect)', value: HotpatchLayer.Memory },
        { label: 'Byte', description: 'GGUF binary modification (mmap)', value: HotpatchLayer.Byte },
        { label: 'Server', description: 'Inference request/response injection', value: HotpatchLayer.Server },
    ], { title: 'Select Hotpatch Layer', placeHolder: 'Which layer should the patch target?' });
    if (!layerPick) {
        return;
    }
    // Prompt for patch name
    const name = await vscode.window.showInputBox({
        title: 'Patch Name',
        placeHolder: 'Enter a name for this hotpatch',
        validateInput: (val) => val.trim().length === 0 ? 'Name required' : undefined,
    });
    if (!name) {
        return;
    }
    // Prompt for patch data (hex or file path depending on layer)
    const data = await vscode.window.showInputBox({
        title: 'Patch Data',
        placeHolder: layerPick.value === HotpatchLayer.Server
            ? 'Transform function name or preset path'
            : 'Hex bytes (e.g., 90 90 90) or file offset:data',
        prompt: `Enter patch data for ${layerPick.label} layer`,
    });
    if (data === undefined) {
        return;
    }
    try {
        const result = await client.sendRequest(METHOD.HOTPATCH_APPLY, {
            layer: layerPick.value,
            name: name.trim(),
            data: data.trim(),
        });
        if (result.success) {
            vscode.window.showInformationMessage(`Hotpatch applied: ${name} — ${result.detail}`);
        }
        else {
            vscode.window.showErrorMessage(`Hotpatch failed: ${result.detail}`);
        }
        hotpatchViewProvider.refresh();
        statsViewProvider.refresh();
    }
    catch (err) {
        handleRequestError('hotpatch/apply', err);
    }
}
// ---------------------------------------------------------------------------
// rawrxd/hotpatch/revert
// ---------------------------------------------------------------------------
async function cmdHotpatchRevert(item) {
    if (!ensureClient()) {
        return;
    }
    let patchName;
    if (item?.patch) {
        patchName = item.patch.name;
    }
    else {
        // Fetch list and let user pick
        try {
            const patches = await client.sendRequest(METHOD.HOTPATCH_LIST, {
                layer: HotpatchLayer.All,
            });
            const active = patches.filter(p => p.active);
            if (active.length === 0) {
                vscode.window.showInformationMessage('No active hotpatches to revert.');
                return;
            }
            const pick = await vscode.window.showQuickPick(active.map(p => ({
                label: p.name,
                description: `${layerName(p.layer)} — ${kindName(p.kind)}`,
                detail: p.detail,
                patch: p,
            })), { title: 'Select Hotpatch to Revert', placeHolder: 'Choose a patch' });
            if (!pick) {
                return;
            }
            patchName = pick.patch.name;
        }
        catch (err) {
            handleRequestError('hotpatch/list (for revert)', err);
            return;
        }
    }
    if (!patchName) {
        return;
    }
    try {
        const result = await client.sendRequest(METHOD.HOTPATCH_REVERT, { name: patchName });
        if (result.success) {
            vscode.window.showInformationMessage(`Hotpatch reverted: ${patchName}`);
        }
        else {
            vscode.window.showErrorMessage(`Revert failed: ${result.detail}`);
        }
        hotpatchViewProvider.refresh();
        statsViewProvider.refresh();
    }
    catch (err) {
        handleRequestError('hotpatch/revert', err);
    }
}
// ---------------------------------------------------------------------------
// rawrxd/hotpatch/diagnostics
// ---------------------------------------------------------------------------
async function cmdHotpatchDiagnostics() {
    if (!ensureClient()) {
        return;
    }
    try {
        const diags = await client.sendRequest(METHOD.HOTPATCH_DIAGNOSTICS, { layer: HotpatchLayer.All });
        outputChannel.appendLine(`[Diagnostics] ${diags.length} diagnostics received`);
        // Group diagnostics by file path (source)
        const diagMap = new Map();
        for (const d of diags) {
            const vscodeSeverity = mapSeverity(d.severity);
            const range = new vscode.Range(0, 0, 0, 0); // Hotpatch diagnostics are address-based, not line-based
            const diagnostic = new vscode.Diagnostic(range, d.message, vscodeSeverity);
            diagnostic.code = d.code;
            diagnostic.source = d.source;
            // Add related patch info as related information
            if (d.relatedPatch) {
                diagnostic.relatedInformation = [
                    new vscode.DiagnosticRelatedInformation(new vscode.Location(vscode.Uri.file('hotpatch'), range), `Patch: ${d.relatedPatch} | Layer: ${layerName(d.layer)} | Addr: ${d.address}`),
                ];
            }
            // Use source as the grouping key (rawrxd-memory, rawrxd-byte, rawrxd-server)
            const key = d.source || 'rawrxd';
            if (!diagMap.has(key)) {
                diagMap.set(key, []);
            }
            diagMap.get(key).push(diagnostic);
        }
        // Push to VS Code diagnostic collection
        diagnosticCollection.clear();
        for (const [source, sourceDiags] of diagMap) {
            // Create a virtual URI for each diagnostic source
            const uri = vscode.Uri.parse(`rawrxd-diag://${source}`);
            diagnosticCollection.set(uri, sourceDiags);
        }
        // Also show inline if configured
        const showInline = vscode.workspace.getConfiguration('rawrxd').get('diagnostics.showInline', true);
        if (showInline && diags.length > 0) {
            const errors = diags.filter(d => d.severity === HotpatchDiagSeverity.Error).length;
            const warnings = diags.filter(d => d.severity === HotpatchDiagSeverity.Warning).length;
            const infoCount = diags.length - errors - warnings;
            vscode.window.showInformationMessage(`RawrXD Diagnostics: ${errors} errors, ${warnings} warnings, ${infoCount} info`);
        }
    }
    catch (err) {
        handleRequestError('hotpatch/diagnostics', err);
    }
}
// ---------------------------------------------------------------------------
// rawrxd/gguf/modelInfo
// ---------------------------------------------------------------------------
async function cmdGGUFModelInfo() {
    if (!ensureClient()) {
        return;
    }
    // Let user pick a GGUF file
    const filePath = await pickGGUFFile();
    if (!filePath) {
        return;
    }
    try {
        const info = await client.sendRequest(METHOD.GGUF_MODEL_INFO, { filePath });
        outputChannel.appendLine(`[GGUF] Model info for: ${info.filePath}`);
        // Show in a formatted webview panel
        const panel = vscode.window.createWebviewPanel('rawrxd.ggufModelInfo', `GGUF: ${path.basename(info.filePath)}`, vscode.ViewColumn.Beside, { enableScripts: false });
        panel.webview.html = renderModelInfoHTML(info);
        ggufViewProvider.setModelInfo(info);
    }
    catch (err) {
        handleRequestError('gguf/modelInfo', err);
    }
}
// ---------------------------------------------------------------------------
// rawrxd/gguf/tensorList
// ---------------------------------------------------------------------------
async function cmdGGUFTensorList() {
    if (!ensureClient()) {
        return;
    }
    const filePath = await pickGGUFFile();
    if (!filePath) {
        return;
    }
    try {
        const tensors = await client.sendRequest(METHOD.GGUF_TENSOR_LIST, { filePath });
        outputChannel.appendLine(`[GGUF] ${tensors.length} tensors in ${path.basename(filePath)}`);
        // Show in quick pick with search
        const items = tensors.map(t => ({
            label: t.name,
            description: `${t.typeName} — ${formatBytes(t.size)}`,
            detail: `dims=[${t.dims.slice(0, t.nDims).join('×')}] offset=${t.offset}`,
            tensor: t,
        }));
        const selected = await vscode.window.showQuickPick(items, {
            title: `Tensors in ${path.basename(filePath)}`,
            placeHolder: `${tensors.length} tensors — type to filter`,
            matchOnDescription: true,
            matchOnDetail: true,
        });
        if (selected) {
            // Copy tensor info to clipboard
            const info = JSON.stringify(selected.tensor, null, 2);
            await vscode.env.clipboard.writeText(info);
            vscode.window.showInformationMessage(`Tensor info copied: ${selected.tensor.name}`);
        }
    }
    catch (err) {
        handleRequestError('gguf/tensorList', err);
    }
}
// ---------------------------------------------------------------------------
// rawrxd/gguf/validate
// ---------------------------------------------------------------------------
async function cmdGGUFValidate() {
    if (!ensureClient()) {
        return;
    }
    const filePath = await pickGGUFFile();
    if (!filePath) {
        return;
    }
    try {
        const result = await client.sendRequest(METHOD.GGUF_VALIDATE, { filePath });
        outputChannel.appendLine(`[GGUF] Validation: ${result.valid ? 'PASS' : 'FAIL'} — ${filePath}`);
        if (result.valid) {
            vscode.window.showInformationMessage(`✅ GGUF Valid: ${path.basename(filePath)} — magic=${result.magicOk ? 'OK' : 'BAD'}, ` +
                `version=${result.versionOk ? 'OK' : 'BAD'}, tensors=${result.tensorIntegrity ? 'OK' : 'BAD'}`);
        }
        else {
            const errMsg = result.errors.length > 0
                ? result.errors.join('; ')
                : 'Structural validation failed';
            vscode.window.showErrorMessage(`❌ GGUF Invalid: ${path.basename(filePath)} — ${errMsg}`);
            // Push as diagnostics
            const uri = vscode.Uri.file(filePath);
            const diags = [
                ...result.errors.map(e => new vscode.Diagnostic(new vscode.Range(0, 0, 0, 0), e, vscode.DiagnosticSeverity.Error)),
                ...result.warnings.map(w => new vscode.Diagnostic(new vscode.Range(0, 0, 0, 0), w, vscode.DiagnosticSeverity.Warning)),
            ];
            diags.forEach(d => { d.source = 'rawrxd-gguf'; });
            diagnosticCollection.set(uri, diags);
        }
    }
    catch (err) {
        handleRequestError('gguf/validate', err);
    }
}
// ---------------------------------------------------------------------------
// rawrxd/workspace/symbols
// ---------------------------------------------------------------------------
async function cmdWorkspaceSymbols() {
    if (!ensureClient()) {
        return;
    }
    const query = await vscode.window.showInputBox({
        title: 'Symbol Search',
        placeHolder: 'Enter symbol name or prefix (uses ASM-accelerated lookup)',
        prompt: 'Search the hotpatch-aware symbol table',
    });
    try {
        const symbols = await client.sendRequest(METHOD.WORKSPACE_SYMBOLS, { query: query || '' });
        outputChannel.appendLine(`[Symbols] ${symbols.length} matches for "${query || '*'}"`);
        if (symbols.length === 0) {
            vscode.window.showInformationMessage('No symbols found.');
            return;
        }
        const items = symbols.map(s => ({
            label: `$(symbol-${symbolIcon(s.kind)}) ${s.name}`,
            description: `${layerName(s.layer)} — ${kindName(s.kind)}`,
            detail: s.detail,
            symbol: s,
        }));
        const selected = await vscode.window.showQuickPick(items, {
            title: 'Hotpatch Workspace Symbols',
            placeHolder: `${symbols.length} symbols`,
            matchOnDescription: true,
            matchOnDetail: true,
        });
        if (selected && selected.symbol.filePath) {
            const uri = vscode.Uri.file(selected.symbol.filePath);
            const pos = new vscode.Position(Math.max(0, selected.symbol.line - 1 || 0), selected.symbol.startChar || 0);
            await vscode.window.showTextDocument(uri, {
                selection: new vscode.Range(pos, pos),
            });
        }
    }
    catch (err) {
        handleRequestError('workspace/symbols', err);
    }
}
// ---------------------------------------------------------------------------
// rawrxd/workspace/stats
// ---------------------------------------------------------------------------
async function cmdWorkspaceStats() {
    if (!ensureClient()) {
        return;
    }
    try {
        const stats = await client.sendRequest(METHOD.WORKSPACE_STATS, {});
        outputChannel.appendLine('[Stats] Workspace statistics retrieved');
        statsViewProvider.setStats(stats);
        // Show as info message with details
        const lines = [
            `📊 RawrXD Workspace Stats`,
            `────────────────────────`,
            `Requests handled:      ${stats.requestsHandled}`,
            `Notifications sent:    ${stats.notificationsSent}`,
            `Diagnostic refreshes:  ${stats.diagnosticRefreshes}`,
            `Symbol rebuilds:       ${stats.symbolRebuilds}`,
            `Errors:                ${stats.errors}`,
            `────────────────────────`,
            `Memory patches:  ${stats.memoryPatchCount}`,
            `Byte patches:    ${stats.bytePatchCount}`,
            `Server patches:  ${stats.serverPatchCount}`,
            `Total symbols:   ${stats.symbolCount}`,
        ];
        outputChannel.appendLine(lines.join('\n'));
        // Show in output channel and focus it
        outputChannel.show(true);
    }
    catch (err) {
        handleRequestError('workspace/stats', err);
    }
}
// ===========================================================================
// TREE VIEW PROVIDERS
// ===========================================================================
// ---------------------------------------------------------------------------
// Hotpatch Tree Provider
// ---------------------------------------------------------------------------
class HotpatchTreeItem extends vscode.TreeItem {
    patch;
    constructor(patch) {
        super(patch.name, vscode.TreeItemCollapsibleState.None);
        this.patch = patch;
        this.description = `${layerName(patch.layer)} — ${kindName(patch.kind)}`;
        this.tooltip = new vscode.MarkdownString(`**${patch.name}**\n\n` +
            `- Layer: ${layerName(patch.layer)}\n` +
            `- Kind: ${kindName(patch.kind)}\n` +
            `- Address: \`${patch.address}\`\n` +
            `- Size: ${patch.size} bytes\n` +
            `- Active: ${patch.active ? '✅' : '❌'}\n` +
            `- Detail: ${patch.detail}`);
        this.iconPath = patch.active
            ? new vscode.ThemeIcon('check', new vscode.ThemeColor('testing.iconPassed'))
            : new vscode.ThemeIcon('circle-outline');
        this.contextValue = patch.active ? 'activePatch' : 'inactivePatch';
    }
}
class HotpatchTreeProvider {
    _onDidChangeTreeData = new vscode.EventEmitter();
    onDidChangeTreeData = this._onDidChangeTreeData.event;
    patches = [];
    setPatches(patches) {
        this.patches = patches;
        this._onDidChangeTreeData.fire(undefined);
    }
    refresh() {
        if (client) {
            void client.sendRequest(METHOD.HOTPATCH_LIST, { layer: HotpatchLayer.All })
                .then(p => this.setPatches(p))
                .catch(() => { });
        }
    }
    getTreeItem(element) {
        return element;
    }
    getChildren() {
        return this.patches.map(p => new HotpatchTreeItem(p));
    }
}
// ---------------------------------------------------------------------------
// GGUF Tree Provider
// ---------------------------------------------------------------------------
class GGUFTreeItem extends vscode.TreeItem {
    constructor(label, description, tooltip) {
        super(label, vscode.TreeItemCollapsibleState.None);
        this.description = description;
        this.tooltip = tooltip;
    }
}
class GGUFTreeProvider {
    _onDidChangeTreeData = new vscode.EventEmitter();
    onDidChangeTreeData = this._onDidChangeTreeData.event;
    model;
    setModelInfo(info) {
        this.model = info;
        this._onDidChangeTreeData.fire(undefined);
    }
    refresh() {
        this._onDidChangeTreeData.fire(undefined);
    }
    getTreeItem(element) {
        return element;
    }
    getChildren() {
        if (!this.model) {
            return [new GGUFTreeItem('No model loaded', 'Use rawrxd.gguf.modelInfo')];
        }
        const m = this.model;
        return [
            new GGUFTreeItem('Architecture', m.architecture),
            new GGUFTreeItem('Quant Type', m.quantType),
            new GGUFTreeItem('Parameters', formatNumber(m.paramCount)),
            new GGUFTreeItem('Tensors', `${m.tensorCount}`),
            new GGUFTreeItem('Layers', `${m.layerCount}`),
            new GGUFTreeItem('Context Length', `${m.contextLength}`),
            new GGUFTreeItem('Embedding Dim', `${m.embeddingLength}`),
            new GGUFTreeItem('Heads', `${m.headCount}`),
            new GGUFTreeItem('File Size', formatBytes(m.fileSize)),
            new GGUFTreeItem('GGUF Version', `v${m.version}`),
            new GGUFTreeItem('Metadata Keys', `${m.metadataCount}`),
        ];
    }
}
// ---------------------------------------------------------------------------
// Stats Tree Provider
// ---------------------------------------------------------------------------
class StatsTreeProvider {
    _onDidChangeTreeData = new vscode.EventEmitter();
    onDidChangeTreeData = this._onDidChangeTreeData.event;
    stats;
    setStats(stats) {
        this.stats = stats;
        this._onDidChangeTreeData.fire(undefined);
    }
    refresh() {
        if (client) {
            void client.sendRequest(METHOD.WORKSPACE_STATS, {})
                .then(s => this.setStats(s))
                .catch(() => { });
        }
    }
    getTreeItem(element) {
        return element;
    }
    getChildren() {
        if (!this.stats) {
            return [new GGUFTreeItem('Connecting...', 'Waiting for server')];
        }
        const s = this.stats;
        return [
            new GGUFTreeItem('Requests', `${s.requestsHandled}`, 'Total JSON-RPC requests handled'),
            new GGUFTreeItem('Notifications', `${s.notificationsSent}`, 'Server→Client notifications sent'),
            new GGUFTreeItem('Diag Refreshes', `${s.diagnosticRefreshes}`, 'Diagnostic refresh cycles'),
            new GGUFTreeItem('Symbol Rebuilds', `${s.symbolRebuilds}`, 'Symbol index rebuild count'),
            new GGUFTreeItem('Errors', `${s.errors}`, 'Total error count'),
            new GGUFTreeItem('───────', '──────'),
            new GGUFTreeItem('Memory Patches', `${s.memoryPatchCount}`, 'Active RAM patches (VirtualProtect)'),
            new GGUFTreeItem('Byte Patches', `${s.bytePatchCount}`, 'Active GGUF binary patches (mmap)'),
            new GGUFTreeItem('Server Patches', `${s.serverPatchCount}`, 'Active inference transforms'),
            new GGUFTreeItem('Symbols', `${s.symbolCount}`, 'Total symbols in ASM-accelerated index'),
        ];
    }
}
// ===========================================================================
// UTILITY FUNCTIONS
// ===========================================================================
function ensureClient() {
    if (!client || client.state !== node_1.State.Running) {
        vscode.window.showWarningMessage('RawrXD LSP server is not running.');
        return false;
    }
    return true;
}
function handleRequestError(method, err) {
    const msg = err instanceof Error ? err.message : String(err);
    outputChannel.appendLine(`[RawrXD] ERROR in ${method}: ${msg}`);
    vscode.window.showErrorMessage(`RawrXD ${method} failed: ${msg}`);
}
async function pickGGUFFile() {
    // Check if active editor is a GGUF file
    const activeEditor = vscode.window.activeTextEditor;
    if (activeEditor?.document.uri.fsPath.endsWith('.gguf')) {
        return activeEditor.document.uri.fsPath;
    }
    // Search workspace for GGUF files
    const files = await vscode.workspace.findFiles('**/*.gguf', '**/node_modules/**', 20);
    if (files.length === 0) {
        // Let user browse
        const selected = await vscode.window.showOpenDialog({
            canSelectFiles: true,
            canSelectMany: false,
            filters: { 'GGUF Models': ['gguf'] },
            title: 'Select GGUF Model File',
        });
        return selected?.[0]?.fsPath;
    }
    if (files.length === 1) {
        return files[0].fsPath;
    }
    // Multiple files — let user pick
    const items = files.map(f => ({
        label: path.basename(f.fsPath),
        description: vscode.workspace.asRelativePath(f),
        uri: f,
    }));
    const pick = await vscode.window.showQuickPick(items, {
        title: 'Select GGUF Model',
        placeHolder: 'Choose a GGUF file',
    });
    return pick?.uri.fsPath;
}
function showPatchDetail(patch) {
    const detail = [
        `# Hotpatch: ${patch.name}`,
        '',
        `| Property | Value |`,
        `| --- | --- |`,
        `| Layer | ${layerName(patch.layer)} |`,
        `| Kind | ${kindName(patch.kind)} |`,
        `| Active | ${patch.active ? '✅ Yes' : '❌ No'} |`,
        `| Address | \`${patch.address}\` |`,
        `| Size | ${patch.size} bytes |`,
        `| Hash | \`${patch.hash}\` |`,
        `| File | ${patch.filePath || 'N/A'} |`,
        '',
        `## Detail`,
        patch.detail || 'No additional details.',
    ].join('\n');
    outputChannel.appendLine(detail);
    outputChannel.show(true);
}
function mapSeverity(s) {
    switch (s) {
        case HotpatchDiagSeverity.Error: return vscode.DiagnosticSeverity.Error;
        case HotpatchDiagSeverity.Warning: return vscode.DiagnosticSeverity.Warning;
        case HotpatchDiagSeverity.Information: return vscode.DiagnosticSeverity.Information;
        case HotpatchDiagSeverity.Hint: return vscode.DiagnosticSeverity.Hint;
        default: return vscode.DiagnosticSeverity.Information;
    }
}
function layerName(layer) {
    switch (layer) {
        case HotpatchLayer.Memory: return 'Memory';
        case HotpatchLayer.Byte: return 'Byte';
        case HotpatchLayer.Server: return 'Server';
        case HotpatchLayer.All: return 'All';
        default: return `Layer(${layer})`;
    }
}
function kindName(kind) {
    switch (kind) {
        case HotpatchSymbolKind.MemoryPatch: return 'MemoryPatch';
        case HotpatchSymbolKind.BytePatch: return 'BytePatch';
        case HotpatchSymbolKind.ServerPatch: return 'ServerPatch';
        case HotpatchSymbolKind.GGUFTensor: return 'Tensor';
        case HotpatchSymbolKind.GGUFMetadata: return 'Metadata';
        case HotpatchSymbolKind.PatchPreset: return 'Preset';
        case HotpatchSymbolKind.InjectionPoint: return 'InjectionPoint';
        case HotpatchSymbolKind.ProxyRewrite: return 'ProxyRewrite';
        default: return `Kind(${kind})`;
    }
}
function symbolIcon(kind) {
    switch (kind) {
        case HotpatchSymbolKind.MemoryPatch: return 'method';
        case HotpatchSymbolKind.BytePatch: return 'file';
        case HotpatchSymbolKind.ServerPatch: return 'event';
        case HotpatchSymbolKind.GGUFTensor: return 'array';
        case HotpatchSymbolKind.GGUFMetadata: return 'key';
        case HotpatchSymbolKind.PatchPreset: return 'package';
        case HotpatchSymbolKind.InjectionPoint: return 'interface';
        case HotpatchSymbolKind.ProxyRewrite: return 'ruler';
        default: return 'misc';
    }
}
function formatBytes(bytes) {
    if (bytes < 1024) {
        return `${bytes} B`;
    }
    if (bytes < 1024 * 1024) {
        return `${(bytes / 1024).toFixed(1)} KB`;
    }
    if (bytes < 1024 * 1024 * 1024) {
        return `${(bytes / (1024 * 1024)).toFixed(1)} MB`;
    }
    return `${(bytes / (1024 * 1024 * 1024)).toFixed(2)} GB`;
}
function formatNumber(n) {
    if (n >= 1e12) {
        return `${(n / 1e12).toFixed(1)}T`;
    }
    if (n >= 1e9) {
        return `${(n / 1e9).toFixed(1)}B`;
    }
    if (n >= 1e6) {
        return `${(n / 1e6).toFixed(1)}M`;
    }
    if (n >= 1e3) {
        return `${(n / 1e3).toFixed(1)}K`;
    }
    return `${n}`;
}
function renderModelInfoHTML(info) {
    return `<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>GGUF Model Info</title>
    <style>
        body {
            font-family: var(--vscode-font-family, 'Segoe UI', sans-serif);
            color: var(--vscode-foreground, #ccc);
            background: var(--vscode-editor-background, #1e1e1e);
            padding: 16px;
            line-height: 1.6;
        }
        h1 {
            color: var(--vscode-textLink-foreground, #3794ff);
            font-size: 1.4em;
            margin-bottom: 8px;
        }
        .grid {
            display: grid;
            grid-template-columns: 1fr 1fr;
            gap: 8px 24px;
            max-width: 600px;
        }
        .label {
            color: var(--vscode-descriptionForeground, #888);
            font-size: 0.9em;
        }
        .value {
            font-weight: bold;
            font-size: 1em;
        }
        .mono {
            font-family: var(--vscode-editor-font-family, 'Consolas', monospace);
        }
        hr {
            border: none;
            border-top: 1px solid var(--vscode-widget-border, #444);
            margin: 16px 0;
        }
        .badge {
            display: inline-block;
            padding: 2px 8px;
            border-radius: 4px;
            font-size: 0.85em;
            font-weight: bold;
        }
        .badge-arch {
            background: var(--vscode-badge-background, #4d4d4d);
            color: var(--vscode-badge-foreground, #fff);
        }
        .badge-quant {
            background: #2d5a27;
            color: #8bdb81;
        }
    </style>
</head>
<body>
    <h1>📦 ${escapeHtml(path.basename(info.filePath))}</h1>
    <p>
        <span class="badge badge-arch">${escapeHtml(info.architecture)}</span>
        <span class="badge badge-quant">${escapeHtml(info.quantType)}</span>
    </p>
    <hr>
    <div class="grid">
        <span class="label">Parameters</span>
        <span class="value">${formatNumber(info.paramCount)}</span>

        <span class="label">Tensors</span>
        <span class="value">${info.tensorCount}</span>

        <span class="label">Layers</span>
        <span class="value">${info.layerCount}</span>

        <span class="label">Context Length</span>
        <span class="value">${info.contextLength.toLocaleString()}</span>

        <span class="label">Embedding Dim</span>
        <span class="value">${info.embeddingLength}</span>

        <span class="label">Attention Heads</span>
        <span class="value">${info.headCount}</span>

        <span class="label">File Size</span>
        <span class="value">${formatBytes(info.fileSize)}</span>

        <span class="label">GGUF Version</span>
        <span class="value mono">v${info.version}</span>

        <span class="label">Metadata Keys</span>
        <span class="value">${info.metadataCount}</span>
    </div>
    <hr>
    <p class="mono" style="font-size: 0.85em; color: var(--vscode-descriptionForeground);">
        ${escapeHtml(info.filePath)}
    </p>
</body>
</html>`;
}
function escapeHtml(text) {
    return text
        .replace(/&/g, '&amp;')
        .replace(/</g, '&lt;')
        .replace(/>/g, '&gt;')
        .replace(/"/g, '&quot;');
}
//# sourceMappingURL=extension.js.map