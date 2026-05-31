'use strict';

const crypto = require('crypto');

function _asString(value, fallback = '') {
    if (typeof value === 'string') return value;
    if (value === undefined || value === null) return fallback;
    return String(value);
}

function sha256Hex(value) {
    return crypto.createHash('sha256').update(_asString(value), 'utf8').digest('hex');
}

function _stableClone(value) {
    if (Array.isArray(value)) {
        return value.map(_stableClone);
    }
    if (value && typeof value === 'object') {
        const sorted = {};
        const keys = Object.keys(value).sort();
        for (const key of keys) {
            sorted[key] = _stableClone(value[key]);
        }
        return sorted;
    }
    return value;
}

function _stableStringify(value) {
    return JSON.stringify(_stableClone(value));
}

function buildBrokerDecisionPayload(ctx, decision, resultCode) {
    return {
        lineageId: _asString(ctx && ctx.lineageId),
        requestId: _asString(ctx && ctx.requestId),
        extensionId: _asString(ctx && ctx.extensionId),
        capability: _asString(ctx && ctx.capability),
        argsHash: _asString(ctx && ctx.argsHash),
        manifestHash: _asString(ctx && ctx.manifestHash, 'none'),
        decision: _asString(decision),
        resultCode: _asString(resultCode),
    };
}

function computeBrokerDecisionHash(ctx, decision, resultCode) {
    return sha256Hex(_stableStringify(buildBrokerDecisionPayload(ctx, decision, resultCode)));
}

function computeExecutionResultHash(ctx, success, resultCode) {
    return sha256Hex(_stableStringify({
        lineageId: _asString(ctx && ctx.lineageId),
        requestId: _asString(ctx && ctx.requestId),
        extensionId: _asString(ctx && ctx.extensionId),
        capability: _asString(ctx && ctx.capability),
        argsHash: _asString(ctx && ctx.argsHash),
        manifestHash: _asString(ctx && ctx.manifestHash, 'none'),
        brokerDecisionHash: _asString(ctx && ctx.brokerDecisionHash),
        success: !!success,
        resultCode: _asString(resultCode),
    }));
}

function _eventDigestPayload(event) {
    return {
        type: _asString(event && event.type),
        timestamp: _asString(event && event.timestamp),
        extensionId: _asString(event && event.extensionId),
        requestId: _asString(event && event.requestId),
        lineageId: _asString(event && event.lineageId),
        capability: _asString(event && event.capability),
        argsHash: _asString(event && event.argsHash),
        manifestHash: _asString(event && event.manifestHash, 'none'),
        decision: _asString(event && event.decision),
        resultCode: _asString(event && event.resultCode),
        success: typeof event.success === 'boolean' ? event.success : null,
        brokerDecisionHash: _asString(event && event.brokerDecisionHash),
        executionResultHash: _asString(event && event.executionResultHash),
    };
}

function computeEventDigest(event) {
    return sha256Hex(_stableStringify(_eventDigestPayload(event)));
}

function computeNextEventChainHash(previousChainHash, event) {
    return sha256Hex(_asString(previousChainHash) + '|' + computeEventDigest(event));
}

function buildLineageSignaturePayload(event) {
    return _stableStringify({
        lineageId: _asString(event && event.lineageId),
        requestId: _asString(event && event.requestId),
        extensionId: _asString(event && event.extensionId),
        capability: _asString(event && event.capability),
        argsHash: _asString(event && event.argsHash),
        manifestHash: _asString(event && event.manifestHash, 'none'),
        brokerDecisionHash: _asString(event && event.brokerDecisionHash),
        executionResultHash: _asString(event && event.executionResultHash),
        eventChainHash: _asString(event && event.eventChainHash),
        decision: _asString(event && event.decision),
        resultCode: _asString(event && event.resultCode),
        success: typeof event.success === 'boolean' ? event.success : null,
        executeResultTs: _asString(event && event.executeResultTs),
    });
}

