'use strict';

const fs = require('fs');
const http = require('http');
const os = require('os');
const path = require('path');
const crypto = require('crypto');
const manager = require('./extension-host-manager');
const { canonicalizeManifestForSigning } = require('./manifest-signature');
const { AUDIT_SCHEMA_VERSION, STAGE_INDEX } = require('./capability-audit-schema');
const runtime = process.execPath;
const script = path.join(__dirname, 'mock-extension-runtime.js');
const restrictedRuntime = path.join(__dirname, 'restricted-extension-runtime.js');
const sandboxProbe = path.join(__dirname, 'sandbox-probe-extension.js');
const vscodeApiProbe = path.join(__dirname, 'vscode-api-probe-extension.js');

function assert(condition, message) {
    if (!condition) {
        throw new Error(message);
    }
}

function sleep(ms) {
    return new Promise((resolve) => setTimeout(resolve, ms));
}

async function captureStderr(fn) {
    const originalWrite = process.stderr.write;
    let buffer = '';
    process.stderr.write = function(chunk, encoding, callback) {
        buffer += Buffer.isBuffer(chunk) ? chunk.toString('utf8') : String(chunk);
        if (typeof callback === 'function') {
            callback();
        }
        return true;
    };
    try {
        await fn();
        return buffer;
    } finally {
        process.stderr.write = originalWrite;
    }
}

async function waitForState(id, expectedState, timeoutMs = 3000) {
    const deadline = Date.now() + timeoutMs;
    while (Date.now() < deadline) {
        const status = manager.getStatus().find((entry) => entry.id === id);
        if (status && status.state === expectedState) {
            return status;
        }
        await sleep(25);
    }
    const current = manager.getStatus().find((entry) => entry.id === id);
    throw new Error(`Extension ${id} did not reach state ${expectedState}; current=${current ? current.state : 'missing'}`);
}

async function withExtension(mode, fn, spawnOptions = {}) {
    const id = manager.spawnExtensionHost(runtime, {
        name: `mock_${mode}`,
        args: [script],
        env: { RAWRXD_EXT_TEST_MODE: mode },
        ...spawnOptions,
    });
    await waitForState(id, 'ready');
    try {
        await fn(id);
    } finally {
        manager.shutdown(id);
        await sleep(50);
    }
}

async function withSandboxedExtension(policy, fn, extensionEntry = sandboxProbe, extensionName = 'sandbox_probe', options = {}) {
    const id = manager.spawnExtensionHost(runtime, {
        name: extensionName,
        args: [
            '--disallow-code-generation-from-strings',
            '--no-addons',
            restrictedRuntime,
        ],
        env: { RAWRXD_EXTENSION_ENTRY: extensionEntry, ...(options.env || {}) },
        sandboxPolicy: policy,
        extensionManifest: options.extensionManifest,
        manifestTrustAnchors: options.manifestTrustAnchors,
        requirePreExecutionAuth: !!options.requirePreExecutionAuth,
        requireSignatures: !!options.requireSignatures,
        signatureTrustAnchors: options.signatureTrustAnchors,
        preExecutionSigningKeyId: options.preExecutionSigningKeyId,
        preExecutionSigningPrivateKeyPem: options.preExecutionSigningPrivateKeyPem,
        preExecutionAuthContext: options.preExecutionAuthContext,
    });
    await waitForState(id, 'ready');
    try {
        await fn(id);
    } finally {
        manager.shutdown(id);
        await sleep(50);
    }
}

function signManifest(manifest, privateKey, keyId) {
    const canonical = canonicalizeManifestForSigning(manifest);
    const signature = crypto.sign(null, Buffer.from(canonical, 'utf8'), privateKey).toString('base64');
    return {
        ...manifest,
        signature: {
            algorithm: 'ed25519',
            keyId,
            value: signature,
        },
    };
}

