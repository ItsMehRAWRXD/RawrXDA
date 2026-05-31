'use strict';

const fs = require('fs');
const path = require('path');

const DEFAULT_BACKOFF_MS = [250, 500, 1000, 2000];

function _isRateLimitError(error) {
    const text = String(error && error.message ? error.message : error || '').toLowerCase();
    return text.includes('429') || text.includes('rate_limit') || text.includes('user_weekly_rate_limited');
}

function _ensureQueueFile(queueFile) {
    fs.mkdirSync(path.dirname(queueFile), { recursive: true });
    if (!fs.existsSync(queueFile)) {
        fs.writeFileSync(queueFile, '[]\n', 'utf8');
    }
}

function _readQueue(queueFile) {
    _ensureQueueFile(queueFile);
    try {
        const parsed = JSON.parse(fs.readFileSync(queueFile, 'utf8'));
        return Array.isArray(parsed) ? parsed : [];
    } catch (_) {
        return [];
    }
}

function _writeQueue(queueFile, items) {
    _ensureQueueFile(queueFile);
    fs.writeFileSync(queueFile, JSON.stringify(items, null, 2) + '\n', 'utf8');
}

function enqueueTask(queueFile, task) {
    const queue = _readQueue(queueFile);
    queue.push({
        id: task && task.id ? task.id : `q-${Date.now()}-${Math.random().toString(36).slice(2, 8)}`,
        ts: new Date().toISOString(),
        task,
    });
    _writeQueue(queueFile, queue);
    return queue[queue.length - 1];
}

async function executeWithBackoff(executor, options = {}) {
    const maxAttempts = Number.isInteger(options.maxAttempts) ? Math.max(1, options.maxAttempts) : 4;
    const backoffMs = Array.isArray(options.backoffMs) && options.backoffMs.length > 0
        ? options.backoffMs
        : DEFAULT_BACKOFF_MS;
    const fallbackExecutor = typeof options.fallbackExecutor === 'function' ? options.fallbackExecutor : null;

    let lastError = null;
    for (let attempt = 1; attempt <= maxAttempts; attempt++) {
        try {
            const result = await executor(attempt);
            return {
                ok: true,
                attempt,
                usedFallback: false,
                result,
            };
        } catch (error) {
            lastError = error;
            if (!_isRateLimitError(error)) {
                throw error;
            }
            if (attempt < maxAttempts) {
                const delay = backoffMs[Math.min(backoffMs.length - 1, attempt - 1)] || backoffMs[backoffMs.length - 1];
                await new Promise((resolve) => setTimeout(resolve, Math.max(0, delay)));
                continue;
            }
        }
    }

    if (fallbackExecutor) {
        const fallbackResult = await fallbackExecutor(lastError);
        return {
            ok: true,
            attempt: maxAttempts,
            usedFallback: true,
            result: fallbackResult,
        };
    }

    throw lastError || new Error('rate_limit_retry_exhausted');
}

module.exports = {
    executeWithBackoff,
    enqueueTask,
};
