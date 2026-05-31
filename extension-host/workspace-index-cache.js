'use strict';

const fs = require('fs');
const path = require('path');

function _walk(root, files, limit) {
    if (files.length >= limit) return;
    let entries = [];
    try {
        entries = fs.readdirSync(root, { withFileTypes: true });
    } catch (_) {
        return;
    }
    for (const entry of entries) {
        if (files.length >= limit) return;
        const full = path.join(root, entry.name);
        if (entry.isDirectory()) {
            if (entry.name === '.git' || entry.name === 'node_modules') continue;
            _walk(full, files, limit);
            continue;
        }
        files.push(full);
    }
}

function createWorkspaceIndexCache(cacheFilePath) {
    function readCache() {
        try {
            const raw = fs.readFileSync(cacheFilePath, 'utf8');
            const parsed = JSON.parse(raw);
            if (parsed && typeof parsed === 'object') return parsed;
        } catch (_) {}
        return { updatedAt: '', roots: [], files: [] };
    }

    function writeCache(data) {
        fs.mkdirSync(path.dirname(cacheFilePath), { recursive: true });
        fs.writeFileSync(cacheFilePath, JSON.stringify(data, null, 2) + '\n', 'utf8');
    }

    function refresh(roots, options = {}) {
        const maxFiles = Number.isInteger(options.maxFiles) ? Math.max(10, options.maxFiles) : 5000;
        const files = [];
        for (const root of roots) {
            if (typeof root !== 'string' || !root) continue;
            _walk(root, files, maxFiles);
            if (files.length >= maxFiles) break;
        }
        const next = {
            updatedAt: new Date().toISOString(),
            roots: Array.isArray(roots) ? roots.slice() : [],
            files,
        };
        writeCache(next);
        return { source: 'scan', ...next };
    }

    function getIndex(roots, options = {}) {
        const cache = readCache();
        const maxAgeMs = Number.isInteger(options.maxAgeMs) ? options.maxAgeMs : 15000;
        const hasFreshCache = cache.updatedAt && (Date.now() - Date.parse(cache.updatedAt)) <= maxAgeMs;
        if (hasFreshCache && Array.isArray(cache.files) && cache.files.length > 0) {
            return { source: 'cache', ...cache };
        }

        // Placeholder for diff-service path. On failure, always fall back to scan.
        try {
            if (typeof options.diffProvider === 'function') {
                const diffResult = options.diffProvider();
                if (diffResult && Array.isArray(diffResult.files)) {
                    const next = {
                        updatedAt: new Date().toISOString(),
                        roots: Array.isArray(roots) ? roots.slice() : [],
                        files: diffResult.files,
                    };
                    writeCache(next);
                    return { source: 'diff', ...next };
                }
            }
        } catch (_) {
            // Intentional fallthrough.
        }

        return refresh(roots, options);
    }

    return {
        getIndex,
        refresh,
    };
}

module.exports = {
    createWorkspaceIndexCache,
};
