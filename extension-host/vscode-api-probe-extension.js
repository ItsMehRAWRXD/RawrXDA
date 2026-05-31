'use strict';

let registered = false;
let disposable = null;

globalThis.handleRequest = async function handleRequest(request, rawrxd, vscode) {
    switch (request.type) {
    case 'vscode.command.roundtrip': {
        const commandId = request.payload && request.payload.commandId
            ? request.payload.commandId
            : 'rawrxd.test.roundtrip';
        if (!registered) {
            disposable = vscode.commands.registerCommand(commandId, async (input) => {
                return { echoed: input, apiVersion: vscode.version };
            });
            registered = true;
        }
        const result = await vscode.commands.executeCommand(commandId, request.payload && request.payload.value);
        return result;
    }
    case 'vscode.command.execute_unknown':
        return vscode.commands.executeCommand('rawrxd.unknown.command', request.payload || null);
    case 'vscode.fs.write': {
        const payload = request.payload || {};
        await vscode.workspace.fs.writeFile(payload.path, Buffer.from(String(payload.content || ''), 'utf8'));
        return { ok: true, path: payload.path };
    }
    case 'vscode.fs.read': {
        const payload = request.payload || {};
        const data = await vscode.workspace.fs.readFile(payload.path);
        return { content: Buffer.from(data).toString('utf8') };
    }
    case 'vscode.window.info': {
        const message = await vscode.window.showInformationMessage((request.payload && request.payload.message) || 'hello');
        return { message };
    }
    case 'vscode.unsupported.debug':
        return vscode.debug.startDebugging();
    default:
        if (disposable && request.type === 'vscode.command.dispose') {
            disposable.dispose();
            disposable = null;
            registered = false;
            return { disposed: true };
        }
        return { ok: true, type: request.type };
    }
};
