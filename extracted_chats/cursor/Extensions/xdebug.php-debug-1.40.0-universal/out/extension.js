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
const which = __importStar(require("which"));
const path = __importStar(require("path"));
const controlSocket_1 = require("./controlSocket");
function activate(context) {
    context.subscriptions.push(vscode.debug.registerDebugConfigurationProvider('php', {
        async resolveDebugConfiguration(folder, debugConfiguration, token) {
            var _a;
            const isDynamic = (!debugConfiguration.type || debugConfiguration.type === 'php') &&
                !debugConfiguration.request &&
                !debugConfiguration.name;
            if (isDynamic) {
                const editor = vscode.window.activeTextEditor;
                if (editor && editor.document.languageId === 'php') {
                    debugConfiguration.type = 'php';
                    debugConfiguration.name = 'Launch (dynamic)';
                    debugConfiguration.request = 'launch';
                    debugConfiguration.program = debugConfiguration.program || '${file}';
                    debugConfiguration.cwd = debugConfiguration.cwd || '${fileDirname}';
                    debugConfiguration.port = 0;
                    debugConfiguration.runtimeArgs = ['-dxdebug.start_with_request=yes'];
                    debugConfiguration.env = {
                        XDEBUG_MODE: 'debug,develop',
                        XDEBUG_CONFIG: 'client_port=${port}',
                    };
                    // debugConfiguration.stopOnEntry = true
                }
            }
            if ((debugConfiguration.program || debugConfiguration.runtimeArgs) &&
                !debugConfiguration.runtimeExecutable) {
                // See if we have runtimeExecutable configured
                const conf = vscode.workspace.getConfiguration('php.debug');
                const executablePath = conf.get('executablePath');
                if (executablePath) {
                    debugConfiguration.runtimeExecutable = executablePath;
                }
                // See if it's in path
                if (!debugConfiguration.runtimeExecutable) {
                    try {
                        await which.default('php');
                    }
                    catch (e) {
                        const selected = await vscode.window.showErrorMessage('PHP executable not found. Install PHP and add it to your PATH or set the php.debug.executablePath setting', 'Open settings');
                        if (selected === 'Open settings') {
                            await vscode.commands.executeCommand('workbench.action.openGlobalSettings', {
                                query: 'php.debug.executablePath',
                            });
                            return undefined;
                        }
                    }
                }
            }
            if (((_a = debugConfiguration.proxy) === null || _a === void 0 ? void 0 : _a.enable) === true) {
                // Proxy configuration
                if (!debugConfiguration.proxy.key) {
                    const conf = vscode.workspace.getConfiguration('php.debug');
                    const ideKey = conf.get('ideKey');
                    if (ideKey) {
                        debugConfiguration.proxy.key = ideKey;
                    }
                }
            }
            if (folder && folder.uri.scheme !== 'file') {
                // replace
                if (debugConfiguration.pathMappings) {
                    for (const key in debugConfiguration.pathMappings) {
                        debugConfiguration.pathMappings[key] = debugConfiguration.pathMappings[key].replace('${workspaceFolder}', folder.uri.toString());
                    }
                }
                // The following path are currently NOT mapped
                /*
                debugConfiguration.skipEntryPaths = debugConfiguration.skipEntryPaths?.map(v =>
                    v.replace('${workspaceFolder}', folder.uri.toString())
                )
                debugConfiguration.skipFiles = debugConfiguration.skipFiles?.map(v =>
                    v.replace('${workspaceFolder}', folder.uri.toString())
                )
                debugConfiguration.ignore = debugConfiguration.ignore?.map(v =>
                    v.replace('${workspaceFolder}', folder.uri.toString())
                )
                */
            }
            return debugConfiguration;
        },
    }));
    context.subscriptions.push(vscode.commands.registerCommand('extension.php-debug.runEditorContents', (resource) => {
        let targetResource = resource;
        if (!targetResource && vscode.window.activeTextEditor) {
            targetResource = vscode.window.activeTextEditor.document.uri;
        }
        if (targetResource) {
            void vscode.debug.startDebugging(undefined, {
                type: 'php',
                name: '',
                request: '',
                noDebug: true,
                program: targetResource.fsPath,
                cwd: path.dirname(targetResource.fsPath),
            });
        }
    }), vscode.commands.registerCommand('extension.php-debug.debugEditorContents', (resource) => {
        let targetResource = resource;
        if (!targetResource && vscode.window.activeTextEditor) {
            targetResource = vscode.window.activeTextEditor.document.uri;
        }
        if (targetResource) {
            void vscode.debug.startDebugging(undefined, {
                type: 'php',
                name: '',
                request: '',
                stopOnEntry: true,
                program: targetResource.fsPath,
                cwd: path.dirname(targetResource.fsPath),
            });
        }
    }));
    context.subscriptions.push(vscode.commands.registerCommand('extension.php-debug.startWithStopOnEntry', async (uri) => {
        await vscode.commands.executeCommand('workbench.action.debug.start', {
            config: {
                stopOnEntry: true,
            },
        });
    }));
    context.subscriptions.push(vscode.commands.registerCommand('extension.php-debug.listenForXdebug', async () => {
        await vscode.debug.startDebugging(undefined, {
            name: 'Listen for Xdebug',
            type: 'php',
            request: 'launch',
            port: 9003,
        });
    }));
    const copyVar = async (arg, context) => {
        var _a;
        const aci = vscode.debug.activeStackItem;
        if (aci && aci instanceof vscode.DebugStackFrame) {
            const ret = (await ((_a = vscode.debug.activeDebugSession) === null || _a === void 0 ? void 0 : _a.customRequest('evaluate', {
                context,
                expression: arg.variable.evaluateName,
                frameId: aci.frameId,
                variablesReference: arg.variable.variablesReference,
            })));
            await vscode.env.clipboard.writeText(ret.result);
        }
        else {
            await vscode.window.showErrorMessage('Cannot derermine active debug session');
        }
    };
    context.subscriptions.push(vscode.commands.registerCommand('extension.php-debug.copyVarExport', async (arg, p2, p3) => {
        await copyVar(arg, 'clipboard-var_export');
    }));
    context.subscriptions.push(vscode.commands.registerCommand('extension.php-debug.copyJson', async (arg, p2, p3) => {
        await copyVar(arg, 'clipboard-json');
    }));
    context.subscriptions.push(vscode.commands.registerCommand('extension.php-debug.copyRaw', async (arg, p2, p3) => {
        await copyVar(arg, 'clipboard-raw');
    }));
    context.subscriptions.push(vscode.commands.registerCommand('extension.php-debug.pauseXdebugControlSocket', async () => {
        const controlSocket = new controlSocket_1.ControlSocket();
        const sockets = await vscode.window.withProgress({
            location: vscode.ProgressLocation.Window,
            title: 'Loading PHP processes...',
        }, async (progress) => {
            progress.report({ increment: 0 });
            const sockets = await controlSocket.listControlSockets();
            progress.report({ increment: 100 });
            return sockets;
        });
        const qpi = sockets.map(v => ({
            label: v.ctrlSocket,
            description: v.ps ? `Mem: ${v.ps.memory}` : '',
            detail: v.ps ? v.ps.fileUri : '',
        }));
        const i = await vscode.window.showQuickPick(qpi, { canPickMany: false, title: 'Select a PHP process...' });
        if (i) {
            await controlSocket.requestPause(i.label);
        }
    }));
}
exports.activate = activate;
//# sourceMappingURL=extension.js.map