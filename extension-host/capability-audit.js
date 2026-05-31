'use strict';

const fs = require('fs');
const path = require('path');
const os = require('os');
const crypto = require('crypto');
const { buildCanonicalEvent } = require('./capability-audit-schema');
const {
    computeBrokerDecisionHash,
    computeExecutionResultHash,
    computeNextEventChainHash,
    signLineageBinding,
    buildDenyReasonDigest,
} = require('./capability-lineage-binding');

const LOG_DIR = path.join(
    process.env.LOCALAPPDATA || path.join(os.homedir(), 'AppData', 'Local'),
    'RawrXD',
    'Logs'
);
const LOG_FILE = path.join(LOG_DIR, 'capability-lineage.log');
let sequenceCounter = 0;
let lineageSigner = (() => {
    const keyId = typeof process.env.RAWRXD_AUDIT_SIGNING_KEY_ID === 'string'
        ? process.env.RAWRXD_AUDIT_SIGNING_KEY_ID.trim()
        : '';
    const privateKeyPem = typeof process.env.RAWRXD_AUDIT_SIGNING_PRIVATE_KEY === 'string'
        ? process.env.RAWRXD_AUDIT_SIGNING_PRIVATE_KEY
        : '';
    if (!keyId || !privateKeyPem) return null;
    try {
        return {
            keyId,
            privateKey: crypto.createPrivateKey(privateKeyPem),
        };
    } catch (_) {
        return null;
    }
})();

function _isPlainObject(value) {
    return value !== null && typeof value === 'object' && !Array.isArray(value);
}

function stableClone(value) {
    if (Array.isArray(value)) {
        return value.map((entry) => stableClone(entry));
    }
    if (_isPlainObject(value)) {
        const out = {};
        const keys = Object.keys(value).sort();
        for (const key of keys) {
            out[key] = stableClone(value[key]);
        }
        return out;
    }
    if (typeof value === 'bigint') return value.toString();
    if (Buffer.isBuffer(value)) return value.toString('base64');
    if (value === undefined) return null;
    return value;
}

function stableStringify(value) {
    return JSON.stringify(stableClone(value));
}

function hashArgs(payload) {
    const normalized = stableStringify(payload || {});
    return crypto.createHash('sha256').update(normalized, 'utf8').digest('hex');
}

function serializeEvent(event) {
    return stableStringify(buildCanonicalEvent(event)) + '\n';
}

function appendEvent(event) {
    fs.mkdirSync(LOG_DIR, { recursive: true });
    fs.appendFileSync(LOG_FILE, serializeEvent(event), 'utf8');
}

function ensureAuditSinkWritable() {
    fs.mkdirSync(LOG_DIR, { recursive: true });
    const fd = fs.openSync(LOG_FILE, 'a');
    try {
        fs.fsyncSync(fd);
    } finally {
        fs.closeSync(fd);
    }
}

function configureLineageSigning(config) {
    if (!config) {
        lineageSigner = null;
        return;
    }
    if (!config.keyId || !config.privateKeyPem) {
        throw new Error('invalid_lineage_signing_config');
    }
    lineageSigner = {
        keyId: String(config.keyId),
        privateKey: crypto.createPrivateKey(String(config.privateKeyPem)),
    };
}

function _nextChain(ctx, event) {
    const next = computeNextEventChainHash(ctx.eventChainHash || '', event);
    ctx.eventChainHash = next;
    return next;
}

function createTraceContext({ extensionId, requestId, lineageId, capability, argsHash, policyDigest }) {
    const now = new Date().toISOString();
    return {
        extensionId,
        requestId,
        lineageId,
        capability,
        argsHash,
        manifestHash: 'none',
        requestTs: now,
        decisionTs: null,
        executeStartTs: null,
        executeResultTs: null,
        decision: null,
        resultCode: null,
        preExecutionAuthHash: '',
        preExecutionSignatureKeyId: '',
        preExecutionSignatureAlgorithm: '',
        preExecutionSignatureValue: '',
        brokerDecisionHash: '',
        executionResultHash: '',
        eventChainHash: '',
        policyDigest: typeof policyDigest === 'string' ? policyDigest : '',
        denyReasonDigest: '',
    };
}

function emitRequest(ctx) {
    const event = {
        sequence: sequenceCounter++,
        type: 'capability.request',
        extensionId: ctx.extensionId,
        requestId: ctx.requestId,
        lineageId: ctx.lineageId,
        capability: ctx.capability,
        argsHash: ctx.argsHash,
        decision: ctx.decision,
        resultCode: ctx.resultCode,
        requestTs: ctx.requestTs,
        decisionTs: ctx.decisionTs,
        executeStartTs: ctx.executeStartTs,
        executeResultTs: ctx.executeResultTs,
        timestamp: ctx.requestTs,
        manifestHash: ctx.manifestHash || 'none',
        preExecutionAuthHash: ctx.preExecutionAuthHash || '',
        preExecutionSignatureKeyId: ctx.preExecutionSignatureKeyId || '',
        preExecutionSignatureAlgorithm: ctx.preExecutionSignatureAlgorithm || '',
        preExecutionSignatureValue: ctx.preExecutionSignatureValue || '',
        brokerDecisionHash: ctx.brokerDecisionHash || '',
        executionResultHash: ctx.executionResultHash || '',
        policyDigest: ctx.policyDigest || '',
        denyReasonDigest: ctx.denyReasonDigest || '',
    };
    event.eventChainHash = _nextChain(ctx, event);
    appendEvent(event);
}

