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
exports.DbgpConnection = exports.ENCODING = void 0;
const events_1 = require("events");
const iconv = __importStar(require("iconv-lite"));
const xmldom_1 = require("@xmldom/xmldom");
/** The encoding all Xdebug messages are encoded with */
exports.ENCODING = 'utf-8';
/** The two states the connection switches between */
var ParsingState;
(function (ParsingState) {
    ParsingState[ParsingState["DataLength"] = 0] = "DataLength";
    ParsingState[ParsingState["Response"] = 1] = "Response";
})(ParsingState || (ParsingState = {}));
/** Wraps the NodeJS Socket and calls handleResponse() whenever a full response arrives */
class DbgpConnection extends events_1.EventEmitter {
    constructor(socket) {
        super();
        this._socket = socket;
        this._parsingState = ParsingState.DataLength;
        this._chunksDataLength = 0;
        this._chunks = [];
        this._closePromise = new Promise(resolve => (this._closePromiseResolveFn = resolve));
        socket.on('data', (data) => this._handleDataChunk(data));
        socket.on('error', (error) => this.emit('error', error));
        socket.on('close', () => {
            var _a;
            (_a = this._closePromiseResolveFn) === null || _a === void 0 ? void 0 : _a.call(this);
            this.emit('close');
        });
    }
    _handleDataChunk(data) {
        // Anatomy of packets: [data length] [NULL] [xml] [NULL]
        // are we waiting for the data length or for the response?
        if (this._parsingState === ParsingState.DataLength) {
            // does data contain a NULL byte?
            const nullByteIndex = data.indexOf(0);
            if (nullByteIndex !== -1) {
                // YES -> we received the data length and are ready to receive the response
                const lastPiece = data.slice(0, nullByteIndex);
                this._chunks.push(lastPiece);
                this._chunksDataLength += lastPiece.length;
                this._dataLength = parseInt(iconv.decode(Buffer.concat(this._chunks, this._chunksDataLength), exports.ENCODING), 10);
                // reset buffered chunks
                this._chunks = [];
                this._chunksDataLength = 0;
                // switch to response parsing state
                this._parsingState = ParsingState.Response;
                // if data contains more info (except the NULL byte)
                if (data.length > nullByteIndex + 1) {
                    // handle the rest of the packet as part of the response
                    const rest = data.slice(nullByteIndex + 1);
                    this._handleDataChunk(rest);
                }
            }
            else {
                // NO -> this is only part of the data length. We wait for the next data event
                this._chunks.push(data);
                this._chunksDataLength += data.length;
            }
        }
        else if (this._parsingState === ParsingState.Response) {
            // does the new data together with the buffered data add up to the data length?
            if (this._chunksDataLength + data.length >= this._dataLength) {
                // YES -> we received the whole response
                // append the last piece of the response
                const lastResponsePiece = data.slice(0, this._dataLength - this._chunksDataLength);
                this._chunks.push(lastResponsePiece);
                this._chunksDataLength += lastResponsePiece.length;
                const response = Buffer.concat(this._chunks, this._chunksDataLength);
                // call response handler
                const xml = iconv.decode(response, exports.ENCODING);
                const parser = new xmldom_1.DOMParser({
                    onError: (level, msg) => {
                        if (level === 'warning') {
                            this.emit('warning', msg);
                        }
                        if (level === 'error' || level === 'fatalError') {
                            this.emit('error', new Error(msg));
                        }
                    },
                });
                this.emit('log', `-> ${xml.replace(/[\0\n]/g, '')}`);
                const document = parser.parseFromString(xml, 'application/xml');
                this.emit('message', document);
                // reset buffer
                this._chunks = [];
                this._chunksDataLength = 0;
                // switch to data length parsing state
                this._parsingState = ParsingState.DataLength;
                // if data contains more info (except the NULL byte)
                if (data.length > lastResponsePiece.length + 1) {
                    // handle the rest of the packet (after the NULL byte) as data length
                    const rest = data.slice(lastResponsePiece.length + 1);
                    this._handleDataChunk(rest);
                }
            }
            else {
                // NO -> this is not the whole response yet. We buffer it and wait for the next data event.
                this._chunks.push(data);
                this._chunksDataLength += data.length;
            }
        }
    }
    write(command) {
        return new Promise((resolve, reject) => {
            if (this._socket.writable) {
                this.emit('log', `<- ${command.toString().replace(/[\0\n]/g, '')}`);
                this._socket.write(command, () => {
                    resolve();
                });
            }
            else {
                reject(new Error('socket not writable'));
            }
        });
    }
    /** closes the underlying socket */
    close() {
        this._socket.destroy();
        return this._closePromise;
    }
}
exports.DbgpConnection = DbgpConnection;
//# sourceMappingURL=dbgp.js.map