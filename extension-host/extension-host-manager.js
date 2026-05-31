'use strict';

// extension-host-manager.js
// Day 6 Gate: Extension Host Runtime Foundation
// Manages isolated child-process extension hosts with crash restart and lifecycle telemetry.

const { spawn, spawnSync } = require('child_process');
const fs   = require('fs');
const path = require('path');
const os   = require('os');
const http = require('http');
const https = require('https');
const crypto = require('crypto');
const {
    sha256Hex,
    buildPreExecutionAuthorizationPayload,
    computeBrokerDecisionHash,
    signPreExecutionAuthorization,
    verifyPreExecutionGate,
} = require('./capability-lineage-binding');
const {
    parsePolicy,
    assertFileRead,
    assertFileWrite,
    assertNetwork,
    assertProcess,
    assertStorage,
    assertNativeAddon,
} = require('./sandbox-policy');
const { verifyManifestSignature } = require('./manifest-signature');
const {
    LOG_FILE: CAPABILITY_AUDIT_LOG_FILE,
    hashArgs,
    createTraceContext,
    ensureAuditSinkWritable,
    emitRequest,
    emitDecision,
    emitExecuteStart,
    emitExecuteResult,
} = require('./capability-audit');
const { routeFileWrite } = require('./tool-operation-router');
const { executeWithBackoff, enqueueTask } = require('./resilient-execution');
const { createWorkspaceIndexCache } = require('./workspace-index-cache');
const { createLoopController } = require('./autonomy-loop-controller');

// ─── Lifecycle log ────────────────────────────────────────────────────────────
const LOG_DIR = path.join(
    process.env.LOCALAPPDATA || path.join(os.homedir(), 'AppData', 'Local'),
    'RawrXD', 'Logs'
);
const LOG_FILE = path.join(LOG_DIR, 'extension-host.log');
const RESILIENT_QUEUE_FILE = path.join(LOG_DIR, 'autonomy-task-queue.json');
const WORKSPACE_INDEX_CACHE_FILE = path.join(LOG_DIR, 'workspace-index-cache.json');

try { fs.mkdirSync(LOG_DIR, { recursive: true }); } catch (_) {}
const workspaceIndexCache = createWorkspaceIndexCache(WORKSPACE_INDEX_CACHE_FILE);

/**
 * Append an ISO-timestamped log line to the lifecycle log and stderr.
 * @param {string} tag    One of SPAWN|READY|CRASH|RESTART|DISABLED|SHUTDOWN|IPC_OK|IPC_FAIL
 * @param {Object} fields Key-value pairs appended as k=v tokens.
 */
function logEvent(tag, fields) {
    const ts = new Date().toISOString();
    const parts = Object.entries(fields)
        .map(([k, v]) => `${k}=${String(v).replace(/[\s\n\r]/g, '_').slice(0, 200)}`)
        .join(' ');
    const line = `${ts} [${tag}] ${parts}\n`;
    try { fs.appendFileSync(LOG_FILE, line); } catch (_) {}
    process.stderr.write(line);
}

// ─── Constants ────────────────────────────────────────────────────────────────
const MAX_RETRIES   = 3;
const BACKOFF_MS    = [500, 1000, 2000];   // indexed by retries - 1
const IPC_TIMEOUT_MS = 10000;
const MAX_PAYLOAD_BYTES = 65536;
const MAX_REQUEST_RETRIES = 1;
const IPC_SCHEMA_VERSION = 1;
const MAX_SANDBOX_VIOLATIONS = 3;
const CAPABILITY_TIMEOUT_MS = 5000;
const RATE_LIMIT_BACKOFF_MS = [300, 600, 1200, 2400];
const MAX_RATE_LIMIT_RETRIES = 4;
const DEFAULT_MAX_AUTONOMY_DEPTH = 12;
const DEFAULT_RESOURCE_SAMPLE_INTERVAL_MS = 750;
const LOW_VRAM_SIGNAL = 'SIG_LOW_VRAM';
const SYNC_PEER_PROBE_TIMEOUT_MS = 600;
let auditSinkSelfTestCompleted = false;

let syncPeerRegistry = [];

const LSP_ERROR_CODE_MAP = Object.freeze({
    0: 'LSP_ERR_NONE',
    1: 'LSP_ERR_IO',
    2: 'LSP_ERR_TIMEOUT',
    3: 'LSP_ERR_HEADER_MISSING',
    4: 'LSP_ERR_HEADER_BAD_LENGTH',
    5: 'LSP_ERR_FRAME_OVERSIZE',
    6: 'LSP_ERR_OUTPUT_TOO_SMALL',
    7: 'LSP_ERR_ID_MISMATCH',
    8: 'LSP_ERR_RESULT_MISSING',
});

// ─── Registry ─────────────────────────────────────────────────────────────────
// Map<id, ExtRecord>
// ExtRecord: { name, extensionPath, spawnArgs, spawnEnv, sandboxPolicy, sandboxViolations,
//              sandboxViolationLimit, extensionManifest, proc, pid, state, retries, lastBootMs, spawnTime, pendingRequests: Map, _seq }
// state: 'starting' | 'ready' | 'crashed' | 'disabled' | 'stopped'
const extensions = new Map();

function _asString(value, fallback = '') {
    if (typeof value === 'string') return value;
    if (value === undefined || value === null) return fallback;
    return String(value);
}

function _parseLspErrorCode(raw) {
    const text = _asString(raw).trim();
    if (!text) return null;

    const m = text.match(/(?:lsp(?:_|\s)?(?:last)?(?:_|\s)?error|lsp_error_code)\s*[:=]\s*(0x[0-9a-fA-F]+|\d+)/i);
    if (m && m[1]) {
        const parsed = Number(m[1]);
        return Number.isInteger(parsed) ? parsed : null;
    }

    const hexOnly = text.match(/^0x[0-9a-fA-F]+$/);
    if (hexOnly) {
        const parsed = Number(text);
        return Number.isInteger(parsed) ? parsed : null;
    }

    const intOnly = text.match(/^\d+$/);
    if (intOnly) {
        const parsed = Number(text);
        return Number.isInteger(parsed) ? parsed : null;
    }

    return null;
}

function _findLspErrorNameInText(raw) {
    const text = _asString(raw);
    const m = text.match(/\bLSP_ERR_[A-Z_]+\b/);
    return m ? m[0] : '';
}

function _normalizeLspDiagnostic(raw) {
    const original = _asString(raw);
    const explicitName = _findLspErrorNameInText(original);
    if (explicitName) {
        return {
            code: null,
            name: explicitName,
            error: original,
        };
    }

    const code = _parseLspErrorCode(original);
    if (!Number.isInteger(code)) {
        return {
            code: null,
            name: '',
            error: original,
        };
    }

    const name = LSP_ERROR_CODE_MAP[code] || 'LSP_ERR_UNKNOWN';
    const hasLabel = /\bLSP_ERR_[A-Z_]+\b/.test(original);
    const error = hasLabel ? original : `${original}:${name}`;
    return { code, name, error };
}

function _unwrapPromiseAnyError(error) {
    if (!error || typeof error !== 'object') {
        return error;
    }
    if (!Array.isArray(error.errors) || error.errors.length === 0) {
        return error;
    }
    for (const candidate of error.errors) {
        if (candidate instanceof Error) {
            return candidate;
        }
    }
    const first = error.errors[0];
    return first instanceof Error ? first : new Error(_asString(first, 'promise_any_failed'));
}

function _isLoopBusyError(error) {
    return !!(error && typeof error.message === 'string' && /one_loop_policy_violation:step_already_active/i.test(error.message));
}

function _normalizeCapabilityType(type, payload) {
    const raw = _asString(type).trim().toLowerCase();
    if (raw.startsWith('capability.')) return raw.slice('capability.'.length);
    if (raw === 'runtime.info') return 'runtime.info';
    if (raw === 'fs.read' || raw === 'file.read') return 'fs.read';
    if (raw === 'fs.write' || raw === 'file.write') return 'fs.write';
    if (raw === 'net.request' || raw === 'network.request') return 'net.request';
    if (raw === 'process.exec' || raw === 'process.spawn') return 'process.exec';
    if (raw === 'addon.load') return 'addon.load';
    if (raw === 'ui.notify') return 'ui.notify';
    if (raw === 'cmd.execute' || raw === 'commands.execute') return 'cmd.execute';
    if (raw === 'kv.get' || raw === 'kv.set' || raw === 'kv.delete') return raw;
    if (payload && typeof payload === 'object') {
        const op = _asString(payload.action).toLowerCase();
        if (raw === 'filesystem' && op === 'read') return 'fs.read';
        if (raw === 'filesystem' && op === 'write') return 'fs.write';
    }
    return raw;
}

function _normalizeProcessCommandName(command) {
    const raw = _asString(command).trim().toLowerCase();
    if (!raw) return '';
    const normalized = raw.replace(/\\/g, '/');
    const segments = normalized.split('/').filter(Boolean);
    return segments.length > 0 ? segments[segments.length - 1] : normalized;
}

function _buildManifestProcessAllowlist(extensionManifest) {
    if (!extensionManifest || typeof extensionManifest !== 'object') {
        return null;
    }
    const config = extensionManifest.processAllowlist;
    if (!config || typeof config !== 'object' || Array.isArray(config)) {
        return null;
    }
    const commands = Array.isArray(config.commands) ? config.commands : [];
    const allowlist = new Set();
    for (const entry of commands) {
        const raw = _asString(entry).trim().toLowerCase();
        if (!raw) continue;
        allowlist.add(raw);
        allowlist.add(_normalizeProcessCommandName(raw));
    }
    return allowlist;
}

function _assertManifestProcessAllowlist(allowlist, command) {
    if (!allowlist || allowlist.size === 0) {
        return;
    }
    const raw = _asString(command).trim().toLowerCase();
    const base = _normalizeProcessCommandName(command);
    if (!raw || (!allowlist.has(raw) && !allowlist.has(base))) {
        const err = new Error('process_manifest_denied');
        err.detail = _asString(command, 'spawn');
        throw err;
    }
}

