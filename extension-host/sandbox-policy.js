'use strict';

const path = require('path');
const net = require('net');

class SandboxViolation extends Error {
    constructor(code, detail) {
        super(detail ? `${code}: ${detail}` : code);
        this.name = 'SandboxViolation';
        this.code = code;
        this.detail = detail || code;
    }
}

function _normalizePath(inputPath) {
    if (typeof inputPath !== 'string' || !inputPath.trim()) {
        throw new SandboxViolation('invalid_path', 'Path must be a non-empty string');
    }
    return path.resolve(inputPath);
}

function _normalizeRoots(list, code) {
    if (!Array.isArray(list)) {
        throw new SandboxViolation(code, 'Policy root list must be an array');
    }
    return list.map(_normalizePath);
}

function parsePolicy(raw) {
    let policy;
    try {
        policy = typeof raw === 'string' ? JSON.parse(raw) : raw;
    } catch {
        throw new SandboxViolation('invalid_policy', 'Sandbox policy JSON is invalid');
    }
    if (!policy || typeof policy !== 'object') {
        throw new SandboxViolation('invalid_policy', 'Sandbox policy must be an object');
    }
    return Object.freeze({
        version: policy.version || 1,
        readRoots: _normalizeRoots(policy.readRoots || [], 'invalid_read_roots'),
        writeRoots: _normalizeRoots(policy.writeRoots || [], 'invalid_write_roots'),
        networkAllowlist: Array.isArray(policy.networkAllowlist) ? policy.networkAllowlist.map((v) => String(v).toLowerCase()) : [],
        allowProcessSpawn: policy.allowProcessSpawn === true,
        processAllowlist: Array.isArray(policy.processAllowlist)
            ? policy.processAllowlist.map((v) => String(v).toLowerCase())
            : [],
        allowNativeAddons: policy.allowNativeAddons === true,
        allowPrivateNetwork: policy.allowPrivateNetwork === true,
        allowStorage: policy.allowStorage !== false,
        violationLimit: Number.isInteger(policy.violationLimit) ? Math.max(1, policy.violationLimit) : 3,
    });
}

function _isUnderRoots(normalizedPath, roots) {
    return roots.some((root) => normalizedPath === root || normalizedPath.startsWith(root + path.sep));
}

function enforceFileRead(inputPath, policy) {
    const normalizedPath = _normalizePath(inputPath);
    if (!_isUnderRoots(normalizedPath, policy.readRoots)) {
        throw new SandboxViolation('file_read_denied', normalizedPath);
    }
    return normalizedPath;
}

function enforceFileWrite(inputPath, policy) {
    const normalizedPath = _normalizePath(inputPath);
    if (!_isUnderRoots(normalizedPath, policy.writeRoots)) {
        throw new SandboxViolation('file_write_denied', normalizedPath);
    }
    return normalizedPath;
}

function _isPrivateHost(hostname) {
    const value = hostname.toLowerCase();
    if (value === 'localhost') return true;
    if (net.isIP(value) === 4) {
        if (value.startsWith('127.')) return true;
        if (value.startsWith('10.')) return true;
        if (value.startsWith('192.168.')) return true;
        if (/^172\.(1[6-9]|2\d|3[0-1])\./.test(value)) return true;
    }
    return false;
}

function enforceNetwork(rawUrl, policy) {
    let parsed;
    try {
        parsed = new URL(rawUrl);
    } catch {
        throw new SandboxViolation('network_invalid_url', String(rawUrl));
    }
    const host = parsed.hostname.toLowerCase();
    if (_isPrivateHost(host) && !policy.allowPrivateNetwork) {
        throw new SandboxViolation('network_private_denied', host);
    }
    if (!policy.networkAllowlist.includes(host)) {
        throw new SandboxViolation('network_denied', host);
    }
    return parsed;
}

function enforceProcessSpawn(policy, command) {
    if (!policy.allowProcessSpawn) {
        throw new SandboxViolation('process_spawn_denied', String(command || 'spawn'));
    }

    if (policy.processAllowlist.length > 0) {
        const normalized = String(command || '').trim().toLowerCase();
        if (!policy.processAllowlist.includes(normalized)) {
            throw new SandboxViolation('process_denied', String(command || 'spawn'));
        }
    }
}

function enforceNativeAddon(policy, moduleName) {
    if (!policy.allowNativeAddons) {
        throw new SandboxViolation('native_addon_denied', String(moduleName || 'addon'));
    }
}

function canRead(policy, inputPath) {
    try {
        enforceFileRead(inputPath, policy);
        return true;
    } catch (_) {
        return false;
    }
}

function canWrite(policy, inputPath) {
    try {
        enforceFileWrite(inputPath, policy);
        return true;
    } catch (_) {
        return false;
    }
}

function canNetwork(policy, rawUrl) {
    try {
        enforceNetwork(rawUrl, policy);
        return true;
    } catch (_) {
        return false;
    }
}

function canProcess(policy, command) {
    try {
        enforceProcessSpawn(policy, command);
        return true;
    } catch (_) {
        return false;
    }
}

function canStorage(policy) {
    return !!(policy && policy.allowStorage !== false);
}

function canNativeAddon(policy, moduleName) {
    try {
        enforceNativeAddon(policy, moduleName);
        return true;
    } catch (_) {
        return false;
    }
}

function assertFileRead(policy, inputPath) {
    return enforceFileRead(inputPath, policy);
}

function assertFileWrite(policy, inputPath) {
    return enforceFileWrite(inputPath, policy);
}

function assertNetwork(policy, rawUrl) {
    return enforceNetwork(rawUrl, policy);
}

function assertProcess(policy, command) {
    return enforceProcessSpawn(policy, command);
}

function assertStorage(policy) {
    if (!canStorage(policy)) {
        throw new SandboxViolation('storage_denied', 'extension_storage');
    }
}

function assertNativeAddon(policy, moduleName) {
    return enforceNativeAddon(policy, moduleName);
}

module.exports = {
    SandboxViolation,
    parsePolicy,
    canRead,
    canWrite,
    canNetwork,
    canProcess,
    canStorage,
    canNativeAddon,
    assertFileRead,
    assertFileWrite,
    assertNetwork,
    assertProcess,
    assertStorage,
    assertNativeAddon,
    enforceFileRead,
    enforceFileWrite,
    enforceNetwork,
    enforceProcessSpawn,
    enforceNativeAddon,
};