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
exports.XdebugCloudConnection = void 0;
const crc32 = __importStar(require("buffer-crc32"));
const net = __importStar(require("net"));
const dbgp_1 = require("./dbgp");
const tls = __importStar(require("tls"));
const iconv = __importStar(require("iconv-lite"));
const xdebug = __importStar(require("./xdebugConnection"));
const events_1 = require("events");
class XdebugCloudConnection extends events_1.EventEmitter {
    constructor(token, testSocket) {
        super();
        this._logging = true;
        if (testSocket != null) {
            this._netSocket = testSocket;
            this._tlsSocket = testSocket;
        }
        else {
            this._netSocket = new net.Socket();
            this._tlsSocket = new tls.TLSSocket(this._netSocket);
        }
        this._token = token;
        this._resolveFn = null;
        this._rejectFn = null;
        this._dbgpConnection = new dbgp_1.DbgpConnection(this._tlsSocket);
        this._dbgpConnection.on('log', (text) => {
            if (this._logging) {
                this.emit('log', text);
            }
        });
        this._dbgpConnection.on('message', (response) => {
            var _a, _b, _c, _d, _e, _f;
            if (response.documentElement.nodeName === 'cloudinit') {
                if (response.documentElement.firstChild && response.documentElement.firstChild.nodeName === 'error') {
                    (_a = this._rejectFn) === null || _a === void 0 ? void 0 : _a.call(this, new Error(`Error in CloudInit ${(_b = response.documentElement.firstChild.textContent) !== null && _b !== void 0 ? _b : ''}`));
                }
                else {
                    (_c = this._resolveFn) === null || _c === void 0 ? void 0 : _c.call(this);
                }
            }
            else if (response.documentElement.nodeName === 'cloudstop') {
                if (response.documentElement.firstChild && response.documentElement.firstChild.nodeName === 'error') {
                    (_d = this._rejectFn) === null || _d === void 0 ? void 0 : _d.call(this, new Error(`Error in CloudStop ${(_e = response.documentElement.firstChild.textContent) !== null && _e !== void 0 ? _e : ''}`));
                }
                else {
                    (_f = this._resolveFn) === null || _f === void 0 ? void 0 : _f.call(this);
                }
            }
            else if (response.documentElement.nodeName === 'init') {
                this._logging = false;
                // spawn a new xdebug.Connection
                const cx = new xdebug.Connection(new InnerCloudTransport(this._tlsSocket));
                cx.once('close', () => (this._logging = true));
                cx.emit('message', response);
                this.emit('connection', cx);
            }
        });
        this._dbgpConnection.on('error', (err) => {
            var _a;
            this.emit('log', `dbgp error: ${err.toString()}`);
            (_a = this._rejectFn) === null || _a === void 0 ? void 0 : _a.call(this, err instanceof Error ? err : new Error(err));
        });
        /*
        this._netSocket.on('error', (err: Error) => {
            this.emit('log', `netSocket error ${err.toString()}`)
            this._rejectFn?.(err instanceof Error ? err : new Error(err))
        })
        */
        /*
        this._netSocket.on('connect', () => {
            this.emit('log', `netSocket connected`)
            //  this._resolveFn?.()
        })
        this._tlsSocket.on('secureConnect', () => {
            this.emit('log', `tlsSocket secureConnect`)
            //this._resolveFn?.()
        })
        */
        /*
        this._netSocket.on('close', had_error => {
            this.emit('log', 'netSocket close')
            this._rejectFn?.() // err instanceof Error ? err : new Error(err))
        })
        this._tlsSocket.on('close', had_error => {
            this.emit('log', 'tlsSocket close')
            this._rejectFn?.()
        })
        */
        this._dbgpConnection.on('close', () => {
            var _a;
            this.emit('log', `dbgp close`);
            (_a = this._rejectFn) === null || _a === void 0 ? void 0 : _a.call(this); // err instanceof Error ? err : new Error(err))
            this.emit('close');
        });
    }
    computeCloudHost(token) {
        const c = crc32.default(token);
        const last = c[3] & 0x0f;
        const url = `${String.fromCharCode(97 + last)}.cloud.xdebug.com`;
        return url;
    }
    async connect() {
        await new Promise((resolveFn, rejectFn) => {
            this._resolveFn = resolveFn;
            this._rejectFn = rejectFn;
            this._netSocket
                .connect({
                host: this.computeCloudHost(this._token),
                servername: this.computeCloudHost(this._token),
                port: 9021,
            }, resolveFn)
                .on('error', rejectFn);
        });
        const commandString = `cloudinit -i 1 -u ${this._token}\0`;
        const data = iconv.encode(commandString, dbgp_1.ENCODING);
        const p2 = new Promise((resolveFn, rejectFn) => {
            this._resolveFn = resolveFn;
            this._rejectFn = rejectFn;
        });
        await this._dbgpConnection.write(data);
        await p2;
    }
    async stop() {
        if (!this._tlsSocket.writable) {
            return Promise.resolve();
        }
        const commandString = `cloudstop -i 2 -u ${this._token}\0`;
        const data = iconv.encode(commandString, dbgp_1.ENCODING);
        const p2 = new Promise((resolveFn, rejectFn) => {
            this._resolveFn = resolveFn;
            this._rejectFn = rejectFn;
        });
        await this._dbgpConnection.write(data);
        return p2;
    }
    async close() {
        return new Promise(resolve => {
            this._tlsSocket.end(resolve);
        });
    }
    async connectAndStop() {
        await new Promise((resolveFn, rejectFn) => {
            // this._resolveFn = resolveFn
            this._rejectFn = rejectFn;
            this._netSocket
                .connect({
                host: this.computeCloudHost(this._token),
                servername: this.computeCloudHost(this._token),
                port: 9021,
            }, resolveFn)
                .on('error', rejectFn);
        });
        await this.stop();
        await this.close();
    }
}
exports.XdebugCloudConnection = XdebugCloudConnection;
class InnerCloudTransport extends events_1.EventEmitter {
    constructor(_socket) {
        super();
        this._socket = _socket;
        this._open = true;
        this._socket.on('data', (data) => {
            if (this._open)
                this.emit('data', data);
        });
        this._socket.on('error', (error) => {
            if (this._open)
                this.emit('error', error);
        });
        this._socket.on('close', () => {
            if (this._open)
                this.emit('close');
        });
    }
    get writable() {
        return this._open && this._socket.writable;
    }
    write(buffer, cb) {
        return this._socket.write(buffer, cb);
    }
    end(callback) {
        if (this._open) {
            this._open = false;
            this.emit('close');
        }
        return this;
    }
    destroy(error) {
        if (this._open) {
            this._open = false;
            this.emit('close');
        }
        return this;
    }
}
//# sourceMappingURL=cloud.js.map