function buildPreExecutionAuthorizationPayload(ctx) {
    return _stableStringify({
        ...buildBrokerDecisionPayload(ctx, ctx && ctx.decision, ctx && ctx.resultCode),
        brokerDecisionHash: _asString(ctx && ctx.brokerDecisionHash),
        requestTs: _asString(ctx && ctx.requestTs),
        decisionTs: _asString(ctx && ctx.decisionTs),
        eventChainHash: _asString(ctx && ctx.eventChainHash),
    });
}

function signLineageBinding(event, signer) {
    if (!signer || !signer.privateKey || !signer.keyId) return null;
    const payload = buildLineageSignaturePayload(event);
    const value = crypto.sign(null, Buffer.from(payload, 'utf8'), signer.privateKey).toString('base64');
    return {
        signatureKeyId: signer.keyId,
        signatureAlgorithm: 'ed25519',
        signatureValue: value,
    };
}

function signPreExecutionAuthorization(ctx, signer) {
    if (!signer || !signer.privateKey || !signer.keyId) return null;
    const payload = buildPreExecutionAuthorizationPayload(ctx);
    const value = crypto.sign(null, Buffer.from(payload, 'utf8'), signer.privateKey).toString('base64');
    return {
        signatureKeyId: signer.keyId,
        signatureAlgorithm: 'ed25519',
        signatureValue: value,
    };
}

function verifyLineageSignature(event, trustAnchors) {
    const keyId = _asString(event && event.signatureKeyId);
    const algorithm = _asString(event && event.signatureAlgorithm).toLowerCase();
    const value = _asString(event && event.signatureValue);

    if (!keyId && !algorithm && !value) {
        return { present: false, ok: false, reason: 'signature_missing' };
    }
    if (!keyId || !algorithm || !value) {
        return { present: true, ok: false, reason: 'signature_incomplete' };
    }
    if (algorithm !== 'ed25519') {
        return { present: true, ok: false, reason: 'signature_algorithm_unsupported' };
    }

    const anchors = trustAnchors && typeof trustAnchors === 'object' ? trustAnchors : {};
    const publicKeyPem = anchors[keyId];
    if (!publicKeyPem) {
        return { present: true, ok: false, reason: 'signature_untrusted_key' };
    }

    let signature;
    try {
        signature = Buffer.from(value, 'base64');
    } catch (_) {
        return { present: true, ok: false, reason: 'signature_encoding_invalid' };
    }

    try {
        const payload = buildLineageSignaturePayload(event);
        const publicKey = crypto.createPublicKey(publicKeyPem);
        const ok = crypto.verify(null, Buffer.from(payload, 'utf8'), publicKey, signature);
        return { present: true, ok, reason: ok ? 'ok' : 'signature_mismatch' };
    } catch (_) {
        return { present: true, ok: false, reason: 'signature_verification_error' };
    }
}

function verifyPreExecutionAuthorization(ctx, trustAnchors) {
    const keyId = _asString(ctx && ctx.signatureKeyId);
    const algorithm = _asString(ctx && ctx.signatureAlgorithm).toLowerCase();
    const value = _asString(ctx && ctx.signatureValue);

    if (!keyId && !algorithm && !value) {
        return { present: false, ok: false, reason: 'signature_missing' };
    }
    if (!keyId || !algorithm || !value) {
        return { present: true, ok: false, reason: 'signature_incomplete' };
    }
    if (algorithm !== 'ed25519') {
        return { present: true, ok: false, reason: 'signature_algorithm_unsupported' };
    }

    const anchors = trustAnchors && typeof trustAnchors === 'object' ? trustAnchors : {};
    const publicKeyPem = anchors[keyId];
    if (!publicKeyPem) {
        return { present: true, ok: false, reason: 'signature_untrusted_key' };
    }

    let signature;
    try {
        signature = Buffer.from(value, 'base64');
    } catch (_) {
        return { present: true, ok: false, reason: 'signature_encoding_invalid' };
    }

    try {
        const payload = buildPreExecutionAuthorizationPayload(ctx);
        const publicKey = crypto.createPublicKey(publicKeyPem);
        const ok = crypto.verify(null, Buffer.from(payload, 'utf8'), publicKey, signature);
        return { present: true, ok, reason: ok ? 'ok' : 'signature_mismatch' };
    } catch (_) {
        return { present: true, ok: false, reason: 'signature_verification_error' };
    }
}

