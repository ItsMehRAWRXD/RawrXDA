// ============================================================================
// Context Window Token Usage Widget — Universal Drop-in Component
// ============================================================================
// Usage: Include this script in any HTML IDE to get a context window display.
//   <script src="context-window-widget.js"></script>
//   Then call: ContextWindowWidget.init({ maxTokens: 128000 });
//
// Or use inline: copy the ContextWindowWidget object into your <script> block.
// ============================================================================

const ContextWindowWidget = (() => {
    // ─── State ───
    let state = {
        maxTokens: 128000,
        segments: {
            system:      { label: 'System Instructions', tokens: 0, color: '#569cd6' },
            tools:       { label: 'Tool Definitions',    tokens: 0, color: '#c586c0' },
            userContext: { label: 'User Context',        tokens: 0, color: '#4ec9b0' },
            messages:    { label: 'Messages',            tokens: 0, color: '#dcdcaa' },
            toolResults: { label: 'Tool Results',        tokens: 0, color: '#ce9178' },
        },
        warningThreshold: 0.75,
        dangerThreshold:  0.90,
    };

    let widgetEl = null;
    let onUpdate = null;

    // ─── Approximate token counter (GPT-style ~4 chars/token) ───
    function estimateTokens(text) {
        if (!text) return 0;
        return Math.ceil(text.length / 4);
    }

    // ─── Format token count (e.g., 97500 → "97.5K") ───
    function formatTokens(n) {
        if (n >= 1000000) return (n / 1000000).toFixed(1) + 'M';
        if (n >= 1000)    return (n / 1000).toFixed(1) + 'K';
        return String(n);
    }

    // ─── Inject widget CSS ───
    function injectStyles() {
        if (document.getElementById('ctx-window-styles')) return;
        const style = document.createElement('style');
        style.id = 'ctx-window-styles';
        style.textContent = `
            .ctx-window-widget {
                position: fixed;
                bottom: 28px;
                right: 12px;
                width: 260px;
                background: rgba(30, 30, 30, 0.97);
                border: 1px solid #3e3e42;
                border-radius: 8px;
                padding: 12px 14px;
                font-family: 'Segoe UI', 'Consolas', monospace;
                font-size: 12px;
                color: #cccccc;
                z-index: 99999;
                backdrop-filter: blur(12px);
                box-shadow: 0 4px 24px rgba(0,0,0,0.5);
                transition: opacity 0.2s, transform 0.2s;
                user-select: none;
            }
            .ctx-window-widget.collapsed {
                width: auto;
                padding: 6px 12px;
                cursor: pointer;
            }
            .ctx-window-widget.collapsed .ctx-body { display: none; }
            .ctx-window-widget.collapsed .ctx-collapse-btn { display: none; }
            .ctx-window-header {
                display: flex;
                align-items: center;
                justify-content: space-between;
                margin-bottom: 8px;
                font-weight: 600;
                font-size: 11px;
                color: #e0e0e0;
                letter-spacing: 0.5px;
                text-transform: uppercase;
            }
            .ctx-collapse-btn {
                cursor: pointer;
                opacity: 0.6;
                font-size: 14px;
                line-height: 1;
                padding: 2px 4px;
                border-radius: 3px;
            }
            .ctx-collapse-btn:hover { opacity: 1; background: rgba(255,255,255,0.1); }
            .ctx-token-summary {
                font-size: 20px;
                font-weight: 700;
                color: #d4d4d4;
                margin-bottom: 2px;
            }
            .ctx-token-summary .ctx-max { color: #858585; font-size: 14px; font-weight: 400; }
            .ctx-pct-line {
                display: flex;
                align-items: center;
                gap: 8px;
                margin-bottom: 10px;
            }
            .ctx-pct-badge {
                font-size: 13px;
                font-weight: 700;
                padding: 1px 6px;
                border-radius: 4px;
            }
            .ctx-pct-badge.ok     { color: #4ec9b0; background: rgba(78,201,176,0.12); }
            .ctx-pct-badge.warn   { color: #ffd700; background: rgba(255,215,0,0.12); }
            .ctx-pct-badge.danger { color: #f44747; background: rgba(244,71,71,0.15); }
            .ctx-progress-track {
                width: 100%;
                height: 6px;
                background: #2d2d30;
                border-radius: 3px;
                overflow: hidden;
                margin-bottom: 12px;
            }
            .ctx-progress-fill {
                height: 100%;
                border-radius: 3px;
                transition: width 0.4s ease, background 0.3s;
            }
            .ctx-segment-row {
                display: flex;
                align-items: center;
                justify-content: space-between;
                padding: 3px 0;
                font-size: 11px;
            }
            .ctx-segment-label {
                display: flex;
                align-items: center;
                gap: 6px;
                color: #b0b0b0;
            }
            .ctx-segment-dot {
                width: 8px;
                height: 8px;
                border-radius: 50%;
                flex-shrink: 0;
            }
            .ctx-segment-pct {
                font-weight: 600;
                color: #d4d4d4;
                min-width: 38px;
                text-align: right;
            }
            .ctx-segment-bar {
                width: 100%;
                height: 4px;
                background: #2d2d30;
                border-radius: 2px;
                margin: 8px 0 4px;
                display: flex;
                overflow: hidden;
            }
            .ctx-segment-bar-part {
                height: 100%;
                transition: width 0.4s ease;
            }
            .ctx-warning-text {
                font-size: 10px;
                color: #858585;
                margin-top: 8px;
                font-style: italic;
                line-height: 1.3;
            }
            .ctx-warning-text.active { color: #ffd700; }
            .ctx-status-inline {
                display: inline-flex;
                align-items: center;
                gap: 6px;
                font-size: 11px;
                color: #b0b0b0;
                cursor: pointer;
                padding: 1px 8px;
                border-radius: 3px;
            }
            .ctx-status-inline:hover { background: rgba(255,255,255,0.06); }
            .ctx-status-inline .ctx-mini-bar {
                width: 50px;
                height: 4px;
                background: #2d2d30;
                border-radius: 2px;
                overflow: hidden;
                display: inline-block;
                vertical-align: middle;
            }
            .ctx-status-inline .ctx-mini-fill {
                height: 100%;
                border-radius: 2px;
                transition: width 0.3s;
            }
        `;
        document.head.appendChild(style);
    }

    // ─── Build the widget DOM ───
    function createWidget() {
        injectStyles();

        const el = document.createElement('div');
        el.className = 'ctx-window-widget';
        el.id = 'ctx-window-widget';
        el.innerHTML = buildHTML();
        document.body.appendChild(el);

        // Collapse toggle
        el.querySelector('.ctx-collapse-btn').addEventListener('click', (e) => {
            e.stopPropagation();
            el.classList.add('collapsed');
        });
        el.addEventListener('click', () => {
            if (el.classList.contains('collapsed')) {
                el.classList.remove('collapsed');
            }
        });

        widgetEl = el;
        return el;
    }

    // ─── Generate the inner HTML ───
    function buildHTML() {
        const total = getTotalTokens();
        const pct = state.maxTokens > 0 ? (total / state.maxTokens) : 0;
        const pctNum = Math.round(pct * 100);
        const pctClass = pct >= state.dangerThreshold ? 'danger' : pct >= state.warningThreshold ? 'warn' : 'ok';
        const barColor = pct >= state.dangerThreshold ? '#f44747' : pct >= state.warningThreshold ? '#ffd700' : '#007acc';

        let segmentRows = '';
        let barParts = '';
        for (const [key, seg] of Object.entries(state.segments)) {
            const segPct = state.maxTokens > 0 ? ((seg.tokens / state.maxTokens) * 100) : 0;
            segmentRows += `
                <div class="ctx-segment-row">
                    <span class="ctx-segment-label">
                        <span class="ctx-segment-dot" style="background:${seg.color}"></span>
                        ${seg.label}
                    </span>
                    <span class="ctx-segment-pct">${segPct.toFixed(1)}%</span>
                </div>`;
            barParts += `<div class="ctx-segment-bar-part" style="width:${segPct}%;background:${seg.color}"></div>`;
        }

        const freeTokens = Math.max(0, state.maxTokens - total);
        const freePct = state.maxTokens > 0 ? ((freeTokens / state.maxTokens) * 100) : 100;
        barParts += `<div class="ctx-segment-bar-part" style="width:${freePct}%;background:#2d2d30"></div>`;

        const warningActive = pct >= state.warningThreshold;

        return `
            <div class="ctx-window-header">
                <span>Context Window</span>
                <span class="ctx-collapse-btn" title="Collapse">&#x2715;</span>
            </div>
            <div class="ctx-body">
                <div class="ctx-token-summary">
                    ${formatTokens(total)} <span class="ctx-max">/ ${formatTokens(state.maxTokens)} tokens</span>
                </div>
                <div class="ctx-pct-line">
                    <span class="ctx-pct-badge ${pctClass}">&#x2022; ${pctNum}%</span>
                </div>
                <div class="ctx-segment-bar">${barParts}</div>
                ${segmentRows}
                <div class="ctx-warning-text ${warningActive ? 'active' : ''}">
                    ${warningActive ? '⚠ Quality may decline as limit nears.' : 'Context usage nominal.'}
                </div>
            </div>`;
    }

    // ─── Total tokens ───
    function getTotalTokens() {
        return Object.values(state.segments).reduce((s, seg) => s + seg.tokens, 0);
    }

    // ─── Create the inline status-bar span ───
    function createStatusBarSpan() {
        const total = getTotalTokens();
        const pct = state.maxTokens > 0 ? (total / state.maxTokens) : 0;
        const pctNum = Math.round(pct * 100);
        const barColor = pct >= state.dangerThreshold ? '#f44747' : pct >= state.warningThreshold ? '#ffd700' : '#007acc';
        return `<span id="context-window" class="ctx-status-inline" onclick="ContextWindowWidget.toggle()" title="Context Window Token Usage">
            Ctx: ${formatTokens(total)}/${formatTokens(state.maxTokens)}
            <span class="ctx-mini-bar"><span class="ctx-mini-fill" style="width:${pctNum}%;background:${barColor}"></span></span>
            ${pctNum}%
        </span>`;
    }

    // ─── Refresh display ───
    function refresh() {
        if (widgetEl) {
            const body = widgetEl.querySelector('.ctx-body');
            const wasCollapsed = widgetEl.classList.contains('collapsed');
            widgetEl.innerHTML = buildHTML();
            if (wasCollapsed) widgetEl.classList.add('collapsed');

            widgetEl.querySelector('.ctx-collapse-btn').addEventListener('click', (e) => {
                e.stopPropagation();
                widgetEl.classList.add('collapsed');
            });
            widgetEl.addEventListener('click', () => {
                if (widgetEl.classList.contains('collapsed')) {
                    widgetEl.classList.remove('collapsed');
                }
            });
        }

        // Update inline status bar span if present
        const inlineEl = document.getElementById('context-window');
        if (inlineEl) {
            const total = getTotalTokens();
            const pct = state.maxTokens > 0 ? (total / state.maxTokens) : 0;
            const pctNum = Math.round(pct * 100);
            const barColor = pct >= state.dangerThreshold ? '#f44747' : pct >= state.warningThreshold ? '#ffd700' : '#007acc';
            inlineEl.innerHTML = `Ctx: ${formatTokens(total)}/${formatTokens(state.maxTokens)}
                <span class="ctx-mini-bar"><span class="ctx-mini-fill" style="width:${pctNum}%;background:${barColor}"></span></span>
                ${pctNum}%`;
        }

        if (onUpdate) onUpdate(getState());
    }

    // ─── Public state getter ───
    function getState() {
        const total = getTotalTokens();
        const pct = state.maxTokens > 0 ? (total / state.maxTokens) : 0;
        return {
            totalTokens: total,
            maxTokens: state.maxTokens,
            percentage: Math.round(pct * 100),
            segments: { ...state.segments },
            warning: pct >= state.warningThreshold,
            danger: pct >= state.dangerThreshold,
        };
    }

    // ═══════════════════════════════════════════════════════════════
    // Public API
    // ═══════════════════════════════════════════════════════════════
    return {
        /**
         * Initialize the context window widget.
         * @param {Object} opts
         * @param {number}  opts.maxTokens    - Max context window (default 128000)
         * @param {string}  opts.position     - 'float' (default), 'statusbar', 'both'
         * @param {string}  opts.statusBarId  - ID of status-info div to append inline span
         * @param {Function} opts.onUpdate    - Callback on each update
         */
        init(opts = {}) {
            if (opts.maxTokens)  state.maxTokens  = opts.maxTokens;
            if (opts.warningThreshold) state.warningThreshold = opts.warningThreshold;
            if (opts.dangerThreshold)  state.dangerThreshold  = opts.dangerThreshold;
            if (opts.onUpdate) onUpdate = opts.onUpdate;

            const position = opts.position || 'both';

            // Always inject CSS
            injectStyles();

            // Status bar inline
            if (position === 'statusbar' || position === 'both') {
                const barId = opts.statusBarId || 'status-info';
                const barEl = document.getElementById(barId);
                if (barEl) {
                    barEl.insertAdjacentHTML('beforeend', ' | ' + createStatusBarSpan());
                }
            }

            // Floating widget
            if (position === 'float' || position === 'both') {
                createWidget();
            }

            // Seed with empty initial state
            this.update({});

            console.log('[ContextWindow] Initialized: ' + formatTokens(state.maxTokens) + ' max');
        },

        /**
         * Update token counts for one or more segments.
         * @param {Object} segments - { system: 4800, tools: 11520, ... }
         */
        update(segments) {
            for (const [key, value] of Object.entries(segments)) {
                if (state.segments[key]) {
                    state.segments[key].tokens = typeof value === 'number' ? value : estimateTokens(value);
                }
            }
            refresh();
        },

        /**
         * Set token count from raw text for each segment.
         * @param {Object} texts - { system: "...", tools: "...", messages: "..." }
         */
        updateFromText(texts) {
            for (const [key, text] of Object.entries(texts)) {
                if (state.segments[key]) {
                    state.segments[key].tokens = estimateTokens(text);
                }
            }
            refresh();
        },

        /** Set the maximum token budget */
        setMaxTokens(n) {
            state.maxTokens = n;
            refresh();
        },

        /** Toggle floating widget visibility */
        toggle() {
            if (!widgetEl) { createWidget(); return; }
            widgetEl.classList.toggle('collapsed');
        },

        /** Show the floating widget */
        show() {
            if (!widgetEl) createWidget();
            widgetEl.classList.remove('collapsed');
            widgetEl.style.display = '';
        },

        /** Hide the floating widget */
        hide() {
            if (widgetEl) widgetEl.style.display = 'none';
        },

        /** Get current state */
        getState,

        /** Estimate tokens from text */
        estimateTokens,

        /** Format token count */
        formatTokens,

        /** Destroy the widget */
        destroy() {
            if (widgetEl) { widgetEl.remove(); widgetEl = null; }
            const s = document.getElementById('ctx-window-styles');
            if (s) s.remove();
        },

        /**
         * Auto-track: hook into AI chat to auto-count tokens.
         * Call after init(). Watches message sends and responses.
         * @param {Object} opts - { systemPrompt, toolDefs, chatContainerId }
         */
        autoTrack(opts = {}) {
            // Set initial system + tools if provided
            if (opts.systemPrompt) {
                state.segments.system.tokens = estimateTokens(opts.systemPrompt);
            }
            if (opts.toolDefs) {
                state.segments.tools.tokens = estimateTokens(opts.toolDefs);
            }

            // Observe chat container for new messages
            const chatId = opts.chatContainerId || 'ai-chat';
            const chatEl = document.getElementById(chatId);
            if (chatEl) {
                const observer = new MutationObserver(() => {
                    const messages = chatEl.querySelectorAll('[class*="message"], [class*="chat"]');
                    let userTokens = 0, responseTokens = 0;
                    messages.forEach(msg => {
                        const text = msg.textContent || '';
                        const isUser = msg.classList.contains('user') || 
                                       msg.getAttribute('data-role') === 'user' ||
                                       msg.style.textAlign === 'right';
                        if (isUser) userTokens += estimateTokens(text);
                        else responseTokens += estimateTokens(text);
                    });
                    state.segments.messages.tokens = userTokens;
                    state.segments.toolResults.tokens = responseTokens;
                    refresh();
                });
                observer.observe(chatEl, { childList: true, subtree: true, characterData: true });
            }
            refresh();
        }
    };
})();
