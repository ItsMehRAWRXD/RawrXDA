'use strict';

const crypto = require('crypto');

const AUDIT_SCHEMA_VERSION = 1;

const EVENT_TYPES = Object.freeze([
    'capability.request',
    'capability.decision',
    'capability.execute.start',
    'capability.execute.result',
]);

const STAGE_INDEX = Object.freeze({
    'capability.request': 0,
    'capability.decision': 1,
    'capability.execute.start': 2,
    'capability.execute.result': 3,
});

function _asString(value, fallback = '') {
    if (typeof value === 'string') return value;
    if (value === undefined || value === null) return fallback;
    return String(value);
}

function _asNullableBoolean(value) {
    return typeof value === 'boolean' ? value : null;
}

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

function sha256Hex(value) {
    return crypto.createHash('sha256').update(String(value || ''), 'utf8').digest('hex');
}

function hashStructured(value) {
    return sha256Hex(stableStringify(value));
}

function computeDecisionHash(event) {
    return hashStructured({
        lineageId: _asString(event && event.lineageId),
        requestId: _asString(event && event.requestId),
        capability: _asString(event && event.capability),
        argsHash: _asString(event && event.argsHash),
        manifestHash: _asString(event && event.manifestHash),
        decision: _asString(event && event.decision),
        resultCode: _asString(event && event.resultCode),
    });
}

function computeExecutionHash(event) {
    return hashStructured({
        lineageId: _asString(event && event.lineageId),
        requestId: _asString(event && event.requestId),
        capability: _asString(event && event.capability),
        argsHash: _asString(event && event.argsHash),
        manifestHash: _asString(event && event.manifestHash),
        success: _asNullableBoolean(event && event.success),
        resultCode: _asString(event && event.resultCode),
    });
}

function computeEventHash(event) {
    return hashStructured({
        schemaVersion: Number.isInteger(event && event.schemaVersion) ? event.schemaVersion : AUDIT_SCHEMA_VERSION,
        sequence: Number.isInteger(event && event.sequence) ? event.sequence : 0,
        type: _asString(event && event.type),
        stageIndex: Number.isInteger(event && event.stageIndex) ? event.stageIndex : (STAGE_INDEX[_asString(event && event.type)] || 0),
        timestamp: _asString(event && event.timestamp),
        extensionId: _asString(event && event.extensionId),
        requestId: _asString(event && event.requestId),
        lineageId: _asString(event && event.lineageId),
        capability: _asString(event && event.capability),
        argsHash: _asString(event && event.argsHash),
        decision: _asString(event && event.decision),
        resultCode: _asString(event && event.resultCode),
        success: _asNullableBoolean(event && event.success),
        requestTs: _asString(event && event.requestTs),
        decisionTs: _asString(event && event.decisionTs),
        executeStartTs: _asString(event && event.executeStartTs),
        executeResultTs: _asString(event && event.executeResultTs),
        manifestHash: _asString(event && event.manifestHash),
        preExecutionAuthHash: _asString(event && event.preExecutionAuthHash),
        preExecutionSignatureKeyId: _asString(event && event.preExecutionSignatureKeyId),
        preExecutionSignatureAlgorithm: _asString(event && event.preExecutionSignatureAlgorithm),
        preExecutionSignatureValue: _asString(event && event.preExecutionSignatureValue),
        decisionHash: _asString(event && event.decisionHash),
        executionHash: _asString(event && event.executionHash),
        prevEventHash: _asString(event && event.prevEventHash),
    });
}

function computeLineageChainHash(previousChainHash, eventHash) {
    return hashStructured({
        previousChainHash: _asString(previousChainHash),
        eventHash: _asString(eventHash),
    });
}

