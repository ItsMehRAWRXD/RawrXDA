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
exports.documents = void 0;
const vscode = __importStar(require("vscode"));
class State extends Map {
    constructor() {
        super(...arguments);
        Object.defineProperty(this, "remove", {
            enumerable: true,
            configurable: true,
            writable: true,
            value: new vscode.EventEmitter()
        });
        Object.defineProperty(this, "add", {
            enumerable: true,
            configurable: true,
            writable: true,
            value: new vscode.EventEmitter()
        });
        // eslint-disable-next-line @typescript-eslint/member-ordering
        Object.defineProperty(this, "onRemove", {
            enumerable: true,
            configurable: true,
            writable: true,
            value: this.remove.event
        });
        // eslint-disable-next-line @typescript-eslint/member-ordering
        Object.defineProperty(this, "onAdd", {
            enumerable: true,
            configurable: true,
            writable: true,
            value: this.add.event
        });
    }
    set(uri, field) {
        super.set(uri, field);
        this.add.fire(uri);
        return this;
    }
    delete(uri) {
        const removed = super.delete(uri);
        if (removed) {
            this.remove.fire(uri);
        }
        return removed;
    }
}
exports.documents = new State();
//# sourceMappingURL=state.js.map