function _normalizeNetworkHost(input) {
    const raw = _asString(input).trim().toLowerCase();
    if (!raw) return '';
    try {
        const parsed = new URL(raw.includes('://') ? raw : `http://${raw}`);
        return _asString(parsed.hostname).trim().toLowerCase();
    } catch (_) {
        return raw;
    }
}

function _buildManifestNetworkAllowlist(extensionManifest) {
    if (!extensionManifest || typeof extensionManifest !== 'object') {
        return null;
    }
    const config = extensionManifest.networkAllowlist;
    if (!config || typeof config !== 'object' || Array.isArray(config)) {
        return null;
    }
    const hosts = Array.isArray(config.hosts) ? config.hosts : [];
    const allowlist = new Set();
    for (const entry of hosts) {
        const host = _normalizeNetworkHost(entry);
        if (!host) continue;
        allowlist.add(host);
    }
    return allowlist;
}

function _assertManifestNetworkAllowlist(allowlist, rawUrl) {
    if (!allowlist || allowlist.size === 0) {
        return;
    }
    let host = '';
    try {
        host = new URL(_asString(rawUrl)).hostname.toLowerCase();
    } catch (_) {
        const err = new Error('network_manifest_denied');
        err.detail = _asString(rawUrl);
        throw err;
    }
    if (!allowlist.has(host)) {
        const err = new Error('network_manifest_denied');
        err.detail = host;
        throw err;
    }
}

function _negotiateManifestOrThrow(extensionManifest, extensionManifestRaw) {
    if (!extensionManifestRaw && !extensionManifest) {
        return;
    }

    if (typeof extensionManifestRaw === 'string' && extensionManifestRaw) {
        try {
            JSON.parse(extensionManifestRaw);
        } catch (_) {
            throw Object.assign(new Error('manifest_negotiation_failed:manifest_invalid_json'), { detail: 'manifest_invalid_json' });
        }
    }

    if (!extensionManifest || typeof extensionManifest !== 'object') {
        return;
    }

    if ((extensionManifest.abiVersion || 1) !== 1) {
        throw Object.assign(new Error('manifest_negotiation_failed:manifest_abi_mismatch'), { detail: 'manifest_abi_mismatch' });
    }

    const knownCapabilities = new Set(['filesystem', 'network', 'process', 'addon', 'ui', 'commands', 'cmd', 'kv', 'info', 'runtime']);
    const requires = Array.isArray(extensionManifest.requires) ? extensionManifest.requires : [];
    for (const req of requires) {
        const text = _asString(req).trim();
        if (!text.startsWith('capability:')) {
            continue;
        }
        const capSpec = text.slice('capability:'.length);
        const at = capSpec.indexOf('@');
        const capName = at >= 0 ? capSpec.slice(0, at) : capSpec;
        const capVersion = at >= 0 ? Number(capSpec.slice(at + 1)) : 1;

        if (!knownCapabilities.has(capName)) {
            throw Object.assign(new Error('manifest_negotiation_failed:manifest_unknown_capability:capability:' + capName), { detail: 'manifest_unknown_capability:capability:' + capName });
        }
        if (Number.isFinite(capVersion) && capVersion > 1) {
            const detail = 'manifest_capability_version_mismatch:capability:' + capName + '@' + capVersion + '>1';
            throw Object.assign(new Error('manifest_negotiation_failed:' + detail), { detail });
        }
    }

    if (Object.prototype.hasOwnProperty.call(extensionManifest, 'processAllowlist')) {
        const processAllowlist = extensionManifest.processAllowlist;
        if (!processAllowlist || typeof processAllowlist !== 'object' || Array.isArray(processAllowlist)) {
            throw Object.assign(new Error('manifest_negotiation_failed:manifest_process_allowlist_invalid'), { detail: 'manifest_process_allowlist_invalid' });
        }
        const version = Number.isInteger(processAllowlist.version) ? processAllowlist.version : 1;
        if (version !== 1) {
            throw Object.assign(new Error('manifest_negotiation_failed:manifest_process_allowlist_version_mismatch'), { detail: 'manifest_process_allowlist_version_mismatch' });
        }
        const commands = processAllowlist.commands;
        if (!Array.isArray(commands) || commands.length === 0 || commands.some((entry) => !_asString(entry).trim())) {
            throw Object.assign(new Error('manifest_negotiation_failed:manifest_process_allowlist_invalid_commands'), { detail: 'manifest_process_allowlist_invalid_commands' });
        }
        const hasProcessCapability = requires.some((entry) => {
            const text = _asString(entry).trim().toLowerCase();
            return text === 'capability:process@1' || text === 'capability:process';
        });
        if (!hasProcessCapability) {
            throw Object.assign(new Error('manifest_negotiation_failed:manifest_process_allowlist_requires_process_capability'), { detail: 'manifest_process_allowlist_requires_process_capability' });
        }
    }

    if (Object.prototype.hasOwnProperty.call(extensionManifest, 'networkAllowlist')) {
        const networkAllowlist = extensionManifest.networkAllowlist;
        if (!networkAllowlist || typeof networkAllowlist !== 'object' || Array.isArray(networkAllowlist)) {
            throw Object.assign(new Error('manifest_negotiation_failed:manifest_network_allowlist_invalid'), { detail: 'manifest_network_allowlist_invalid' });
        }
        const version = Number.isInteger(networkAllowlist.version) ? networkAllowlist.version : 1;
        if (version !== 1) {
            throw Object.assign(new Error('manifest_negotiation_failed:manifest_network_allowlist_version_mismatch'), { detail: 'manifest_network_allowlist_version_mismatch' });
        }
        const hosts = networkAllowlist.hosts;
        if (!Array.isArray(hosts) || hosts.length === 0 || hosts.some((entry) => !_normalizeNetworkHost(entry))) {
            throw Object.assign(new Error('manifest_negotiation_failed:manifest_network_allowlist_invalid_hosts'), { detail: 'manifest_network_allowlist_invalid_hosts' });
        }
        const hasNetworkCapability = requires.some((entry) => {
            const text = _asString(entry).trim().toLowerCase();
            return text === 'capability:network@1' || text === 'capability:network';
        });
        if (!hasNetworkCapability) {
            throw Object.assign(new Error('manifest_negotiation_failed:manifest_network_allowlist_requires_network_capability'), { detail: 'manifest_network_allowlist_requires_network_capability' });
        }
    }
}

function createCapabilityBroker(options = {}) {
    const policy = options.policy || null;
    const invalidPolicyReason = options.invalidPolicyReason || null;
    const extensionManifest = options.extensionManifest || null;
    const extensionManifestRaw = options.extensionManifestRaw || null;

    _negotiateManifestOrThrow(extensionManifest, extensionManifestRaw);
    const manifestProcessAllowlist = _buildManifestProcessAllowlist(extensionManifest);
    const manifestNetworkAllowlist = _buildManifestNetworkAllowlist(extensionManifest);

    return {
        request(type, payload = {}) {
            if (!policy || invalidPolicyReason) {
                const err = new Error('invalid_policy');
                err.detail = invalidPolicyReason || 'Sandbox policy unavailable';
                throw err;
            }

            const normalized = _normalizeCapabilityType(type, payload || {});
            const data = payload && typeof payload === 'object' ? payload : {};

            switch (normalized) {
            case 'fs.read': {
                const safePath = assertFileRead(policy, data.path);
                return { ok: true, descriptor: { capability: 'fs.read', args: { path: safePath } } };
            }
            case 'fs.write': {
                const safePath = assertFileWrite(policy, data.path);
                return { ok: true, descriptor: { capability: 'fs.write', args: { path: safePath, data: data.data } } };
            }
            case 'net.request': {
                const parsed = assertNetwork(policy, data.url);
                _assertManifestNetworkAllowlist(manifestNetworkAllowlist, parsed.toString());
                return { ok: true, descriptor: { capability: 'net.request', args: { ...data, url: parsed.toString() } } };
            }
            case 'process.exec': {
                assertProcess(policy, data.command);
                _assertManifestProcessAllowlist(manifestProcessAllowlist, data.command);
                return { ok: true, descriptor: { capability: 'process.exec', args: { command: _asString(data.command), args: Array.isArray(data.args) ? data.args : [] } } };
            }
            case 'kv.get':
            case 'kv.set':
            case 'kv.delete': {
                assertStorage(policy);
                return { ok: true, descriptor: { capability: normalized, args: { key: _asString(data.key), value: data.value } } };
            }
            case 'runtime.info':
                return { ok: true, descriptor: { capability: 'runtime.info', args: {} } };
            case 'addon.load':
                assertNativeAddon(policy, data.name);
                return { ok: true, descriptor: { capability: 'addon.load', args: { name: _asString(data.name) } } };
            case 'ui.notify':
                return { ok: true, descriptor: { capability: 'ui.notify', args: { message: _asString(data.message), level: _asString(data.level, 'info') } } };
            case 'cmd.execute':
                return { ok: true, descriptor: { capability: 'cmd.execute', args: { command: _asString(data.command), args: Array.isArray(data.args) ? data.args : [] } } };
            default: {
                const err = new Error('unknown_capability');
                err.detail = normalized;
                throw err;
            }
            }
        },
    };
}

function _createBrokerForRecord(record) {
    return createCapabilityBroker({
        policy: record && record.sandboxPolicy ? record.sandboxPolicy : null,
        pluginId: record && record.name ? record.name : 'plugin.unknown',
        invalidPolicyReason: record && record.invalidSandboxPolicy ? record.invalidSandboxPolicy : null,
        extensionManifest: record && record.extensionManifest ? record.extensionManifest : null,
        extensionManifestRaw: record && record.spawnEnv && typeof record.spawnEnv.RAWRXD_EXTENSION_MANIFEST === 'string'
            ? record.spawnEnv.RAWRXD_EXTENSION_MANIFEST
            : null,
    });
}