function buildCanonicalEvent(input) {
    const type = _asString(input && input.type, '');
    if (!EVENT_TYPES.includes(type)) {
        throw new Error('invalid_audit_event_type');
    }

    const stageIndex = STAGE_INDEX[type];
    const sequence = Number.isInteger(input && input.sequence) && input.sequence >= 0
        ? input.sequence
        : 0;

    return {
        schemaVersion: AUDIT_SCHEMA_VERSION,
        sequence,
        type,
        stageIndex,
        timestamp: _asString(input && input.timestamp),
        extensionId: _asString(input && input.extensionId),
        requestId: _asString(input && input.requestId),
        lineageId: _asString(input && input.lineageId),
        capability: _asString(input && input.capability),
        argsHash: _asString(input && input.argsHash),
        decision: _asString(input && input.decision, ''),
        resultCode: _asString(input && input.resultCode, ''),
        success: _asNullableBoolean(input && input.success),
        requestTs: _asString(input && input.requestTs),
        decisionTs: _asString(input && input.decisionTs),
        executeStartTs: _asString(input && input.executeStartTs),
        executeResultTs: _asString(input && input.executeResultTs),
        manifestHash: _asString(input && input.manifestHash),
        preExecutionAuthHash: _asString(input && input.preExecutionAuthHash),
        preExecutionSignatureKeyId: _asString(input && input.preExecutionSignatureKeyId),
        preExecutionSignatureAlgorithm: _asString(input && input.preExecutionSignatureAlgorithm),
        preExecutionSignatureValue: _asString(input && input.preExecutionSignatureValue),
        brokerDecisionHash: _asString(input && input.brokerDecisionHash),
        executionResultHash: _asString(input && input.executionResultHash),
        eventChainHash: _asString(input && input.eventChainHash),
        signatureKeyId: _asString(input && input.signatureKeyId),
        signatureAlgorithm: _asString(input && input.signatureAlgorithm),
        signatureValue: _asString(input && input.signatureValue),
        decisionHash: _asString(input && input.decisionHash),
        executionHash: _asString(input && input.executionHash),
        prevEventHash: _asString(input && input.prevEventHash),
        eventHash: _asString(input && input.eventHash),
        lineageChainHash: _asString(input && input.lineageChainHash),
        policyDigest: _asString(input && input.policyDigest),
        denyReasonDigest: _asString(input && input.denyReasonDigest),
    };
}

function validateCanonicalEvent(event) {
    if (!event || typeof event !== 'object' || Array.isArray(event)) {
        throw new Error('invalid_audit_event_shape');
    }
    if (event.schemaVersion !== AUDIT_SCHEMA_VERSION) {
        throw new Error('unsupported_audit_schema_version');
    }
    if (!EVENT_TYPES.includes(event.type)) {
        throw new Error('invalid_audit_event_type');
    }
    if (!Number.isInteger(event.stageIndex) || event.stageIndex !== STAGE_INDEX[event.type]) {
        throw new Error('invalid_audit_stage_index');
    }

    const requiredText = ['timestamp', 'extensionId', 'requestId', 'lineageId', 'capability', 'argsHash'];
    for (const field of requiredText) {
        if (typeof event[field] !== 'string' || !event[field]) {
            throw new Error('missing_audit_field:' + field);
        }
    }

    if (!Number.isInteger(event.sequence) || event.sequence < 0) {
        throw new Error('invalid_audit_sequence');
    }
    if (event.success !== null && typeof event.success !== 'boolean') {
        throw new Error('invalid_audit_success');
    }

    const hashFields = [
        'manifestHash',
        'preExecutionAuthHash',
        'preExecutionSignatureKeyId',
        'preExecutionSignatureAlgorithm',
        'preExecutionSignatureValue',
        'brokerDecisionHash',
        'executionResultHash',
        'eventChainHash',
        'decisionHash',
        'executionHash',
        'prevEventHash',
        'eventHash',
        'lineageChainHash',
    ];
    for (const field of hashFields) {
        if (typeof event[field] !== 'string') {
            throw new Error('invalid_audit_hash_field:' + field);
        }
    }

    const signatureFields = ['signatureKeyId', 'signatureAlgorithm', 'signatureValue'];
    for (const field of signatureFields) {
        if (typeof event[field] !== 'string') {
            throw new Error('invalid_audit_signature_field:' + field);
        }
    }
}

module.exports = {
    AUDIT_SCHEMA_VERSION,
    EVENT_TYPES,
    STAGE_INDEX,
    buildCanonicalEvent,
    validateCanonicalEvent,
    stableStringify,
    sha256Hex,
    hashStructured,
    computeDecisionHash,
    computeExecutionHash,
    computeEventHash,
    computeLineageChainHash,
};
