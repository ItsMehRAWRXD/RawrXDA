'use strict';

const fs = require('fs');
const { validateCanonicalEvent, STAGE_INDEX } = require('./capability-audit-schema');
const {
    sha256Hex,
    buildPreExecutionAuthorizationPayload,
    computeBrokerDecisionHash,
    computeExecutionResultHash,
    computeNextEventChainHash,
    verifyPreExecutionAuthorization,
    verifyLineageSignature,
    buildDenyReasonDigest,
    verifyDenyReasonDigest,
} = require('./capability-lineage-binding');

function _readEvents(filePath) {
    if (!fs.existsSync(filePath)) return [];
    const raw = fs.readFileSync(filePath, 'utf8');
    const lines = raw.split(/\r?\n/).map((line) => line.trim()).filter(Boolean);
    return lines.map((line) => {
        const parsed = JSON.parse(line);
        validateCanonicalEvent(parsed);
        return parsed;
    });
}

function _ts(value) {
    const parsed = Date.parse(value);
    return Number.isFinite(parsed) ? parsed : NaN;
}

function _assertTimestamp(label, value, lineageId) {
    if (typeof value !== 'string' || !value || Number.isNaN(_ts(value))) {
        throw new Error('invalid_timestamp_' + label + ':' + lineageId);
    }
}

