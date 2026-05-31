'use strict';

const LOOP_STATES = Object.freeze({
    PLANNING: 'PLANNING',
    EXECUTING: 'EXECUTING',
    VERIFYING: 'VERIFYING',
    COMPLETE: 'COMPLETE',
});

function createLoopController(options = {}) {
    const maxDepth = Number.isInteger(options.maxDepth) ? Math.max(1, options.maxDepth) : 12;
    const id = typeof options.id === 'string' && options.id ? options.id : `loop-${Date.now()}`;
    const checkpoints = [];
    let state = LOOP_STATES.PLANNING;
    let depth = 0;
    let stepToken = null;

    function _assertNotComplete() {
        if (state === LOOP_STATES.COMPLETE) {
            throw new Error('loop_already_complete');
        }
    }

    function beginStep(token) {
        _assertNotComplete();
        if (stepToken !== null) {
            throw new Error('one_loop_policy_violation:step_already_active');
        }
        if (depth >= maxDepth) {
            throw new Error('loop_depth_exceeded:' + maxDepth);
        }
        depth += 1;
        stepToken = token || `${id}:step:${depth}`;
        state = LOOP_STATES.PLANNING;
        return stepToken;
    }

    function transition(nextState) {
        _assertNotComplete();
        const allowed = {
            [LOOP_STATES.PLANNING]: [LOOP_STATES.EXECUTING],
            [LOOP_STATES.EXECUTING]: [LOOP_STATES.VERIFYING],
            [LOOP_STATES.VERIFYING]: [LOOP_STATES.COMPLETE, LOOP_STATES.PLANNING],
            [LOOP_STATES.COMPLETE]: [],
        };
        const next = String(nextState || '').toUpperCase();
        if (!allowed[state] || !allowed[state].includes(next)) {
            throw new Error(`invalid_loop_transition:${state}->${next}`);
        }
        state = next;
        if (state === LOOP_STATES.COMPLETE || state === LOOP_STATES.PLANNING) {
            stepToken = null;
        }
        return state;
    }

    function checkpoint(kind, detail) {
        checkpoints.push({
            ts: new Date().toISOString(),
            state,
            kind: String(kind || 'checkpoint'),
            detail: detail || {},
        });
    }

    function shouldContinue() {
        return state !== LOOP_STATES.COMPLETE && depth < maxDepth;
    }

    function snapshot() {
        return {
            id,
            state,
            depth,
            maxDepth,
            stepToken,
            checkpoints: checkpoints.slice(),
        };
    }

    return {
        id,
        LOOP_STATES,
        beginStep,
        transition,
        checkpoint,
        shouldContinue,
        snapshot,
    };
}

module.exports = {
    LOOP_STATES,
    createLoopController,
};