function _safeAuditEmit(fn, ...args) {
    try {
        fn(...args);
    } catch (_) {
        // Audit logging is additive and must not break capability execution.
    }
}

function _runAuditSinkSelfTest() {
    if (auditSinkSelfTestCompleted) {
        return;
    }

    try {
        ensureAuditSinkWritable();
        auditSinkSelfTestCompleted = true;
        logEvent('AUDIT_OK', {
            extension: 'extension-host-manager',
            sink: CAPABILITY_AUDIT_LOG_FILE,
        });
    } catch (error) {
        const detail = error && error.message ? error.message : 'unknown_error';
        throw new Error('audit_sink_unavailable:' + detail);
    }
}

function _isRateLimitError(error) {
    const text = String(error && error.message ? error.message : error || '').toLowerCase();
    return text.includes('429') || text.includes('rate_limit') || text.includes('user_weekly_rate_limited');
}

function _assertPreExecutionAuthorized(record, trace, decision, resultCode) {
    const authOptions = record && record.preExecutionAuth && typeof record.preExecutionAuth === 'object'
        ? record.preExecutionAuth
        : {
            requirePreExecutionAuth: false,
            requireSignatures: false,
            signatureTrustAnchors: null,
            signer: null,
        };
    const context = {
        ...trace,
        decision,
        resultCode,
        decisionTs: new Date().toISOString(),
    };
    const authContext = authOptions.authContext && typeof authOptions.authContext === 'object'
        ? authOptions.authContext
        : null;
    const hasExplicitBrokerHashOverride = !!(authContext && Object.prototype.hasOwnProperty.call(authContext, 'brokerDecisionHash'));

    if (authContext) {
        Object.assign(context, authContext);
    }

    // Canonical path: compute broker hash from the final merged context so signer and verifier
    // use mathematically identical inputs. Explicit brokerDecisionHash override is reserved for
    // tamper-path tests that must fail closed.
    if (!hasExplicitBrokerHashOverride) {
        context.brokerDecisionHash = computeBrokerDecisionHash(context, context.decision, context.resultCode || '');
    }
    if (authOptions.requireSignatures) {
        const signature = signPreExecutionAuthorization(context, authOptions.signer || null);
        if (signature) {
            Object.assign(context, signature);
        }
    }
    const authPayload = buildPreExecutionAuthorizationPayload(context);
    const preExecutionAuthHash = sha256Hex(authPayload);

    trace.decisionTs = _asString(context.decisionTs);
    trace.preExecutionAuthHash = preExecutionAuthHash;
    trace.preExecutionSignatureKeyId = _asString(context.signatureKeyId);
    trace.preExecutionSignatureAlgorithm = _asString(context.signatureAlgorithm);
    trace.preExecutionSignatureValue = _asString(context.signatureValue);

    const gate = verifyPreExecutionGate(context, authOptions);
    if (!gate.ok) {
        const err = new Error('pre_execution_auth_failed:' + (gate.code || 'unknown'));
        err.detail = gate.reason || gate.code || 'unknown';
        throw err;
    }
}

// ─── Internal helpers ─────────────────────────────────────────────────────────

function _failPending(record, reason) {
    for (const [, pending] of record.pendingRequests) {
        clearTimeout(pending.timer);
        pending.reject(new Error(reason));
    }
    record.pendingRequests.clear();
}

function _emitShutdown(record, origin) {
    if (!record || record.shutdownLogged) return;
    record.shutdownLogged = true;
    logEvent('SHUTDOWN', {
        extension: record.name,
        pid: record.pid,
        origin: _asString(origin, 'unknown'),
    });
}

function _clearResourceWatchdog(record) {
    if (!record || !record.resourceWatchdogTimer) return;
    clearInterval(record.resourceWatchdogTimer);
    record.resourceWatchdogTimer = null;
}

function _readProcessResourceSample(pid) {
    if (!Number.isInteger(pid) || pid <= 0) return null;
    if (process.platform !== 'win32') return null;

    const command = `$p=Get-Process -Id ${pid} -ErrorAction SilentlyContinue; if ($null -eq $p) { '' } else { "$($p.WorkingSet64),$($p.CPU)" }`;
    let result;
    try {
        result = spawnSync('powershell.exe', ['-NoProfile', '-Command', command], {
            encoding: 'utf8',
            windowsHide: true,
            timeout: 1200,
        });
    } catch (_) {
        return null;
    }
    if (!result || result.status !== 0) return null;
    const output = _asString(result.stdout).trim();
    if (!output) return null;
    const parts = output.split(',');
    if (parts.length < 2) return null;
    const workingSetBytes = Number(parts[0]);
    const cpuSeconds = Number(parts[1]);
    if (!Number.isFinite(workingSetBytes) || !Number.isFinite(cpuSeconds)) return null;
    return { workingSetBytes, cpuSeconds };
}

function _startResourceWatchdog(record) {
    const limits = record && record.resourceLimits ? record.resourceLimits : null;
    if (!limits) return;
    _clearResourceWatchdog(record);

    const intervalMs = Number.isInteger(limits.sampleIntervalMs) && limits.sampleIntervalMs >= 100
        ? limits.sampleIntervalMs
        : DEFAULT_RESOURCE_SAMPLE_INTERVAL_MS;

    record.resourceWatchdogTimer = setInterval(() => {
        if (!record.proc || record.state === 'stopped' || record.state === 'disabled') {
            return;
        }

        const sample = _readProcessResourceSample(record.pid);
        if (!sample) return;

        let breached = null;
        if (Number.isInteger(limits.maxWorkingSetBytes) && limits.maxWorkingSetBytes > 0 && sample.workingSetBytes > limits.maxWorkingSetBytes) {
            breached = {
                metric: 'working_set',
                value: sample.workingSetBytes,
                limit: limits.maxWorkingSetBytes,
            };
        } else if (Number.isFinite(limits.maxCpuSeconds) && limits.maxCpuSeconds > 0 && sample.cpuSeconds > limits.maxCpuSeconds) {
            breached = {
                metric: 'cpu_seconds',
                value: sample.cpuSeconds,
                limit: limits.maxCpuSeconds,
            };
        }

        if (!breached) return;

        record.state = 'disabled';
        record.shutdownOrigin = 'resource_watchdog';
        _failPending(record, 'Extension disabled due to resource limits');
        logEvent('DISABLED', {
            extension: record.name,
            reason: 'resource_limits_exceeded',
            metric: breached.metric,
            value: breached.value,
            limit: breached.limit,
        });
        if (record.proc) {
            try { record.proc.kill(); } catch (_) {}
        }
    }, intervalMs);

    if (record.resourceWatchdogTimer && typeof record.resourceWatchdogTimer.unref === 'function') {
        record.resourceWatchdogTimer.unref();
    }
}

function _scheduleRestart(id, record) {
    record.state = 'crashed';
    record.retries++;
    if (record.retries > MAX_RETRIES) {
        _clearResourceWatchdog(record);
        record.state = 'disabled';
        logEvent('DISABLED', { extension: record.name, reason: 'max_retries_exceeded' });
        _failPending(record, 'Extension ' + record.name + ' disabled (max retries exceeded)');
        return;
    }
    const delayMs = BACKOFF_MS[record.retries - 1] || 2000;
    logEvent('RESTART', { extension: record.name, attempt: record.retries });
    setTimeout(() => {
        if (record.state !== 'disabled' && record.state !== 'stopped') {
            _doSpawn(id, record);
        }
    }, delayMs);
}

// ─── Trust Anchor Store ──────────────────────────────────────────────────────
// Loaded once from trust-anchors.json at the extension-host root.  Each entry
// in `anchors` is keyId -> PEM public key.  Options and env-var overrides always
// take precedence; the file is the last-resort bootstrap source.
const TRUST_ANCHORS_FILE = path.join(__dirname, 'trust-anchors.json');
let _trustAnchorsFromFile = null;
let _trustAnchorsFileLoaded = false;

function _loadTrustAnchorsFile() {
    if (_trustAnchorsFileLoaded) return _trustAnchorsFromFile;
    _trustAnchorsFileLoaded = true;
    try {
        const raw = fs.readFileSync(TRUST_ANCHORS_FILE, 'utf8');
        const parsed = JSON.parse(raw);
        if (parsed && typeof parsed === 'object' && parsed.anchors && typeof parsed.anchors === 'object') {
            _trustAnchorsFromFile = parsed.anchors;
        } else {
            _trustAnchorsFromFile = {};
        }
    } catch (_) {
        _trustAnchorsFromFile = {};
    }
    return _trustAnchorsFromFile;
}

function _policyHash(policy) {
    const raw = JSON.stringify(policy || {});
    return crypto.createHash('sha256').update(raw, 'utf8').digest('hex').slice(0, 16);
}

function _resolveManifestTrustAnchors(options = {}) {
    if (options.manifestTrustAnchors && typeof options.manifestTrustAnchors === 'object') {
        return options.manifestTrustAnchors;
    }
    if (typeof process.env.RAWRXD_MANIFEST_TRUST_ANCHORS === 'string' && process.env.RAWRXD_MANIFEST_TRUST_ANCHORS) {
        try {
            const parsed = JSON.parse(process.env.RAWRXD_MANIFEST_TRUST_ANCHORS);
            if (parsed && typeof parsed === 'object') {
                return parsed;
            }
        } catch (_) {}
    }
    // Last resort: trust-anchors.json on disk
    return { ..._loadTrustAnchorsFile() };
}

