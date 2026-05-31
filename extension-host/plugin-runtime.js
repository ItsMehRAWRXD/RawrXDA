'use strict';

const fs = require('fs');
const path = require('path');
const vm = require('vm');
const EventEmitter = require('events');
const crypto = require('crypto');
const {
    SandboxViolation,
    parsePolicy,
} = require('./sandbox-policy');

function emitViolation(error) {
    process.stderr.write('SECURITY_VIOLATION ' + JSON.stringify({
        code: error && error.code ? error.code : 'sandbox_error',
        detail: error && (error.detail || error.message) ? (error.detail || error.message) : 'unknown',
    }) + '\n');
}

function unsupportedApi(name) {
    return async function unsupportedApiCall() {
        throw new Error('unsupported_api:' + name);
    };
}

function _denyFromResult(result, fallbackCode) {
    const code = result && result.code ? result.code : fallbackCode;
    const error = result && result.error ? result.error : code;
    throw new SandboxViolation(code, error);
}

function _createCapabilityClient(pluginId) {
    const pending = new Map();
    let seq = 0;

    function sendRequest(type, payload) {
        const requestId = `${pluginId}-${Date.now()}-${++seq}`;
        const lineageId = typeof crypto.randomUUID === 'function'
            ? crypto.randomUUID()
            : crypto.randomBytes(16).toString('hex');
        const envelope = {
            version: 1,
            kind: 'request',
            requestId,
            lineageId,
            type,
            payload,
            timestamp: new Date().toISOString(),
            source: 'plugin-runtime',
            target: 'extension-host-manager',
        };

        return new Promise((resolve, reject) => {
            pending.set(requestId, { resolve, reject });
            process.stdout.write(JSON.stringify(envelope) + '\n');
        });
    }

    return {
        pending,
        async request(type, payload) {
            const response = await sendRequest(String(type || ''), payload || {});
            if (!response.success) {
                const code = typeof response.error === 'string'
                    ? response.error
                    : (response.error && response.error.code ? response.error.code : 'capability_error');
                const detail = response.error && response.error.detail ? response.error.detail : code;
                throw new SandboxViolation(code, detail);
            }
            return response.result;
        },
    };
}

function _createLegacyRawrxdAdapter(capabilities) {
    return {
        async readTextFile(filePath) {
            const result = await capabilities.request('fs.read', { path: filePath });
            if (result && typeof result === 'object' && Object.prototype.hasOwnProperty.call(result, 'data')) {
                return result.data;
            }
            return result;
        },
        async writeTextFile(filePath, content) {
            return capabilities.request('fs.write', { path: filePath, data: content });
        },
        async httpRequest(rawUrl) {
            return capabilities.request('net.request', { url: rawUrl, method: 'GET' });
        },
        async spawnProcess(command, args) {
            return capabilities.request('process.exec', { command, args: Array.isArray(args) ? args : [] });
        },
        async requireModule(name) {
            return capabilities.request('addon.load', { name });
        },
    };
}

function _createLegacyVscodeAdapter(capabilities) {
    const commandRegistry = new Map();
    return {
        version: '1.90.0-rawrxd-day9-slice-capability-abi',
        commands: {
            registerCommand(commandId, handler) {
                if (typeof commandId !== 'string' || !commandId) {
                    throw new Error('invalid_command_id');
                }
                if (typeof handler !== 'function') {
                    throw new Error('invalid_command_handler');
                }
                commandRegistry.set(commandId, handler);
                return {
                    dispose() {
                        commandRegistry.delete(commandId);
                    },
                };
            },
            async executeCommand(commandId, ...args) {
                if (!commandRegistry.has(commandId)) {
                    const result = await capabilities.request('cmd.execute', { command: commandId, args });
                    if (result && result.handledBy) {
                        return result;
                    }
                    throw new Error('unsupported_or_unregistered_command:' + commandId);
                }
                return commandRegistry.get(commandId)(...args);
            },
        },
        workspace: {
            fs: {
                async readFile(uriOrPath) {
                    const result = await capabilities.request('fs.read', { path: String(uriOrPath) });
                    const content = result && typeof result === 'object' && Object.prototype.hasOwnProperty.call(result, 'data')
                        ? result.data
                        : result;
                    return Buffer.from(String(content || ''), 'utf8');
                },
                async writeFile(uriOrPath, data) {
                    const text = Buffer.isBuffer(data)
                        ? data.toString('utf8')
                        : String(data || '');
                    return capabilities.request('fs.write', { path: String(uriOrPath), data: text });
                },
            },
        },
        window: {
            async showInformationMessage(message) {
                const result = await capabilities.request('ui.notify', { message: String(message || ''), level: 'info' });
                return result && result.message ? result.message : String(message || '');
            },
        },
        debug: {
            startDebugging: unsupportedApi('debug.startDebugging'),
        },
    };
}

