// Phase 4b: Manager-Side Race Coordination helper
// This contains the updated signalHiveOffload implementation with speculative racing

/**
 * Signal the bridge to activate hive offload routing for the primary sync peer.
 * If speculative inference is enabled, emits parallel requests to top-2 latency peers
 * and returns the first-to-ack result.
 * @param {string} id Extension host ID
 * @param {Map} extensions Extension registry
 * @param {Function} sendToExtension IPC sender
 * @param {Function} logEvent Event logger
 * @param {string} IPC_TIMEOUT_MS Timeout constant
 * @param {Array} syncPeerRegistry Peer registry
 * @param {{ source?: string }} [telemetry]
 * @param {{ timeoutMs?: number, source?: string, speculativeEnabled?: boolean }} [options]
 * @returns {Promise}
 */
async function signalHiveOffloadRace(id, extensions, sendToExtension, logEvent, IPC_TIMEOUT_MS, syncPeerRegistry, telemetry = {}, options = {}) {
    function _asString(value, fallback = '') {
        if (typeof value === 'string') return value;
        if (value === undefined || value === null) return fallback;
        return String(value);
    }

    const record = extensions.get(id);
    if (!record) {
        throw new Error('Extension id not found: ' + id);
    }

    // Helper: Get top N peers by latency
    function _getTopNPeersByLatency(topN = 2) {
        if (!Array.isArray(syncPeerRegistry) || syncPeerRegistry.length === 0) {
            return [];
        }
        const sorted = [...syncPeerRegistry]
            .filter(p => p && typeof p === 'object' && p.state === 'healthy')
            .sort((a, b) => {
                const aLat = Number(a.latencyMs) || 999999;
                const bLat = Number(b.latencyMs) || 999999;
                return aLat - bLat;
            });
        return sorted.slice(0, topN);
    }

    const requestId = Number((Date.now() >>> 0));
    const speculativeEnabled = (options.speculativeEnabled !== false);
    const payload = {
        requestId,
        source: _asString(telemetry && telemetry.source, 'hive-telemetry'),
        ts: new Date().toISOString(),
    };

    let unsupported = false;
    let ack = null;
    let ackConfirm = null;
    let ackConfirmUnsupported = false;
    let specResult = null;
    let winnerRequestId = requestId;

    try {
        // Primary offload request
        const primaryPromise = sendToExtension(id, 'hive.offload', payload, {
            timeoutMs: Number.isInteger(options.timeoutMs) && options.timeoutMs > 0
                ? Math.min(options.timeoutMs, IPC_TIMEOUT_MS)
                : 800,
            retryCount: 0,
            source: options.source || 'manager.hive_offload',
        });

        // If speculative inference enabled, race against a secondary peer
        let racePromise = null;
        if (speculativeEnabled) {
            const topPeers = _getTopNPeersByLatency(2);
            const secondaryPeer = topPeers.length > 1 ? topPeers[1] : null;
            
            if (secondaryPeer) {
                const specRequestId = Number((Date.now() >>> 0)) + 1;
                const specPayload = {
                    ...payload,
                    requestId: specRequestId,
                    source: 'hive-speculative',
                };
                
                // Try to send speculative request to secondary peer
                racePromise = (async () => {
                    try {
                        const result = await sendToExtension(secondaryPeer.id, 'hive.speculative', specPayload, {
                            timeoutMs: Number.isInteger(options.timeoutMs) && options.timeoutMs > 0
                                ? Math.min(options.timeoutMs, IPC_TIMEOUT_MS)
                                : 800,
                            retryCount: 0,
                            source: 'manager.hive_speculative',
                        });
                        return { success: true, requestId: specRequestId, peer: secondaryPeer.id, result };
                    } catch (err) {
                        return { success: false, requestId: specRequestId, peer: secondaryPeer.id, error: err };
                    }
                })();
            }
        }

        // Race: wait for first successful request
        if (racePromise) {
            try {
                const raceResult = await Promise.race([
                    primaryPromise.then(res => ({ source: 'primary', ack: res, requestId })),
                    racePromise.then(res => ({ source: 'speculative', ...res })),
                ]);

                if (raceResult.source === 'primary') {
                    ack = raceResult.ack;
                    winnerRequestId = raceResult.requestId;
                } else if (raceResult.source === 'speculative' && raceResult.success) {
                    specResult = raceResult.result;
                    winnerRequestId = raceResult.requestId;
                    
                    // Signal winner outcome to bridge
                    try {
                        await sendToExtension(raceResult.peer, 'hive.speculative.ack', {
                            requestId: raceResult.requestId,
                            outcomeCode: 0,  // SPEC_RACE_WON
                            peerId: syncPeerRegistry.findIndex(p => p && p.id === raceResult.peer),
                            source: 'manager.race-winner',
                            ts: new Date().toISOString(),
                        }, {
                            timeoutMs: 500,
                            retryCount: 0,
                            source: 'manager.spec_race_won',
                        });
                    } catch (_) {}
                    
                    // Try to send LOST to primary peer
                    try {
                        await sendToExtension(id, 'hive.speculative.ack', {
                            requestId,
                            outcomeCode: 1,  // SPEC_RACE_LOST
                            source: 'manager.race-loser',
                            ts: new Date().toISOString(),
                        }, {
                            timeoutMs: 500,
                            retryCount: 0,
                            source: 'manager.spec_race_lost_primary',
                        });
                    } catch (_) {}
                }
            } catch (raceError) {
                // Race timed out or both failed; fall back to primary request
                try {
                    ack = await primaryPromise;
                } catch (primaryError) {
                    ack = null;
                }
            }
        } else {
            // No speculative race available; just use primary
            ack = await primaryPromise;
        }

        const outcomeCode = ack ? 0 : 2;
        try {
            ackConfirm = await sendToExtension(id, 'hive.offload.ack', {
                requestId: winnerRequestId,
                outcomeCode,
                source: _asString(telemetry && telemetry.source, 'hive-telemetry-ack'),
                ts: new Date().toISOString(),
            }, {
                timeoutMs: Number.isInteger(options.timeoutMs) && options.timeoutMs > 0
                    ? Math.min(options.timeoutMs, IPC_TIMEOUT_MS)
                    : 800,
                retryCount: 0,
                source: options.source || 'manager.hive_offload_ack',
            });
        } catch (ackError) {
            if (ackError && typeof ackError.message === 'string' && /unknown_capability|unsupported/i.test(ackError.message)) {
                ackConfirmUnsupported = true;
                logEvent('IPC_FAIL', {
                    extension: record.name,
                    type: 'hive.offload.ack',
                    error: 'signal_unsupported:hive.offload.ack',
                });
            } else {
                throw ackError;
            }
        }
    } catch (error) {
        if (error && typeof error.message === 'string' && /unknown_capability|unsupported/i.test(error.message)) {
            unsupported = true;
            logEvent('IPC_FAIL', {
                extension: record.name,
                type: 'hive.offload',
                error: 'signal_unsupported:hive.offload',
            });
        } else {
            throw error;
        }
    }

    return {
        active: !unsupported && !!ack,
        ack,
        ackConfirm,
        ackConfirmUnsupported,
        requestId,
        winnerRequestId,
        specResult,
        speculativeEnabled,
        unsupported,
    };
}

module.exports = { signalHiveOffloadRace };