async function main() {
    const test = manager._test;
    assert(typeof test._buildRequestEnvelope === 'function', 'missing _buildRequestEnvelope export');
    assert(typeof test._validateResponseEnvelope === 'function', 'missing _validateResponseEnvelope export');
    assert(typeof test._normalizeLspDiagnostic === 'function', 'missing _normalizeLspDiagnostic export');
    assert(typeof test._runAuditSinkSelfTest === 'function', 'missing _runAuditSinkSelfTest export');
    assert(typeof test._parseSyncPeerEntry === 'function', 'missing _parseSyncPeerEntry export');
    assert(typeof manager.configureSyncPeers === 'function', 'missing configureSyncPeers export');
    assert(typeof manager.discoverSyncPeers === 'function', 'missing discoverSyncPeers export');
    assert(typeof manager.capabilityAuditLogFile === 'string' && manager.capabilityAuditLogFile.length > 0, 'missing capability audit log path');
    assert(typeof manager.signalLowVram === 'function', 'missing signalLowVram export');
    assert(typeof manager.signalHiveOffload === 'function', 'missing signalHiveOffload export');
    assert(typeof manager.signalShedCheck === 'function', 'missing signalShedCheck export');

    {
        const parsedUrl = test._parseSyncPeerEntry('http://127.0.0.1:1234/health');
        assert(parsedUrl && parsedUrl.kind === 'url', 'sync peer url parse failed');
        const parsedExt = test._parseSyncPeerEntry({ kind: 'extension', id: 'bridge-peer' });
        assert(parsedExt && parsedExt.kind === 'extension' && parsedExt.id === 'bridge-peer', 'sync peer extension parse failed');
    }

    {
        const server = http.createServer((req, res) => {
            res.statusCode = 200;
            res.end('ok');
        });
        await new Promise((resolve) => server.listen(0, '127.0.0.1', resolve));
        const address = server.address();
        const endpoint = `http://127.0.0.1:${address.port}/health`;

        manager.configureSyncPeers([
            endpoint,
            'http://127.0.0.1:9/unreachable',
        ]);
        const discovery = await manager.discoverSyncPeers({ timeoutMs: 400 });
        assert(discovery && discovery.total === 2, 'sync peer discovery count mismatch');
        assert(discovery.healthy >= 1, 'sync peer discovery did not report healthy peer');
        const unhealthy = discovery.peers.find((peer) => peer.state !== 'healthy');
        assert(!!unhealthy, 'sync peer discovery expected unreachable peer missing');

        await new Promise((resolve) => server.close(resolve));
    }

    {
        // Multi-peer table: configureSyncPeers caps at 4 slots (SYNC_PEER_TABLE_SIZE).
        const registered5 = [
            'http://127.0.0.1:1/p1',
            'http://127.0.0.1:2/p2',
            'http://127.0.0.1:3/p3',
            'http://127.0.0.1:4/p4',
            'http://127.0.0.1:5/p5',
        ];
        const count = manager.configureSyncPeers(registered5);
        assert(count === 4, 'sync peer table should cap at 4 slots, got ' + count);
        // Reset to empty after this test.
        manager.configureSyncPeers([]);
    }

    {
        const mapped = test._normalizeLspDiagnostic('lsp_error_code=5');
        assert(mapped && mapped.name === 'LSP_ERR_FRAME_OVERSIZE', 'lsp code mapping failed');
        assert(/LSP_ERR_FRAME_OVERSIZE/.test(mapped.error), 'lsp mapped error label missing');
        const mappedNew = test._normalizeLspDiagnostic('lsp_error_code=8');
        assert(mappedNew && mappedNew.name === 'LSP_ERR_RESULT_MISSING', 'new lsp code mapping failed');
        const passthrough = test._normalizeLspDiagnostic('plain_error');
        assert(passthrough && passthrough.name === '', 'non-lsp error should not map');
    }

    test._runAuditSinkSelfTest();
    assert(fs.existsSync(manager.capabilityAuditLogFile), 'audit sink self-test did not create log file');

    const mockRecord = { name: 'mock', _seq: 0 };
    const envelope = test._buildRequestEnvelope(mockRecord, 'compile', { ok: true }, { source: 'test-runner' });
    assert(envelope.version === 1, 'envelope version mismatch');
    assert(envelope.kind === 'request', 'envelope kind mismatch');
    assert(typeof envelope.requestId === 'string' && envelope.requestId.includes('mock-'), 'requestId missing');
    assert(envelope.source === 'test-runner', 'source missing');

    assert(test._validateResponseEnvelope({
        version: 1,
        kind: 'response',
        requestId: 'r1',
        type: 'compile',
        success: true,
        result: { ok: true },
        timestamp: new Date().toISOString(),
        source: 'mock-extension-runtime',
    }) === null, 'valid response rejected');

    await withExtension('echo', async (id) => {
        const result = await manager.sendToExtension(id, 'compile', { file: 'x.asm' }, { timeoutMs: 500, source: 'test-runner' });
        assert(result.echoedType === 'compile', 'echoed type mismatch');
        assert(result.echoedPayload.file === 'x.asm', 'echoed payload mismatch');
        assert(result.source === 'test-runner', 'request source not preserved');

        const signalResult = await manager.signalLowVram(id, {
            freeBytes: 512 * 1024 * 1024,
            usedBytes: 15 * 1024 * 1024 * 1024,
            totalBytes: 16 * 1024 * 1024 * 1024,
            lane: 'cpu_fallback',
            source: 'test-gpu-probe',
        }, {
            timeoutMs: 400,
            source: 'test-runner',
        });
        assert(signalResult.signal === 'SIG_LOW_VRAM', 'low-vram signal name mismatch');
        assert(signalResult.lane === 'cpu_fallback', 'low-vram lane mismatch');
        assert(signalResult.unsupported === false, 'low-vram signal unexpectedly unsupported');
        assert(signalResult.ack && signalResult.ack.echoedType === 'runtime.signal', 'low-vram signal type not delivered');
        assert(signalResult.ack.echoedPayload && signalResult.ack.echoedPayload.signal === 'SIG_LOW_VRAM', 'low-vram payload signal missing');

        const status = manager.getStatus().find((entry) => entry.id === id);
        assert(status && status.backpressureMode === 'low_vram', 'status did not reflect low-vram mode');
        assert(status && status.backpressureLane === 'cpu_fallback', 'status did not reflect low-vram lane');

        const offloadResult = await manager.signalHiveOffload(id, { source: 'test-hive-probe' }, { timeoutMs: 400, source: 'test-runner' });
        assert(typeof offloadResult === 'object' && offloadResult !== null, 'signalHiveOffload response missing');
        assert(offloadResult.ack && offloadResult.ack.echoedType === 'hive.offload', 'hive.offload command type not echoed back');
        assert(Number.isInteger(offloadResult.requestId), 'hive.offload requestId missing');
        assert(offloadResult.ackConfirm && offloadResult.ackConfirm.echoedType === 'hive.offload.ack', 'hive.offload.ack command type not echoed back');
        assert(offloadResult.ackConfirm.echoedPayload && offloadResult.ackConfirm.echoedPayload.requestId === offloadResult.requestId, 'hive.offload.ack requestId mismatch');
        assert(offloadResult.ackConfirm.echoedPayload && offloadResult.ackConfirm.echoedPayload.outcomeCode === 0, 'hive.offload.ack outcomeCode mismatch');
        assert(offloadResult.unsupported === false, 'hive.offload signal unexpectedly unsupported');

        const duplicateAckPayload = {
            requestId: offloadResult.requestId,
            outcomeCode: 0,
            source: 'test-duplicate-ack',
            ts: new Date().toISOString(),
        };
        const duplicateAck1 = await manager.sendToExtension(id, 'hive.offload.ack', duplicateAckPayload, { timeoutMs: 400, source: 'test-runner' });
        const duplicateAck2 = await manager.sendToExtension(id, 'hive.offload.ack', duplicateAckPayload, { timeoutMs: 400, source: 'test-runner' });
        assert(duplicateAck1 && duplicateAck1.echoedType === 'hive.offload.ack', 'first duplicate ACK did not round-trip');
        assert(duplicateAck2 && duplicateAck2.echoedType === 'hive.offload.ack', 'second duplicate ACK did not round-trip');

        const shedResult = await manager.signalShedCheck(id, {}, { timeoutMs: 400, source: 'test-runner' });
        assert(typeof shedResult === 'object' && shedResult !== null, 'signalShedCheck response missing');
        assert(shedResult.ack && shedResult.ack.echoedType === 'shed.check', 'shed.check command type not echoed back');
        assert(shedResult.unsupported === false, 'shed.check signal unexpectedly unsupported');

        const secondaryId = manager.spawnExtensionHost(runtime, {
            name: 'mock_echo_secondary',
            args: [script],
            env: { RAWRXD_EXT_TEST_MODE: 'echo' },
        });
        await waitForState(secondaryId, 'ready');
        try {
            manager.configureSyncPeers([
                { kind: 'extension', id, latencyMs: 2 },
                { kind: 'extension', id: secondaryId, latencyMs: 1 },
            ]);
            const raceResult = await manager.signalHiveOffload(id, { source: 'test-hive-race' }, {
                timeoutMs: 500,
                source: 'test-runner',
                speculativeEnabled: true,
            });
            assert(raceResult && raceResult.raceUsed === true, 'speculative race was not activated');
            assert(raceResult.active === true, 'speculative race did not produce active offload state');
            assert(Number.isInteger(raceResult.winnerRequestId), 'speculative race winnerRequestId missing');
            assert(raceResult.ackConfirm && raceResult.ackConfirm.echoedType === 'hive.offload.ack', 'race offload ack confirm missing');
            assert(raceResult.ackConfirm.echoedPayload.requestId === raceResult.winnerRequestId, 'race winner requestId not reflected in ack confirm');
            const raceStatus = manager.getStatus().find((entry) => entry.id === id);
            assert(raceStatus && raceStatus.raceTelemetry && raceStatus.raceTelemetry.racesStarted === 1, 'speculative race telemetry did not count race start');
            assert(raceStatus && raceStatus.raceTelemetry && raceStatus.raceTelemetry.primaryWins + raceStatus.raceTelemetry.speculativeWins === 1, 'speculative race telemetry did not record a winner');
        } finally {
            manager.configureSyncPeers([]);
            manager.shutdown(secondaryId);
            await sleep(50);
        }
    });

    await withExtension('echo', async (id) => {
        let saw = false;
        try {
            await manager.sendToExtension(id, 'compile', { file: 'x.asm' }, { timeoutMs: 500, source: 'test-runner' });
        } catch (err) {
            saw = /pre_execution_auth_failed/.test(err.message);
        }
        assert(saw, 'pre-execution gate did not hard-prevent tampered context');
    }, {
        requirePreExecutionAuth: true,
        preExecutionAuthContext: {
            brokerDecisionHash: 'tampered',
        },
    });

    await withExtension('error-response', async (id) => {
        let saw = false;
        try {
            await manager.sendToExtension(id, 'compile', { file: 'bad.asm' }, { timeoutMs: 500, source: 'test-runner' });
        } catch (err) {
            saw = /mock failure/.test(err.message);
        }
        assert(saw, 'error response was not propagated');
    });

    await withExtension('lsp-error-response', async (id) => {
        let saw = false;
        try {
            await manager.sendToExtension(id, 'compile', { file: 'bad-lsp.asm' }, { timeoutMs: 500, source: 'test-runner' });
        } catch (err) {
            saw = /LSP_ERR_FRAME_OVERSIZE/.test(err.message);
        }
        assert(saw, 'lsp response error code was not normalized');
    });

    await withExtension('lsp-code-stderr', async (id) => {
        const result = await manager.sendToExtension(id, 'compile', { file: 'stderr-lsp.asm' }, { timeoutMs: 500, source: 'test-runner' });
        assert(result && result.ok === true, 'stderr lsp mode did not return success');
    });

    {
        const lifecycleLog = fs.existsSync(manager.logFile)
            ? fs.readFileSync(manager.logFile, 'utf8')
            : '';
        assert(/extension=mock_lsp-code-stderr/.test(lifecycleLog), 'missing lsp stderr extension lifecycle log entries');
        assert(/lsp_error=LSP_ERR_HEADER_BAD_LENGTH/.test(lifecycleLog), 'stderr lsp diagnostic mapping not emitted');
    }

    await withExtension('malformed-json', async (id) => {
        let saw = false;
        try {
            await manager.sendToExtension(id, 'compile', { file: 'bad.asm' }, { timeoutMs: 120, source: 'test-runner' });
        } catch (err) {
            saw = /timeout/.test(err.message);
        }
        assert(saw, 'malformed json path did not time out');
    });

    await withExtension('invalid-envelope', async (id) => {
        let saw = false;
        try {
            await manager.sendToExtension(id, 'compile', { file: 'bad.asm' }, { timeoutMs: 120, source: 'test-runner' });
        } catch (err) {
            saw = /timeout/.test(err.message);
        }
        assert(saw, 'invalid envelope path did not time out');
    });

    await withExtension('timeout', async (id) => {
        let saw = false;
        try {
            await manager.sendToExtension(id, 'compile', { file: 'slow.asm' }, { timeoutMs: 120, retryCount: 1, source: 'test-runner' });
        } catch (err) {
            saw = /timeout/.test(err.message);
        }
        assert(saw, 'timeout path did not fail');
    });

    await withExtension('timeout', async (id) => {
        let saw = false;
        try {
            await manager.signalHiveOffload(id, { source: 'test-hive-timeout' }, { timeoutMs: 120, source: 'test-runner' });
        } catch (err) {
            saw = /timeout/.test(err.message);
        }
        assert(saw, 'hive.offload timeout path did not fail');
    });

    await withExtension('echo', async (id) => {
        const secondaryId = manager.spawnExtensionHost(runtime, {
            name: 'mock_timeout_secondary_stress',
            args: [script],
            env: { RAWRXD_EXT_TEST_MODE: 'timeout' },
        });
        await waitForState(secondaryId, 'ready');
        try {
            const captured = await captureStderr(async () => {
                manager.configureSyncPeers([
                    { kind: 'extension', id, latencyMs: 1 },
                    { kind: 'extension', id: secondaryId, latencyMs: 2 },
                ]);
                for (let index = 0; index < 3; index++) {
                    const result = await manager.signalHiveOffload(id, { source: 'test-hive-race-stress' }, {
                        timeoutMs: 150,
                        source: 'test-runner',
                        speculativeEnabled: true,
                    });
                    assert(result.raceUsed === true, 'stress race path was not activated');
                    assert(result.active === true, 'stress race primary winner did not remain active');
                    assert(result.ack && result.ack.echoedType === 'hive.offload', 'stress race primary ack missing');
                    assert(result.ackConfirm && result.ackConfirm.echoedType === 'hive.offload.ack', 'stress race ack confirm missing');
                }
                manager.configureSyncPeers([]);
                manager.shutdown(secondaryId);
                await sleep(50);
            });
            assert(!/one_loop_policy_violation:step_already_active/i.test(captured), 'stress path still emitted step_already_active teardown noise');
            assert(!/extension=mock_timeout_secondary_stress type=hive\.speculative error=Extension_shut_down/i.test(captured), 'stress path still emitted speculative shutdown noise');
            const stressStatus = manager.getStatus().find((entry) => entry.id === id);
            assert(stressStatus && stressStatus.raceTelemetry && stressStatus.raceTelemetry.racesStarted === 3, 'stress telemetry did not count all races');
            assert(stressStatus && stressStatus.raceTelemetry && stressStatus.raceTelemetry.primaryWins === 3, 'stress telemetry did not count primary wins');
            assert(stressStatus && stressStatus.raceTelemetry && stressStatus.raceTelemetry.timeoutFailures === 0, 'stress telemetry incorrectly counted timeout failures');
        } finally {
            manager.configureSyncPeers([]);
            if (manager.getStatus().find((entry) => entry.id === secondaryId)) {
                manager.shutdown(secondaryId);
                await sleep(50);
            }
        }
    });

    await withExtension('timeout', async (id) => {
        const secondaryId = manager.spawnExtensionHost(runtime, {
            name: 'mock_timeout_secondary_dual',
            args: [script],
            env: { RAWRXD_EXT_TEST_MODE: 'timeout' },
        });
        await waitForState(secondaryId, 'ready');
        try {
            manager.configureSyncPeers([
                { kind: 'extension', id, latencyMs: 1 },
                { kind: 'extension', id: secondaryId, latencyMs: 2 },
            ]);
            let saw = false;
            try {
                await manager.signalHiveOffload(id, { source: 'test-hive-race-dual-timeout' }, {
                    timeoutMs: 120,
                    source: 'test-runner',
                    speculativeEnabled: true,
                });
            } catch (err) {
                saw = /timeout/.test(err.message);
            }
            assert(saw, 'dual-timeout speculative race did not surface timeout');
            const timeoutStatus = manager.getStatus().find((entry) => entry.id === id);
            assert(timeoutStatus && timeoutStatus.raceTelemetry && timeoutStatus.raceTelemetry.racesStarted === 1, 'dual-timeout telemetry did not count race start');
            assert(timeoutStatus && timeoutStatus.raceTelemetry && timeoutStatus.raceTelemetry.timeoutFailures === 1, 'dual-timeout telemetry did not count timeout failure');
        } finally {
            manager.configureSyncPeers([]);
            manager.shutdown(secondaryId);
            await sleep(50);
        }
    });

    await withExtension('exit-midflight', async (id) => {
        let saw = false;
        try {
            await manager.sendToExtension(id, 'compile', { file: 'crash.asm' }, { timeoutMs: 700, source: 'test-runner' });
        } catch (err) {
            saw = /Process exited with code 13/.test(err.message);
        }
        assert(saw, 'midflight crash was not propagated');
    });

    await withExtension('delayed', async (id) => {
        let saw = false;
        try {
            await manager.sendToExtension(id, 'compile', { file: 'late.asm' }, { timeoutMs: 100, source: 'test-runner' });
        } catch (err) {
            saw = /timeout/.test(err.message);
        }
        assert(saw, 'delayed response did not time out');
        await sleep(350);
    });

    let oversized = false;
    await withExtension('echo', async (id) => {
        try {
            await manager.sendToExtension(id, 'compile', { data: 'x'.repeat(70000) }, { timeoutMs: 200, source: 'test-runner' });
        } catch (err) {
            oversized = /65536 byte IPC limit/.test(err.message);
        }
    });
    assert(oversized, 'oversized payload was not rejected');

    const tempRoot = fs.mkdtempSync(path.join(os.tmpdir(), 'rawrxd-sandbox-'));
    const workspaceRoot = path.join(tempRoot, 'workspace');
    const storageRoot = path.join(tempRoot, 'storage');
    const systemLikeRoot = path.join(tempRoot, 'system');
    fs.mkdirSync(workspaceRoot, { recursive: true });
    fs.mkdirSync(storageRoot, { recursive: true });
    fs.mkdirSync(systemLikeRoot, { recursive: true });
    const allowedReadPath = path.join(workspaceRoot, 'allowed.txt');
    const deniedReadPath = path.join(systemLikeRoot, 'secret.txt');
    fs.writeFileSync(allowedReadPath, 'workspace-ok', 'utf8');
    fs.writeFileSync(deniedReadPath, 'nope', 'utf8');

    const sandboxPolicy = {
        version: 1,
        readRoots: [workspaceRoot, storageRoot],
        writeRoots: [storageRoot],
        networkAllowlist: [],
        allowProcessSpawn: false,
        allowNativeAddons: false,
        violationLimit: 10,
    };

    await withSandboxedExtension(sandboxPolicy, async (id) => {
        const readResult = await manager.sendToExtension(id, 'file.read', { path: allowedReadPath }, { timeoutMs: 500, source: 'test-runner' });
        assert(readResult.content === 'workspace-ok', 'allowed read failed');

        const writeTarget = path.join(storageRoot, 'written.txt');
        const writeResult = await manager.sendToExtension(id, 'file.write', { path: writeTarget, content: 'stored' }, { timeoutMs: 500, source: 'test-runner' });
        assert(writeResult.written === true, 'allowed write failed');
        assert(fs.readFileSync(writeTarget, 'utf8') === 'stored', 'write content mismatch');

        let deniedRead = false;
        try {
            await manager.sendToExtension(id, 'file.read', { path: deniedReadPath }, { timeoutMs: 500, source: 'test-runner' });
        } catch (err) {
            deniedRead = /file_read_denied/.test(err.message);
        }
        assert(deniedRead, 'system path read was not denied');

        let traversalDenied = false;
        try {
            await manager.sendToExtension(id, 'file.read', { path: path.join(storageRoot, '..', 'system', 'secret.txt') }, { timeoutMs: 500, source: 'test-runner' });
        } catch (err) {
            traversalDenied = /file_read_denied/.test(err.message);
        }
        assert(traversalDenied, 'path traversal was not denied');

        let networkDenied = false;
        try {
            await manager.sendToExtension(id, 'network.request', { url: 'http://127.0.0.1:11434/' }, { timeoutMs: 500, source: 'test-runner' });
        } catch (err) {
            networkDenied = /network_private_denied|network_denied/.test(err.message);
        }
        assert(networkDenied, 'network request was not denied');

        let processDenied = false;
        try {
            await manager.sendToExtension(id, 'process.spawn', { command: 'cmd.exe' }, { timeoutMs: 500, source: 'test-runner' });
        } catch (err) {
            processDenied = /process_spawn_denied/.test(err.message);
        }
        assert(processDenied, 'process spawn was not denied');

        let processExecDenied = false;
        try {
            await manager.sendToExtension(id, 'process.exec', { command: 'cmd.exe', args: [] }, { timeoutMs: 500, source: 'test-runner' });
        } catch (err) {
            processExecDenied = /process_denied|process_spawn_denied/.test(err.message);
        }
        assert(processExecDenied, 'capability broker did not deny process exec');

        let addonDenied = false;
        try {
            await manager.sendToExtension(id, 'addon.load', { name: 'ffi-napi' }, { timeoutMs: 500, source: 'test-runner' });
        } catch (err) {
            addonDenied = /native_addon_denied/.test(err.message);
        }
        assert(addonDenied, 'native addon load was not denied');
    });

    const badPolicy = '{bad-json';
    const invalidId = manager.spawnExtensionHost(runtime, {
        name: 'sandbox_invalid_policy',
        args: ['--disallow-code-generation-from-strings', '--no-addons', restrictedRuntime],
        env: {
            RAWRXD_EXTENSION_ENTRY: sandboxProbe,
            RAWRXD_SANDBOX_POLICY: badPolicy,
        },
    });
    await waitForState(invalidId, 'ready');
    let invalidPolicyDenied = false;
    try {
        await manager.sendToExtension(invalidId, 'file.read', { path: allowedReadPath }, { timeoutMs: 500, source: 'test-runner' });
    } catch (err) {
        invalidPolicyDenied = /Sandbox policy unavailable|invalid_policy/.test(err.message);
    }
    assert(invalidPolicyDenied, 'invalid policy did not fail closed');
    manager.shutdown(invalidId);
    await sleep(50);

    const thresholdPolicy = { ...sandboxPolicy, violationLimit: 3 };
    const violationId = manager.spawnExtensionHost(runtime, {
        name: 'sandbox_violation_limit',
        args: ['--disallow-code-generation-from-strings', '--no-addons', restrictedRuntime],
        env: { RAWRXD_EXTENSION_ENTRY: sandboxProbe },
        sandboxPolicy: thresholdPolicy,
    });
    await waitForState(violationId, 'ready');
    for (let index = 0; index < 3; index++) {
        try {
            await manager.sendToExtension(violationId, 'process.spawn', { command: 'cmd.exe' }, { timeoutMs: 500, source: 'test-runner' });
        } catch (_) {}
    }
    await sleep(200);
    const disabledStatus = manager.getStatus().find((entry) => entry.id === violationId);
    assert(disabledStatus && disabledStatus.state === 'disabled', 'sandbox violation threshold did not disable extension');
    manager.shutdown(violationId);
    await sleep(50);

    if (process.platform === 'win32') {
        const watchdogId = manager.spawnExtensionHost(runtime, {
            name: 'sandbox_resource_watchdog',
            args: [script],
            env: { RAWRXD_EXT_TEST_MODE: 'echo' },
            resourceLimits: {
                maxWorkingSetBytes: 1,
                sampleIntervalMs: 150,
            },
        });
        await waitForState(watchdogId, 'ready');
        await waitForState(watchdogId, 'disabled', 5000);
        manager.shutdown(watchdogId);
        await sleep(50);
    }

    const day9Policy = {
        version: 1,
        readRoots: [workspaceRoot, storageRoot],
        writeRoots: [storageRoot],
        networkAllowlist: [],
        allowProcessSpawn: false,
        allowNativeAddons: false,
        violationLimit: 10,
    };

    await withSandboxedExtension(day9Policy, async (id) => {
        const roundtrip = await manager.sendToExtension(
            id,
            'vscode.command.roundtrip',
            { commandId: 'rawrxd.test.roundtrip', value: 'slice-ok' },
            { timeoutMs: 500, source: 'test-runner' }
        );
        assert(roundtrip.echoed === 'slice-ok', 'vscode command roundtrip failed');
        assert(/day9-slice/.test(roundtrip.apiVersion), 'vscode API version missing');

        const vsWritePath = path.join(storageRoot, 'vscode-write.txt');
        await manager.sendToExtension(
            id,
            'vscode.fs.write',
            { path: vsWritePath, content: 'vscode-fs-ok' },
            { timeoutMs: 500, source: 'test-runner' }
        );
        const vsRead = await manager.sendToExtension(
            id,
            'vscode.fs.read',
            { path: vsWritePath },
            { timeoutMs: 500, source: 'test-runner' }
        );
        assert(vsRead.content === 'vscode-fs-ok', 'vscode fs read/write flow failed');

        const info = await manager.sendToExtension(
            id,
            'vscode.window.info',
            { message: 'hello-slice' },
            { timeoutMs: 500, source: 'test-runner' }
        );
        assert(info.message === 'hello-slice', 'vscode window info flow failed');

        let unknownCommandDenied = false;
        try {
            await manager.sendToExtension(
                id,
                'vscode.command.execute_unknown',
                {},
                { timeoutMs: 500, source: 'test-runner' }
            );
        } catch (err) {
            unknownCommandDenied = /unsupported_or_unregistered_command/.test(err.message);
        }
        assert(unknownCommandDenied, 'unregistered command did not fail predictably');

        let unsupportedDenied = false;
        try {
            await manager.sendToExtension(
                id,
                'vscode.unsupported.debug',
                {},
                { timeoutMs: 500, source: 'test-runner' }
            );
        } catch (err) {
            unsupportedDenied = /unsupported_api:debug\.startDebugging/.test(err.message);
        }
        assert(unsupportedDenied, 'unsupported API did not fail predictably');
    }, vscodeApiProbe, 'vscode_api_probe');

    const day10Manifest = {
        abiVersion: 1,
        requires: ['capability:commands@1', 'capability:workspace.fs@1', 'capability:window.messages@1'],
    };
    const trustedSigner = crypto.generateKeyPairSync('ed25519');
    const untrustedSigner = crypto.generateKeyPairSync('ed25519');
    const trustAnchors = {
        'trusted-root': trustedSigner.publicKey.export({ format: 'pem', type: 'spki' }),
    };
    const signedDay10Manifest = signManifest(day10Manifest, trustedSigner.privateKey, 'trusted-root');

    await withSandboxedExtension(
        day9Policy,
        async (id) => {
            const roundtrip = await manager.sendToExtension(
                id,
                'vscode.command.roundtrip',
                { commandId: 'rawrxd.test.negotiated', value: 'abi-ok' },
                { timeoutMs: 500, source: 'test-runner' }
            );
            assert(roundtrip.echoed === 'abi-ok', 'manifest-negotiated command roundtrip failed');
        },
        vscodeApiProbe,
        'vscode_api_probe_manifest_ok',
        {
            extensionManifest: signedDay10Manifest,
            manifestTrustAnchors: trustAnchors,
        }
    );

    async function expectManifestFailure(name, spawnOptions, expectedPattern) {
        let id = null;
        let denied = false;
        try {
            id = manager.spawnExtensionHost(runtime, {
                name,
                args: ['--disallow-code-generation-from-strings', '--no-addons', restrictedRuntime],
                env: { RAWRXD_EXTENSION_ENTRY: vscodeApiProbe, ...(spawnOptions.env || {}) },
                sandboxPolicy: day9Policy,
                extensionManifest: spawnOptions.extensionManifest,
                manifestTrustAnchors: spawnOptions.manifestTrustAnchors,
            });
            await waitForState(id, 'ready');
            await manager.sendToExtension(
                id,
                'vscode.window.info',
                { message: 'should-fail' },
                { timeoutMs: 500, source: 'test-runner' }
            );
        } catch (err) {
            denied = expectedPattern.test(err.message);
        }
        if (id) {
            manager.shutdown(id);
            await sleep(50);
        }
        assert(denied, `manifest negotiation did not fail as expected for ${name}`);
    }

    await expectManifestFailure(
        'vscode_api_probe_manifest_missing_signature',
        {
            extensionManifest: day10Manifest,
            manifestTrustAnchors: trustAnchors,
        },
        /manifest_signature_failed:manifest_missing_signature/
    );

    const tamperedManifest = {
        ...signedDay10Manifest,
        requires: [...signedDay10Manifest.requires, 'capability:commands@2'],
    };
    await expectManifestFailure(
        'vscode_api_probe_manifest_tampered',
        {
            extensionManifest: tamperedManifest,
            manifestTrustAnchors: trustAnchors,
        },
        /manifest_signature_failed:manifest_signature_mismatch/
    );

    const wrongKeySignedManifest = signManifest(day10Manifest, untrustedSigner.privateKey, 'trusted-root');
    await expectManifestFailure(
        'vscode_api_probe_manifest_wrong_key',
        {
            extensionManifest: wrongKeySignedManifest,
            manifestTrustAnchors: trustAnchors,
        },
        /manifest_signature_failed:manifest_signature_mismatch/
    );

    const unknownKeySignedManifest = signManifest(day10Manifest, untrustedSigner.privateKey, 'untrusted-root');
    await expectManifestFailure(
        'vscode_api_probe_manifest_untrusted_key',
        {
            extensionManifest: unknownKeySignedManifest,
            manifestTrustAnchors: trustAnchors,
        },
        /manifest_signature_failed:manifest_untrusted_signing_key/
    );

    await expectManifestFailure(
        'vscode_api_probe_manifest_bad_json',
        { env: { RAWRXD_EXTENSION_MANIFEST: '{bad-json' } },
        /manifest_negotiation_failed:manifest_invalid_json/
    );

    await expectManifestFailure(
        'vscode_api_probe_manifest_abi_mismatch',
        {
            extensionManifest: signManifest({ abiVersion: 2, requires: [] }, trustedSigner.privateKey, 'trusted-root'),
            manifestTrustAnchors: trustAnchors,
        },
        /manifest_negotiation_failed:manifest_abi_mismatch/
    );

    await expectManifestFailure(
        'vscode_api_probe_manifest_unknown_cap',
        {
            extensionManifest: signManifest({ abiVersion: 1, requires: ['capability:unknown@1'] }, trustedSigner.privateKey, 'trusted-root'),
            manifestTrustAnchors: trustAnchors,
        },
        /manifest_negotiation_failed:manifest_unknown_capability:capability:unknown/
    );

    await expectManifestFailure(
        'vscode_api_probe_manifest_version_high',
        {
            extensionManifest: signManifest({ abiVersion: 1, requires: ['capability:commands@2'] }, trustedSigner.privateKey, 'trusted-root'),
            manifestTrustAnchors: trustAnchors,
        },
        /manifest_negotiation_failed:manifest_capability_version_mismatch:capability:commands@2>1/
    );

    const processEnabledPolicy = {
        ...day9Policy,
        allowProcessSpawn: true,
        processAllowlist: [],
    };
    const signedProcessAllowlistManifest = signManifest({
        abiVersion: 1,
        requires: ['capability:process@1'],
        processAllowlist: {
            version: 1,
            commands: ['cmd.exe'],
        },
    }, trustedSigner.privateKey, 'trusted-root');

    await withSandboxedExtension(
        processEnabledPolicy,
        async (id) => {
            const execResult = await manager.sendToExtension(
                id,
                'process.exec',
                { command: 'cmd.exe', args: ['/c', 'exit', '0'] },
                { timeoutMs: 1000, source: 'test-runner' }
            );
            assert(execResult && typeof execResult.code === 'number', 'signed process allowlist execution did not return process result');

            let deniedByManifest = false;
            try {
                await manager.sendToExtension(
                    id,
                    'process.exec',
                    { command: 'powershell.exe', args: ['-NoProfile', '-Command', 'exit 0'] },
                    { timeoutMs: 1000, source: 'test-runner' }
                );
            } catch (err) {
                deniedByManifest = /process_manifest_denied/.test(err.message);
            }
            assert(deniedByManifest, 'unsigned process exception was not denied by manifest allowlist');
        },
        sandboxProbe,
        'sandbox_process_allowlist_signed',
        {
            extensionManifest: signedProcessAllowlistManifest,
            manifestTrustAnchors: trustAnchors,
        }
    );

    await expectManifestFailure(
        'vscode_api_probe_manifest_process_allowlist_invalid',
        {
            extensionManifest: signManifest({
                abiVersion: 1,
                requires: ['capability:process@1'],
                processAllowlist: {
                    version: 1,
                    commands: [],
                },
            }, trustedSigner.privateKey, 'trusted-root'),
            manifestTrustAnchors: trustAnchors,
        },
        /manifest_negotiation_failed:manifest_process_allowlist_invalid_commands/
    );

    const localServer = require('http').createServer((req, res) => {
        res.writeHead(200, { 'content-type': 'text/plain' });
        res.end('network-ok');
    });
    await new Promise((resolve, reject) => {
        localServer.listen(0, '127.0.0.1', (error) => {
            if (error) return reject(error);
            resolve();
        });
    });
    const localPort = localServer.address() && localServer.address().port;

    const networkEnabledPolicy = {
        ...day9Policy,
        allowPrivateNetwork: true,
        networkAllowlist: ['127.0.0.1', '127.0.0.2'],
    };
    const signedNetworkAllowlistManifest = signManifest({
        abiVersion: 1,
        requires: ['capability:network@1'],
        networkAllowlist: {
            version: 1,
            hosts: ['127.0.0.1'],
        },
    }, trustedSigner.privateKey, 'trusted-root');

    try {
        await withSandboxedExtension(
            networkEnabledPolicy,
            async (id) => {
                const allowedResult = await manager.sendToExtension(
                    id,
                    'network.request',
                    { url: `http://127.0.0.1:${localPort}/` },
                    { timeoutMs: 1000, source: 'test-runner' }
                );
                assert(allowedResult && allowedResult.statusCode === 200, 'signed network allowlist did not allow expected host');
                assert(allowedResult.body === 'network-ok', 'network response payload mismatch');

                let deniedByManifestNetwork = false;
                try {
                    await manager.sendToExtension(
                        id,
                        'network.request',
                        { url: `http://127.0.0.2:${localPort}/` },
                        { timeoutMs: 1000, source: 'test-runner' }
                    );
                } catch (err) {
                    deniedByManifestNetwork = /network_manifest_denied/.test(err.message);
                }
                assert(deniedByManifestNetwork, 'unsigned network host exception was not denied by manifest allowlist');
            },
            sandboxProbe,
            'sandbox_network_allowlist_signed',
            {
                extensionManifest: signedNetworkAllowlistManifest,
                manifestTrustAnchors: trustAnchors,
            }
        );
    } finally {
        await new Promise((resolve) => localServer.close(() => resolve()));
    }

    await expectManifestFailure(
        'vscode_api_probe_manifest_network_allowlist_invalid',
        {
            extensionManifest: signManifest({
                abiVersion: 1,
                requires: ['capability:network@1'],
                networkAllowlist: {
                    version: 1,
                    hosts: [],
                },
            }, trustedSigner.privateKey, 'trusted-root'),
            manifestTrustAnchors: trustAnchors,
        },
        /manifest_negotiation_failed:manifest_network_allowlist_invalid_hosts/
    );

    const preExecutionSigner = crypto.generateKeyPairSync('ed25519');
    const preExecutionTrustAnchors = {
        'preexec-root': preExecutionSigner.publicKey.export({ format: 'pem', type: 'spki' }),
    };
    const preExecutionPrivateKeyPem = preExecutionSigner.privateKey.export({ format: 'pem', type: 'pkcs8' });

    await withSandboxedExtension(
        day9Policy,
        async (id) => {
            const readResult = await manager.sendToExtension(
                id,
                'file.read',
                { path: allowedReadPath },
                { timeoutMs: 500, source: 'test-runner' }
            );
            assert(readResult && readResult.content === 'workspace-ok', 'signed pre-execution capability flow failed');

            assert(fs.existsSync(manager.capabilityAuditLogFile), 'capability audit log missing for signed pre-execution flow');
            const signedTraceLines = fs.readFileSync(manager.capabilityAuditLogFile, 'utf8')
                .split(/\r?\n/)
                .map((line) => line.trim())
                .filter(Boolean)
                .map((line) => JSON.parse(line))
                .filter((event) => event.extensionId === 'sandbox_preexec_signed_ok');
            const signedDecision = signedTraceLines.find((event) => event.type === 'capability.decision' && event.decision === 'allow');
            const signedExecuteStart = signedTraceLines.find((event) => signedDecision && event.type === 'capability.execute.start' && event.requestId === signedDecision.requestId);
            const signedExecuteResult = signedTraceLines.find((event) => signedDecision && event.type === 'capability.execute.result' && event.requestId === signedDecision.requestId);
            assert(signedDecision && signedDecision.preExecutionSignatureKeyId, 'missing pre-exec signature key id on signed decision');
            assert(signedDecision && signedDecision.preExecutionSignatureAlgorithm, 'missing pre-exec signature algorithm on signed decision');
            assert(signedDecision && signedDecision.preExecutionSignatureValue, 'missing pre-exec signature value on signed decision');
            assert(signedExecuteStart && signedExecuteStart.preExecutionSignatureValue === signedDecision.preExecutionSignatureValue, 'signed execute.start pre-exec signature mismatch');
            assert(signedExecuteResult && signedExecuteResult.preExecutionSignatureValue === signedDecision.preExecutionSignatureValue, 'signed execute.result pre-exec signature mismatch');
        },
        sandboxProbe,
        'sandbox_preexec_signed_ok',
        {
            requirePreExecutionAuth: true,
            requireSignatures: true,
            signatureTrustAnchors: preExecutionTrustAnchors,
            preExecutionSigningKeyId: 'preexec-root',
            preExecutionSigningPrivateKeyPem: preExecutionPrivateKeyPem,
        }
    );

    let preExecutionDenied = false;
    await withSandboxedExtension(
        day9Policy,
        async (id) => {
            try {
                await manager.sendToExtension(
                    id,
                    'file.read',
                    { path: allowedReadPath },
                    { timeoutMs: 500, source: 'test-runner' }
                );
            } catch (err) {
                preExecutionDenied = /pre_execution_auth_failed:preexec_signature_missing/.test(err.message);
            }
        },
        sandboxProbe,
        'sandbox_preexec_signed_missing',
        {
            requirePreExecutionAuth: true,
            requireSignatures: true,
            signatureTrustAnchors: preExecutionTrustAnchors,
        }
    );
    assert(preExecutionDenied, 'missing signed pre-execution authorization did not fail closed');

    if (fs.existsSync(manager.capabilityAuditLogFile)) {
        fs.unlinkSync(manager.capabilityAuditLogFile);
    }

    await withSandboxedExtension(day9Policy, async (id) => {
        const writeTarget = path.join(storageRoot, 'trace-check.txt');
        const writeResult = await manager.sendToExtension(
            id,
            'file.write',
            { path: writeTarget, content: 'trace-ok' },
            { timeoutMs: 500, source: 'test-runner' }
        );
        assert(writeResult.written === true, 'trace precondition write failed');
    }, sandboxProbe, 'sandbox_trace_probe');

    assert(fs.existsSync(manager.capabilityAuditLogFile), 'capability audit log was not created');
    const traceLines = fs.readFileSync(manager.capabilityAuditLogFile, 'utf8')
        .split(/\r?\n/)
        .map((line) => line.trim())
        .filter(Boolean)
        .map((line) => JSON.parse(line));

    const writeRequest = traceLines.find((event) => event.type === 'capability.request' && event.capability === 'file.write');
    assert(writeRequest, 'missing capability.request event for file.write');

    const correlated = traceLines.filter((event) => event.requestId === writeRequest.requestId);
    const eventTypes = new Set(correlated.map((event) => event.type));
    assert(eventTypes.has('capability.request'), 'missing request event in trace correlation');
    assert(eventTypes.has('capability.decision'), 'missing decision event in trace correlation');
    assert(eventTypes.has('capability.execute.start'), 'missing execute.start event in trace correlation');
    assert(eventTypes.has('capability.execute.result'), 'missing execute.result event in trace correlation');
    assert(correlated.every((event) => event.schemaVersion === AUDIT_SCHEMA_VERSION), 'schema version mismatch in audit events');
    assert(correlated.every((event) => event.stageIndex === STAGE_INDEX[event.type]), 'stage index mismatch in audit events');
    assert(correlated.every((event) => typeof event.manifestHash === 'string' && event.manifestHash.length > 0), 'missing manifest hash in audit events');
    assert(correlated.every((event) => typeof event.eventChainHash === 'string' && event.eventChainHash.length > 0), 'missing event chain hash in audit events');
    const ordered = [...correlated].sort((a, b) => a.sequence - b.sequence);
    for (let index = 1; index < ordered.length; index++) {
        assert(ordered[index - 1].sequence < ordered[index].sequence, 'non-monotonic event sequence detected');
        assert(ordered[index - 1].stageIndex <= ordered[index].stageIndex, 'non-deterministic stage ordering detected');
    }
    const decisionEvent = correlated.find((event) => event.type === 'capability.decision');
    const executeStartEvent = correlated.find((event) => event.type === 'capability.execute.start');
    const executeResultEvent = correlated.find((event) => event.type === 'capability.execute.result');
    assert(decisionEvent && decisionEvent.brokerDecisionHash, 'missing broker decision hash');
    assert(executeStartEvent && executeStartEvent.brokerDecisionHash === decisionEvent.brokerDecisionHash, 'execute.start broker hash mismatch');
    assert(executeResultEvent && executeResultEvent.brokerDecisionHash === decisionEvent.brokerDecisionHash, 'execute.result broker hash mismatch');
    assert(decisionEvent && typeof decisionEvent.preExecutionAuthHash === 'string' && decisionEvent.preExecutionAuthHash.length > 0, 'missing pre-exec auth hash');
    assert(executeStartEvent && executeStartEvent.preExecutionAuthHash === decisionEvent.preExecutionAuthHash, 'execute.start pre-exec auth hash mismatch');
    assert(executeResultEvent && executeResultEvent.preExecutionAuthHash === decisionEvent.preExecutionAuthHash, 'execute.result pre-exec auth hash mismatch');
    assert(executeResultEvent && executeResultEvent.executionResultHash, 'missing execution result hash');
    assert(typeof writeRequest.lineageId === 'string' && writeRequest.lineageId.length > 0, 'missing lineageId on request event');
    assert(correlated.every((event) => event.lineageId === writeRequest.lineageId), 'lineageId drift detected across correlated trace events');

    // ── Trust-anchor file store ──────────────────────────────────────────────
    assert(typeof test._loadTrustAnchorsFile === 'function', 'missing _loadTrustAnchorsFile export');
    const fileAnchors = test._loadTrustAnchorsFile();
    assert(fileAnchors !== null && typeof fileAnchors === 'object', 'trust-anchors.json did not return an object');

    // ── policyDigest wired into events ───────────────────────────────────────
    // correlated events (write lineage) are from a sandboxed extension that has a policy.
    // Every event in the deny-path lineages should carry a consistent policyDigest.
    // The sandboxed write lineage above used sandboxPolicy, so its decision event should
    // carry a non-empty policyDigest that is consistent across the lineage.
    assert(
        correlated.every((ev) => typeof ev.policyDigest === 'string'),
        'policyDigest field missing from correlated audit events'
    );
    const firstPolicy = correlated[0].policyDigest;
    assert(
        correlated.every((ev) => ev.policyDigest === firstPolicy),
        'policyDigest inconsistent within lineage'
    );

    // ── denyReasonDigest wired into deny events ──────────────────────────────
    // Re-read the audit log and find any deny decision events to verify denyReasonDigest.
    if (fs.existsSync(manager.capabilityAuditLogFile)) {
        const logLines = fs.readFileSync(manager.capabilityAuditLogFile, 'utf8')
            .split(/\r?\n/).map((l) => l.trim()).filter(Boolean);
        for (const line of logLines) {
            let ev;
            try { ev = JSON.parse(line); } catch (_) { continue; }
            if (ev.type === 'capability.decision' && ev.decision === 'deny') {
                assert(
                    typeof ev.denyReasonDigest === 'string',
                    'deny decision event missing denyReasonDigest field'
                );
                assert(
                    ev.denyReasonDigest !== null,
                    'deny decision event has null denyReasonDigest'
                );
            }
        }
    }

    console.log('manager envelope + sandbox + vscode slice + manifest negotiation tests ok');
}

main()
    .then(() => {
        manager.shutdownAll();
        console.log('PASS');
    })
    .catch((err) => {
        manager.shutdownAll();
        console.error('FAIL:', err.message);
        process.exit(1);
    });