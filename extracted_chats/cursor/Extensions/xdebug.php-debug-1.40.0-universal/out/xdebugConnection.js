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
exports.Connection = exports.FeatureGetResponse = exports.FeatureSetResponse = exports.EvalResponse = exports.EvalResultProperty = exports.PropertyValueResponse = exports.PropertyGetNameResponse = exports.PropertyGetResponse = exports.ContextGetResponse = exports.SyntheticProperty = exports.Property = exports.BaseProperty = exports.ContextNamesResponse = exports.Context = exports.SourceResponse = exports.StackGetResponse = exports.StackFrame = exports.BreakpointGetResponse = exports.BreakpointListResponse = exports.BreakpointSetResponse = exports.ConditionalBreakpoint = exports.ExceptionBreakpoint = exports.CallBreakpoint = exports.LineBreakpoint = exports.Breakpoint = exports.Stream = exports.UserNotify = exports.BreakpointResolvedNotify = exports.Notify = exports.StatusResponse = exports.Response = exports.XdebugError = exports.InitPacket = void 0;
const iconv = __importStar(require("iconv-lite"));
const dbgp_1 = require("./dbgp");
const dbgp_2 = require("./dbgp");
/** The first packet we receive from Xdebug. Returned by waitForInitPacket() */
class InitPacket {
    /**
     * @param  {XMLDocument} document - An XML document to read from
     * @param  {Connection} connection
     */
    constructor(document, connection) {
        var _a, _b, _c, _d, _e, _f;
        const documentElement = document.documentElement;
        this.fileUri = documentElement.getAttribute('fileuri');
        this.language = documentElement.getAttribute('language');
        this.protocolVersion = documentElement.getAttribute('protocol_version');
        this.ideKey = documentElement.getAttribute('idekey');
        this.engineVersion = (_b = (_a = documentElement.getElementsByTagName('engine').item(0)) === null || _a === void 0 ? void 0 : _a.getAttribute('version')) !== null && _b !== void 0 ? _b : '';
        this.engineName = (_d = (_c = documentElement.getElementsByTagName('engine').item(0)) === null || _c === void 0 ? void 0 : _c.textContent) !== null && _d !== void 0 ? _d : '';
        this.appid = (_e = documentElement.getAttribute('appid')) !== null && _e !== void 0 ? _e : '';
        this.ctrlSocket = (_f = documentElement.getAttributeNS('https://xdebug.org/dbgp/xdebug', 'ctrl_socket')) !== null && _f !== void 0 ? _f : '';
        this.connection = connection;
    }
}
exports.InitPacket = InitPacket;
/** Error class for errors returned from Xdebug */
class XdebugError extends Error {
    constructor(message, code) {
        super(message);
        this.code = code;
        this.message = message;
        this.name = 'XdebugError';
    }
}
exports.XdebugError = XdebugError;
/** The base class for all Xdebug responses to commands executed on a connection */
class Response {
    /**
     * constructs a new Response object from an XML document.
     * If there is an error child node, an exception is thrown with the appropriate code and message.
     *
     * @param  {XMLDocument} document - An XML document to read from
     * @param  {Connection} connection
     */
    constructor(document, connection) {
        const documentElement = document.documentElement;
        if (documentElement.firstChild && documentElement.firstChild.nodeName === 'error') {
            const errorNode = documentElement.firstChild;
            const code = parseInt(errorNode.getAttribute('code'));
            const message = errorNode.textContent;
            throw new XdebugError(message, code);
        }
        this.transactionId = parseInt(documentElement.getAttribute('transaction_id'));
        this.command = documentElement.getAttribute('command');
        this.connection = connection;
    }
}
exports.Response = Response;
/** A response to the status command */
class StatusResponse extends Response {
    constructor(document, connection) {
        super(document, connection);
        const documentElement = document.documentElement;
        this.status = documentElement.getAttribute('status');
        this.reason = documentElement.getAttribute('reason');
        if (documentElement.hasChildNodes()) {
            const messageNode = documentElement.firstChild;
            if (messageNode.hasAttribute('exception')) {
                this.exception = {
                    name: messageNode.getAttribute('exception'),
                    message: messageNode.textContent,
                };
                if (messageNode.hasAttribute('code')) {
                    this.exception.code = parseInt(messageNode.getAttribute('code'));
                }
            }
            if (messageNode.hasAttribute('filename')) {
                this.fileUri = messageNode.getAttribute('filename');
            }
            if (messageNode.hasAttribute('lineno')) {
                this.line = parseInt(messageNode.getAttribute('lineno'));
            }
        }
    }
}
exports.StatusResponse = StatusResponse;
/** Abstract base class for all notify packets */
class Notify {
    /** dynamically detects the type of notify and returns the appropriate object */
    static fromXml(document, connection) {
        switch (document.documentElement.getAttribute('name')) {
            case 'breakpoint_resolved':
                return new BreakpointResolvedNotify(document, connection);
            case 'user':
                return new UserNotify(document, connection);
            default:
                return new Notify(document);
        }
    }
    /** Constructs a notify object from an XML node from a Xdebug response */
    constructor(document) {
        this.name = document.documentElement.getAttribute('name');
    }
}
exports.Notify = Notify;
/** Class for breakpoint_resolved notify */
class BreakpointResolvedNotify extends Notify {
    /** Constructs a notify object from an XML node from a Xdebug response */
    constructor(document, connection) {
        super(document);
        this.breakpoint = Breakpoint.fromXml(document.documentElement.firstChild, connection);
    }
}
exports.BreakpointResolvedNotify = BreakpointResolvedNotify;
/** Class for user notify */
class UserNotify extends Notify {
    /** Constructs a notify object from an XML node from a Xdebug response */
    constructor(document, connection) {
        super(document);
        if (document.documentElement.hasChildNodes()) {
            const properties = document.documentElement.getElementsByTagName('property');
            if (properties.length > 0) {
                this.property = new EvalResultProperty(properties[0]);
                // Name is required by DAP, but user notify does not set it
                this.property.name = '';
            }
            const locations = document.documentElement.getElementsByTagName('xdebug:location');
            if (locations.length > 0) {
                this.line = parseInt(locations[0].getAttribute('lineno'));
                this.fileUri = locations[0].getAttribute('filename');
            }
        }
    }
}
exports.UserNotify = UserNotify;
class Stream {
    /** Constructs a stream object from an XML node from a Xdebug response */
    constructor(document) {
        this.type = document.documentElement.getAttribute('type');
        const encoding = document.documentElement.getAttribute('encoding');
        if (encoding) {
            this.value = iconv.encode(document.documentElement.textContent, encoding).toString();
        }
        else {
            this.value = document.documentElement.textContent;
        }
    }
}
exports.Stream = Stream;
/** Abstract base class for all breakpoints */
class Breakpoint {
    /** dynamically detects the type of breakpoint and returns the appropriate object */
    static fromXml(breakpointNode, connection) {
        switch (breakpointNode.getAttribute('type')) {
            case 'exception':
                return new ExceptionBreakpoint(breakpointNode, connection);
            case 'line':
                return new LineBreakpoint(breakpointNode, connection);
            case 'conditional':
                return new ConditionalBreakpoint(breakpointNode, connection);
            case 'call':
                return new CallBreakpoint(breakpointNode, connection);
            default:
                throw new Error(`Invalid type ${breakpointNode.getAttribute('type')}`);
        }
    }
    constructor() {
        if (typeof arguments[0] === 'object') {
            // from XML
            const breakpointNode = arguments[0];
            this.connection = arguments[1];
            this.type = breakpointNode.getAttribute('type');
            this.id = parseInt(breakpointNode.getAttribute('id'));
            this.state = breakpointNode.getAttribute('state');
            this.resolved = breakpointNode.getAttribute('resolved');
            if (breakpointNode.hasAttribute('hit_condition')) {
                this.hitCondition = breakpointNode.getAttribute('hit_condition');
            }
            if (breakpointNode.hasAttribute('hit_value')) {
                this.hitValue = parseInt(breakpointNode.getAttribute('hit_value'));
            }
        }
        else {
            this.type = arguments[0];
            this.hitCondition = arguments[1];
            this.hitValue = arguments[2];
        }
    }
    /** Removes the breakpoint by sending a breakpoint_remove command */
    remove() {
        return this.connection.sendBreakpointRemoveCommand(this);
    }
}
exports.Breakpoint = Breakpoint;
/** class for line breakpoints. Returned from a breakpoint_list or passed to sendBreakpointSetCommand */
class LineBreakpoint extends Breakpoint {
    constructor() {
        if (typeof arguments[0] === 'object') {
            const breakpointNode = arguments[0];
            const connection = arguments[1];
            super(breakpointNode, connection);
            this.line = parseInt(breakpointNode.getAttribute('lineno'));
            this.fileUri = breakpointNode.getAttribute('filename');
        }
        else {
            // construct from arguments
            super('line', arguments[2], arguments[3]);
            this.fileUri = arguments[0];
            this.line = arguments[1];
        }
    }
}
exports.LineBreakpoint = LineBreakpoint;
/** class for call breakpoints. Returned from a breakpoint_list or passed to sendBreakpointSetCommand */
class CallBreakpoint extends Breakpoint {
    constructor() {
        if (typeof arguments[0] === 'object') {
            const breakpointNode = arguments[0];
            const connection = arguments[1];
            super(breakpointNode, connection);
            this.fn = breakpointNode.getAttribute('function');
            this.expression = breakpointNode.getAttribute('expression'); // Base64 encoded?
        }
        else {
            // construct from arguments
            super('call', arguments[2], arguments[3]);
            this.fn = arguments[0];
            this.expression = arguments[1];
        }
    }
}
exports.CallBreakpoint = CallBreakpoint;
/** class for exception breakpoints. Returned from a breakpoint_list or passed to sendBreakpointSetCommand */
class ExceptionBreakpoint extends Breakpoint {
    constructor() {
        if (typeof arguments[0] === 'object') {
            // from XML
            const breakpointNode = arguments[0];
            const connection = arguments[1];
            super(breakpointNode, connection);
            this.exception = breakpointNode.getAttribute('exception');
        }
        else {
            // from arguments
            super('exception', arguments[1], arguments[2]);
            this.exception = arguments[0];
        }
    }
}
exports.ExceptionBreakpoint = ExceptionBreakpoint;
/** class for conditional breakpoints. Returned from a breakpoint_list or passed to sendBreakpointSetCommand */
class ConditionalBreakpoint extends Breakpoint {
    constructor() {
        if (typeof arguments[0] === 'object') {
            // from XML
            const breakpointNode = arguments[0];
            const connection = arguments[1];
            super(breakpointNode, connection);
            this.expression = breakpointNode.getAttribute('expression'); // Base64 encoded?
        }
        else {
            // from arguments
            super('conditional', arguments[3], arguments[4]);
            this.expression = arguments[0];
            this.fileUri = arguments[1];
            this.line = arguments[2];
        }
    }
}
exports.ConditionalBreakpoint = ConditionalBreakpoint;
/** Response to a breakpoint_set command */
class BreakpointSetResponse extends Response {
    constructor(document, connection) {
        super(document, connection);
        this.breakpointId = parseInt(document.documentElement.getAttribute('id'));
        this.resolved = document.documentElement.getAttribute('resolved');
    }
}
exports.BreakpointSetResponse = BreakpointSetResponse;
/** The response to a breakpoint_list command */
class BreakpointListResponse extends Response {
    /**
     * @param  {XMLDocument} document
     * @param  {Connection} connection
     */
    constructor(document, connection) {
        super(document, connection);
        this.breakpoints = Array.from(document.documentElement.childNodes).map((breakpointNode) => Breakpoint.fromXml(breakpointNode, connection));
    }
}
exports.BreakpointListResponse = BreakpointListResponse;
/** The response to a breakpoint_get command */
class BreakpointGetResponse extends Response {
    /**
     * @param  {XMLDocument} document
     * @param  {Connection} connection
     */
    constructor(document, connection) {
        super(document, connection);
        this.breakpoint = Breakpoint.fromXml(document.documentElement.firstChild, connection);
    }
}
exports.BreakpointGetResponse = BreakpointGetResponse;
/** One stackframe inside a stacktrace retrieved through stack_get */
class StackFrame {
    /**
     * @param  {Element} stackFrameNode
     * @param  {Connection} connection
     */
    constructor(stackFrameNode, connection) {
        this.name = stackFrameNode.getAttribute('where');
        this.fileUri = stackFrameNode.getAttribute('filename');
        this.type = stackFrameNode.getAttribute('type');
        this.line = parseInt(stackFrameNode.getAttribute('lineno'));
        this.level = parseInt(stackFrameNode.getAttribute('level'));
        this.connection = connection;
    }
    /** Returns the available contexts (scopes, such as "Local" and "Superglobals") by doing a context_names command */
    async getContexts() {
        return (await this.connection.sendContextNamesCommand(this)).contexts;
    }
}
exports.StackFrame = StackFrame;
/** The response to a stack_get command */
class StackGetResponse extends Response {
    /**
     * @param  {XMLDocument} document
     * @param  {Connection} connection
     */
    constructor(document, connection) {
        super(document, connection);
        this.stack = Array.from(document.documentElement.childNodes).map((stackFrameNode) => new StackFrame(stackFrameNode, connection));
    }
}
exports.StackGetResponse = StackGetResponse;
class SourceResponse extends Response {
    constructor(document, connection) {
        super(document, connection);
        this.source = Buffer.from(document.documentElement.textContent, 'base64').toString();
    }
}
exports.SourceResponse = SourceResponse;
/** A context inside a stack frame, like "Local" or "Superglobals" */
class Context {
    /**
     * @param  {Element} contextNode
     * @param  {StackFrame} stackFrame
     */
    constructor(contextNode, stackFrame) {
        this.id = parseInt(contextNode.getAttribute('id'));
        this.name = contextNode.getAttribute('name');
        this.stackFrame = stackFrame;
    }
    /**
     * Returns the properties (variables) inside this context by doing a context_get command
     *
     * @returns Promise.<Property[]>
     */
    async getProperties() {
        return (await this.stackFrame.connection.sendContextGetCommand(this)).properties;
    }
}
exports.Context = Context;
/** Response to a context_names command */
class ContextNamesResponse extends Response {
    /**
     * @param  {XMLDocument} document
     * @param  {StackFrame} stackFrame
     */
    constructor(document, stackFrame) {
        super(document, stackFrame.connection);
        this.contexts = Array.from(document.documentElement.childNodes).map((contextNode) => new Context(contextNode, stackFrame));
    }
}
exports.ContextNamesResponse = ContextNamesResponse;
/** The parent for properties inside a scope and properties retrieved through eval requests */
class BaseProperty {
    constructor(propertyNode) {
        var _a;
        if (propertyNode === null) {
            return;
        }
        if (propertyNode.hasAttribute('name')) {
            this.name = propertyNode.getAttribute('name');
        }
        else if (propertyNode.getElementsByTagName('name').length > 0) {
            this.name = decodeTag(propertyNode, 'name');
        }
        this.type = propertyNode.getAttribute('type');
        if (propertyNode.hasAttribute('classname')) {
            this.class = propertyNode.getAttribute('classname');
        }
        else if (propertyNode.getElementsByTagName('classname').length > 0) {
            this.class = decodeTag(propertyNode, 'classname');
        }
        if (propertyNode.hasAttribute('facet')) {
            this.facets = propertyNode.getAttribute('facet').split(' ');
        }
        if (propertyNode.hasAttribute('size')) {
            this.size = parseInt((_a = propertyNode.getAttribute('size')) !== null && _a !== void 0 ? _a : '0');
        }
        this.hasChildren = !!parseInt(propertyNode.getAttribute('children'));
        if (this.hasChildren) {
            this.numberOfChildren = parseInt(propertyNode.getAttribute('numchildren'));
        }
        else {
            if (propertyNode.getElementsByTagName('value').length > 0) {
                this.value = decodeTag(propertyNode, 'value');
            }
            else {
                const encoding = propertyNode.getAttribute('encoding');
                if (encoding) {
                    this.value = iconv.encode(propertyNode.textContent, encoding).toString();
                }
                else {
                    this.value = propertyNode.textContent;
                }
            }
        }
    }
}
exports.BaseProperty = BaseProperty;
/** a property (variable) inside a context or a child of another property */
class Property extends BaseProperty {
    /**
     * @param  {Element} propertyNode
     * @param  {Context} context
     */
    constructor(propertyNode, context) {
        super(propertyNode);
        if (propertyNode.hasAttribute('fullname')) {
            this.fullName = propertyNode.getAttribute('fullname');
        }
        else if (propertyNode.getElementsByTagName('fullname').length > 0) {
            this.fullName = decodeTag(propertyNode, 'fullname');
        }
        this.context = context;
        if (this.hasChildren) {
            this.children = Array.from(propertyNode.childNodes)
                .filter(node => node.nodeName === 'property')
                .map((propertyNode) => new Property(propertyNode, context));
        }
    }
    /**
     * Sets the value of this property through a property_set command
     */
    set(value) {
        return this.context.stackFrame.connection.sendPropertySetCommand(this, value);
    }
    /**
     * Returns the child properties of this property by doing another property_get
     *
     * @returns Promise.<Property[]>
     */
    async getChildren(page = 0) {
        if (!this.hasChildren) {
            throw new Error('This property has no children');
        }
        return (await this.context.stackFrame.connection.sendPropertyGetCommand(this, page)).children;
    }
}
exports.Property = Property;
function decodeTag(propertyNode, tagName) {
    const tag = propertyNode.getElementsByTagName(tagName).item(0);
    const encoding = tag.getAttribute('encoding');
    if (encoding) {
        return iconv.encode(tag.textContent, encoding).toString();
    }
    else {
        return tag.textContent;
    }
}
class SyntheticProperty extends BaseProperty {
    constructor(name, type, value, children) {
        super(null);
        this.name = name;
        this.type = type;
        this.value = value;
        this.hasChildren = children.length > 0;
        this.numberOfChildren = children.length;
        this.children = children;
    }
}
exports.SyntheticProperty = SyntheticProperty;
/** The response to a context_get command */
class ContextGetResponse extends Response {
    /**
     * @param  {XMLDocument} document
     * @param  {Context} context
     */
    constructor(document, context) {
        super(document, context.stackFrame.connection);
        this.properties = Array.from(document.documentElement.childNodes).map((propertyNode) => new Property(propertyNode, context));
    }
}
exports.ContextGetResponse = ContextGetResponse;
/** The response to a property_get command */
class PropertyGetResponse extends Response {
    /**
     * @param  {XMLDocument} document
     * @param  {Context} context
     */
    constructor(document, context) {
        super(document, context.stackFrame.connection);
        this.children = Array.from(document.documentElement.firstChild.childNodes)
            .filter(node => node.nodeName === 'property')
            .map((propertyNode) => new Property(propertyNode, context));
    }
}
exports.PropertyGetResponse = PropertyGetResponse;
/** The response to a property_get by name command */
class PropertyGetNameResponse extends Response {
    /**
     * @param  {XMLDocument} document
     * @param  {Context} context
     */
    constructor(document, context) {
        super(document, context.stackFrame.connection);
        this.property = new Property(document.documentElement.firstChild, context);
    }
}
exports.PropertyGetNameResponse = PropertyGetNameResponse;
/** The response to a property_value command */
class PropertyValueResponse extends Response {
    constructor(document, connection) {
        var _a, _b;
        super(document, connection);
        if (document.documentElement.hasAttribute('size')) {
            this.size = parseInt((_a = document.documentElement.getAttribute('size')) !== null && _a !== void 0 ? _a : '0');
        }
        this.type = (_b = document.documentElement.getAttribute('type')) !== null && _b !== void 0 ? _b : '';
        if (document.documentElement.getElementsByTagName('value').length > 0) {
            this.value = decodeTag(document.documentElement, 'value');
        }
        else {
            const encoding = document.documentElement.getAttribute('encoding');
            if (encoding) {
                this.value = iconv.encode(document.documentElement.textContent, encoding).toString();
            }
            else {
                this.value = document.documentElement.textContent;
            }
        }
    }
}
exports.PropertyValueResponse = PropertyValueResponse;
/** class for properties returned from eval commands. These don't have a full name or an ID, but have all children already inlined. */
class EvalResultProperty extends BaseProperty {
    constructor(propertyNode) {
        super(propertyNode);
        if (this.hasChildren) {
            this.children = Array.from(propertyNode.childNodes)
                .filter(node => node.nodeName === 'property')
                .map((propertyNode) => new EvalResultProperty(propertyNode));
        }
    }
}
exports.EvalResultProperty = EvalResultProperty;
/** The response to an eval command */
class EvalResponse extends Response {
    constructor(document, connection) {
        super(document, connection);
        if (document.documentElement.hasChildNodes()) {
            this.result = new EvalResultProperty(document.documentElement.firstChild);
        }
    }
}
exports.EvalResponse = EvalResponse;
/** The response to a feature_set command */
class FeatureSetResponse extends Response {
    constructor(document, connection) {
        super(document, connection);
        this.feature = document.documentElement.getAttribute('feature');
    }
}
exports.FeatureSetResponse = FeatureSetResponse;
/** The response to a feature_get command */
class FeatureGetResponse extends Response {
    constructor(document, connection) {
        super(document, connection);
        this.feature = document.documentElement.getAttribute('feature');
        this.supported = document.documentElement.getAttribute('supported');
    }
}
exports.FeatureGetResponse = FeatureGetResponse;
/**
 * Escapes a value to pass it as an argument in an XDebug command
 */
