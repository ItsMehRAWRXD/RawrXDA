'use strict';

const {
    SandboxViolation,
    assertFileRead,
    assertFileWrite,
    assertNetwork,
    assertProcess,
    assertStorage,
    assertNativeAddon,
} = require('./sandbox-policy');

const ABI_VERSION = 1;
const MANIFEST_CAPS = Object.freeze({
    commands: 1,
    'workspace.fs': 1,
    'window.messages': 1,
    process: 1,
    network: 1,
    storage: 1,
    addons: 1,
    events: 1,
    runtime: 1,
});

function _normalizeType(type) {
    const value = String(type || '').trim().toLowerCase();
    if (value.startsWith('capability.')) {
        return value.slice('capability.'.length);
    }
    return value;
}

function _manifestFailure(reason) {
    return {
        ok: false,
        code: 'manifest_negotiation_failed:' + reason,
        error: reason,
    };
}

function _negotiateManifest(options) {
    const raw = options && typeof options.extensionManifestRaw === 'string' && options.extensionManifestRaw.trim()
        ? options.extensionManifestRaw
        : null;
    const provided = options && options.extensionManifest && typeof options.extensionManifest === 'object'
        ? options.extensionManifest
        : null;

    let manifest = null;
    if (raw) {
        try {
            manifest = JSON.parse(raw);
        } catch (_) {
            return { failure: 'manifest_invalid_json' };
        }
    } else {
        manifest = provided;
    }

    if (!manifest) {
        return {
            failure: null,
            strict: false,
            allowed: new Set(),
        };
    }

    if (!Number.isInteger(manifest.abiVersion) || manifest.abiVersion !== ABI_VERSION) {
        return { failure: 'manifest_abi_mismatch' };
    }

    const requires = Array.isArray(manifest.requires) ? manifest.requires : [];
    const allowed = new Set();

    for (const requirement of requires) {
        const text = String(requirement || '');
        const match = /^capability:([a-zA-Z0-9_.-]+)@(\d+)$/.exec(text);
        if (!match) {
            return { failure: 'manifest_invalid_requirement:' + text };
        }
        const capability = match[1];
        const version = Number(match[2]);
        if (!Object.prototype.hasOwnProperty.call(MANIFEST_CAPS, capability)) {
            return { failure: 'manifest_unknown_capability:capability:' + capability };
        }
        const supported = MANIFEST_CAPS[capability];
        if (version > supported) {
            return { failure: 'manifest_capability_version_mismatch:capability:' + capability + '@' + version + '>' + supported };
        }
        allowed.add(capability);
    }

    return {
        failure: null,
        strict: true,
        allowed,
    };
}

