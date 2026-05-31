'use strict';

const fs = require('fs');
const os = require('os');
const path = require('path');
const {
    routeFileWrite,
} = require('./tool-operation-router');
const {
    createLoopController,
} = require('./autonomy-loop-controller');
const {
    executeWithBackoff,
    enqueueTask,
} = require('./resilient-execution');
const {
    createWorkspaceIndexCache,
} = require('./workspace-index-cache');

function assert(condition, message) {
    if (!condition) throw new Error(message);
}

async function main() {
    const temp = fs.mkdtempSync(path.join(os.tmpdir(), 'rawrxd-agentic-rel-'));

    const routeTarget = path.join(temp, 'ops', 'a.txt');
    const createResult = routeFileWrite({
        path: routeTarget,
        content: 'alpha',
        operationId: 'op-create',
    });
    assert(createResult.action === 'create_file', 'expected create route');
    assert(fs.readFileSync(routeTarget, 'utf8') === 'alpha', 'create write failed');

    const editResult = routeFileWrite({
        path: routeTarget,
        content: 'beta',
        operationId: 'op-edit',
        expectedExists: true,
    });
    assert(editResult.action === 'edit_file', 'expected edit route');
    assert(editResult.skipped === false, 'edit unexpectedly skipped');

    const skipResult = routeFileWrite({
        path: routeTarget,
        content: 'beta',
        operationId: 'op-skip',
    });
    assert(skipResult.skipped === true, 'idempotent skip not triggered');

    const loop = createLoopController({ id: 'test-loop', maxDepth: 2 });
    loop.beginStep('s1');
    loop.transition('EXECUTING');
    loop.transition('VERIFYING');
    loop.transition('PLANNING');
    loop.checkpoint('after-step', { ok: true });
    assert(loop.snapshot().state === 'PLANNING', 'loop did not reset to planning');

    let attempt = 0;
    const backoffResult = await executeWithBackoff(async () => {
        attempt++;
        if (attempt < 3) {
            throw new Error('user_weekly_rate_limited (429)');
        }
        return { ok: true };
    }, {
        maxAttempts: 4,
        backoffMs: [1, 1, 1, 1],
    });
    assert(backoffResult.result.ok === true, 'backoff retry failed');
    assert(backoffResult.attempt === 3, 'unexpected retry attempt count');

    const queueFile = path.join(temp, 'queue', 'tasks.json');
    const queued = enqueueTask(queueFile, { type: 'capability.call', payload: { x: 1 } });
    const queuedData = JSON.parse(fs.readFileSync(queueFile, 'utf8'));
    assert(queued && queued.id, 'queue id missing');
    assert(Array.isArray(queuedData) && queuedData.length === 1, 'queue persistence failed');

    const wsRoot = path.join(temp, 'ws');
    fs.mkdirSync(path.join(wsRoot, 'src'), { recursive: true });
    fs.writeFileSync(path.join(wsRoot, 'src', 'x.js'), 'console.log(1);\n', 'utf8');

    const cache = createWorkspaceIndexCache(path.join(temp, 'cache', 'workspace-index.json'));
    const scanIndex = cache.getIndex([wsRoot], { maxFiles: 20 });
    assert(scanIndex.source === 'scan', 'expected scan fallback source');
    assert(scanIndex.files.length >= 1, 'workspace scan returned no files');

    const cachedIndex = cache.getIndex([wsRoot], { maxAgeMs: 60000 });
    assert(cachedIndex.source === 'cache', 'expected cached index source');

    console.log('PASS');
}

main().catch((error) => {
    console.error('FAIL:', error.message);
    process.exit(1);
});
