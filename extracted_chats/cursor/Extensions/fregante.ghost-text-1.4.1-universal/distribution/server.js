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
exports.stopServer = exports.startServer = exports.Eaddrinuse = void 0;
const http = __importStar(require("node:http"));
const vscode = __importStar(require("vscode"));
const ws_1 = require("ws");
let server;
function getPort() {
    return vscode.workspace.getConfiguration('ghostText').get('serverPort', 4001);
}
async function pingResponder(_, response) {
    response.writeHead(200, {
        'Content-Type': 'application/json',
    });
    response.end(JSON.stringify({
        // eslint-disable-next-line @typescript-eslint/naming-convention
        ProtocolVersion: 1,
        // eslint-disable-next-line @typescript-eslint/naming-convention
        WebSocketPort: getPort(),
    }));
}
class Eaddrinuse extends Error {
}
exports.Eaddrinuse = Eaddrinuse;
async function listen(server) {
    const port = getPort();
    return new Promise((resolve, reject) => {
        server
            .listen(getPort())
            .once('listening', resolve)
            .once('error', (error) => {
            if (error.code === 'EADDRINUSE') {
                reject(new Eaddrinuse(`The port ${port} is already in use`));
            }
            else {
                reject(error);
            }
        });
    });
}
async function startServer(subscriptions, onConnection) {
    console.log('GhostText: Server starting');
    server?.close();
    server = http.createServer(pingResponder);
    await listen(server);
    const ws = new ws_1.Server({ server });
    ws.on('connection', onConnection);
    console.log('GhostText: Server started');
    void vscode.commands.executeCommand('setContext', 'ghostText.server', true);
    subscriptions.push({
        dispose() {
            server?.close();
        },
    });
}
exports.startServer = startServer;
function stopServer() {
    server?.close();
    server = undefined;
    console.log('GhostText: Server stopped');
    void vscode.commands.executeCommand('setContext', 'ghostText.server', false);
}
exports.stopServer = stopServer;
//# sourceMappingURL=server.js.map