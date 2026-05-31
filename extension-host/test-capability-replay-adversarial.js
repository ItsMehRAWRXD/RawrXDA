'use strict';

const fs = require('fs');
const os = require('os');
const path = require('path');
const { verifyReplayLog } = require('./capability-replay-verifier');
const { buildCanonicalEvent } = require('./capability-audit-schema');
const {
    computeBrokerDecisionHash,
    computeExecutionResultHash,
    computeNextEventChainHash,
} = require('./capability-lineage-binding');

function assert(condition, message) {
    if (!condition) {
        throw new Error(message);
    }
}

function writeEvents(filePath, events) {
    const lines = events.map((event) => JSON.stringify(event));
    fs.writeFileSync(filePath, lines.join('\n') + '\n', 'utf8');
}

function makeEvent(type, sequence, base, extras) {
    return buildCanonicalEvent({
        type,
        sequence,
        timestamp: base.timestamp,
        extensionId: base.extensionId,
        requestId: base.requestId,
        lineageId: base.lineageId,
        capability: base.capability,
        argsHash: base.argsHash,
        requestTs: base.requestTs,
        decisionTs: base.decisionTs,
        executeStartTs: base.executeStartTs,
        executeResultTs: base.executeResultTs,
        manifestHash: base.manifestHash || 'none',
        brokerDecisionHash: '',
        executionResultHash: '',
        eventChainHash: '',
        decision: '',
        resultCode: '',
        success: null,
        ...(extras || {}),
    });
}

function sealLineage(events) {
    const sorted = [...events].sort((a, b) => {
        if (a.sequence !== b.sequence) return a.sequence - b.sequence;
        return a.stageIndex - b.stageIndex;
    });

    const requestEvent = sorted.find((event) => event.type === 'capability.request');
    const decisionEvent = sorted.find((event) => event.type === 'capability.decision');
    const executeResultEvent = sorted.find((event) => event.type === 'capability.execute.result');

    let brokerDecisionHash = '';
    if (requestEvent && decisionEvent) {
        brokerDecisionHash = computeBrokerDecisionHash({
            lineageId: requestEvent.lineageId,
            requestId: requestEvent.requestId,
            extensionId: requestEvent.extensionId,
            capability: requestEvent.capability,
            argsHash: requestEvent.argsHash,
            manifestHash: requestEvent.manifestHash || 'none',
        }, decisionEvent.decision, decisionEvent.resultCode || '');
    }

    let executionResultHash = '';
    if (requestEvent && executeResultEvent && brokerDecisionHash) {
        executionResultHash = computeExecutionResultHash({
            lineageId: requestEvent.lineageId,
            requestId: requestEvent.requestId,
            extensionId: requestEvent.extensionId,
            capability: requestEvent.capability,
            argsHash: requestEvent.argsHash,
            manifestHash: requestEvent.manifestHash || 'none',
            brokerDecisionHash,
        }, executeResultEvent.success, executeResultEvent.resultCode || '');
    }

    let chain = '';
    for (const event of sorted) {
        if (event.type === 'capability.decision' || event.type === 'capability.execute.start' || event.type === 'capability.execute.result') {
            event.brokerDecisionHash = brokerDecisionHash;
        }
        if (event.type === 'capability.execute.result') {
            event.executionResultHash = executionResultHash;
        }
        event.eventChainHash = computeNextEventChainHash(chain, event);
        chain = event.eventChainHash;
    }

    return events;
}

function expectReplayFailure(name, events, expectedPattern) {
    const tempDir = fs.mkdtempSync(path.join(os.tmpdir(), 'rawrxd-replay-adversarial-'));
    const filePath = path.join(tempDir, name + '.log');
    writeEvents(filePath, events);

    let failedAsExpected = false;
    try {
        verifyReplayLog(filePath);
    } catch (error) {
        failedAsExpected = expectedPattern.test(error.message);
    }

    assert(failedAsExpected, 'expected verifier rejection for ' + name);
}

function expectReplaySuccess(name, events) {
    const tempDir = fs.mkdtempSync(path.join(os.tmpdir(), 'rawrxd-replay-adversarial-'));
    const filePath = path.join(tempDir, name + '.log');
    writeEvents(filePath, events);
    return verifyReplayLog(filePath);
}