function _resolvePreExecutionSigner(options = {}) {
    const keyId = typeof options.preExecutionSigningKeyId === 'string' && options.preExecutionSigningKeyId
        ? options.preExecutionSigningKeyId
        : (typeof process.env.RAWRXD_AUDIT_SIGNING_KEY_ID === 'string' ? process.env.RAWRXD_AUDIT_SIGNING_KEY_ID.trim() : '');
    const privateKeyPem = typeof options.preExecutionSigningPrivateKeyPem === 'string' && options.preExecutionSigningPrivateKeyPem
        ? options.preExecutionSigningPrivateKeyPem
        : (typeof process.env.RAWRXD_AUDIT_SIGNING_PRIVATE_KEY === 'string' ? process.env.RAWRXD_AUDIT_SIGNING_PRIVATE_KEY : '');
    if (!keyId || !privateKeyPem) {
        return null;
    }
    try {
        return {
            keyId,
            privateKey: crypto.createPrivateKey(privateKeyPem),
        };
    } catch (_) {
        return null;
    }
}

function _verifyManifestAtIngress(extensionManifest, trustAnchors) {
    if (!extensionManifest) {
        return { ok: true, hash: 'none', keyId: 'none' };
    }

    const verification = verifyManifestSignature(extensionManifest, trustAnchors);
    if (!verification.ok) {
        throw new Error('manifest_signature_failed:' + verification.error);
    }
    return verification;
}

function _buildRequestEnvelope(record, type, payload, options = {}) {
    const requestId = `${record.name}-${Date.now()}-${++record._seq}`;
    return {
        version: IPC_SCHEMA_VERSION,
        kind: 'request',
        requestId,
        type,
        payload,
        timestamp: new Date().toISOString(),
        source: options.source || 'server.js',
        target: record.name,
    };
}

function _buildResponseEnvelope(request, success, result, error, source) {
    const envelope = {
        version: IPC_SCHEMA_VERSION,
        kind: 'response',
        requestId: request.requestId,
        type: request.type,
        success,
        result,
        error,
        timestamp: new Date().toISOString(),
        source: source || 'extension-host-manager',
    };
    if (request && request.lineageId) {
        envelope.lineageId = request.lineageId;
    }
    return envelope;
}

function _normalizeCapabilityName(type) {
    const value = String(type || '').toLowerCase();
    if (value.startsWith('capability.')) {
        return value.slice('capability.'.length);
    }
    return value;
}

function _parseSyncPeerEntry(entry) {
    if (!entry) return null;
    if (typeof entry === 'string') {
        const trimmed = entry.trim();
        if (!trimmed) return null;
        return { id: trimmed, kind: 'url', endpoint: trimmed };
    }
    if (typeof entry !== 'object') return null;
    const kind = _asString(entry.kind || entry.type, 'url').toLowerCase();
    if (kind === 'extension') {
        const id = _asString(entry.id).trim();
        if (!id) return null;
        return { id, kind: 'extension' };
    }
    const endpoint = _asString(entry.url || entry.endpoint || entry.id).trim();
    if (!endpoint) return null;
    return {
        id: _asString(entry.id, endpoint).trim() || endpoint,
        kind: 'url',
        endpoint,
    };
}

const SYNC_PEER_TABLE_SIZE = 4;

function configureSyncPeers(peers) {
    const raw = Array.isArray(peers) ? peers : [];
    const parsed = raw
        .map((entry) => _parseSyncPeerEntry(entry))
        .filter((entry) => !!entry)
        .slice(0, SYNC_PEER_TABLE_SIZE);
    syncPeerRegistry = parsed;
    return parsed.length;
}

function _probePeerUrl(url, timeoutMs) {
    return new Promise((resolve) => {
        const startedAt = Date.now();
        let parsed;
        try {
            parsed = new URL(url);
        } catch (_) {
            resolve({ state: 'invalid', latencyMs: Date.now() - startedAt, error: 'invalid_url' });
            return;
        }

        const lib = parsed.protocol === 'https:' ? https : http;
        const req = lib.request({
            protocol: parsed.protocol,
            hostname: parsed.hostname,
            port: parsed.port || undefined,
            path: parsed.pathname || '/',
            method: 'GET',
            timeout: timeoutMs,
        }, (res) => {
            const code = Number(res.statusCode || 0);
            res.resume();
            resolve({
                state: code >= 200 && code < 500 ? 'healthy' : 'degraded',
                latencyMs: Date.now() - startedAt,
                statusCode: code,
            });
        });

        req.on('timeout', () => {
            req.destroy(new Error('timeout'));
        });
        req.on('error', (error) => {
            resolve({
                state: 'unreachable',
                latencyMs: Date.now() - startedAt,
                error: error && error.message ? error.message : 'probe_error',
            });
        });
        req.end();
    });
}

async function discoverSyncPeers(options = {}) {
    const timeoutMs = Number.isInteger(options.timeoutMs) && options.timeoutMs > 0
        ? options.timeoutMs
        : SYNC_PEER_PROBE_TIMEOUT_MS;
    const peers = Array.isArray(options.peers) && options.peers.length > 0
        ? options.peers
        : syncPeerRegistry;

    const rows = [];
    for (const rawPeer of peers) {
        const peer = _parseSyncPeerEntry(rawPeer);
        if (!peer) continue;

        if (peer.kind === 'extension') {
            const status = extensions.get(peer.id);
            rows.push({
                id: peer.id,
                kind: 'extension',
                state: status && status.state === 'ready' ? 'healthy' : 'unreachable',
                latencyMs: 0,
                error: status && status.state === 'ready' ? '' : 'extension_not_ready',
            });
            continue;
        }

        const probe = await _probePeerUrl(peer.endpoint, timeoutMs);
        rows.push({
            id: peer.id,
            kind: 'url',
            endpoint: peer.endpoint,
            state: probe.state,
            latencyMs: Number.isFinite(probe.latencyMs) ? probe.latencyMs : 0,
            statusCode: Number.isInteger(probe.statusCode) ? probe.statusCode : 0,
            error: _asString(probe.error),
        });
    }

    return {
        ts: new Date().toISOString(),
        total: rows.length,
        healthy: rows.filter((row) => row.state === 'healthy').length,
        peers: rows,
    };
}

function _validateResponseEnvelope(msg) {
    if (!msg || typeof msg !== 'object') return 'response_not_object';
    if (msg.version !== IPC_SCHEMA_VERSION) return 'unsupported_version';
    if (msg.kind !== 'response') return 'invalid_kind';
    if (typeof msg.requestId !== 'string' || !msg.requestId) return 'missing_request_id';
    if (typeof msg.type !== 'string' || !msg.type) return 'missing_type';
    if (typeof msg.timestamp !== 'string' || !msg.timestamp) return 'missing_timestamp';
    if (typeof msg.source !== 'string' || !msg.source) return 'missing_source';
    if (typeof msg.success !== 'boolean') return 'missing_success';
    if (!msg.success && typeof msg.error !== 'string' && (!msg.error || typeof msg.error.code !== 'string')) return 'missing_error';
    return null;
}

function _validateRequestEnvelope(msg) {
    if (!msg || typeof msg !== 'object') return 'request_not_object';
    if (msg.version !== IPC_SCHEMA_VERSION) return 'unsupported_version';
    if (msg.kind !== 'request') return 'invalid_kind';
    if (typeof msg.requestId !== 'string' || !msg.requestId) return 'missing_request_id';
    if (typeof msg.type !== 'string' || !msg.type) return 'missing_type';
    if (typeof msg.timestamp !== 'string' || !msg.timestamp) return 'missing_timestamp';
    if (typeof msg.source !== 'string' || !msg.source) return 'missing_source';
    return null;
}

function _writeEnvelope(record, envelope) {
    if (!record.proc || !record.proc.stdin || record.proc.stdin.destroyed) {
        throw new Error('Extension process is not writable');
    }
    const msgStr = JSON.stringify(envelope) + '\n';
    if (Buffer.byteLength(msgStr) > MAX_PAYLOAD_BYTES) {
        throw new Error('Payload exceeds ' + MAX_PAYLOAD_BYTES + ' byte IPC limit');
    }
    record.proc.stdin.write(msgStr);
}

function _rejectPending(record, requestId, error) {
    const pending = record.pendingRequests.get(requestId);
    if (!pending) return;
    record.pendingRequests.delete(requestId);
    clearTimeout(pending.timer);
    pending.reject(error instanceof Error ? error : new Error(String(error)));
}

function _dispatchRequest(record, envelope, pending, attempt) {
    const timeoutMs = pending.timeoutMs;
    const timer = setTimeout(() => {
        record.pendingRequests.delete(envelope.requestId);
        const canRetry = attempt < pending.retryCount;
        logEvent('IPC_FAIL', {
            extension: record.name,
            type: envelope.type,
            error: canRetry ? 'timeout_retrying' : 'timeout',
            requestId: envelope.requestId,
            attempt: attempt + 1,
        });
        if (canRetry) {
            _dispatchRequest(record, envelope, pending, attempt + 1);
            return;
        }
        pending.reject(new Error('IPC timeout after ' + timeoutMs + 'ms'));
    }, timeoutMs);

    record.pendingRequests.set(envelope.requestId, { ...pending, timer });
    _writeEnvelope(record, envelope);
}

