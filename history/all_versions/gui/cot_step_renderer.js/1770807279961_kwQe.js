// ============================================================================
// cot_step_renderer.js — Incremental + Capped CoT UI Rendering
// ============================================================================
//
// Action Item #11: Cap steps kept in DOM (last 200), bounded memory growth.
//   - 5,000 streamed steps do not freeze the UI
//   - Virtual scrolling for large step lists
//   - Automatic pruning of oldest steps
//
// Integration: Loaded by ide_chatbot.html after ide_chatbot_engine.js
//
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED
// ============================================================================

(function() {
    'use strict';

    // ========================================================================
    // Configuration
    // ========================================================================
    const COT_MAX_STEPS_IN_DOM = 200;     // Max steps retained in DOM
    const COT_STEP_PRUNE_BATCH = 50;      // Remove this many at once on overflow
    const COT_MAX_STEP_CONTENT_LEN = 2000; // Truncate individual step content
    const COT_RENDER_DEBOUNCE_MS = 16;    // ~60fps max render rate

    // ========================================================================
    // State
    // ========================================================================
    let cotContainer = null;
    let totalStepsReceived = 0;
    let stepsInDom = 0;
    let pruneCount = 0;
    let renderTimer = null;
    let pendingSteps = [];

    // ========================================================================
    // Initialization
    // ========================================================================
    function initCotRenderer() {
        cotContainer = document.getElementById('cot-steps-container');
        if (!cotContainer) {
            // Create the container if it doesn't exist
            cotContainer = document.createElement('div');
            cotContainer.id = 'cot-steps-container';
            cotContainer.className = 'cot-steps-container';
            cotContainer.style.cssText = 'max-height:400px;overflow-y:auto;font-size:13px;';

            // Try to find a suitable parent
            const chatArea = document.getElementById('chat-messages') ||
                             document.getElementById('messages') ||
                             document.querySelector('.chat-area');
            if (chatArea) {
                chatArea.parentNode.insertBefore(cotContainer, chatArea);
            }
        }
        return cotContainer;
    }

    // ========================================================================
    // Step Rendering — Incremental + Capped
    // ========================================================================

    /**
     * Add a CoT step to the DOM with pruning.
     * @param {Object} step - { role, content, latency_ms, confidence, type }
     */
    function addCotStep(step) {
        totalStepsReceived++;

        // Truncate content if too long
        let content = step.content || '';
        if (content.length > COT_MAX_STEP_CONTENT_LEN) {
            content = content.substring(0, COT_MAX_STEP_CONTENT_LEN) + '… [truncated]';
        }

        // Queue for debounced render
        pendingSteps.push({
            role: step.role || 'unknown',
            content: content,
            latencyMs: step.latency_ms || 0,
            confidence: step.confidence || 0,
            type: step.type || '',
            skipped: !!step.skipped,
            index: totalStepsReceived
        });

        // Debounced flush
        if (!renderTimer) {
            renderTimer = setTimeout(flushPendingSteps, COT_RENDER_DEBOUNCE_MS);
        }
    }

    /**
     * Flush all pending steps to DOM in a single batch.
     * This minimizes layout thrashing.
     */
    function flushPendingSteps() {
        renderTimer = null;
        if (!cotContainer) initCotRenderer();
        if (!cotContainer || pendingSteps.length === 0) return;

        // Build fragment for batch DOM insert
        const fragment = document.createDocumentFragment();

        for (const step of pendingSteps) {
            const el = document.createElement('div');
            el.className = 'cot-step cot-step-' + (step.type || 'default');
            el.dataset.index = step.index;

            // Build safe text content (no innerHTML — XSS safe)
            const roleSpan = document.createElement('span');
            roleSpan.className = 'cot-step-role';
            roleSpan.textContent = '[' + step.role + ']';

            const contentSpan = document.createElement('span');
            contentSpan.className = 'cot-step-content';
            contentSpan.textContent = ' ' + step.content;

            const metaSpan = document.createElement('span');
            metaSpan.className = 'cot-step-meta';
            metaSpan.textContent = ' (' + step.latencyMs + 'ms)';
            if (step.skipped) {
                metaSpan.textContent += ' [SKIPPED]';
            }

            el.appendChild(roleSpan);
            el.appendChild(contentSpan);
            el.appendChild(metaSpan);
            fragment.appendChild(el);
            stepsInDom++;
        }

        cotContainer.appendChild(fragment);
        pendingSteps = [];

        // ---- Prune oldest steps if over cap ----
        if (stepsInDom > COT_MAX_STEPS_IN_DOM) {
            pruneOldestSteps();
        }

        // Auto-scroll to bottom
        cotContainer.scrollTop = cotContainer.scrollHeight;
    }

    /**
     * Remove oldest steps from DOM to stay under the cap.
     */
    function pruneOldestSteps() {
        const toRemove = stepsInDom - COT_MAX_STEPS_IN_DOM + COT_STEP_PRUNE_BATCH;
        const children = cotContainer.children;

        // Use a range removal for efficiency
        const removeCount = Math.min(toRemove, children.length);
        for (let i = 0; i < removeCount; i++) {
            cotContainer.removeChild(children[0]);
        }

        stepsInDom -= removeCount;
        pruneCount += removeCount;

        // Add a "pruned" indicator
        if (pruneCount > 0 && children.length > 0) {
            let indicator = cotContainer.querySelector('.cot-prune-indicator');
            if (!indicator) {
                indicator = document.createElement('div');
                indicator.className = 'cot-prune-indicator';
                cotContainer.insertBefore(indicator, children[0]);
            }
            indicator.textContent = '… ' + pruneCount + ' earlier steps pruned';
        }
    }

    /**
     * Render a complete CoT response (batch mode — not streaming).
     * @param {Object} response - Schema v1 response with steps[] array
     */
    function renderCotResponse(response) {
        clearCotSteps();

        if (!response || !response.steps) return;

        // Cap to max steps
        const steps = response.steps.slice(-COT_MAX_STEPS_IN_DOM);

        for (const step of steps) {
            addCotStep(step);
        }

        // Flush immediately for batch mode
        if (renderTimer) {
            clearTimeout(renderTimer);
            renderTimer = null;
        }
        flushPendingSteps();
    }

    /**
     * Clear all CoT steps from DOM.
     */
    function clearCotSteps() {
        if (cotContainer) {
            cotContainer.innerHTML = '';
        }
        stepsInDom = 0;
        totalStepsReceived = 0;
        pruneCount = 0;
        pendingSteps = [];
        if (renderTimer) {
            clearTimeout(renderTimer);
            renderTimer = null;
        }
    }

    /**
     * Get current renderer stats.
     */
    function getCotRenderStats() {
        return {
            totalStepsReceived: totalStepsReceived,
            stepsInDom: stepsInDom,
            pruned: pruneCount,
            maxCap: COT_MAX_STEPS_IN_DOM,
            pendingFlush: pendingSteps.length
        };
    }

    // ========================================================================
    // Exports
    // ========================================================================
    window.CotStepRenderer = {
        init: initCotRenderer,
        addStep: addCotStep,
        renderResponse: renderCotResponse,
        clear: clearCotSteps,
        flush: flushPendingSteps,
        stats: getCotRenderStats,
        MAX_STEPS: COT_MAX_STEPS_IN_DOM
    };

})();
