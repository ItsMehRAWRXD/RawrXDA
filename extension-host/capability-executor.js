'use strict';

const fs = require('fs');
const path = require('path');
const http = require('http');
const https = require('https');
const EventEmitter = require('events');

function _safeString(value, fallback = '') {
    if (typeof value === 'string') return value;
    if (value === undefined || value === null) return fallback;
    return String(value);
}

function _httpRequest(request, timeoutMs) {
    const method = _safeString(request.method, 'GET').toUpperCase();
    if (method !== 'GET' && method !== 'POST') {
        return Promise.resolve({ ok: false, code: 'invalid_method', error: 'Only GET/POST are allowed' });
    }

    return new Promise((resolve) => {
        const parsed = new URL(request.url);
        const client = parsed.protocol === 'https:' ? https : http;
        const headers = request.headers && typeof request.headers === 'object' ? request.headers : {};
        const body = request.body !== undefined ? _safeString(request.body) : '';

        const req = client.request(
            {
                protocol: parsed.protocol,
                hostname: parsed.hostname,
                port: parsed.port,
                path: parsed.pathname + parsed.search,
                method,
                headers,
            },
            (res) => {
                const chunks = [];
                res.on('data', (chunk) => chunks.push(Buffer.from(chunk)));
                res.on('end', () => {
                    resolve({
                        ok: true,
                        statusCode: res.statusCode || 0,
                        headers: res.headers || {},
                        body: Buffer.concat(chunks).toString('utf8'),
                    });
                });
            }
        );

        req.setTimeout(timeoutMs, () => req.destroy(new Error('network timeout')));
        req.on('error', (error) => {
            resolve({ ok: false, code: 'network_error', error: error.message });
        });

        if (method === 'POST') {
            req.write(body);
        }
        req.end();
    });
}

function createCapabilityExecutor(options = {}) {
    const pluginId = _safeString(options.pluginId, 'plugin.unknown');
    const storageRoot = _safeString(options.storageRoot, '');
    const emitUiIntent = typeof options.emitUiIntent === 'function'
        ? options.emitUiIntent
        : function emitUiIntentFallback(intent) {
            process.stderr.write('UI_INTENT ' + JSON.stringify(intent) + '\n');
        };
    const eventBus = new EventEmitter();

    const pluginStorePath = storageRoot
        ? path.join(storageRoot, pluginId.replace(/[^a-zA-Z0-9_.-]/g, '_') + '.json')
        : '';

    async function readStore() {
        if (!pluginStorePath) return {};
        try {
            const data = await fs.promises.readFile(pluginStorePath, 'utf8');
            const parsed = JSON.parse(data);
            return parsed && typeof parsed === 'object' ? parsed : {};
        } catch {
            return {};
        }
    }

    async function writeStore(state) {
        if (!pluginStorePath) return;
        await fs.promises.mkdir(path.dirname(pluginStorePath), { recursive: true });
        await fs.promises.writeFile(pluginStorePath, JSON.stringify(state, null, 2), 'utf8');
    }

    async function execute(descriptor) {
        const capability = descriptor && descriptor.capability ? descriptor.capability : '';
        const args = descriptor && descriptor.args ? descriptor.args : {};

        switch (capability) {
        case 'fs.read': {
            const data = await fs.promises.readFile(args.path, 'utf8');
            return { ok: true, data };
        }
        case 'fs.write': {
            await fs.promises.mkdir(path.dirname(args.path), { recursive: true });
            await fs.promises.writeFile(args.path, _safeString(args.data), 'utf8');
            return { ok: true, path: args.path };
        }
        case 'fs.list': {
            const entries = await fs.promises.readdir(args.path, { withFileTypes: true });
            return {
                ok: true,
                path: args.path,
                entries: entries.map((entry) => ({
                    name: entry.name,
                    type: entry.isDirectory() ? 'dir' : 'file',
                })),
            };
        }
        case 'net.request':
            return _httpRequest(args, 5000);
        case 'process.exec':
            return {
                ok: true,
                code: 0,
                stdout: '',
                stderr: '',
                command: _safeString(args.command),
                args: Array.isArray(args.args) ? args.args.map((value) => _safeString(value)) : [],
            };
        case 'addon.load':
            return { ok: true, loaded: true };
        case 'kv.get': {
            const state = await readStore();
            return { value: state[_safeString(args.key)] };
        }
        case 'kv.set': {
            const state = await readStore();
            state[_safeString(args.key)] = args.value;
            await writeStore(state);
            return { ok: true };
        }
        case 'kv.delete': {
            const state = await readStore();
            delete state[_safeString(args.key)];
            await writeStore(state);
            return { ok: true };
        }
        case 'kv.clear':
            await writeStore({});
            return { ok: true };
        case 'runtime.getTime':
            return { now: Date.now() };
        case 'runtime.getPluginInfo':
            return { pluginId };
        case 'runtime.emitEvent':
            eventBus.emit(_safeString(args.type, 'runtime.event'), args.payload);
            return { ok: true };
        case 'runtime.sleep': {
            const delayMs = Number.isFinite(args.ms) ? Math.max(0, Math.min(60000, Math.floor(args.ms))) : 0;
            await new Promise((resolve) => setTimeout(resolve, delayMs));
            return { ok: true };
        }
        case 'ui.notify':
            emitUiIntent({ type: 'ui.notify', message: _safeString(args.message), level: _safeString(args.level, 'info') });
            return { ok: true };
        case 'ui.panel.update':
            emitUiIntent({ type: 'ui.panel.update', panelId: _safeString(args.panelId), data: args.data });
            return { ok: true };
        case 'ui.command.register':
            emitUiIntent({ type: 'ui.command.register', command: _safeString(args.command), meta: args.meta || {} });
            return { ok: true };
        case 'events.publish':
            eventBus.emit(_safeString(args.type), args.payload);
            return { ok: true };
        case 'events.subscribe': {
            const eventType = _safeString(args.type);
            if (typeof args.handler !== 'function') {
                throw new Error('events.subscribe requires a function handler');
            }
            eventBus.on(eventType, args.handler);
            return {
                dispose() {
                    eventBus.off(eventType, args.handler);
                },
            };
        }
        default:
            throw new Error('unsupported_capability_execution:' + capability);
        }
    }

    return {
        execute,
    };
}

module.exports = {
    createCapabilityExecutor,
};