function _createCapabilityRuntime(record) {
    return {
        fs: {
            async read(filePath) {
                return fs.promises.readFile(filePath, 'utf8');
            },
            async write(filePath, data) {
                const op = routeFileWrite({
                    path: filePath,
                    content: String(data || ''),
                    operationId: `${record.name}:${Date.now()}:${filePath}`,
                });
                return {
                    written: !op.skipped,
                    path: filePath,
                    operationId: op.operationId,
                    route: op.action,
                    skipped: op.skipped,
                    idempotencyHash: op.idempotencyHash,
                };
            },
        },
        net: {
            async request(req) {
                return new Promise((resolve, reject) => {
                    const parsed = new URL(req.url);
                    const client = parsed.protocol === 'https:' ? https : http;
                    const method = typeof req.method === 'string' ? req.method.toUpperCase() : 'GET';
                    const headers = req.headers && typeof req.headers === 'object' ? req.headers : {};
                    const body = req.body !== undefined ? String(req.body) : '';
                    const request = client.request({
                        protocol: parsed.protocol,
                        hostname: parsed.hostname,
                        port: parsed.port || undefined,
                        path: parsed.pathname + parsed.search,
                        method,
                        headers,
                    }, (response) => {
                        const chunks = [];
                        response.on('data', (chunk) => chunks.push(Buffer.from(chunk)));
                        response.on('end', () => {
                            resolve({
                                statusCode: response.statusCode || 0,
                                headers: response.headers || {},
                                body: Buffer.concat(chunks).toString('utf8'),
                            });
                        });
                    });
                    request.setTimeout(CAPABILITY_TIMEOUT_MS, () => {
                        request.destroy(new Error('network timeout'));
                    });
                    request.on('error', reject);
                    if (method !== 'GET' && body) {
                        request.write(body);
                    }
                    request.end();
                });
            },
        },
        process: {
            async exec(command, args) {
                return new Promise((resolve, reject) => {
                    const child = spawn(command, Array.isArray(args) ? args : [], {
                        stdio: ['ignore', 'pipe', 'pipe'],
                        windowsHide: true,
                    });
                    const stdout = [];
                    const stderr = [];
                    let settled = false;
                    const timer = setTimeout(() => {
                        if (settled) return;
                        settled = true;
                        try { child.kill(); } catch (_) {}
                        reject(new Error('process timeout'));
                    }, CAPABILITY_TIMEOUT_MS);
                    child.stdout.on('data', (chunk) => stdout.push(Buffer.from(chunk)));
                    child.stderr.on('data', (chunk) => stderr.push(Buffer.from(chunk)));
                    child.on('error', (error) => {
                        if (settled) return;
                        settled = true;
                        clearTimeout(timer);
                        reject(error);
                    });
                    child.on('exit', (code) => {
                        if (settled) return;
                        settled = true;
                        clearTimeout(timer);
                        resolve({
                            code: code === null ? -1 : code,
                            stdout: Buffer.concat(stdout).toString('utf8'),
                            stderr: Buffer.concat(stderr).toString('utf8'),
                        });
                    });
                });
            },
        },
        kv: {
            async get(key) {
                return record.kvStore.has(String(key)) ? record.kvStore.get(String(key)) : null;
            },
            async set(key, value) {
                record.kvStore.set(String(key), value);
                return { ok: true, key: String(key) };
            },
            async delete(key) {
                return { ok: true, deleted: record.kvStore.delete(String(key)) };
            },
        },
        addon: {
            async load(name) {
                return { loaded: true, name: String(name || '') };
            },
        },
        ui: {
            async notify(message, level) {
                logEvent('UI_NOTIFY', {
                    extension: record.name,
                    level: String(level || 'info'),
                    message: String(message || ''),
                });
                return { shown: true, message: String(message || '') };
            },
        },
        cmd: {
            async execute(command, args) {
                throw new Error('unsupported_or_unregistered_command:' + String(command || ''));
            },
        },
        info() {
            const roots = record.workspaceRoot ? [record.workspaceRoot] : [];
            const workspaceIndex = roots.length > 0
                ? workspaceIndexCache.getIndex(roots, { maxFiles: 2500 })
                : { source: 'none', updatedAt: '', files: [], roots: [] };
            return {
                id: record.id,
                name: record.name,
                version: 'day9-capability-abi',
                workspace: {
                    path: record.workspaceRoot || '',
                },
                workspaceIndex: {
                    source: workspaceIndex.source,
                    updatedAt: workspaceIndex.updatedAt,
                    fileCount: Array.isArray(workspaceIndex.files) ? workspaceIndex.files.length : 0,
                },
            };
        },
    };
}

async function _handleCapabilityRequest(record, msg) {
    const lineageId = msg && typeof msg.lineageId === 'string' && msg.lineageId
        ? msg.lineageId
        : (typeof crypto.randomUUID === 'function' ? crypto.randomUUID() : crypto.randomBytes(16).toString('hex'));
    msg.lineageId = lineageId;
    const trace = createTraceContext({
        extensionId: record.name,
        requestId: msg.requestId,
        lineageId,
        capability: _normalizeCapabilityName(msg.type),
        argsHash: hashArgs(msg.payload || {}),
        policyDigest: record.policyDigest || '',
    });
    trace.manifestHash = record && record.manifestTrust && typeof record.manifestTrust.hash === 'string'
        ? record.manifestTrust.hash
        : 'none';
    _safeAuditEmit(emitRequest, trace);

    try {
        const broker = _createBrokerForRecord(record);
        const decision = broker.request(msg.type, msg.payload || {});
        if (!decision.ok) {
            throw Object.assign(new Error(decision.code || 'capability_error'), {
                detail: decision.error || null,
            });
        }

        _assertPreExecutionAuthorized(record, trace, 'allow', 'policy_allow');
        _safeAuditEmit(emitDecision, trace, 'allow', 'policy_allow');
        _safeAuditEmit(emitExecuteStart, trace);

        const runtime = _createCapabilityRuntime(record);
        const capability = decision.descriptor.capability;
        const args = decision.descriptor.args || {};
        let result;

        switch (capability) {
        case 'fs.read':
            result = await runtime.fs.read(args.path);
            break;
        case 'fs.write':
            result = await runtime.fs.write(args.path, args.data);
            break;
        case 'net.request':
            result = await runtime.net.request(args);
            break;
        case 'process.exec':
            result = await runtime.process.exec(args.command, args.args);
            break;
        case 'kv.get':
            result = { value: await runtime.kv.get(args.key) };
            break;
        case 'kv.set':
            result = await runtime.kv.set(args.key, args.value);
            break;
        case 'kv.delete':
            result = await runtime.kv.delete(args.key);
            break;
        case 'runtime.info':
            result = runtime.info();
            break;
        case 'addon.load':
            result = await runtime.addon.load(args.name);
            break;
        case 'ui.notify':
            result = await runtime.ui.notify(args.message, args.level);
            break;
        case 'cmd.execute':
            result = await runtime.cmd.execute(args.command, args.args);
            break;
        default:
            throw Object.assign(new Error('unknown_capability'), { detail: capability });
        }

        _safeAuditEmit(emitExecuteResult, trace, true, 'ok');

        _writeEnvelope(record, _buildResponseEnvelope(msg, true, result, undefined, 'broker'));
    } catch (error) {
        const denyCodePrefixes = [
            'pre_execution_auth_failed',
            'invalid_policy',
            'manifest_negotiation_failed',
            'process_manifest_denied',
            'network_manifest_denied',
            'file_read_denied',
            'file_write_denied',
            'network_denied',
            'network_private_denied',
            'process_denied',
            'process_spawn_denied',
            'storage_denied',
            'native_addon_denied',
            'unknown_capability',
            'invalid_capability_request',
        ];
        const baseCode = error && error.message ? error.message : 'capability_error';
        const detail = error && typeof error.detail === 'string' && error.detail
            ? error.detail
            : null;
        const code = detail && !baseCode.endsWith(':' + detail)
            ? `${baseCode}:${detail}`
            : baseCode;
        const isDeny = denyCodePrefixes.some((prefix) => baseCode === prefix || baseCode.startsWith(prefix + ':'));
        if (isDeny) {
            _safeAuditEmit(emitDecision, trace, 'deny', code);
        } else {
            if (!trace.decisionTs) {
                _safeAuditEmit(emitDecision, trace, 'allow', 'policy_allow');
            }
            _safeAuditEmit(emitExecuteResult, trace, false, code);
        }

        _writeEnvelope(record, _buildResponseEnvelope(msg, false, undefined, {
            code,
            detail,
        }, 'broker'));
    }
}

function _handleChildLine(id, record, line) {
    let msg;
    try {
        msg = JSON.parse(line);
    } catch (_) {
        logEvent('IPC_FAIL', {
            extension: record.name,
            type: 'parse',
            error: 'invalid_json_line',
        });
        return;
    }

    if (msg.kind === 'request') {
        const requestError = _validateRequestEnvelope(msg);
        if (requestError) {
            logEvent('IPC_FAIL', {
                extension: record.name,
                type: msg && typeof msg.type === 'string' ? msg.type : 'unknown',
                error: requestError,
                requestId: msg && msg.requestId ? msg.requestId : 'missing',
            });
            return;
        }
        _handleCapabilityRequest(record, msg).catch((error) => {
            logEvent('IPC_FAIL', {
                extension: record.name,
                type: msg.type,
                error: error.message,
                requestId: msg.requestId,
            });
        });
        return;
    }

    const validationError = _validateResponseEnvelope(msg);
    if (validationError) {
        logEvent('IPC_FAIL', {
            extension: record.name,
            type: msg && typeof msg.type === 'string' ? msg.type : 'unknown',
            error: validationError,
            requestId: msg && msg.requestId ? msg.requestId : 'missing',
        });
        return;
    }

    if (!record.pendingRequests.has(msg.requestId)) {
        logEvent('IPC_FAIL', {
            extension: record.name,
            type: msg.type,
            error: 'unknown_request_id',
            requestId: msg.requestId,
        });
        return;
    }

    const pending = record.pendingRequests.get(msg.requestId);
    record.pendingRequests.delete(msg.requestId);
    clearTimeout(pending.timer);

    if (msg.success) {
        pending.resolve(msg.result !== undefined ? msg.result : msg);
    } else {
        const errorCode = typeof msg.error === 'string'
            ? msg.error
            : (msg.error && typeof msg.error.code === 'string' ? msg.error.code : 'unknown_error');
        const normalized = _normalizeLspDiagnostic(errorCode);
        pending.reject(new Error(normalized.error || errorCode));
    }
}

