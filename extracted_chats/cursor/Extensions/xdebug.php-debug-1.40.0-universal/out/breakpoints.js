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
exports.BreakpointAdapter = exports.BreakpointManager = void 0;
const vscode = __importStar(require("@vscode/debugadapter"));
const events_1 = require("events");
const xdebug = __importStar(require("./xdebugConnection"));
const util = __importStar(require("util"));
/**
 * Keeps track of VS Code breakpoint IDs and maps them to Xdebug breakpoints.
 * Emits changes of breakpoints to BreakpointAdapter.
 */
class BreakpointManager extends events_1.EventEmitter {
    constructor() {
        super(...arguments);
        this._lineBreakpoints = new Map();
        this._exceptionBreakpoints = new Map();
        this._callBreakpoints = new Map();
        this._nextId = 1;
    }
    sourceKey(source) {
        return source.path;
    }
    setBreakPoints(source, fileUri, breakpoints) {
        var _a;
        // let vscodeBreakpoints: VSCodeDebugProtocol.Breakpoint[]
        let toAdd = new Map();
        const toRemove = [];
        const sourceKey = this.sourceKey(source);
        // remove all existing breakpoints in the file
        if (this._lineBreakpoints.has(sourceKey)) {
            (_a = this._lineBreakpoints.get(sourceKey)) === null || _a === void 0 ? void 0 : _a.forEach((_, key) => toRemove.push(key));
        }
        // clear all breakpoints in this path
        const sourceBreakpoints = new Map();
        this._lineBreakpoints.set(sourceKey, sourceBreakpoints);
        const vscodeBreakpoints = breakpoints.map(sourceBreakpoint => {
            let xdebugBreakpoint;
            let hitValue;
            let hitCondition;
            if (sourceBreakpoint.hitCondition) {
                const match = sourceBreakpoint.hitCondition.match(/^\s*(>=|==|%)?\s*(\d+)\s*$/);
                if (match) {
                    hitCondition = match[1] || '==';
                    hitValue = parseInt(match[2]);
                }
                else {
                    const vscodeBreakpoint = {
                        verified: false,
                        line: sourceBreakpoint.line,
                        source: source,
                        // id: this._nextId++,
                        message: 'Invalid hit condition. Specify a number, optionally prefixed with one of the operators >= (default), == or %',
                    };
                    return vscodeBreakpoint;
                }
            }
            if (sourceBreakpoint.condition) {
                xdebugBreakpoint = new xdebug.ConditionalBreakpoint(sourceBreakpoint.condition, fileUri, sourceBreakpoint.line, hitCondition, hitValue);
            }
            else {
                xdebugBreakpoint = new xdebug.LineBreakpoint(fileUri, sourceBreakpoint.line, hitCondition, hitValue);
            }
            const vscodeBreakpoint = {
                verified: this.listeners('add').length === 0,
                line: sourceBreakpoint.line,
                source: source,
                id: this._nextId++,
            };
            sourceBreakpoints.set(vscodeBreakpoint.id, xdebugBreakpoint);
            return vscodeBreakpoint;
        });
        toAdd = sourceBreakpoints;
        if (toRemove.length > 0) {
            this.emit('remove', toRemove);
        }
        if (toAdd.size > 0) {
            this.emit('add', toAdd);
        }
        return vscodeBreakpoints;
    }
    setExceptionBreakPoints(filters) {
        const vscodeBreakpoints = [];
        let toAdd = new Map();
        const toRemove = [];
        // always remove all breakpoints
        this._exceptionBreakpoints.forEach((_, key) => toRemove.push(key));
        this._exceptionBreakpoints.clear();
        filters.forEach(filter => {
            const xdebugBreakpoint = new xdebug.ExceptionBreakpoint(filter);
            const vscodeBreakpoint = {
                verified: this.listeners('add').length === 0,
                id: this._nextId++,
            };
            this._exceptionBreakpoints.set(vscodeBreakpoint.id, xdebugBreakpoint);
            vscodeBreakpoints.push(vscodeBreakpoint);
        });
        toAdd = this._exceptionBreakpoints;
        if (toRemove.length > 0) {
            this.emit('remove', toRemove);
        }
        if (toAdd.size > 0) {
            this.emit('add', toAdd);
        }
        return vscodeBreakpoints;
    }
    setFunctionBreakPointsRequest(breakpoints) {
        let vscodeBreakpoints = [];
        let toAdd = new Map();
        const toRemove = [];
        // always remove all breakpoints
        this._callBreakpoints.forEach((_, key) => toRemove.push(key));
        this._callBreakpoints.clear();
        vscodeBreakpoints = breakpoints.map(functionBreakpoint => {
            let hitValue;
            let hitCondition;
            if (functionBreakpoint.hitCondition) {
                const match = functionBreakpoint.hitCondition.match(/^\s*(>=|==|%)?\s*(\d+)\s*$/);
                if (match) {
                    hitCondition = match[1] || '==';
                    hitValue = parseInt(match[2]);
                }
                else {
                    const vscodeBreakpoint = {
                        verified: false,
                        // id: this._nextId++,
                        message: 'Invalid hit condition. Specify a number, optionally prefixed with one of the operators >= (default), == or %',
                    };
                    return vscodeBreakpoint;
                }
            }
            const xdebugBreakpoint = new xdebug.CallBreakpoint(functionBreakpoint.name, functionBreakpoint.condition, hitCondition, hitValue);
            const vscodeBreakpoint = {
                verified: this.listeners('add').length === 0,
                id: this._nextId++,
            };
            this._callBreakpoints.set(vscodeBreakpoint.id, xdebugBreakpoint);
            return vscodeBreakpoint;
        });
        toAdd = this._callBreakpoints;
        if (toRemove.length > 0) {
            this.emit('remove', toRemove);
        }
        if (toAdd.size > 0) {
            this.emit('add', toAdd);
        }
        return vscodeBreakpoints;
    }
    process() {
        // this will trigger a process on all adapters
        this.emit('process');
    }
    getAll() {
        const toAdd = new Map();
        for (const [_, lbp] of this._lineBreakpoints) {
            for (const [id, bp] of lbp) {
                toAdd.set(id, bp);
            }
        }
        for (const [id, bp] of this._exceptionBreakpoints) {
            toAdd.set(id, bp);
        }
        for (const [id, bp] of this._callBreakpoints) {
            toAdd.set(id, bp);
        }
        return toAdd;
    }
}
exports.BreakpointManager = BreakpointManager;
/**
 * Listens to changes from BreakpointManager and delivers them their own Xdebug Connection.
 * If DBGp connection is busy, track changes locally.
 */