function _mapRequest(type, payload, policy) {
    const normalized = _normalizeType(type);
    const body = payload && typeof payload === 'object' ? payload : {};

    switch (normalized) {
    case 'file.read':
    case 'fs.read':
        return {
            capability: 'fs.read',
            args: { path: assertFileRead(policy, body.path) },
            manifestCapability: 'workspace.fs',
        };
    case 'file.write':
    case 'fs.write':
        return {
            capability: 'fs.write',
            args: {
                path: assertFileWrite(policy, body.path),
                data: body.content !== undefined ? body.content : body.data,
            },
            manifestCapability: 'workspace.fs',
        };
    case 'fs.list':
        return {
            capability: 'fs.list',
            args: { path: assertFileRead(policy, body.path) },
            manifestCapability: 'workspace.fs',
        };
    case 'network.request':
    case 'net.request':
        assertNetwork(policy, body.url);
        return {
            capability: 'net.request',
            args: {
                url: String(body.url || ''),
                method: body.method,
                headers: body.headers,
                body: body.body,
            },
            manifestCapability: 'network',
        };
    case 'process.spawn':
    case 'process.exec':
        assertProcess(policy, body.command);
        return {
            capability: 'process.exec',
            args: {
                command: String(body.command || ''),
                args: Array.isArray(body.args) ? body.args : [],
            },
            manifestCapability: 'process',
        };
    case 'addon.load':
        assertNativeAddon(policy, body.name);
        return {
            capability: 'addon.load',
            args: { name: String(body.name || '') },
            manifestCapability: 'addons',
        };
    case 'cmd.execute':
        return {
            capability: 'cmd.execute',
            args: {
                command: String(body.command || ''),
                args: Array.isArray(body.args) ? body.args : [],
            },
            manifestCapability: 'commands',
        };
    case 'ui.notify':
        return {
            capability: 'ui.notify',
            args: {
                message: String(body.message || ''),
                level: String(body.level || 'info'),
            },
            manifestCapability: 'window.messages',
        };
    case 'ui.panel.update':
        return {
            capability: 'ui.panel.update',
            args: {
                panelId: String(body.panelId || ''),
                data: body.data,
            },
            manifestCapability: 'window.messages',
        };
    case 'ui.command.register':
        return {
            capability: 'ui.command.register',
            args: {
                command: String(body.command || ''),
                meta: body.meta || {},
            },
            manifestCapability: 'commands',
        };
    case 'events.publish':
        return {
            capability: 'events.publish',
            args: {
                type: String(body.type || ''),
                payload: body.payload,
            },
            manifestCapability: 'events',
        };
    case 'events.subscribe':
        return {
            capability: 'events.subscribe',
            args: {
                type: String(body.type || ''),
                handler: body.handler,
            },
            manifestCapability: 'events',
        };
    case 'runtime.info':
    case 'runtime.getplugininfo':
        return {
            capability: 'runtime.info',
            args: {},
            manifestCapability: 'runtime',
        };
    case 'runtime.gettime':
        return {
            capability: 'runtime.getTime',
            args: {},
            manifestCapability: 'runtime',
        };
    case 'runtime.emitevent':
        return {
            capability: 'runtime.emitEvent',
            args: {
                type: String(body.type || ''),
                payload: body.payload,
            },
            manifestCapability: 'runtime',
        };
    case 'runtime.sleep':
        return {
            capability: 'runtime.sleep',
            args: { ms: Number(body.ms) || 0 },
            manifestCapability: 'runtime',
        };
    case 'kv.get':
        assertStorage(policy);
        return {
            capability: 'kv.get',
            args: { key: String(body.key || '') },
            manifestCapability: 'storage',
        };
    case 'kv.set':
        assertStorage(policy);
        return {
            capability: 'kv.set',
            args: {
                key: String(body.key || ''),
                value: body.value,
            },
            manifestCapability: 'storage',
        };
    case 'kv.delete':
        assertStorage(policy);
        return {
            capability: 'kv.delete',
            args: { key: String(body.key || '') },
            manifestCapability: 'storage',
        };
    case 'kv.clear':
        assertStorage(policy);
        return {
            capability: 'kv.clear',
            args: {},
            manifestCapability: 'storage',
        };
    default:
        throw new SandboxViolation('invalid_capability_request', normalized || String(type || 'unknown'));
    }
}

function createCapabilityBroker(options = {}) {
    const policy = options.policy || null;
    const invalidPolicyReason = options.invalidPolicyReason || null;
    const manifest = _negotiateManifest(options);

    function request(type, payload) {
        if (manifest.failure) {
            return _manifestFailure(manifest.failure);
        }
        if (invalidPolicyReason || !policy) {
            return {
                ok: false,
                code: 'invalid_policy',
                error: 'Sandbox policy unavailable' + (invalidPolicyReason ? ': ' + invalidPolicyReason : ''),
            };
        }

        try {
            const descriptor = _mapRequest(type, payload, policy);
            if (manifest.strict && descriptor.manifestCapability && !manifest.allowed.has(descriptor.manifestCapability)) {
                return _manifestFailure('manifest_capability_not_declared:capability:' + descriptor.manifestCapability);
            }
            return {
                ok: true,
                descriptor: {
                    capability: descriptor.capability,
                    args: descriptor.args,
                },
            };
        } catch (error) {
            if (error instanceof SandboxViolation) {
                return {
                    ok: false,
                    code: error.code || 'capability_denied',
                    error: error.detail || error.message,
                };
            }
            return {
                ok: false,
                code: 'capability_error',
                error: error && error.message ? error.message : String(error),
            };
        }
    }

    return {
        request,
    };
}

module.exports = {
    createCapabilityBroker,
};