function buildDenyReasonDigest(ctx, reason) {
    return sha256Hex(_stableStringify({
        lineageId: _asString(ctx && ctx.lineageId),
        requestId: _asString(ctx && ctx.requestId),
        extensionId: _asString(ctx && ctx.extensionId),
        capability: _asString(ctx && ctx.capability),
        argsHash: _asString(ctx && ctx.argsHash),
        manifestHash: _asString(ctx && ctx.manifestHash, 'none'),
        decision: 'deny',
        reason: _asString(reason),
        decisionTs: _asString(ctx && ctx.decisionTs),
    }));
}

function verifyDenyReasonDigest(ctx, reason, digest) {
    if (!digest) return { present: false, ok: false, reason: 'deny_reason_digest_missing' };
    const expected = buildDenyReasonDigest(ctx, reason);
    return {
        present: true,
        ok: expected === digest,
        reason: expected === digest ? 'ok' : 'deny_reason_digest_mismatch',
    };
}

function verifyPreExecutionGate(ctx, options = {}) {
    const requirePreExecutionAuth = !!(options && options.requirePreExecutionAuth);
    if (!requirePreExecutionAuth) {
        return { ok: true, enforced: false, reason: 'disabled' };
    }

    if (!ctx || typeof ctx !== 'object') {
        return { ok: false, enforced: true, code: 'preexec_ctx_invalid' };
    }

    const decision = _asString(ctx.decision).toLowerCase();
    if (decision !== 'allow') {
        return { ok: false, enforced: true, code: 'preexec_decision_not_allow' };
    }

    const expectedDecisionHash = computeBrokerDecisionHash(ctx, decision, _asString(ctx.resultCode));
    if (_asString(ctx.brokerDecisionHash) !== expectedDecisionHash) {
        return { ok: false, enforced: true, code: 'preexec_broker_hash_mismatch' };
    }

    if (!_asString(ctx.eventChainHash)) {
        return { ok: false, enforced: true, code: 'preexec_lineage_chain_missing' };
    }

    const requireSignatures = !!(options && options.requireSignatures);
    if (!requireSignatures) {
        return { ok: true, enforced: true, reason: 'ok' };
    }

    const trustAnchors = options && options.signatureTrustAnchors && typeof options.signatureTrustAnchors === 'object'
        ? options.signatureTrustAnchors
        : null;

    const signatureCheck = verifyPreExecutionAuthorization(ctx, trustAnchors);
    if (!signatureCheck.present) {
        return { ok: false, enforced: true, code: 'preexec_signature_missing' };
    }
    if (!signatureCheck.ok) {
        return {
            ok: false,
            enforced: true,
            code: 'preexec_signature_invalid',
            reason: signatureCheck.reason,
        };
    }

    return { ok: true, enforced: true, reason: 'ok' };
}

module.exports = {
    sha256Hex,
    buildBrokerDecisionPayload,
    computeBrokerDecisionHash,
    computeExecutionResultHash,
    computeEventDigest,
    computeNextEventChainHash,
    buildPreExecutionAuthorizationPayload,
    buildLineageSignaturePayload,
    buildDenyReasonDigest,
    verifyDenyReasonDigest,
    signPreExecutionAuthorization,
    signLineageBinding,
    verifyPreExecutionAuthorization,
    verifyLineageSignature,
    verifyPreExecutionGate,
};