function _handleSandboxViolation(id, record, line) {
    const prefix = 'SECURITY_VIOLATION ';
    if (!line.startsWith(prefix)) return false;

    let detail = { code: 'unknown_violation', detail: line.slice(prefix.length) };
    try {
        detail = JSON.parse(line.slice(prefix.length));
    } catch (_) {}

    record.sandboxViolations++;
    logEvent('SECURITY_DENY', {
        extension: record.name,
        code: detail.code || 'unknown_violation',
        detail: detail.detail || '',
        count: record.sandboxViolations,
    });

    if (record.sandboxViolations >= record.sandboxViolationLimit && record.state !== 'disabled') {
        _clearResourceWatchdog(record);
        record.state = 'disabled';
        record.shutdownOrigin = 'security_violation';
        _failPending(record, 'Extension disabled due to sandbox violations');
        logEvent('DISABLED', { extension: record.name, reason: 'sandbox_violations_exceeded' });
        if (record.proc) {
            try { record.proc.kill(); } catch (_) {}
        }
    }
    return true;
}

function _doSpawn(id, record) {
    _clearResourceWatchdog(record);
    record.state    = 'starting';
    record.spawnTime = Date.now();
    record.shutdownLogged = false;
    record.shutdownOrigin = '';

    let proc;
    try {
        proc = spawn(record.extensionPath, record.spawnArgs || [], {
            stdio: ['pipe', 'pipe', 'pipe'],
            windowsHide: true,
            env: record.spawnEnv || process.env,
        });
    } catch (spawnErr) {
        // spawn() threw synchronously (e.g. invalid path type)
        logEvent('CRASH', {
            extension: record.name, pid: 0,
            exit_code: -1, attempt: record.retries + 1
        });
        _scheduleRestart(id, record);
        return;
    }

    record.proc = proc;
    record.pid  = proc.pid || 0;
    logEvent('SPAWN', {
        extension: record.name,
        pid: record.pid,
        policy: record.sandboxPolicy ? _policyHash(record.sandboxPolicy) : 'none',
        manifest: record.extensionManifest ? _policyHash(record.extensionManifest) : 'none',
        manifest_sig: record.manifestTrust && record.manifestTrust.hash ? record.manifestTrust.hash.slice(0, 12) : 'none',
    });
    _startResourceWatchdog(record);

    // ── stdout: JSON-line responses ──
    let stdoutBuf = '';
    proc.stdout.on('data', (chunk) => {
        stdoutBuf += chunk.toString('utf8');
        const lines = stdoutBuf.split('\n');
        stdoutBuf = lines.pop();   // keep incomplete trailing fragment
        for (const line of lines) {
            const t = line.trim();
            if (t) _handleChildLine(id, record, t);
        }
    });

    // ── stderr: forward to lifecycle log ──
    let stderrBuf = '';
    proc.stderr.on('data', (chunk) => {
        stderrBuf += chunk.toString('utf8');
        const lines = stderrBuf.split('\n');
        stderrBuf = lines.pop();
        for (const line of lines) {
            const msg = line.trim().slice(0, 500);
            if (!msg) continue;
            if (_handleSandboxViolation(id, record, msg)) continue;
            const normalized = _normalizeLspDiagnostic(msg);
            logEvent('IPC_FAIL', {
                extension: record.name,
                type: 'stderr',
                error: normalized.error || msg,
                lsp_error: normalized.name || 'none',
            });
        }
    });

    // ── spawn event: process actually started ──
    proc.on('spawn', () => {
        record.lastBootMs = Date.now() - record.spawnTime;
        record.state = 'ready';
        logEvent('READY', {
            extension: record.name,
            pid: record.pid,
            boot_ms: record.lastBootMs,
        });
    });

    // ── error: binary not found, permission denied, etc. ──
    proc.on('error', (err) => {
        _clearResourceWatchdog(record);
        if (record.state === 'stopped') return;
        logEvent('CRASH', {
            extension: record.name, pid: record.pid,
            exit_code: -1, attempt: record.retries + 1
        });
        _failPending(record, 'Process error: ' + err.message);
        _scheduleRestart(id, record);
    });

    // ── exit: unexpected termination ──
    proc.on('exit', (code, signal) => {
        _clearResourceWatchdog(record);
        if (record.state === 'stopped' || record.state === 'disabled') {
            const origin = record.shutdownOrigin || (record.state === 'disabled' ? 'security_violation' : 'child_exit');
            _emitShutdown(record, origin);
            return;
        }
        const exitCode = code !== null ? code : (signal || -1);
        logEvent('CRASH', {
            extension: record.name, pid: record.pid,
            exit_code: exitCode, attempt: record.retries + 1
        });
        _failPending(record, 'Process exited with code ' + exitCode);
        _scheduleRestart(id, record);
    });
}

// ─── Public API ──────────────────────────────────────────────────────────────

/**
 * Spawn an extension as an isolated child process.
 * The child communicates over stdin/stdout using newline-delimited JSON:
 *   IN  → { version, kind:'request', requestId, type, payload, timestamp, source, target }
 *   OUT ← { version, kind:'response', requestId, type, success, result|error, timestamp, source }
 * Crashes are detected, logged, and the process is restarted up to MAX_RETRIES (3) times.
 *
 * @param  {string} extensionPath  Absolute path to the extension executable.
 * @param  {{ args?: string[], env?: Object, name?: string, sandboxPolicy?: Object, extensionManifest?: Object, manifestTrustAnchors?: Object, requirePreExecutionAuth?: boolean, requireSignatures?: boolean, signatureTrustAnchors?: Object, preExecutionSigningKeyId?: string, preExecutionSigningPrivateKeyPem?: string }} [options]
 * @returns {string}               Stable id for subsequent calls.
 */
function spawnExtensionHost(extensionPath, options = {}) {
    _runAuditSinkSelfTest();

    if (typeof extensionPath !== 'string' || !extensionPath) {
        throw new TypeError('extensionPath must be a non-empty string');
    }
    const name = typeof options.name === 'string' && options.name
        ? options.name
        : path.basename(extensionPath, path.extname(extensionPath));
    const id   = name + '_' + Date.now() + '_' + Math.random().toString(36).slice(2, 7);
    const trustAnchors = _resolveManifestTrustAnchors(options);
    const manifestTrust = _verifyManifestAtIngress(options.extensionManifest || null, trustAnchors);
    const preExecutionSigner = _resolvePreExecutionSigner(options);
    let sandboxPolicy = options.sandboxPolicy || null;
    let invalidSandboxPolicy = null;
    if (!sandboxPolicy && options.env && typeof options.env.RAWRXD_SANDBOX_POLICY === 'string') {
        try {
            sandboxPolicy = parsePolicy(options.env.RAWRXD_SANDBOX_POLICY);
        } catch (error) {
            invalidSandboxPolicy = error.message;
        }
    }

    const policyDigest = sandboxPolicy ? _policyHash(sandboxPolicy) : '';

    const record = {
        id,
        name,
        extensionPath,
        spawnArgs: Array.isArray(options.args) ? options.args.slice() : [],
        spawnEnv: options.env ? { ...process.env, ...options.env } : { ...process.env },
        sandboxPolicy,
        policyDigest,
        invalidSandboxPolicy,
        sandboxViolations: 0,
        sandboxViolationLimit: sandboxPolicy && Number.isInteger(sandboxPolicy.violationLimit)
            ? Math.max(1, sandboxPolicy.violationLimit)
            : MAX_SANDBOX_VIOLATIONS,
        extensionManifest: options.extensionManifest || null,
        manifestTrust,
        preExecutionAuth: {
            requirePreExecutionAuth: !!options.requirePreExecutionAuth,
            requireSignatures: !!options.requireSignatures,
            signatureTrustAnchors: options.signatureTrustAnchors && typeof options.signatureTrustAnchors === 'object'
                ? options.signatureTrustAnchors
                : null,
            signer: preExecutionSigner,
            authContext: options.preExecutionAuthContext && typeof options.preExecutionAuthContext === 'object'
                ? options.preExecutionAuthContext
                : null,
        },
        proc: null,
        pid: 0,
        state: 'starting',
        retries: 0,
        lastBootMs: 0,
        spawnTime: 0,
        pendingRequests: new Map(),
        kvStore: new Map(),
        workspaceRoot: sandboxPolicy && Array.isArray(sandboxPolicy.readRoots) && sandboxPolicy.readRoots.length > 0
            ? sandboxPolicy.readRoots[0]
            : '',
        loopController: createLoopController({
            id: name + '-loop',
            maxDepth: Number.isInteger(options.maxAutonomyDepth)
                ? options.maxAutonomyDepth
                : (Number.isInteger(Number(process.env.RAWRXD_AUTONOMY_MAX_DEPTH))
                    ? Number(process.env.RAWRXD_AUTONOMY_MAX_DEPTH)
                    : DEFAULT_MAX_AUTONOMY_DEPTH),
        }),
        resourceLimits: options.resourceLimits && typeof options.resourceLimits === 'object'
            ? {
                maxWorkingSetBytes: Number.isInteger(options.resourceLimits.maxWorkingSetBytes) && options.resourceLimits.maxWorkingSetBytes > 0
                    ? options.resourceLimits.maxWorkingSetBytes
                    : null,
                maxCpuSeconds: Number.isFinite(options.resourceLimits.maxCpuSeconds) && options.resourceLimits.maxCpuSeconds > 0
                    ? Number(options.resourceLimits.maxCpuSeconds)
                    : null,
                sampleIntervalMs: Number.isInteger(options.resourceLimits.sampleIntervalMs) && options.resourceLimits.sampleIntervalMs >= 100
                    ? options.resourceLimits.sampleIntervalMs
                    : DEFAULT_RESOURCE_SAMPLE_INTERVAL_MS,
            }
            : null,
        resourceWatchdogTimer: null,
        backpressure: {
            mode: 'normal',
            lane: 'normal',
            signal: '',
            telemetry: null,
            updatedAt: '',
        },
        raceTelemetry: {
            racesStarted: 0,
            primaryWins: 0,
            speculativeWins: 0,
            timeoutFailures: 0,
        },
        shutdownLogged: false,
        shutdownOrigin: '',
        _seq: 0,
    };

    if (record.sandboxPolicy) {
        record.spawnEnv.RAWRXD_SANDBOX_POLICY = JSON.stringify(record.sandboxPolicy);
    }
    if (record.extensionManifest) {
        record.spawnEnv.RAWRXD_EXTENSION_MANIFEST = JSON.stringify(record.extensionManifest);
    }
    extensions.set(id, record);
    _doSpawn(id, record);
    return id;
}