function verifyReplayLog(filePath, options = {}) {
    const events = _readEvents(filePath);
    const byLineage = new Map();
    const trustAnchors = options && options.signatureTrustAnchors && typeof options.signatureTrustAnchors === 'object'
        ? options.signatureTrustAnchors
        : null;
    const requireSignatures = !!(options && options.requireSignatures);
    const requirePreExecutionSignatures = !!(options && options.requirePreExecutionSignatures);

    for (const event of events) {
        if (!event || typeof event !== 'object') {
            throw new Error('invalid_event_shape');
        }
        if (!event.lineageId || typeof event.lineageId !== 'string') {
            throw new Error('missing_lineage_id');
        }
        if (!byLineage.has(event.lineageId)) {
            byLineage.set(event.lineageId, []);
        }
        byLineage.get(event.lineageId).push(event);
    }

    let allowCount = 0;
    let denyCount = 0;
    let signedLineages = 0;
    let preExecutionSignedLineages = 0;

    for (const [lineageId, lineageEvents] of byLineage) {
        lineageEvents.sort((a, b) => {
            if (a.sequence !== b.sequence) return a.sequence - b.sequence;
            if (a.stageIndex !== b.stageIndex) return a.stageIndex - b.stageIndex;
            return _ts(a.timestamp) - _ts(b.timestamp);
        });

        const seenSequences = new Set();
        for (let index = 0; index < lineageEvents.length; index++) {
            const event = lineageEvents[index];
            if (seenSequences.has(event.sequence)) {
                throw new Error('duplicate_sequence_in_lineage:' + lineageId);
            }
            seenSequences.add(event.sequence);
            if (index > 0 && event.sequence <= lineageEvents[index - 1].sequence) {
                throw new Error('non_monotonic_sequence_in_lineage:' + lineageId);
            }
        }

        const canonicalRequestId = lineageEvents[0].requestId;
        const canonicalCapability = lineageEvents[0].capability;
        const canonicalRequestTs = lineageEvents[0].requestTs;
        const canonicalManifestHash = lineageEvents[0].manifestHash;
        const canonicalPolicyDigest = lineageEvents[0].policyDigest || '';

        for (const event of lineageEvents) {
            if (event.requestId !== canonicalRequestId) {
                throw new Error('request_id_drift:' + lineageId);
            }
            if (event.capability !== canonicalCapability) {
                throw new Error('capability_drift:' + lineageId);
            }
            if (event.requestTs !== canonicalRequestTs) {
                throw new Error('request_ts_drift:' + lineageId);
            }
            if (event.manifestHash !== canonicalManifestHash) {
                throw new Error('manifest_hash_drift:' + lineageId);
            }
            const evPolicyDigest = event.policyDigest || '';
            if (evPolicyDigest !== canonicalPolicyDigest) {
                throw new Error('policy_digest_drift:' + lineageId);
            }
        }

        const requestEvents = lineageEvents.filter((event) => event.type === 'capability.request');
        const decisionEvents = lineageEvents.filter((event) => event.type === 'capability.decision');
        const execStartEvents = lineageEvents.filter((event) => event.type === 'capability.execute.start');
        const execResultEvents = lineageEvents.filter((event) => event.type === 'capability.execute.result');

        if (requestEvents.length !== 1) throw new Error('invalid_request_count:' + lineageId);
        if (decisionEvents.length !== 1) throw new Error('invalid_decision_count:' + lineageId);

        const argsHash = requestEvents[0].argsHash;
        for (const event of lineageEvents) {
            if (event.argsHash !== argsHash) {
                throw new Error('args_hash_mismatch:' + lineageId);
            }
        }

        const requestEvent = requestEvents[0];
        const decisionEvent = decisionEvents[0];

        _assertTimestamp('request', requestEvent.timestamp, lineageId);
        _assertTimestamp('request_snapshot', requestEvent.requestTs, lineageId);
        if (requestEvent.timestamp !== requestEvent.requestTs) {
            throw new Error('request_timestamp_mismatch:' + lineageId);
        }

        _assertTimestamp('decision', decisionEvent.timestamp, lineageId);
        _assertTimestamp('decision_snapshot', decisionEvent.decisionTs, lineageId);
        if (decisionEvent.timestamp !== decisionEvent.decisionTs) {
            throw new Error('decision_timestamp_mismatch:' + lineageId);
        }
        if (decisionEvent.requestTs !== requestEvent.requestTs) {
            throw new Error('decision_request_ts_mismatch:' + lineageId);
        }

        const requestTs = _ts(requestEvent.timestamp);
        const decisionTs = _ts(decisionEvent.timestamp);
        if (!(requestTs <= decisionTs)) {
            throw new Error('request_after_decision:' + lineageId);
        }

        const stageSequence = lineageEvents.map((event) => STAGE_INDEX[event.type]);
        for (let index = 1; index < stageSequence.length; index++) {
            if (stageSequence[index] < stageSequence[index - 1]) {
                throw new Error('out_of_order_transition:' + lineageId);
            }
        }

        let chain = '';
        for (const event of lineageEvents) {
            const expected = computeNextEventChainHash(chain, event);
            if (event.eventChainHash !== expected) {
                throw new Error('event_chain_hash_mismatch:' + lineageId);
            }
            chain = expected;
        }

        const expectedDecisionHash = computeBrokerDecisionHash({
            lineageId,
            requestId: canonicalRequestId,
            extensionId: requestEvent.extensionId,
            capability: canonicalCapability,
            argsHash,
            manifestHash: canonicalManifestHash,
        }, decisionEvent.decision, decisionEvent.resultCode || '');

        if (decisionEvent.brokerDecisionHash !== expectedDecisionHash) {
            throw new Error('broker_decision_hash_mismatch:' + lineageId);
        }

        const decision = decisionEvent.decision;
        if (decision === 'deny') {
            denyCount++;
            if (execStartEvents.length > 0) {
                throw new Error('deny_has_execute_start:' + lineageId);
            }
            if (execResultEvents.length > 0) {
                throw new Error('deny_has_execute_result:' + lineageId);
            }
            // Verify deny reason digest if present
            const storedDenyDigest = decisionEvent.denyReasonDigest || '';
            if (storedDenyDigest) {
                const denyCtx = {
                    lineageId,
                    requestId: canonicalRequestId,
                    extensionId: requestEvent.extensionId,
                    capability: canonicalCapability,
                    argsHash,
                    manifestHash: canonicalManifestHash,
                    decisionTs: decisionEvent.decisionTs,
                };
                const denyCheck = verifyDenyReasonDigest(denyCtx, decisionEvent.resultCode || '', storedDenyDigest);
                if (!denyCheck.ok) {
                    throw new Error('deny_reason_digest_mismatch:' + lineageId);
                }
            }
            continue;
        }

        if (decision !== 'allow') {
            throw new Error('unknown_decision:' + lineageId);
        }

        allowCount++;
        if (execStartEvents.length !== 1) throw new Error('invalid_execute_start_count:' + lineageId);
        if (execResultEvents.length !== 1) throw new Error('invalid_execute_result_count:' + lineageId);

        const executeStartEvent = execStartEvents[0];
        const executeResultEvent = execResultEvents[0];

        if (typeof decisionEvent.preExecutionAuthHash !== 'string' || !decisionEvent.preExecutionAuthHash) {
            throw new Error('missing_preexec_auth_hash:' + lineageId);
        }
        if (executeStartEvent.preExecutionAuthHash !== decisionEvent.preExecutionAuthHash) {
            throw new Error('preexec_hash_drift_execute_start:' + lineageId);
        }
        if (executeResultEvent.preExecutionAuthHash !== decisionEvent.preExecutionAuthHash) {
            throw new Error('preexec_hash_drift_execute_result:' + lineageId);
        }

        const expectedPreExecHash = sha256Hex(buildPreExecutionAuthorizationPayload({
            lineageId,
            requestId: canonicalRequestId,
            extensionId: requestEvent.extensionId,
            capability: canonicalCapability,
            argsHash,
            manifestHash: canonicalManifestHash,
            decision: decisionEvent.decision,
            resultCode: decisionEvent.resultCode || '',
            brokerDecisionHash: expectedDecisionHash,
            requestTs: requestEvent.requestTs,
            decisionTs: decisionEvent.decisionTs,
            eventChainHash: requestEvent.eventChainHash,
        }));
        if (decisionEvent.preExecutionAuthHash !== expectedPreExecHash) {
            throw new Error('preexec_auth_hash_mismatch:' + lineageId);
        }

        const decisionPreExecSigFields = [
            decisionEvent.preExecutionSignatureKeyId,
            decisionEvent.preExecutionSignatureAlgorithm,
            decisionEvent.preExecutionSignatureValue,
        ];
        const executeStartPreExecSigFields = [
            executeStartEvent.preExecutionSignatureKeyId,
            executeStartEvent.preExecutionSignatureAlgorithm,
            executeStartEvent.preExecutionSignatureValue,
        ];
        const executeResultPreExecSigFields = [
            executeResultEvent.preExecutionSignatureKeyId,
            executeResultEvent.preExecutionSignatureAlgorithm,
            executeResultEvent.preExecutionSignatureValue,
        ];
        const decisionHasPreExecSignature = decisionPreExecSigFields.some((field) => typeof field === 'string' && field.length > 0);

        if (decisionHasPreExecSignature) {
            if (executeStartPreExecSigFields[0] !== decisionPreExecSigFields[0] ||
                executeStartPreExecSigFields[1] !== decisionPreExecSigFields[1] ||
                executeStartPreExecSigFields[2] !== decisionPreExecSigFields[2]) {
                throw new Error('preexec_signature_drift_execute_start:' + lineageId);
            }
            if (executeResultPreExecSigFields[0] !== decisionPreExecSigFields[0] ||
                executeResultPreExecSigFields[1] !== decisionPreExecSigFields[1] ||
                executeResultPreExecSigFields[2] !== decisionPreExecSigFields[2]) {
                throw new Error('preexec_signature_drift_execute_result:' + lineageId);
            }

            const preExecSignatureCheck = verifyPreExecutionAuthorization({
                lineageId,
                requestId: canonicalRequestId,
                extensionId: requestEvent.extensionId,
                capability: canonicalCapability,
                argsHash,
                manifestHash: canonicalManifestHash,
                decision: decisionEvent.decision,
                resultCode: decisionEvent.resultCode || '',
                brokerDecisionHash: expectedDecisionHash,
                requestTs: requestEvent.requestTs,
                decisionTs: decisionEvent.decisionTs,
                eventChainHash: requestEvent.eventChainHash,
                signatureKeyId: decisionEvent.preExecutionSignatureKeyId,
                signatureAlgorithm: decisionEvent.preExecutionSignatureAlgorithm,
                signatureValue: decisionEvent.preExecutionSignatureValue,
            }, trustAnchors);
            if (!preExecSignatureCheck.present || !preExecSignatureCheck.ok) {
                throw new Error('preexec_signature_invalid:' + lineageId + ':' + preExecSignatureCheck.reason);
            }
            preExecutionSignedLineages++;
        } else if (requirePreExecutionSignatures) {
            throw new Error('preexec_signature_missing:' + lineageId);
        }

        if (executeStartEvent.brokerDecisionHash !== expectedDecisionHash) {
            throw new Error('execute_start_broker_hash_mismatch:' + lineageId);
        }

        _assertTimestamp('execute_start', executeStartEvent.timestamp, lineageId);
        _assertTimestamp('execute_start_snapshot', executeStartEvent.executeStartTs, lineageId);
        if (executeStartEvent.timestamp !== executeStartEvent.executeStartTs) {
            throw new Error('execute_start_timestamp_mismatch:' + lineageId);
        }
        if (executeStartEvent.decisionTs !== decisionEvent.decisionTs) {
            throw new Error('execute_start_decision_ts_mismatch:' + lineageId);
        }

        _assertTimestamp('execute_result', executeResultEvent.timestamp, lineageId);
        _assertTimestamp('execute_result_snapshot', executeResultEvent.executeResultTs, lineageId);
        if (executeResultEvent.timestamp !== executeResultEvent.executeResultTs) {
            throw new Error('execute_result_timestamp_mismatch:' + lineageId);
        }
        if (executeResultEvent.executeStartTs !== executeStartEvent.executeStartTs) {
            throw new Error('execute_result_start_ts_mismatch:' + lineageId);
        }

        const executeStartTs = _ts(executeStartEvent.timestamp);
        const executeResultTs = _ts(executeResultEvent.timestamp);

        if (!(decisionTs <= executeStartTs)) {
            throw new Error('decision_after_execute_start:' + lineageId);
        }
        if (!(executeStartTs <= executeResultTs)) {
            throw new Error('execute_start_after_result:' + lineageId);
        }

        if (typeof executeResultEvent.success !== 'boolean') {
            throw new Error('missing_success_flag:' + lineageId);
        }

        const expectedResultHash = computeExecutionResultHash({
            lineageId,
            requestId: canonicalRequestId,
            extensionId: requestEvent.extensionId,
            capability: canonicalCapability,
            argsHash,
            manifestHash: canonicalManifestHash,
            brokerDecisionHash: expectedDecisionHash,
        }, executeResultEvent.success, executeResultEvent.resultCode || '');

        if (executeResultEvent.executionResultHash !== expectedResultHash) {
            throw new Error('execution_result_hash_mismatch:' + lineageId);
        }
        if (executeResultEvent.brokerDecisionHash !== expectedDecisionHash) {
            throw new Error('execute_result_broker_hash_mismatch:' + lineageId);
        }

        const signatureCheck = verifyLineageSignature(executeResultEvent, trustAnchors);
        if (signatureCheck.present) {
            if (!signatureCheck.ok) {
                throw new Error('lineage_signature_invalid:' + lineageId + ':' + signatureCheck.reason);
            }
            signedLineages++;
        } else if (requireSignatures) {
            throw new Error('lineage_signature_missing:' + lineageId);
        }
    }

    return {
        entries: events.length,
        lineages: byLineage.size,
        allowCount,
        denyCount,
        signedLineages,
        preExecutionSignedLineages,
    };
}

module.exports = {
    verifyReplayLog,
};