function main() {
    // Case 1: Out-of-order file lines with canonical sequence are repaired deterministically.
    const reorderBase = {
        extensionId: 'ext.replay.repair',
        requestId: 'req-reorder-1',
        lineageId: 'lin-reorder-1',
        capability: 'file.read',
        argsHash: 'hash-reorder-1',
        requestTs: '2026-01-01T00:00:00.000Z',
        decisionTs: '2026-01-01T00:00:01.000Z',
        executeStartTs: '2026-01-01T00:00:02.000Z',
        executeResultTs: '2026-01-01T00:00:03.000Z',
        timestamp: '',
    };
    const repaired = expectReplaySuccess(
        'line_reorder_repair',
        sealLineage([
            makeEvent('capability.decision', 1, { ...reorderBase, timestamp: reorderBase.decisionTs }, { decision: 'allow' }),
            makeEvent('capability.request', 0, { ...reorderBase, timestamp: reorderBase.requestTs }),
            makeEvent('capability.execute.result', 3, { ...reorderBase, timestamp: reorderBase.executeResultTs }, { success: true, resultCode: 'ok' }),
            makeEvent('capability.execute.start', 2, { ...reorderBase, timestamp: reorderBase.executeStartTs }),
        ])
    );
    assert(repaired.allowCount === 1 && repaired.denyCount === 0, 'expected deterministic repair for reordered lines');

    // Case 2: Duplicate lineage collision with multiple request roots.
    const duplicateBase = {
        extensionId: 'ext.replay.duplicate',
        requestId: 'req-dup-1',
        lineageId: 'lin-dup-1',
        capability: 'file.write',
        argsHash: 'hash-dup-1',
        requestTs: '2026-01-01T00:01:00.000Z',
        decisionTs: '2026-01-01T00:01:02.000Z',
        executeStartTs: '2026-01-01T00:01:03.000Z',
        executeResultTs: '2026-01-01T00:01:04.000Z',
        timestamp: '',
    };
    expectReplayFailure(
        'duplicate_lineage',
        sealLineage([
            makeEvent('capability.request', 0, { ...duplicateBase, timestamp: duplicateBase.requestTs }),
            makeEvent('capability.request', 1, { ...duplicateBase, timestamp: '2026-01-01T00:01:01.000Z' }),
            makeEvent('capability.decision', 2, { ...duplicateBase, timestamp: duplicateBase.decisionTs }, { decision: 'allow' }),
            makeEvent('capability.execute.start', 3, { ...duplicateBase, timestamp: duplicateBase.executeStartTs }),
            makeEvent('capability.execute.result', 4, { ...duplicateBase, timestamp: duplicateBase.executeResultTs }, { success: true, resultCode: 'ok' }),
        ]),
        /invalid_request_count/
    );

    // Case 3: Truncated allow lineage with missing execute.result.
    const truncatedBase = {
        extensionId: 'ext.replay.truncated',
        requestId: 'req-trunc-1',
        lineageId: 'lin-trunc-1',
        capability: 'net.request',
        argsHash: 'hash-trunc-1',
        requestTs: '2026-01-01T00:02:00.000Z',
        decisionTs: '2026-01-01T00:02:01.000Z',
        executeStartTs: '2026-01-01T00:02:02.000Z',
        executeResultTs: '2026-01-01T00:02:03.000Z',
        timestamp: '',
    };
    expectReplayFailure(
        'truncated_allow',
        sealLineage([
            makeEvent('capability.request', 0, { ...truncatedBase, timestamp: truncatedBase.requestTs }),
            makeEvent('capability.decision', 1, { ...truncatedBase, timestamp: truncatedBase.decisionTs }, { decision: 'allow' }),
            makeEvent('capability.execute.start', 2, { ...truncatedBase, timestamp: truncatedBase.executeStartTs }),
        ]),
        /invalid_execute_result_count/
    );

    console.log('adversarial replay corruption cases rejected deterministically');
    console.log('PASS');
}

main();