/**
 * Send a typed message to an extension and await its JSON response.
 * @param  {string}  id      Extension id from spawnExtensionHost.
 * @param  {string}  type    Message type.
 * @param  {*}       payload Serialisable payload (enforced < MAX_PAYLOAD_BYTES).
 * @param  {{ timeoutMs?: number, retryCount?: number, source?: string }} [options]
 * @returns {Promise<*>}
 */
function sendToExtension(id, type, payload, options = {}) {
    return new Promise((resolve, reject) => {
        const record = extensions.get(id);
        if (!record) return reject(new Error('Extension id not found: ' + id));
        if (record.state === 'disabled') {
            return reject(new Error('Extension ' + record.name + ' is disabled'));
        }
        if (record.state !== 'ready') {
            return reject(new Error('Extension ' + record.name + ' not ready (state: ' + record.state + ')'));
        }

        if (typeof type !== 'string' || !type || type.length > 128) {
            return reject(new Error('type must be a non-empty string up to 128 chars'));
        }

        const timeoutMs = Number.isInteger(options.timeoutMs) && options.timeoutMs > 0
            ? Math.min(options.timeoutMs, IPC_TIMEOUT_MS)
            : IPC_TIMEOUT_MS;
        const retryCount = Number.isInteger(options.retryCount) && options.retryCount >= 0
            ? Math.min(options.retryCount, MAX_REQUEST_RETRIES)
            : 0;

        try {
            const startMs = Date.now();
            const envelope = _buildRequestEnvelope(record, type, payload, options);
            const lineageId = options && typeof options.lineageId === 'string' && options.lineageId
                ? options.lineageId
                : (typeof crypto.randomUUID === 'function' ? crypto.randomUUID() : crypto.randomBytes(16).toString('hex'));
            envelope.lineageId = lineageId;

            const trace = createTraceContext({
                extensionId: record.name,
                requestId: envelope.requestId,
                lineageId,
                capability: _normalizeCapabilityName(type),
                argsHash: hashArgs(payload || {}),
            });
            trace.manifestHash = record && record.manifestTrust && typeof record.manifestTrust.hash === 'string'
                ? record.manifestTrust.hash
                : 'none';
            _safeAuditEmit(emitRequest, trace);
            _assertPreExecutionAuthorized(record, trace, 'allow', 'policy_allow');
            _safeAuditEmit(emitDecision, trace, 'allow', 'policy_allow');
            _safeAuditEmit(emitExecuteStart, trace);

            const loop = options.loopController || record.loopController;
            let loopActive = false;
            if (loop) {
                try {
                    loop.beginStep(options.stepToken || `${type}:${envelope.requestId}`);
                    loop.transition('EXECUTING');
                    loop.checkpoint('tool_batch_start', { extension: record.name, type, requestId: envelope.requestId });
                    loopActive = true;
                } catch (loopError) {
                    if (!(options.ignoreLoopBusy === true && _isLoopBusyError(loopError))) {
                        throw loopError;
                    }
                }
            }

            const msgStr = JSON.stringify(envelope) + '\n';

            if (Buffer.byteLength(msgStr) > MAX_PAYLOAD_BYTES) {
                return reject(new Error('Payload exceeds ' + MAX_PAYLOAD_BYTES + ' byte IPC limit'));
            }

            const executeAttempt = () => new Promise((resolveAttempt, rejectAttempt) => {
                const pending = {
                    timeoutMs,
                    retryCount,
                    resolve: resolveAttempt,
                    reject: rejectAttempt,
                    timer: null,
                };
                _dispatchRequest(record, envelope, pending, 0);
            });

            executeWithBackoff(executeAttempt, {
                maxAttempts: Number.isInteger(options.maxRateLimitRetries)
                    ? Math.max(1, options.maxRateLimitRetries)
                    : MAX_RATE_LIMIT_RETRIES,
                backoffMs: RATE_LIMIT_BACKOFF_MS,
                fallbackExecutor: typeof options.fallbackExecutor === 'function'
                    ? options.fallbackExecutor
                    : null,
            }).then((execution) => {
                if (loopActive) {
                    loop.transition('VERIFYING');
                    loop.checkpoint('tool_batch_complete', {
                        extension: record.name,
                        type,
                        requestId: envelope.requestId,
                        usedFallback: !!execution.usedFallback,
                    });
                    loop.transition('PLANNING');
                }
                _safeAuditEmit(emitExecuteResult, trace, true, execution.usedFallback ? 'ok_fallback' : 'ok');
                logEvent('IPC_OK', {
                    extension: record.name,
                    type,
                    ms: Date.now() - startMs,
                    requestId: envelope.requestId,
                    fallback: execution.usedFallback ? '1' : '0',
                });
                resolve(execution.result);
            }).catch((error) => {
                if (loopActive) {
                    try {
                        loop.transition('VERIFYING');
                        loop.checkpoint('tool_batch_failed', {
                            extension: record.name,
                            type,
                            requestId: envelope.requestId,
                            error: error && error.message ? error.message : 'unknown_error',
                        });
                        loop.transition('PLANNING');
                    } catch (_) {}
                }
                if (_isRateLimitError(error)) {
                    const queued = enqueueTask(RESILIENT_QUEUE_FILE, {
                        extensionId: id,
                        extensionName: record.name,
                        type,
                        payload,
                        options: {
                            timeoutMs,
                            retryCount,
                            source: options.source || 'server.js',
                            lineageId,
                        },
                    });
                    error.message = `${error.message}:queued:${queued.id}`;
                }
                _safeAuditEmit(emitExecuteResult, trace, false, error && error.message ? error.message : 'error');
                if (options.suppressErrorLog !== true) {
                    logEvent('IPC_FAIL', {
                        extension: record.name,
                        type,
                        error: error && error.message ? error.message : 'unknown_error',
                        requestId: envelope.requestId,
                    });
                }
                reject(error);
            });
        } catch (err) {
            if (options.suppressErrorLog !== true) {
                logEvent('IPC_FAIL', { extension: record.name, type, error: err.message });
            }
            reject(err);
        }
    });
}

/**
 * Signal the bridge to activate hive offload routing for the primary sync peer.
 * Sets g_hiveOffloadActive in the MASM bridge when the peer is eligible
 * (healthy state, fresh heartbeat, global capacity >= 20%).
 * @param {string} id Extension host ID
 * @param {{ source?: string }} [telemetry]
 * @param {{ timeoutMs?: number, source?: string }} [options]
 * @returns {Promise<{ active: boolean, ack: *, unsupported: boolean }>}
 */
async function signalHiveOffload(id, telemetry = {}, options = {}) {
    const record = extensions.get(id);
    if (!record) {
        throw new Error('Extension id not found: ' + id);
    }

    const requestId = Number((Date.now() >>> 0));
    const speculativeEnabled = _isBridgeSpeculativeInferenceEnabled() && options.speculativeEnabled !== false;
    const payload = {
        requestId,
        source: _asString(telemetry && telemetry.source, 'hive-telemetry'),
        ts: new Date().toISOString(),
    };

    let unsupported = false;
    let ack = null;
    let ackConfirm = null;
    let ackConfirmUnsupported = false;
    let specResult = null;
    let winnerRequestId = requestId;
    let raceUsed = false;
    try {
        const baseTimeoutMs = Number.isInteger(options.timeoutMs) && options.timeoutMs > 0
            ? Math.min(options.timeoutMs, IPC_TIMEOUT_MS)
            : 800;

        const primaryPromise = sendToExtension(id, 'hive.offload', payload, {
            timeoutMs: baseTimeoutMs,
            retryCount: 0,
            source: options.source || 'manager.hive_offload',
        });

        let secondaryPeer = null;
        let secondaryRequestId = 0;
        let secondaryPromise = null;

        if (speculativeEnabled) {
            const topPeers = _getTopNPeersByLatency(2);
            secondaryPeer = topPeers.find((peer) => peer && peer.kind === 'extension' && peer.id !== id) || null;
            if (secondaryPeer && secondaryPeer.id && extensions.has(secondaryPeer.id)) {
                raceUsed = true;
                if (record.raceTelemetry) {
                    record.raceTelemetry.racesStarted += 1;
                }
                secondaryRequestId = Number((Date.now() >>> 0)) + 1;
                const specPayload = {
                    requestId: secondaryRequestId,
                    source: _asString(telemetry && telemetry.source, 'hive-telemetry-speculative'),
                    ts: new Date().toISOString(),
                };
                secondaryPromise = sendToExtension(secondaryPeer.id, 'hive.speculative', specPayload, {
                    timeoutMs: baseTimeoutMs,
                    retryCount: 0,
                    ignoreLoopBusy: true,
                    source: options.source || 'manager.hive_speculative',
                    suppressErrorLog: true,
                });
            }
        }

        if (secondaryPromise) {
            let winner;
            try {
                winner = await Promise.any([
                    primaryPromise.then((value) => ({ lane: 'primary', value })),
                    secondaryPromise.then((value) => ({ lane: 'speculative', value })),
                ]);
            } catch (raceError) {
                throw _unwrapPromiseAnyError(raceError);
            }

            if (winner.lane === 'primary') {
                ack = winner.value;
                winnerRequestId = requestId;
                if (record.raceTelemetry) {
                    record.raceTelemetry.primaryWins += 1;
                }
                try {
                    await sendToExtension(id, 'hive.speculative.ack', {
                        requestId: secondaryRequestId,
                        outcomeCode: 1,
                        source: _asString(telemetry && telemetry.source, 'hive-spec-race-lost'),
                        ts: new Date().toISOString(),
                    }, {
                        timeoutMs: Math.min(baseTimeoutMs, 500),
                        retryCount: 0,
                        source: options.source || 'manager.hive_speculative_ack_lost',
                    });
                } catch (_) {}
            } else {
                specResult = winner.value;
                winnerRequestId = secondaryRequestId || winnerRequestId;
                if (record.raceTelemetry) {
                    record.raceTelemetry.speculativeWins += 1;
                }
                try {
                    await sendToExtension(id, 'hive.speculative.ack', {
                        requestId: winnerRequestId,
                        outcomeCode: 0,
                        peerId: Number.isInteger(specResult && specResult.peerId)
                            ? Number(specResult.peerId)
                            : 0,
                        source: _asString(telemetry && telemetry.source, 'hive-spec-race-won'),
                        ts: new Date().toISOString(),
                    }, {
                        timeoutMs: Math.min(baseTimeoutMs, 500),
                        retryCount: 0,
                        source: options.source || 'manager.hive_speculative_ack_won',
                    });
                } catch (_) {}
            }
        } else {
            ack = await primaryPromise;
            winnerRequestId = requestId;
        }

        const outcomeCode = (ack || specResult) ? 0 : 2;
        try {
            ackConfirm = await sendToExtension(id, 'hive.offload.ack', {
                requestId: winnerRequestId,
                outcomeCode,
                source: _asString(telemetry && telemetry.source, 'hive-telemetry-ack'),
                ts: new Date().toISOString(),
            }, {
                timeoutMs: baseTimeoutMs,
                retryCount: 0,
                source: options.source || 'manager.hive_offload_ack',
            });
        } catch (ackError) {
            if (ackError && typeof ackError.message === 'string' && /unknown_capability|unsupported/i.test(ackError.message)) {
                ackConfirmUnsupported = true;
                logEvent('IPC_FAIL', {
                    extension: record.name,
                    type: 'hive.offload.ack',
                    error: 'signal_unsupported:hive.offload.ack',
                });
            } else {
                throw ackError;
            }
        }
    } catch (error) {
        if (record.raceTelemetry && raceUsed && /timeout/i.test(_asString(error && error.message))) {
            record.raceTelemetry.timeoutFailures += 1;
        }
        if (error && typeof error.message === 'string' && /unknown_capability|unsupported/i.test(error.message)) {
            unsupported = true;
            logEvent('IPC_FAIL', {
                extension: record.name,
                type: 'hive.offload',
                error: 'signal_unsupported:hive.offload',
            });
        } else {
            throw error;
        }
    }

    return {
        active: !unsupported && !!(ack || specResult),
        ack,
        ackConfirm,
        ackConfirmUnsupported,
        requestId,
        winnerRequestId,
        raceUsed,
        specResult,
        speculativeEnabled,
        unsupported,
    };
}