function _readJsonEnv(name, fallback) {
    const raw = process.env[name];
    if (!raw) return fallback;
    try {
        return JSON.parse(raw);
    } catch {
        return fallback;
    }
}

function _writeResponse(request, success, result, error) {
    process.stdout.write(JSON.stringify({
        version: 1,
        kind: 'response',
        requestId: request && request.requestId ? request.requestId : 'missing',
        type: request && request.type ? request.type : 'unknown',
        success,
        result,
        error,
        timestamp: new Date().toISOString(),
        source: 'plugin-runtime',
    }) + '\n');
}

function _validateRequestEnvelope(request) {
    if (!request || typeof request !== 'object') return 'request_not_object';
    if (request.version !== 1) return 'unsupported_version';
    if (request.kind !== 'request') return 'invalid_kind';
    if (typeof request.requestId !== 'string' || !request.requestId) return 'missing_request_id';
    if (typeof request.type !== 'string' || !request.type) return 'missing_type';
    if (!Object.prototype.hasOwnProperty.call(request, 'payload')) return 'missing_payload';
    return null;
}

function startPluginRuntime() {
    let policy = null;
    try {
        policy = parsePolicy(process.env.RAWRXD_SANDBOX_POLICY || '{}');
    } catch (error) {
        emitViolation(error);
        policy = null;
    }

    const extensionEntry = process.env.RAWRXD_EXTENSION_ENTRY;
    if (!extensionEntry) {
        emitViolation(new SandboxViolation('missing_extension_entry', 'RAWRXD_EXTENSION_ENTRY not set'));
        process.exit(42);
        return;
    }

    const manifest = _readJsonEnv('RAWRXD_PLUGIN_MANIFEST', { id: path.basename(extensionEntry, path.extname(extensionEntry)) });
    const pluginId = typeof manifest.id === 'string' && manifest.id ? manifest.id : 'plugin.unknown';
    const messageBus = new EventEmitter();
    const capabilityClient = _createCapabilityClient(pluginId);

    const capabilities = {
        async request(type, payload) {
            return capabilityClient.request(String(type || ''), payload || {});
        },
        fs: {
            read(pathValue) {
                return capabilities.request('fs.read', { path: pathValue });
            },
            write(pathValue, dataValue) {
                return capabilities.request('fs.write', { path: pathValue, data: dataValue });
            },
            list(pathValue) {
                return capabilities.request('fs.list', { path: pathValue });
            },
        },
        net: {
            request(netRequest) {
                return capabilities.request('net.request', netRequest || {});
            },
        },
        process: {
            exec(command, args) {
                return capabilities.request('process.exec', { command, args: Array.isArray(args) ? args : [] });
            },
        },
        runtime: {
            getTime() {
                return Promise.resolve(Date.now());
            },
            getPluginInfo() {
                return capabilities.request('runtime.info', {});
            },
            emitEvent(type, payload) {
                messageBus.emit(String(type || ''), payload);
                return Promise.resolve({ emitted: true });
            },
            sleep(ms) {
                return new Promise((resolve) => setTimeout(resolve, Math.max(0, Number(ms) || 0)));
            },
        },
        kv: {
            get(key) {
                return capabilities.request('kv.get', { key });
            },
            set(key, value) {
                return capabilities.request('kv.set', { key, value });
            },
            delete(key) {
                return capabilities.request('kv.delete', { key });
            },
            clear() {
                return Promise.reject(new Error('unsupported_api:kv.clear'));
            },
        },
        ui: {
            notify(message, level) {
                return capabilities.request('ui.notify', { message, level });
            },
            panel: {
                update(panelId, data) {
                    messageBus.emit('ui.panel.update', { panelId, data });
                    return Promise.resolve({ updated: true });
                },
            },
            command: {
                register(command, meta) {
                    messageBus.emit('ui.command.register', { command, meta });
                    return Promise.resolve({ registered: true });
                },
            },
        },
        events: {
            publish(type, payload) {
                messageBus.emit(String(type || ''), payload);
                return Promise.resolve({ published: true });
            },
            subscribe(type, handler) {
                return messaging.on(type, handler);
            },
        },
        legacy: {
            enforceNativeAddon(name) {
                return capabilities.request('addon.load', { name });
            },
        },
    };

    const runtime = {
        id: pluginId,
        version: 'day9-capability-abi',
        workspace: {
            path: (policy && Array.isArray(policy.readRoots) && policy.readRoots.length > 0) ? policy.readRoots[0] : '',
            read(relPath) {
                return capabilities.request('fs.read', { path: relPath });
            },
            write(relPath, data) {
                return capabilities.request('fs.write', { path: relPath, data });
            },
        },
    };

    const messaging = {
        send(type, payload) {
            messageBus.emit(String(type || ''), payload);
            return { delivered: true };
        },
        on(type, handler) {
            const eventType = String(type || '');
            messageBus.on(eventType, handler);
            return {
                dispose() {
                    messageBus.off(eventType, handler);
                },
            };
        },
    };

    const pluginContext = {
        pluginId,
        requestId: '',
        runtime,
        capabilities,
        messaging,
        state: capabilities.kv,
        logger: {
            info: (message, fields) => process.stderr.write('PLUGIN_LOG ' + JSON.stringify({ level: 'info', pluginId, message, fields: fields || {} }) + '\n'),
            warn: (message, fields) => process.stderr.write('PLUGIN_LOG ' + JSON.stringify({ level: 'warn', pluginId, message, fields: fields || {} }) + '\n'),
            error: (message, fields) => process.stderr.write('PLUGIN_LOG ' + JSON.stringify({ level: 'error', pluginId, message, fields: fields || {} }) + '\n'),
        },
    };

    const legacyRawrxd = _createLegacyRawrxdAdapter(capabilities);
    const legacyVscode = _createLegacyVscodeAdapter(capabilities);

    const context = vm.createContext({
        console,
        setTimeout,
        clearTimeout,
        Buffer,
        runtime,
        capabilities,
        messaging,
        rawrxd: legacyRawrxd,
        vscode: legacyVscode,
        PluginContext: pluginContext,
        globalThis: {},
    });
    context.globalThis = context;

    let source;
    try {
        source = fs.readFileSync(extensionEntry, 'utf8');
    } catch (error) {
        emitViolation(new SandboxViolation('entry_read_failed', error.message));
        process.exit(44);
        return;
    }

    try {
        vm.runInContext(source, context, {
            filename: extensionEntry,
            timeout: 1500,
        });
    } catch (error) {
        emitViolation(new SandboxViolation('entry_eval_failed', error.message));
        process.exit(45);
        return;
    }

    const onLoad = typeof context.onLoad === 'function' ? context.onLoad : null;
    const onActivate = typeof context.onActivate === 'function' ? context.onActivate : null;
    const onDeactivate = typeof context.onDeactivate === 'function' ? context.onDeactivate : null;
    const onDispose = typeof context.onDispose === 'function' ? context.onDispose : null;

    if (onLoad) {
        try {
            onLoad(pluginContext);
        } catch (error) {
            emitViolation(new SandboxViolation('onload_failed', error.message));
        }
    }
    if (onActivate) {
        try {
            onActivate();
        } catch (error) {
            emitViolation(new SandboxViolation('onactivate_failed', error.message));
        }
    }

    async function dispatch(request) {
        pluginContext.requestId = request.requestId;
        if (request.type === 'plugin.deactivate' && onDeactivate) {
            onDeactivate();
            return { ok: true };
        }
        if (request.type === 'plugin.dispose' && onDispose) {
            onDispose();
            return { ok: true };
        }

        if (typeof context.handleRequest === 'function') {
            return context.handleRequest(request, legacyRawrxd, legacyVscode, pluginContext);
        }
        if (typeof context.onRequest === 'function') {
            return context.onRequest(request, pluginContext);
        }
        throw new Error('missing_handler');
    }

    let buffer = '';
    process.stdin.setEncoding('utf8');
    process.stdin.on('data', async (chunk) => {
        buffer += chunk;
        const lines = buffer.split('\n');
        buffer = lines.pop();
        for (const line of lines) {
            const trimmed = line.trim();
            if (!trimmed) continue;
            let request;
            try {
                request = JSON.parse(trimmed);
            } catch {
                continue;
            }

            if (request && request.kind === 'response') {
                const pending = capabilityClient.pending.get(request.requestId);
                if (!pending) {
                    continue;
                }
                capabilityClient.pending.delete(request.requestId);
                pending.resolve(request);
                continue;
            }

            const validationError = _validateRequestEnvelope(request);
            if (validationError) {
                _writeResponse(request, false, undefined, validationError);
                continue;
            }

            try {
                const result = await dispatch(request);
                _writeResponse(request, true, result, undefined);
            } catch (error) {
                if (error instanceof SandboxViolation) {
                    emitViolation(error);
                    _writeResponse(request, false, undefined, error.code || 'sandbox_error');
                    continue;
                }
                _writeResponse(request, false, undefined, error && error.message ? error.message : 'plugin_error');
            }
        }
    });
}

module.exports = {
    startPluginRuntime,
};
