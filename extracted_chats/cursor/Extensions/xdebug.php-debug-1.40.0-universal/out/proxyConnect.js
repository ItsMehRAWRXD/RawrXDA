"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.ProxyConnect = exports.DEFAULTIDEKEY = void 0;
const net_1 = require("net");
const xmldom_1 = require("@xmldom/xmldom");
const events_1 = require("events");
const iconv_lite_1 = require("iconv-lite");
const dbgp_1 = require("./dbgp");
exports.DEFAULTIDEKEY = 'vsc';
/** Informs proxy of incoming connection and who to pass data back to. */
class ProxyConnect extends events_1.EventEmitter {
    constructor(host = '127.0.0.1', port = 9001, idePort = 9003, allowMultipleSessions = true, key = exports.DEFAULTIDEKEY, timeout = 3000, socket) {
        super();
        /** proxy response data parser */
        this._parser = new xmldom_1.DOMParser();
        this._isRegistered = false;
        this._allowMultipleSessions = allowMultipleSessions ? 1 : 0;
        this._host = host;
        this._key = key;
        this._port = port;
        this._idePort = idePort;
        this._timeout = timeout;
        this._socket = socket ? socket : new net_1.Socket();
        this._chunksDataLength = 0;
        this._chunks = [];
        this._resolveFn = null;
        this._rejectFn = null;
        this.msgs = {
            defaultError: 'Unknown proxy Error',
            deregisterInfo: `De-registering ${this._key} with proxy @ ${this._host}:${this._port}`,
            deregisterSuccess: 'De-registration successful',
            duplicateKey: 'IDE Key already exists',
            nonexistentKey: 'No IDE key',
            registerInfo: `Registering ${this._key} on port ${this._idePort} with proxy @ ${this._host}:${this._port}`,
            registerSuccess: 'Registration successful',
            resolve: `Failure to resolve ${this._host}`,
            timeout: `Timeout connecting to ${this._host}:${this._port}`,
            raceCall: 'New command before old finished',
        };
        this._socket.on('error', (err) => {
            var _a;
            // Propagate error up
            this._socket.end();
            this.emit('log_error', err instanceof Error ? err : new Error(err));
            (_a = this._rejectFn) === null || _a === void 0 ? void 0 : _a.call(this, err instanceof Error ? err : new Error(err));
        });
        this._socket.on('lookup', (err, address, family, host) => {
            if (err instanceof Error) {
                this._socket.emit('error', this.msgs.resolve);
            }
        });
        this._socket.on('data', data => {
            this._chunks.push(data);
            this._chunksDataLength += data.length;
        });
        this._socket.on('close', had_error => {
            if (!had_error) {
                this._responseStrategy(Buffer.concat(this._chunks, this._chunksDataLength));
            }
            this._chunksDataLength = 0;
            this._chunks = [];
        });
        this._socket.setTimeout(this._timeout);
        this._socket.on('timeout', () => {
            this._socket.emit('error', this.msgs.timeout);
        });
    }
    _command(cmd, msg) {
        this.emit('log_request', msg);
        this._socket.connect(this._port, this._host, () => {
            this._socket.write(cmd);
            new Promise(resolve => setTimeout(resolve, 500))
                .then(() => {
                if (!this._socket.destroyed) {
                    this._socket.write('\0');
                }
            })
                .catch(err => {
                var _a;
                (_a = this._rejectFn) === null || _a === void 0 ? void 0 : _a.call(this, new Error(err));
            });
        });
    }
    /** Register/Couples ideKey to IP so the proxy knows who to send what */
    sendProxyInitCommand() {
        var _a;
        (_a = this._rejectFn) === null || _a === void 0 ? void 0 : _a.call(this, new Error(this.msgs.raceCall));
        return new Promise((resolveFn, rejectFn) => {
            if (!this._isRegistered) {
                this._resolveFn = resolveFn;
                this._rejectFn = rejectFn;
                this._command(`proxyinit -k ${this._key} -p ${this._idePort} -m ${this._allowMultipleSessions}`, this.msgs.registerInfo);
            }
            else {
                resolveFn();
            }
        });
    }
    /** De-registers/Decouples ideKey from IP, allowing others to use the ideKey */
    sendProxyStopCommand() {
        var _a;
        (_a = this._rejectFn) === null || _a === void 0 ? void 0 : _a.call(this, new Error(this.msgs.raceCall));
        return new Promise((resolveFn, rejectFn) => {
            if (this._isRegistered) {
                this._resolveFn = resolveFn;
                this._rejectFn = rejectFn;
                this._command(`proxystop -k ${this._key}`, this.msgs.deregisterInfo);
            }
            else {
                resolveFn();
            }
        });
    }
    /**
     * The proxy protocol changed over time and initially the response was just XML.
     * Later with re-implementation it follows the same spec as DBGp: [data length] [NULL] [xml] [NULL]
     * @param data Recieved data
     */
    _extractPacket(data) {
        let packetData = data;
        const nullByteIndex = data.indexOf(0);
        if (nullByteIndex !== -1) {
            const lastPiece = data.subarray(0, nullByteIndex);
            const dataLength = parseInt((0, iconv_lite_1.decode)(lastPiece, dbgp_1.ENCODING));
            if (!!dataLength && dataLength <= data.length - nullByteIndex - 1) {
                packetData = data.subarray(nullByteIndex + 1, nullByteIndex + 1 + dataLength);
            }
        }
        return (0, iconv_lite_1.decode)(packetData, dbgp_1.ENCODING);
    }
    /** Parse data from response server and emit the relevant notification. */
    _responseStrategy(data) {
        var _a, _b, _c, _d, _e;
        try {
            const documentElement = this._parser.parseFromString(this._extractPacket(data), 'application/xml')
                .documentElement;
            const isSuccessful = documentElement.getAttribute('success') === '1';
            const error = documentElement.firstChild;
            if (isSuccessful && documentElement.nodeName === 'proxyinit') {
                this._isRegistered = true;
                this.emit('log_response', this.msgs.registerSuccess);
                (_a = this._resolveFn) === null || _a === void 0 ? void 0 : _a.call(this);
            }
            else if (isSuccessful && documentElement.nodeName === 'proxystop') {
                this._isRegistered = false;
                this.emit('log_response', this.msgs.deregisterSuccess);
                (_b = this._resolveFn) === null || _b === void 0 ? void 0 : _b.call(this);
            }
            else if (error && error.nodeName === 'error' && error.firstChild && error.firstChild.textContent) {
                this._socket.emit('error', error.firstChild.textContent);
                (_c = this._rejectFn) === null || _c === void 0 ? void 0 : _c.call(this, new Error(error.firstChild.textContent));
            }
            else {
                this._socket.emit('error', this.msgs.defaultError);
                (_d = this._rejectFn) === null || _d === void 0 ? void 0 : _d.call(this, new Error(this.msgs.defaultError));
            }
        }
        catch (error) {
            (_e = this._rejectFn) === null || _e === void 0 ? void 0 : _e.call(this, new Error(`Proxy read error ${error instanceof Error ? error.message : error}`));
        }
    }
}
exports.ProxyConnect = ProxyConnect;
//# sourceMappingURL=proxyConnect.js.map