class BreakpointAdapter extends events_1.EventEmitter {
    constructor(connection, breakpointManager) {
        super();
        this._map = new Map();
        this._queue = [];
        this._executing = false;
        this._add = (breakpoints) => {
            breakpoints.forEach((xbp, id) => {
                this._queue.push(() => this._map.set(id, { xdebugBreakpoint: xbp, state: 'add' }));
            });
        };
        this._remove = (breakpointIds) => {
            breakpointIds.forEach(id => {
                this._queue.push(() => {
                    if (this._map.has(id)) {
                        const bp = this._map.get(id);
                        if (!bp.xdebugId) {
                            // has not been set
                            this._map.delete(id);
                            return;
                        }
                        bp.state = 'remove';
                    }
                });
            });
        };
        this._notify = (notify) => {
            if (notify.breakpoint.resolved === 'resolved' &&
                (notify.breakpoint instanceof xdebug.LineBreakpoint ||
                    notify.breakpoint instanceof xdebug.ConditionalBreakpoint)) {
                Array.from(this._map.entries())
                    .filter(([id, abp]) => abp.xdebugId === notify.breakpoint.id)
                    .map(([id, abp]) => {
                    this.emit('dapEvent', new vscode.BreakpointEvent('changed', {
                        id: id,
                        verified: true,
                        line: notify.breakpoint.line,
                    }));
                });
            }
        };
        this.process = () => {
            if (this._executing) {
                return this._processPromise;
            }
            this._processPromise = this.__process();
            return this._processPromise;
        };
        this.__process = async () => {
            if (this._executing) {
                // Protect from re-entry
                return;
            }
            try {
                // Protect from re-entry
                this._executing = true;
                // first execute all map modifying operations
                while (this._queue.length > 0) {
                    const f = this._queue.shift();
                    f();
                }
                // do not execute network operations until network channel available
                if (this._connection.isPendingExecuteCommand) {
                    return;
                }
                for (const [id, abp] of this._map) {
                    if (abp.state === 'remove') {
                        try {
                            await this._connection.sendBreakpointRemoveCommand(abp.xdebugId);
                        }
                        catch (err) {
                            this.emit('dapEvent', new vscode.OutputEvent(util.inspect(err) + '\n'));
                        }
                        this._map.delete(id);
                    }
                }
                for (const [id, abp] of this._map) {
                    if (abp.state === 'add') {
                        try {
                            const ret = await this._connection.sendBreakpointSetCommand(abp.xdebugBreakpoint);
                            this._map.set(id, { xdebugId: ret.breakpointId, state: '' });
                            const extra = {};
                            if (ret.resolved === 'resolved' &&
                                (abp.xdebugBreakpoint.type === 'line' || abp.xdebugBreakpoint.type === 'conditional')) {
                                const bp = await this._connection.sendBreakpointGetCommand(ret.breakpointId);
                                extra.line = bp.breakpoint.line;
                            }
                            // TODO copy original breakpoint object
                            this.emit('dapEvent', new vscode.BreakpointEvent('changed', {
                                id: id,
                                verified: ret.resolved !== 'unresolved',
                                ...extra,
                            }));
                        }
                        catch (err) {
                            this.emit('dapEvent', new vscode.OutputEvent(util.inspect(err) + '\n'));
                            // TODO copy original breakpoint object
                            this.emit('dapEvent', new vscode.BreakpointEvent('changed', {
                                id: id,
                                verified: false,
                                message: err.message,
                            }));
                        }
                    }
                }
            }
            catch (error) {
                this.emit('dapEvent', new vscode.OutputEvent(util.inspect(error) + '\n'));
            }
            finally {
                this._executing = false;
            }
            // If there were any concurrent changes to the op-queue, rerun processing right away
            if (this._queue.length > 0) {
                return await this.__process();
            }
        };
        this._connection = connection;
        this._breakpointManager = breakpointManager;
        this._add(breakpointManager.getAll());
        // listeners
        this._breakpointManager.on('add', this._add);
        this._breakpointManager.on('remove', this._remove);
        // eslint-disable-next-line @typescript-eslint/no-misused-promises
        this._breakpointManager.on('process', this.process);
        this._connection.on('close', (error) => {
            this._breakpointManager.off('add', this._add);
            this._breakpointManager.off('remove', this._remove);
            // eslint-disable-next-line @typescript-eslint/no-misused-promises
            this._breakpointManager.off('process', this.process);
        });
        this._connection.on('notify_breakpoint_resolved', this._notify);
    }
}
exports.BreakpointAdapter = BreakpointAdapter;
//# sourceMappingURL=breakpoints.js.map