function emitDecision(ctx, decision, reason) {
    ctx.decision = decision;
    ctx.decisionTs = ctx.decisionTs || new Date().toISOString();
    ctx.brokerDecisionHash = computeBrokerDecisionHash(ctx, decision, reason || '');
    if (decision === 'deny') {
        ctx.denyReasonDigest = buildDenyReasonDigest(ctx, reason || '');
    }
    const event = {
        sequence: sequenceCounter++,
        type: 'capability.decision',
        extensionId: ctx.extensionId,
        requestId: ctx.requestId,
        lineageId: ctx.lineageId,
        capability: ctx.capability,
        argsHash: ctx.argsHash,
        decision: ctx.decision,
        resultCode: reason || null,
        requestTs: ctx.requestTs,
        decisionTs: ctx.decisionTs,
        executeStartTs: ctx.executeStartTs,
        executeResultTs: ctx.executeResultTs,
        timestamp: ctx.decisionTs,
        manifestHash: ctx.manifestHash || 'none',
        preExecutionAuthHash: ctx.preExecutionAuthHash || '',
        preExecutionSignatureKeyId: ctx.preExecutionSignatureKeyId || '',
        preExecutionSignatureAlgorithm: ctx.preExecutionSignatureAlgorithm || '',
        preExecutionSignatureValue: ctx.preExecutionSignatureValue || '',
        brokerDecisionHash: ctx.brokerDecisionHash,
        executionResultHash: ctx.executionResultHash || '',
        policyDigest: ctx.policyDigest || '',
        denyReasonDigest: ctx.denyReasonDigest || '',
    };
    event.eventChainHash = _nextChain(ctx, event);
    appendEvent(event);
}

function emitExecuteStart(ctx) {
    ctx.executeStartTs = new Date().toISOString();
    const event = {
        sequence: sequenceCounter++,
        type: 'capability.execute.start',
        extensionId: ctx.extensionId,
        requestId: ctx.requestId,
        lineageId: ctx.lineageId,
        capability: ctx.capability,
        argsHash: ctx.argsHash,
        decision: ctx.decision,
        resultCode: null,
        requestTs: ctx.requestTs,
        decisionTs: ctx.decisionTs,
        executeStartTs: ctx.executeStartTs,
        executeResultTs: ctx.executeResultTs,
        timestamp: ctx.executeStartTs,
        manifestHash: ctx.manifestHash || 'none',
        preExecutionAuthHash: ctx.preExecutionAuthHash || '',
        preExecutionSignatureKeyId: ctx.preExecutionSignatureKeyId || '',
        preExecutionSignatureAlgorithm: ctx.preExecutionSignatureAlgorithm || '',
        preExecutionSignatureValue: ctx.preExecutionSignatureValue || '',
        brokerDecisionHash: ctx.brokerDecisionHash || '',
        executionResultHash: ctx.executionResultHash || '',
        policyDigest: ctx.policyDigest || '',
        denyReasonDigest: ctx.denyReasonDigest || '',
    };
    event.eventChainHash = _nextChain(ctx, event);
    appendEvent(event);
}

function emitExecuteResult(ctx, success, resultCode) {
    ctx.executeResultTs = new Date().toISOString();
    ctx.resultCode = resultCode || (success ? 'ok' : 'error');
    ctx.executionResultHash = computeExecutionResultHash(ctx, success, ctx.resultCode);
    const event = {
        sequence: sequenceCounter++,
        type: 'capability.execute.result',
        extensionId: ctx.extensionId,
        requestId: ctx.requestId,
        lineageId: ctx.lineageId,
        capability: ctx.capability,
        argsHash: ctx.argsHash,
        decision: ctx.decision,
        resultCode: ctx.resultCode,
        success: !!success,
        requestTs: ctx.requestTs,
        decisionTs: ctx.decisionTs,
        executeStartTs: ctx.executeStartTs,
        executeResultTs: ctx.executeResultTs,
        timestamp: ctx.executeResultTs,
        manifestHash: ctx.manifestHash || 'none',
        preExecutionAuthHash: ctx.preExecutionAuthHash || '',
        preExecutionSignatureKeyId: ctx.preExecutionSignatureKeyId || '',
        preExecutionSignatureAlgorithm: ctx.preExecutionSignatureAlgorithm || '',
        preExecutionSignatureValue: ctx.preExecutionSignatureValue || '',
        brokerDecisionHash: ctx.brokerDecisionHash || '',
        executionResultHash: ctx.executionResultHash,
        policyDigest: ctx.policyDigest || '',
        denyReasonDigest: ctx.denyReasonDigest || '',
    };
    event.eventChainHash = _nextChain(ctx, event);
    const signature = signLineageBinding(event, lineageSigner);
    if (signature) {
        event.signatureKeyId = signature.signatureKeyId;
        event.signatureAlgorithm = signature.signatureAlgorithm;
        event.signatureValue = signature.signatureValue;
    }
    appendEvent(event);
}

module.exports = {
    LOG_FILE,
    stableStringify,
    hashArgs,
    createTraceContext,
    configureLineageSigning,
    ensureAuditSinkWritable,
    emitRequest,
    emitDecision,
    emitExecuteStart,
    emitExecuteResult,
};
