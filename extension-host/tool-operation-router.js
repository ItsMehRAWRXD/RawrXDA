'use strict';

const fs = require('fs');
const crypto = require('crypto');

function _asString(value, fallback = '') {
    if (typeof value === 'string') return value;
    if (value === undefined || value === null) return fallback;
    return String(value);
}

function _sha256(value) {
    return crypto.createHash('sha256').update(String(value || ''), 'utf8').digest('hex');
}

function _readTextSafe(filePath) {
    try {
        return fs.readFileSync(filePath, 'utf8');
    } catch (_) {
        return null;
    }
}

function createOperationId(seed) {
    const text = _asString(seed) || `${Date.now()}:${Math.random()}`;
    return _sha256(text).slice(0, 24);
}

function routeFileWrite(input) {
    const filePath = _asString(input && input.path);
    if (!filePath) {
        throw new Error('invalid_file_path');
    }

    const content = _asString(input && input.content);
    const operationId = _asString(input && input.operationId, createOperationId(filePath + ':' + content));
    const expectedExists = typeof (input && input.expectedExists) === 'boolean'
        ? input.expectedExists
        : null;

    const exists = fs.existsSync(filePath);
    if (expectedExists === true && !exists) {
        throw new Error('precondition_failed:file_missing');
    }
    if (expectedExists === false && exists) {
        throw new Error('precondition_failed:file_exists');
    }

    const action = exists ? 'edit_file' : 'create_file';
    const idempotencyHash = _sha256(filePath + '\n' + content);
    const previous = exists ? _readTextSafe(filePath) : null;
    const previousHash = exists ? _sha256(previous === null ? '' : previous) : '';
    const shouldSkip = exists && previousHash === _sha256(content);

    if (!shouldSkip) {
        fs.mkdirSync(require('path').dirname(filePath), { recursive: true });
        fs.writeFileSync(filePath, content, 'utf8');
    }

    return {
        operationId,
        action,
        path: filePath,
        existedBefore: exists,
        skipped: shouldSkip,
        idempotencyHash,
        previousHash,
        resultHash: _sha256(content),
    };
}

module.exports = {
    createOperationId,
    routeFileWrite,
};