/**
 * Compute top N peers by lowest latency from syncPeerRegistry.
 * @param {number} topN Number of peers to return
 * @returns {Array} Array of peer records sorted by latency (lowest first)
 */
function _getTopNPeersByLatency(topN = 2) {
    if (!Array.isArray(syncPeerRegistry) || syncPeerRegistry.length === 0) {
        return [];
    }
    return syncPeerRegistry
        .filter((p) => {
            if (!p || typeof p !== 'object') return false;
            if (p.kind === 'extension') {
                const ext = extensions.get(p.id);
                return !!(ext && ext.state === 'ready');
            }
            return p.state === 'healthy';
        })
        .sort((a, b) => {
            const aLat = Number(a.latencyMs);
            const bLat = Number(b.latencyMs);
            const aNorm = Number.isFinite(aLat) ? aLat : Number.MAX_SAFE_INTEGER;
            const bNorm = Number.isFinite(bLat) ? bLat : Number.MAX_SAFE_INTEGER;
            return aNorm - bNorm;
        })
        .slice(0, Math.max(1, Number(topN) || 2));
}

/**
 * Check if bridge supports speculative hive inference.
 * @returns {boolean}
 */
function _isBridgeSpeculativeInferenceEnabled() {
    return true;
}

async function signalShedCheck(id, telemetry = {}, options = {}) {
    const record = extensions.get(id);
    if (!record) {
        throw new Error('Extension id not found: ' + id);
    }

    const payload = {
        source: _asString(telemetry && telemetry.source, 'shed-check'),
        ts: new Date().toISOString(),
    };

    let unsupported = false;
    let ack = null;
    try {
        ack = await sendToExtension(id, 'shed.check', payload, {
            timeoutMs: Number.isInteger(options.timeoutMs) && options.timeoutMs > 0
                ? Math.min(options.timeoutMs, IPC_TIMEOUT_MS)
                : 800,
            retryCount: 0,
            source: options.source || 'manager.shed_check',
        });
    } catch (error) {
        if (error && typeof error.message === 'string' && /unknown_capability|unsupported/i.test(error.message)) {
            unsupported = true;
            logEvent('IPC_FAIL', {
                extension: record.name,
                type: 'shed.check',
                error: 'signal_unsupported:shed.check',
            });
        } else {
            throw error;
        }
    }

    return {
        active: !unsupported && !!ack,
        ack,
        unsupported,
    };
}

/**
 * Signal low-VRAM pressure to an extension so it can throttle or switch lanes.
 * Persists manager-side state even if the extension does not support runtime.signal.
 * @param {string} id
 * @param {{ freeBytes?: number, usedBytes?: number, totalBytes?: number, source?: string, lane?: string }} telemetry
 * @param {{ timeoutMs?: number, source?: string }} [options]
 * @returns {Promise<{ signal: string, lane: string, ack: *, unsupported: boolean }>}
 */
async function signalLowVram(id, telemetry = {}, options = {}) {
    const record = extensions.get(id);
    if (!record) {
        throw new Error('Extension id not found: ' + id);
    }

    const lane = typeof telemetry.lane === 'string' && telemetry.lane.trim()
        ? telemetry.lane.trim()
        : 'degraded';
    const snapshot = {
        freeBytes: Number.isFinite(telemetry.freeBytes) ? Number(telemetry.freeBytes) : null,
        usedBytes: Number.isFinite(telemetry.usedBytes) ? Number(telemetry.usedBytes) : null,
        totalBytes: Number.isFinite(telemetry.totalBytes) ? Number(telemetry.totalBytes) : null,
        source: _asString(telemetry.source, 'gpu-telemetry'),
        ts: new Date().toISOString(),
    };

    record.backpressure = {
        mode: 'low_vram',
        lane,
        signal: LOW_VRAM_SIGNAL,
        telemetry: snapshot,
        updatedAt: snapshot.ts,
    };

    let unsupported = false;
    let ack = null;
    try {
        ack = await sendToExtension(id, 'runtime.signal', {
            signal: LOW_VRAM_SIGNAL,
            lane,
            telemetry: snapshot,
        }, {
            timeoutMs: Number.isInteger(options.timeoutMs) && options.timeoutMs > 0
                ? Math.min(options.timeoutMs, IPC_TIMEOUT_MS)
                : 800,
            retryCount: 0,
            source: options.source || 'manager.low_vram',
        });
    } catch (error) {
        if (error && typeof error.message === 'string' && /unknown_capability|unsupported/i.test(error.message)) {
            unsupported = true;
            logEvent('IPC_FAIL', {
                extension: record.name,
                type: 'runtime.signal',
                error: 'signal_unsupported:' + LOW_VRAM_SIGNAL,
            });
        } else {
            throw error;
        }
    }

    return {
        signal: LOW_VRAM_SIGNAL,
        lane,
        ack,
        unsupported,
    };
}

/**
 * Gracefully terminate an extension host process.
 * @param {string} id
 */
function shutdown(id) {
    const record = extensions.get(id);
    if (!record) return;
    _clearResourceWatchdog(record);
    if (!(record.state === 'disabled' && record.shutdownOrigin)) {
        record.shutdownOrigin = 'host_request';
    }
    record.state = 'stopped';
    _failPending(record, 'Extension shut down');
    if (record.proc) {
        try { record.proc.kill(); } catch (_) {}
    }
    _emitShutdown(record, record.shutdownOrigin);
    extensions.delete(id);
}

/** Shut down all managed extension processes (e.g. on IDE exit). */
function shutdownAll() {
    for (const id of [...extensions.keys()]) {
        shutdown(id);
    }
}

/**
 * Return a lightweight status snapshot for all known extensions.
 * @returns {{ id, name, pid, state, retries, lastBootMs }[]}
 */
function getStatus() {
    const list = [];
    for (const [id, r] of extensions) {
        list.push({
            id,
            name:      r.name,
            pid:       r.pid,
            state:     r.state,
            retries:   r.retries,
            lastBootMs: r.lastBootMs,
            backpressureMode: r.backpressure && r.backpressure.mode ? r.backpressure.mode : 'normal',
            backpressureLane: r.backpressure && r.backpressure.lane ? r.backpressure.lane : 'normal',
            raceTelemetry: r.raceTelemetry ? { ...r.raceTelemetry } : null,
        });
    }
    return list;
}

module.exports = {
    spawnExtensionHost,
    sendToExtension,
    signalLowVram,
    signalHiveOffload,
        signalShedCheck,
    configureSyncPeers,
    discoverSyncPeers,
    shutdown,
    shutdownAll,
    getStatus,
    logFile: LOG_FILE,
    capabilityAuditLogFile: CAPABILITY_AUDIT_LOG_FILE,
    queueFile: RESILIENT_QUEUE_FILE,
    workspaceIndexCacheFile: WORKSPACE_INDEX_CACHE_FILE,
    _test: {
        _buildRequestEnvelope,
        _validateResponseEnvelope,
        _policyHash,
        _normalizeLspDiagnostic,
        _resolveManifestTrustAnchors,
        _loadTrustAnchorsFile,
        _isRateLimitError,
        _negotiateManifestOrThrow,
        _runAuditSinkSelfTest,
        _parseSyncPeerEntry,
        _probePeerUrl,
    },
};
