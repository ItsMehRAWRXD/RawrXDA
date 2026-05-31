'use strict';

const fs = require('fs');
const os = require('os');
const path = require('path');
const crypto = require('crypto');
const manager = require('./extension-host-manager');
const { verifyReplayLog } = require('./capability-replay-verifier');
const { AUDIT_SCHEMA_VERSION } = require('./capability-audit-schema');
const { configureLineageSigning } = require('./capability-audit');
const {
    buildBrokerDecisionPayload,
    buildPreExecutionAuthorizationPayload,
    computeBrokerDecisionHash,
    signPreExecutionAuthorization,
    verifyPreExecutionAuthorization,
} = require('./capability-lineage-binding');

function assert(condition, message) {
    if (!condition) throw new Error(message);
}

function assertThrows(fn, expectedPattern, message) {
    let threw = false;
    try {
        fn();
    } catch (error) {
        threw = true;
        if (expectedPattern && !expectedPattern.test(String(error && error.message))) {
            throw new Error(message + ':unexpected_error:' + String(error && error.message));
        }
    }
    if (!threw) {
        throw new Error(message + ':missing_error');
    }
}

function sleep(ms) {
    return new Promise((resolve) => setTimeout(resolve, ms));
}

async function waitForState(id, expected, timeoutMs = 4000) {
    const deadline = Date.now() + timeoutMs;
    while (Date.now() < deadline) {
        const current = manager.getStatus().find((entry) => entry.id === id);
        if (current && current.state === expected) {
            return current;
        }
        await sleep(25);
    }
    throw new Error('state_timeout:' + expected);
}