function escape(value) {
    return '"' + value.replace(/("|\\)/g, '\\$1') + '"';
}
/**
 * This class represents a connection to Xdebug and is instantiated with a socket.
 */
class Connection extends dbgp_1.DbgpConnection {
    /**
     * Whether a command was started that executes PHP, which means the connection will be blocked from
     * running any additional commands until the execution gets to the next stopping point or exits.
     */
    get isPendingExecuteCommand() {
        return this._pendingExecuteCommand;
    }
    featureSet(feature) {
        return this._featureSet.get(feature);
    }
    /** Constructs a new connection that uses the given socket to communicate with Xdebug. */
    constructor(socket) {
        super(socket);
        /** a counter for unique transaction IDs. */
        this._transactionCounter = 1;
        /**
         * a map from transaction IDs to pending commands that have been sent to Xdebug and are awaiting a response.
         * This should in theory only contain max one element at any time.
         */
        this._pendingCommands = new Map();
        /**
         * Xdebug does NOT support async communication.
         * This means before sending a new command, we have to wait until we get a response for the previous.
         * This array is a stack of commands that get passed to _sendCommand once Xdebug can accept commands again.
         */
        this._commandQueue = [];
        this._pendingExecuteCommand = false;
        this._featureSet = new Map();
        this.id = Connection._connectionCounter++;
        this.timeEstablished = new Date();
        this._initPromise = new Promise((resolve, reject) => {
            this._initPromiseResolveFn = resolve;
            this._initPromiseRejectFn = reject;
        });
        this.on('message', (response) => {
            if (response.documentElement.nodeName === 'init') {
                this._initPromiseResolveFn(new InitPacket(response, this));
            }
            else if (response.documentElement.nodeName === 'notify') {
                const n = Notify.fromXml(response, this);
                this.emit('notify_' + n.name, n);
            }
            else if (response.documentElement.nodeName === 'stream') {
                const s = new Stream(response);
                this.emit('stream', s);
            }
            else {
                const transactionId = parseInt(response.documentElement.getAttribute('transaction_id'));
                if (this._pendingCommands.has(transactionId)) {
                    const command = this._pendingCommands.get(transactionId);
                    this._pendingCommands.delete(transactionId);
                    this._pendingExecuteCommand = false;
                    command.resolveFn(response);
                }
                if (this._commandQueue.length > 0) {
                    const command = this._commandQueue.shift();
                    this._executeCommand(command).catch(command.rejectFn);
                }
            }
        });
        this.on('close', () => {
            this._pendingCommands.forEach(command => command.rejectFn(new Error('connection closed (on close)')));
            this._pendingCommands.clear();
            this._commandQueue.forEach(command => command.rejectFn(new Error('connection closed (on close)')));
            this._commandQueue = [];
            this._initPromiseRejectFn(new Error('connection closed (on close)'));
        });
    }
    /** Returns a promise that gets resolved once the init packet arrives */
    waitForInitPacket() {
        return this._initPromise;
    }
    /**
     * Pushes a new command to the queue that will be executed after all the previous commands have finished and we received a response.
     * If the queue is empty AND there are no pending transactions (meaning we already received a response and Xdebug is waiting for
     * commands) the command will be executed immediately.
     */
    _enqueueCommand(name, args, data) {
        return new Promise((resolveFn, rejectFn) => {
            this._enqueue({ name, args, data, resolveFn, rejectFn, isExecuteCommand: false });
        });
    }
    /**
     * Pushes a new execute command (one that results in executing PHP code) to the queue that will be executed after all the previous
     * commands have finished and we received a response.
     * If the queue is empty AND there are no pending transactions (meaning we already received a response and Xdebug is waiting for
     * commands) the command will be executed immediately.
     */
    _enqueueExecuteCommand(name, args, data) {
        return new Promise((resolveFn, rejectFn) => {
            this._enqueue({ name, args, data, resolveFn, rejectFn, isExecuteCommand: true });
        });
    }
    /** Adds the given command to the queue, or executes immediately if no commands are currently being processed. */
    _enqueue(command) {
        if (this._commandQueue.length === 0 && this._pendingCommands.size === 0) {
            this._executeCommand(command).catch(command.rejectFn);
        }
        else {
            this._commandQueue.push(command);
        }
    }
    /**
     * Sends a command to Xdebug with a new transaction ID and calls the callback on the command. This can
     * only be called when Xdebug can actually accept commands, which is after we received a response for the
     * previous command.
     */
    async _executeCommand(command) {
        const transactionId = this._transactionCounter++;
        let commandString = `${command.name} -i ${transactionId}`;
        if (command.args) {
            commandString += ' ' + command.args;
        }
        if (command.data) {
            commandString += ' -- ' + Buffer.from(command.data).toString('base64');
        }
        commandString += '\0';
        const data = iconv.encode(commandString, dbgp_2.ENCODING);
        this._pendingCommands.set(transactionId, command);
        this._pendingExecuteCommand = command.isExecuteCommand;
        await this.write(data);
    }
    close() {
        this._commandQueue = [];
        this._initPromiseRejectFn(new Error('connection closed (close)'));
        return super.close();
    }
    // ------------------------ status --------------------------------------------
    /** Sends a status command */
    async sendStatusCommand() {
        return new StatusResponse(await this._enqueueCommand('status'), this);
    }
    // ------------------------ feature negotiation --------------------------------
    /**
     * Sends a feature_get command
     * feature can be one of
     *  - language_supports_threads
     *  - language_name
     *  - language_version
     *  - encoding
     *  - protocol_version
     *  - supports_async
     *  - data_encoding
     *  - breakpoint_languages
     *  - breakpoint_types
     *  - multiple_sessions
     *  - max_children
     *  - max_data
     *  - max_depth
     *  - extended_properties
     * optional features:
     *  - supports_postmortem
     *  - show_hidden
     *  - notify_ok
     * or any command.
     */
    async sendFeatureGetCommand(feature) {
        return new FeatureGetResponse(await this._enqueueCommand('feature_get', `-n ${feature}`), this);
    }
    /**
     * Sends a feature_set command
     * feature can be one of
     *  - multiple_sessions
     *  - max_children
     *  - max_data
     *  - max_depth
     *  - extended_properties
     * optional features:
     *  - show_hidden
     *  - notify_ok
     */
    async sendFeatureSetCommand(feature, value) {
        const res = new FeatureSetResponse(await this._enqueueCommand('feature_set', `-n ${feature} -v ${value}`), this);
        this._featureSet.set(feature, value);
        return res;
    }
    // ---------------------------- breakpoints ------------------------------------
    /**
     * Sends a breakpoint_set command that sets a breakpoint.
     *
     * @param {Breakpoint} breakpoint - an instance of LineBreakpoint, ConditionalBreakpoint or ExceptionBreakpoint
     * @returns Promise.<BreakpointSetResponse>
     */
    async sendBreakpointSetCommand(breakpoint) {
        let args = `-t ${breakpoint.type}`;
        let data;
        if (breakpoint instanceof LineBreakpoint) {
            args += ` -f ${breakpoint.fileUri} -n ${breakpoint.line}`;
        }
        else if (breakpoint instanceof ExceptionBreakpoint) {
            args += ` -x ${breakpoint.exception}`;
        }
        else if (breakpoint instanceof ConditionalBreakpoint) {
            args += ` -f ${breakpoint.fileUri}`;
            if (typeof breakpoint.line === 'number') {
                args += ` -n ${breakpoint.line}`;
            }
            data = breakpoint.expression;
        }
        else if (breakpoint instanceof CallBreakpoint) {
            args += ` -m ${breakpoint.fn}`;
            data = breakpoint.expression;
        }
        if (breakpoint.hitCondition) {
            args += ` -o ${breakpoint.hitCondition}`;
        }
        if (breakpoint.hitValue) {
            args += ` -h ${breakpoint.hitValue}`;
        }
        return new BreakpointSetResponse(await this._enqueueCommand('breakpoint_set', args, data), this);
    }
    /** sends a breakpoint_list command */
    async sendBreakpointListCommand() {
        return new BreakpointListResponse(await this._enqueueCommand('breakpoint_list'), this);
    }
    /**
     * Sends a breakpoint_get command
     *
     * @param {Breakpoint|number} breakpoint - an instance or id of a breakpoint
     * @returns Promise.<BreakpointGetResponse>
     */
    async sendBreakpointGetCommand(breakpoint) {
        let breakpointId;
        if (typeof breakpoint === 'number') {
            breakpointId = breakpoint;
        }
        else {
            breakpointId = breakpoint.id;
        }
        return new BreakpointGetResponse(await this._enqueueCommand('breakpoint_get', `-d ${breakpointId}`), this);
    }
    /** sends a breakpoint_remove command */
    async sendBreakpointRemoveCommand(breakpoint) {
        let breakpointId;
        if (typeof breakpoint === 'number') {
            breakpointId = breakpoint;
        }
        else {
            breakpointId = breakpoint.id;
        }
        return new Response(await this._enqueueCommand('breakpoint_remove', `-d ${breakpointId}`), this);
    }
    // ----------------------------- continuation ---------------------------------
    /** sends a run command */
    async sendRunCommand() {
        return new StatusResponse(await this._enqueueExecuteCommand('run'), this);
    }
    /** sends a step_into command */
    async sendStepIntoCommand() {
        return new StatusResponse(await this._enqueueExecuteCommand('step_into'), this);
    }
    /** sends a step_over command */
    async sendStepOverCommand() {
        return new StatusResponse(await this._enqueueExecuteCommand('step_over'), this);
    }
    /** sends a step_out command */
    async sendStepOutCommand() {
        return new StatusResponse(await this._enqueueExecuteCommand('step_out'), this);
    }
    /** sends a stop command */
    async sendStopCommand() {
        return new StatusResponse(await this._enqueueCommand('stop'), this);
    }
    // ------------------------------ stack ----------------------------------------
    /** Sends a stack_get command */
    async sendStackGetCommand() {
        return new StackGetResponse(await this._enqueueCommand('stack_get'), this);
    }
    async sendSourceCommand(uri) {
        return new SourceResponse(await this._enqueueCommand('source', `-f ${uri}`), this);
    }
    // ------------------------------ context --------------------------------------
    /** Sends a context_names command. */
    async sendContextNamesCommand(stackFrame) {
        return new ContextNamesResponse(await this._enqueueCommand('context_names', `-d ${stackFrame.level}`), stackFrame);
    }
    /** Sends a context_get command */
    async sendContextGetCommand(context) {
        return new ContextGetResponse(await this._enqueueCommand('context_get', `-d ${context.stackFrame.level} -c ${context.id}`), context);
    }
    // ------------------------------ property --------------------------------------
    /** Sends a property_get command */
    async sendPropertyGetCommand(property, page = 0) {
        return new PropertyGetResponse(await this._enqueueCommand('property_get', `-d ${property.context.stackFrame.level} -c ${property.context.id} -p ${page} -n ${escape(property.fullName)}`), property.context);
    }
    /** Sends a property_get by name command */
    async sendPropertyGetNameCommand(name, context) {
        const escapedFullName = '"' + name.replace(/("|\\)/g, '\\$1') + '"';
        return new PropertyGetNameResponse(await this._enqueueCommand('property_get', `-d ${context.stackFrame.level} -c ${context.id} -n ${escapedFullName}`), context);
    }
    /** Sends a property_value by name command and request full data */
    async sendPropertyValueNameCommand(name, context) {
        const escapedFullName = '"' + name.replace(/("|\\)/g, '\\$1') + '"';
        return new PropertyValueResponse(await this._enqueueCommand('property_value', `-m 0 -d ${context.stackFrame.level} -c ${context.id} -n ${escapedFullName}`), context.stackFrame.connection);
    }
    /** Sends a property_set command */
    async sendPropertySetCommand(property, value) {
        return new Response(await this._enqueueCommand('property_set', `-d ${property.context.stackFrame.level} -c ${property.context.id} -n ${escape(property.fullName)}`, value), property.context.stackFrame.connection);
    }
    // ------------------------------- eval -----------------------------------------
    /** sends an eval command */
    async sendEvalCommand(expression) {
        return new EvalResponse(await this._enqueueCommand('eval', undefined, expression), this);
    }
    // ------------------------------ stream ----------------------------------------
    async sendStdout(mode) {
        return new Response(await this._enqueueCommand('stdout', `-c ${mode}`), this);
    }
    async sendStderr(mode) {
        return new Response(await this._enqueueCommand('stderr', `-c ${mode}`), this);
    }
}
exports.Connection = Connection;
/** a counter for unique connection IDs */
Connection._connectionCounter = 1;
//# sourceMappingURL=xdebugConnection.js.map