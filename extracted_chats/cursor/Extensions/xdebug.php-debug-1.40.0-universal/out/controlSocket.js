"use strict";
var __createBinding = (this && this.__createBinding) || (Object.create ? (function (o, m, k, k2) {
    if (k2 === undefined) k2 = k;
    var desc = Object.getOwnPropertyDescriptor(m, k);
    if (!desc || ("get" in desc ? !m.__esModule : desc.writable || desc.configurable)) {
        desc = { enumerable: true, get: function () { return m[k]; } };
    }
    Object.defineProperty(o, k2, desc);
}) : (function (o, m, k, k2) {
    if (k2 === undefined) k2 = k;
    o[k2] = m[k];
}));
var __setModuleDefault = (this && this.__setModuleDefault) || (Object.create ? (function (o, v) {
    Object.defineProperty(o, "default", { enumerable: true, value: v });
}) : function (o, v) {
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
exports.ControlPS = exports.ControlSocket = void 0;
const net = __importStar(require("net"));
const CP = __importStar(require("child_process"));
const util_1 = require("util");
const xdebugUtils_1 = require("./xdebugUtils");
const xmldom_1 = require("@xmldom/xmldom");
const fs = __importStar(require("fs"));
const iconv_lite_1 = require("iconv-lite");
const dbgp_1 = require("./dbgp");
class ControlSocket {
    /**
     * @returns Returns true if the current platoform is supported for Xdebug control socket (win and linux)
     */
    supportedPlatform() {
        return process.platform === 'linux' || process.platform === 'win32';
    }
    /**
     *
     * @param initPacket
     * @returns Returns true if the current platform and xdebug version are supporting Xdebug control socket.
     */
    supportedInitPacket(initPacket) {
        return this.supportedPlatform() && (0, xdebugUtils_1.supportedEngine)(initPacket, '3.5.0');
    }
    /**
     * Request the pause control socket command
     * @param ctrlSocket Control socket full path
     * @returns
     */
    async requestPause(ctrlSocket) {
        await this.executeCtrlCmd(ctrlSocket, 'pause');
    }
    async requestPS(ctrlSocket) {
        const xml = await this.executeCtrlCmd(ctrlSocket, 'ps');
        const parser = new xmldom_1.DOMParser();
        const document = parser.parseFromString(xml, 'application/xml');
        return new ControlPS(document);
    }
    async executeCtrlCmd(ctrlSocket, cmd) {
        let rawCtrlSocket;
        if (process.platform === 'linux') {
            rawCtrlSocket = `\0${ctrlSocket}`;
        }
        else if (process.platform === 'win32') {
            rawCtrlSocket = `\\\\.\\pipe\\${ctrlSocket}`;
        }
        else {
            throw new Error('Invalid platform for Xdebug control socket');
        }
        return new Promise((resolve, reject) => {
            const s = net.createConnection(rawCtrlSocket, () => {
                s.end(`${cmd}\0`);
            });
            s.setTimeout(3000);
            s.on('timeout', () => {
                reject(new Error('Timed out while reading from Xdebug control socket'));
                s.end();
            });
            s.on('data', data => {
                s.destroy();
                if (data.length > 0 && data.at(data.length - 1) == 0) {
                    resolve((0, iconv_lite_1.decode)(data.subarray(0, data.length - 1), dbgp_1.ENCODING));
                }
                else {
                    resolve((0, iconv_lite_1.decode)(data, dbgp_1.ENCODING));
                }
            });
            s.on('error', error => {
                reject(new Error(`Cannot connect to Xdebug control socket: ${String(error instanceof Error ? error.message : error)}`));
            });
            return;
        });
    }
    async listControlSockets() {
        let retval;
        if (process.platform === 'linux') {
            retval = await this.listControlSocketsLinux();
        }
        else if (process.platform === 'win32') {
            retval = await this.listControlSocketsWin();
        }
        else {
            throw new Error('Invalid platform for Xdebug control socket');
        }
        const retval2 = Promise.all(retval.map(async (v) => {
            try {
                v.ps = await this.requestPS(v.ctrlSocket);
            }
            catch {
                // ignore
            }
            return v;
        }));
        return retval2;
    }
    async listControlSocketsLinux() {
        const re = /@(xdebug-ctrl\.\d+)$/;
        const data = await fs.promises.readFile('/proc/net/unix');
        const lines = data.toString().split('\n');
        const sockets = [];
        for (const line of lines) {
            const matches = line.match(re);
            if (matches && matches.length > 0) {
                sockets.push({ ctrlSocket: matches[1] });
            }
        }
        return sockets;
    }
    async listControlSocketsWin() {
        const exec = (0, util_1.promisify)(CP.exec);
        try {
            const ret = await exec('cmd /C "dir \\\\.\\pipe\\\\xdebug-ctrl* /b"');
            const lines = ret.stdout.split('\r\n');
            const retval = lines
                .filter(v => v.length != 0)
                .map(v => ({ ctrlSocket: v }));
            return retval;
        }
        catch (err) {
            if (err instanceof Error && err.stderr == 'File Not Found\r\n') {
                return [];
            }
            throw err;
        }
    }
}
exports.ControlSocket = ControlSocket;
class ControlPS {
    /**
     * @param  {XMLDocument} document - An XML document to read from
     */
    constructor(document) {
        var _a, _b, _c, _d, _e, _f, _g, _h, _j, _k;
        const documentElement = document.documentElement.firstChild;
        this.fileUri = (_b = (_a = documentElement.getElementsByTagName('fileuri').item(0)) === null || _a === void 0 ? void 0 : _a.textContent) !== null && _b !== void 0 ? _b : '';
        this.engineVersion = (_d = (_c = documentElement.getElementsByTagName('engine').item(0)) === null || _c === void 0 ? void 0 : _c.getAttribute('version')) !== null && _d !== void 0 ? _d : '';
        this.engineName = (_f = (_e = documentElement.getElementsByTagName('engine').item(0)) === null || _e === void 0 ? void 0 : _e.textContent) !== null && _f !== void 0 ? _f : '';
        this.pid = (_h = (_g = documentElement.getElementsByTagName('pid').item(0)) === null || _g === void 0 ? void 0 : _g.textContent) !== null && _h !== void 0 ? _h : '';
        this.memory = parseInt((_k = (_j = documentElement.getElementsByTagName('memory').item(0)) === null || _j === void 0 ? void 0 : _j.textContent) !== null && _k !== void 0 ? _k : '0');
    }
}
exports.ControlPS = ControlPS;
//# sourceMappingURL=controlSocket.js.map