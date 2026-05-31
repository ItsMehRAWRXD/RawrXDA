'use strict';

const crypto = require('crypto');

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

function normalizeManifestForSigning(manifest) {
    if (!manifest || typeof manifest !== 'object' || Array.isArray(manifest)) {
        throw new Error('manifest_invalid_object');
    }

    const normalized = { ...manifest };
    delete normalized.signature;

    if (!Number.isInteger(normalized.schemaVersion) || normalized.schemaVersion < 1) {
        normalized.schemaVersion = 1;
    }
    if (Array.isArray(normalized.requires)) {
        normalized.requires = [...new Set(normalized.requires.map((entry) => String(entry)))].sort();
    }
    if (normalized.processAllowlist && typeof normalized.processAllowlist === 'object' && !Array.isArray(normalized.processAllowlist)) {
        const allowlist = { ...normalized.processAllowlist };
        if (!Number.isInteger(allowlist.version) || allowlist.version < 1) {
            allowlist.version = 1;
        }
        if (Array.isArray(allowlist.commands)) {
            allowlist.commands = [...new Set(
                allowlist.commands
                    .map((entry) => String(entry).trim().toLowerCase())
                    .filter(Boolean)
            )].sort();
        }
        normalized.processAllowlist = allowlist;
    }
    if (normalized.networkAllowlist && typeof normalized.networkAllowlist === 'object' && !Array.isArray(normalized.networkAllowlist)) {
        const allowlist = { ...normalized.networkAllowlist };
        if (!Number.isInteger(allowlist.version) || allowlist.version < 1) {
            allowlist.version = 1;
        }
        if (Array.isArray(allowlist.hosts)) {
            allowlist.hosts = [...new Set(
                allowlist.hosts
                    .map((entry) => String(entry).trim().toLowerCase())
                    .filter(Boolean)
            )].sort();
        }
        normalized.networkAllowlist = allowlist;
    }

    return _stableClone(normalized);
}

function canonicalizeManifestForSigning(manifest) {
    const normalized = normalizeManifestForSigning(manifest);
    return JSON.stringify(normalized);
}

function verifyManifestSignature(manifest, trustAnchors) {
    if (!manifest || typeof manifest !== 'object' || Array.isArray(manifest)) {
        return { ok: false, error: 'manifest_invalid_object' };
    }

    const signature = manifest.signature;
    if (!signature || typeof signature !== 'object') {
        return { ok: false, error: 'manifest_missing_signature' };
    }

    const algorithm = String(signature.algorithm || '').toLowerCase();
    const keyId = String(signature.keyId || '');
    const signatureValue = String(signature.value || '');

    if (algorithm !== 'ed25519') {
        return { ok: false, error: 'manifest_unsupported_signature_algorithm' };
    }
    if (!keyId || !signatureValue) {
        return { ok: false, error: 'manifest_invalid_signature_format' };
    }

    const anchors = trustAnchors && typeof trustAnchors === 'object' ? trustAnchors : {};
    const publicKeyPem = anchors[keyId];
    if (!publicKeyPem) {
        return { ok: false, error: 'manifest_untrusted_signing_key' };
    }

    let canonical;
    try {
        canonical = canonicalizeManifestForSigning(manifest);
    } catch (error) {
        return { ok: false, error: error.message || 'manifest_canonicalization_failed' };
    }

    let signatureBuffer;
    try {
        signatureBuffer = Buffer.from(signatureValue, 'base64');
    } catch {
        return { ok: false, error: 'manifest_invalid_signature_encoding' };
    }

    let isValid = false;
    try {
        const publicKey = crypto.createPublicKey(publicKeyPem);
        isValid = crypto.verify(null, Buffer.from(canonical, 'utf8'), publicKey, signatureBuffer);
    } catch {
        return { ok: false, error: 'manifest_signature_verification_failed' };
    }

    if (!isValid) {
        return { ok: false, error: 'manifest_signature_mismatch' };
    }

    const digest = crypto.createHash('sha256').update(canonical, 'utf8').digest('hex');
    return {
        ok: true,
        hash: digest,
        keyId,
        algorithm: 'ed25519',
    };
}

module.exports = {
    normalizeManifestForSigning,
    canonicalizeManifestForSigning,
    verifyManifestSignature,
};