async function main() {
    const signer = crypto.generateKeyPairSync('ed25519');
    const signerKeyId = 'audit-root-test';
    configureLineageSigning({
        keyId: signerKeyId,
        privateKeyPem: signer.privateKey.export({ format: 'pem', type: 'pkcs8' }),
    });

    const preExecCtx = {
        lineageId: 'lineage-test',
        requestId: 'request-test',
        extensionId: 'extension.test',
        capability: 'file.read',
        argsHash: 'args-hash-test',
        manifestHash: 'manifest-hash-test',
        decision: 'allow',
        resultCode: 'policy_allow',
        requestTs: '2026-04-14T00:00:00.000Z',
        decisionTs: '2026-04-14T00:00:01.000Z',
        eventChainHash: 'chain-hash-test',
    };
    const brokerPayload = buildBrokerDecisionPayload(preExecCtx, preExecCtx.decision, preExecCtx.resultCode);
    const brokerHash = computeBrokerDecisionHash(preExecCtx, preExecCtx.decision, preExecCtx.resultCode);
    const authPayload = JSON.parse(buildPreExecutionAuthorizationPayload({
        ...preExecCtx,
        brokerDecisionHash: brokerHash,
    }));
    for (const field of Object.keys(brokerPayload)) {
        assert(authPayload[field] === brokerPayload[field], 'pre-exec payload drift:' + field);
    }
    assert(authPayload.brokerDecisionHash === brokerHash, 'pre-exec payload missing broker hash');

    const preExecSignature = signPreExecutionAuthorization({
        ...preExecCtx,
        brokerDecisionHash: brokerHash,
    }, {
        keyId: signerKeyId,
        privateKey: signer.privateKey,
    });
    assert(preExecSignature && preExecSignature.signatureValue, 'pre-exec signature generation failed');
    const preExecVerify = verifyPreExecutionAuthorization({
        ...preExecCtx,
        brokerDecisionHash: brokerHash,
        ...preExecSignature,
    }, {
        [signerKeyId]: signer.publicKey.export({ format: 'pem', type: 'spki' }),
    });
    assert(preExecVerify.ok, 'pre-exec signature verification failed');
    const tamperedPreExecVerify = verifyPreExecutionAuthorization({
        ...preExecCtx,
        capability: 'file.write',
        brokerDecisionHash: brokerHash,
        ...preExecSignature,
    }, {
        [signerKeyId]: signer.publicKey.export({ format: 'pem', type: 'spki' }),
    });
    assert(!tamperedPreExecVerify.ok, 'tampered pre-exec signature unexpectedly verified');

    const logPath = manager.capabilityAuditLogFile;
    try {
        if (fs.existsSync(logPath)) {
            fs.unlinkSync(logPath);
        }
    } catch (_) {}

    const tempRoot = fs.mkdtempSync(path.join(os.tmpdir(), 'rawrxd-captrace-'));
    const workspaceRoot = path.join(tempRoot, 'workspace');
    const storageRoot = path.join(tempRoot, 'storage');
    fs.mkdirSync(workspaceRoot, { recursive: true });
    fs.mkdirSync(storageRoot, { recursive: true });
    const allowedReadPath = path.join(workspaceRoot, 'allowed.txt');
    fs.writeFileSync(allowedReadPath, 'ok', 'utf8');

    const runtime = process.execPath;
    const restrictedRuntime = path.join(__dirname, 'restricted-extension-runtime.js');
    const extensionEntry = path.join(__dirname, 'sandbox-probe-extension.js');

    const id = manager.spawnExtensionHost(runtime, {
        name: 'capability_replay_probe',
        args: ['--disallow-code-generation-from-strings', '--no-addons', restrictedRuntime],
        env: { RAWRXD_EXTENSION_ENTRY: extensionEntry },
        sandboxPolicy: {
            version: 1,
            readRoots: [workspaceRoot, storageRoot],
            writeRoots: [storageRoot],
            networkAllowlist: [],
            allowProcessSpawn: false,
            allowNativeAddons: false,
            violationLimit: 20,
        },
        requirePreExecutionAuth: true,
        requireSignatures: true,
        signatureTrustAnchors: {
            [signerKeyId]: signer.publicKey.export({ format: 'pem', type: 'spki' }),
        },
        preExecutionSigningKeyId: signerKeyId,
        preExecutionSigningPrivateKeyPem: signer.privateKey.export({ format: 'pem', type: 'pkcs8' }),
    });

    await waitForState(id, 'ready');

    const allowed = await manager.sendToExtension(id, 'file.read', { path: allowedReadPath }, { timeoutMs: 1000, source: 'replay-test' });
    assert(allowed && allowed.content === 'ok', 'allowed read failed');

    let denied = false;
    try {
        await manager.sendToExtension(id, 'process.exec', { command: 'cmd.exe', args: [] }, { timeoutMs: 1000, source: 'replay-test' });
    } catch (error) {
        denied = /process_spawn_denied|process_denied/.test(error.message);
    }
    assert(denied, 'expected deny for process.exec');

    manager.shutdown(id);
    await sleep(100);

    const summary = verifyReplayLog(logPath, {
        requireSignatures: true,
        signatureTrustAnchors: {
            [signerKeyId]: signer.publicKey.export({ format: 'pem', type: 'spki' }),
        },
    });
    assert(summary.lineages >= 2, 'expected at least 2 lineage entries');
    assert(summary.allowCount >= 1, 'expected at least one allow lineage');
    assert(summary.denyCount >= 1, 'expected at least one deny lineage');
    assert(summary.signedLineages >= 1, 'expected at least one signed lineage');
    assert(summary.preExecutionSignedLineages >= 1, 'expected at least one pre-exec signed lineage');

    const strictPreExecSummary = verifyReplayLog(logPath, {
        requireSignatures: true,
        requirePreExecutionSignatures: true,
        signatureTrustAnchors: {
            [signerKeyId]: signer.publicKey.export({ format: 'pem', type: 'spki' }),
        },
    });
    assert(strictPreExecSummary.preExecutionSignedLineages >= 1, 'expected strict replay mode to accept signed pre-exec lineage');

    const firstLine = fs.readFileSync(logPath, 'utf8').split(/\r?\n/).find((line) => line.trim());
    const firstEvent = JSON.parse(firstLine);
    assert(firstEvent.schemaVersion === AUDIT_SCHEMA_VERSION, 'audit schema version mismatch');

    const parsedEvents = fs.readFileSync(logPath, 'utf8')
        .split(/\r?\n/)
        .filter((line) => line.trim())
        .map((line) => JSON.parse(line));

    const firstAllowDecision = parsedEvents.find((event) => event.type === 'capability.decision' && event.decision === 'allow');
    const firstAllowExecuteStart = parsedEvents.find((event) => event.type === 'capability.execute.start' && firstAllowDecision && event.requestId === firstAllowDecision.requestId);
    const firstAllowExecuteResult = parsedEvents.find((event) => event.type === 'capability.execute.result' && firstAllowDecision && event.requestId === firstAllowDecision.requestId);
    assert(firstAllowDecision && firstAllowDecision.preExecutionAuthHash, 'missing pre-exec auth hash on allow decision');
    assert(firstAllowDecision && firstAllowDecision.preExecutionSignatureValue, 'missing pre-exec signature on allow decision');
    assert(firstAllowExecuteStart && firstAllowExecuteStart.preExecutionAuthHash === firstAllowDecision.preExecutionAuthHash, 'pre-exec hash mismatch on execute.start');
    assert(firstAllowExecuteResult && firstAllowExecuteResult.preExecutionAuthHash === firstAllowDecision.preExecutionAuthHash, 'pre-exec hash mismatch on execute.result');

    const tamperedPreExecSignatureEvents = parsedEvents.map((event) => ({ ...event }));
    const tamperedRequestId = firstAllowDecision.requestId;
    for (const event of tamperedPreExecSignatureEvents) {
        if (event.requestId === tamperedRequestId && typeof event.preExecutionSignatureValue === 'string' && event.preExecutionSignatureValue.length > 0) {
            const first = event.preExecutionSignatureValue.charAt(0);
            const replacement = first === 'A' ? 'B' : 'A';
            event.preExecutionSignatureValue = replacement + event.preExecutionSignatureValue.slice(1);
        }
    }
    const tamperedPreExecSignatureLog = path.join(tempRoot, 'tampered-preexec-signature.log');
    fs.writeFileSync(tamperedPreExecSignatureLog, tamperedPreExecSignatureEvents.map((event) => JSON.stringify(event)).join('\n') + '\n', 'utf8');
    assertThrows(
        () => verifyReplayLog(tamperedPreExecSignatureLog, {
            requireSignatures: true,
            signatureTrustAnchors: {
                [signerKeyId]: signer.publicKey.export({ format: 'pem', type: 'spki' }),
            },
        }),
        /preexec_signature_invalid/,
        'expected pre-exec signature tamper detection'
    );

    const missingPreExecSignatureEvents = parsedEvents.map((event) => ({ ...event }));
    for (const event of missingPreExecSignatureEvents) {
        if (event.requestId === tamperedRequestId) {
            event.preExecutionSignatureKeyId = '';
            event.preExecutionSignatureAlgorithm = '';
            event.preExecutionSignatureValue = '';
        }
    }
    const missingPreExecSignatureLog = path.join(tempRoot, 'missing-preexec-signature.log');
    fs.writeFileSync(missingPreExecSignatureLog, missingPreExecSignatureEvents.map((event) => JSON.stringify(event)).join('\n') + '\n', 'utf8');
    assertThrows(
        () => verifyReplayLog(missingPreExecSignatureLog, {
            requireSignatures: true,
            requirePreExecutionSignatures: true,
            signatureTrustAnchors: {
                [signerKeyId]: signer.publicKey.export({ format: 'pem', type: 'spki' }),
            },
        }),
        /preexec_signature_missing/,
        'expected missing pre-exec signature detection in strict mode'
    );

    const tamperedTimestampEvents = parsedEvents.map((event) => ({ ...event }));
    const requestIndex = tamperedTimestampEvents.findIndex((event) => event.type === 'capability.request');
    assert(requestIndex >= 0, 'missing request event for tamper case');
    tamperedTimestampEvents[requestIndex].timestamp = new Date(Date.parse(tamperedTimestampEvents[requestIndex].timestamp) + 1000).toISOString();
    const tamperedTimestampLog = path.join(tempRoot, 'tampered-timestamp.log');
    fs.writeFileSync(tamperedTimestampLog, tamperedTimestampEvents.map((event) => JSON.stringify(event)).join('\n') + '\n', 'utf8');
    assertThrows(
        () => verifyReplayLog(tamperedTimestampLog),
        /request_timestamp_mismatch|event_chain_hash_mismatch/,
        'expected timestamp mismatch tamper detection'
    );

    const tamperedSequenceEvents = parsedEvents.map((event) => ({ ...event }));
    const firstLineage = tamperedSequenceEvents[0].lineageId;
    const firstLineageIndices = tamperedSequenceEvents
        .map((event, index) => ({ event, index }))
        .filter((entry) => entry.event.lineageId === firstLineage)
        .map((entry) => entry.index);
    assert(firstLineageIndices.length >= 2, 'insufficient lineage events for sequence tamper case');
    tamperedSequenceEvents[firstLineageIndices[1]].sequence = tamperedSequenceEvents[firstLineageIndices[0]].sequence;
    const tamperedSequenceLog = path.join(tempRoot, 'tampered-sequence.log');
    fs.writeFileSync(tamperedSequenceLog, tamperedSequenceEvents.map((event) => JSON.stringify(event)).join('\n') + '\n', 'utf8');
    assertThrows(
        () => verifyReplayLog(tamperedSequenceLog),
        /duplicate_sequence_in_lineage|non_monotonic_sequence_in_lineage|event_chain_hash_mismatch/,
        'expected sequence tamper detection'
    );

    console.log('replay verifier summary:', summary);
    console.log('PASS');
    configureLineageSigning(null);
}

main().catch((error) => {
    try { configureLineageSigning(null); } catch (_) {}
    manager.shutdownAll();
    console.error('FAIL:', error.message);
    process.exit(1);
});
