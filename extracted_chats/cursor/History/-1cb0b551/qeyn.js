(function() {
'use strict';

const FONT_OPTIONS = [
    "'JetBrains Mono', 'Fira Code', 'Consolas', 'Courier New', monospace",
    "'Fira Code', monospace",
    "'Cascadia Code', 'Fira Code', monospace",
    "'IBM Plex Mono', monospace",
    "'Menlo', 'Monaco', 'Consolas', 'Liberation Mono', 'Courier New', monospace",
    "'Segoe UI', Tahoma, Geneva, Verdana, sans-serif",
    "'Inter', 'Segoe UI', sans-serif",
    "'Roboto', sans-serif"
];

const STYLES = `
.settings-panel-root {
    display: flex;
    width: 100%;
    min-height: calc(100vh - 180px);
    color: var(--cursor-text);
}

.settings-panel {
    display: grid;
    grid-template-columns: 260px minmax(0, 1fr);
    gap: 24px;
    width: 100%;
}

.settings-panel__nav {
    background: var(--cursor-bg-secondary);
    border: 1px solid var(--cursor-border);
    border-radius: 10px;
    padding: 18px;
    position: sticky;
    top: 20px;
    height: fit-content;
    align-self: start;
}

.settings-panel__nav h3 {
    font-size: 13px;
    text-transform: uppercase;
    letter-spacing: 0.08em;
    color: var(--cursor-text-secondary);
    margin-bottom: 16px;
}

.settings-panel__nav ul {
    list-style: none;
    display: flex;
    flex-direction: column;
    gap: 8px;
}

.settings-panel__nav a {
    display: flex;
    align-items: center;
    gap: 8px;
    padding: 8px 10px;
    border-radius: 6px;
    color: var(--cursor-text);
    text-decoration: none;
    transition: background 0.2s ease;
}

.settings-panel__nav a:hover {
    background: var(--cursor-bg-hover);
}

.settings-panel__content {
    display: flex;
    flex-direction: column;
    gap: 28px;
    padding-right: 12px;
}

.settings-section {
    background: var(--cursor-bg-secondary);
    border: 1px solid var(--cursor-border);
    border-radius: 12px;
    padding: 24px;
    display: flex;
    flex-direction: column;
    gap: 18px;
}

.settings-section__header {
    display: flex;
    justify-content: space-between;
    align-items: center;
    gap: 16px;
}

.settings-section__header h2 {
    font-size: 18px;
    display: flex;
    align-items: center;
    gap: 10px;
    color: var(--cursor-text);
}

.settings-controls {
    display: grid;
    grid-template-columns: repeat(auto-fit, minmax(220px, 1fr));
    gap: 16px;
}

.settings-control {
    display: flex;
    flex-direction: column;
    gap: 6px;
    background: var(--cursor-bg);
    border: 1px solid var(--cursor-border);
    border-radius: 8px;
    padding: 12px;
}

.settings-control label {
    font-size: 12px;
    font-weight: 600;
    text-transform: uppercase;
    letter-spacing: 0.05em;
    color: var(--cursor-text-secondary);
}

.settings-control input,
.settings-control select,
.settings-control textarea {
    background: var(--cursor-input-bg);
    border: 1px solid var(--cursor-input-border);
    border-radius: 6px;
    padding: 8px 10px;
    color: var(--cursor-text);
    font-size: 13px;
    font-family: var(--app-font-family);
}

.settings-control input[type="range"] {
    padding: 0;
}

.settings-control .settings-value {
    font-size: 12px;
    color: var(--cursor-text-secondary);
}

.settings-grid--hotkeys {
    display: flex;
    flex-direction: column;
    gap: 16px;
}

.settings-hotkey-group {
    border: 1px solid var(--cursor-border);
    border-radius: 8px;
    overflow: hidden;
    background: var(--cursor-bg);
}

.settings-hotkey-group h3 {
    margin: 0;
    padding: 12px 16px;
    font-size: 14px;
    font-weight: 600;
    background: var(--cursor-bg-secondary);
    border-bottom: 1px solid var(--cursor-border);
}

.settings-hotkey-table {
    width: 100%;
    border-collapse: collapse;
}

.settings-hotkey-table th,
.settings-hotkey-table td {
    padding: 10px 14px;
    border-bottom: 1px solid var(--cursor-border);
    font-size: 13px;
    text-align: left;
}

.settings-hotkey-table tr:last-child td {
    border-bottom: none;
}

.settings-hotkey-table input {
    width: 180px;
}

.settings-actions {
    display: flex;
    gap: 10px;
    flex-wrap: wrap;
}

.settings-btn {
    border: 1px solid var(--cursor-border);
    background: var(--cursor-bg);
    color: var(--cursor-text);
    padding: 8px 14px;
    border-radius: 6px;
    cursor: pointer;
    font-size: 12px;
    transition: background 0.2s ease;
}

.settings-btn:hover {
    background: var(--cursor-bg-hover);
}

.settings-btn--primary {
    background: var(--cursor-accent);
    color: #0d0d0d;
    border-color: transparent;
}

.settings-btn--primary:hover {
    background: var(--cursor-accent-hover);
}

.settings-status {
    margin-top: 8px;
    font-size: 12px;
    color: var(--cursor-text-secondary);
    min-height: 18px;
}

.settings-panel-loading {
    display: flex;
    align-items: center;
    justify-content: center;
    min-height: 300px;
    color: var(--cursor-text-secondary);
    font-size: 14px;
}

.settings-color-input {
    display: flex;
    gap: 8px;
    align-items: center;
}

.settings-color-input input[type="text"] {
    flex: 1;
}

@media (max-width: 1200px) {
    .settings-panel {
        grid-template-columns: 1fr;
    }

    .settings-panel__nav {
        position: static;
        top: auto;
    }
}
`;

class SettingsPanel {
    constructor() {
        this.container = null;
        this.tabId = null;
        this.tabSystem = null;
        this.currentSettings = null;
        this.defaults = null;
        this.pendingPatch = null;
        this.patchTimer = null;
        this.statusTimer = null;
        this.statusElement = null;
        this.initialized = false;
        this.settingsApi = window.electron?.settings || null;

        this.injectStyles();
        this.bootstrap();
    }

    injectStyles() {
        if (document.getElementById('settings-panel-styles')) return;
        const style = document.createElement('style');
        style.id = 'settings-panel-styles';
        style.textContent = STYLES;
        document.head.appendChild(style);
    }

    bootstrap() {
        if (!this.settingsApi) {
            console.warn('[SettingsPanel] ⚠️ Settings API unavailable');
            return;
        }

        this.settingsApi.onBootstrap((snapshot) => {
            this.currentSettings = clone(snapshot);
            if (!this.defaults) this.defaults = clone(snapshot);
            this.renderIfMounted();
        });

        this.settingsApi.onDidChange((event) => {
            this.applyUpdateEvent(event);
        });

        this.settingsApi.getAll().then((res) => {
            if (res?.success) {
                this.currentSettings = clone(res.settings);
                this.renderIfMounted();
            }
        });

        this.settingsApi.getDefaults().then((res) => {
            if (res?.success) {
                this.defaults = clone(res.settings);
            }
        });
    }

    async ensureSettingsLoaded() {
        if (this.currentSettings) return;
        if (!this.settingsApi) return;
        const res = await this.settingsApi.getAll();
        if (res?.success) {
            this.currentSettings = clone(res.settings);
        }
    }

    renderIfMounted() {
        if (this.container) {
            this.render();
        }
    }

    async open(tabSystem) {
        this.tabSystem = tabSystem;
        await this.ensureSettingsLoaded();

        if (this.tabId && this.tabSystem && this.tabSystem.tabs && this.tabSystem.tabs.has && this.tabSystem.tabs.has(this.tabId)) {
            this.tabSystem.switchToTab(this.tabId);
            this.renderIfMounted();
            return;
        }

        const content = `
            <div class="settings-panel-root">
                <div class="settings-panel-loading">Loading settings...</div>
            </div>
        `;

        this.tabId = this.tabSystem.createTab('Settings', '⚙️', content, 'settings');
        this.container = document.querySelector(`#content-${this.tabId} .settings-panel-root`);
        this.render();
    }

    render() {
        if (!this.container) return;
        if (!this.currentSettings) {
            this.container.innerHTML = '<div class="settings-panel-loading">Loading settings...</div>';
            return;
        }

        const appearance = this.currentSettings.appearance || {};
        const transparency = appearance.transparency || {};
        const layout = this.currentSettings.layout || {};
        const hotkeys = this.currentSettings.hotkeys || {};

        this.container.innerHTML = `
            <div class="settings-panel">
                <aside class="settings-panel__nav">
                    <h3>Customize</h3>
                    <ul>
                        <li><a href="#settings-appearance">🎨 Appearance</a></li>
                        <li><a href="#settings-transparency">🪟 Transparency</a></li>
                        <li><a href="#settings-layout">🧩 Layout</a></li>
                        <li><a href="#settings-hotkeys">⌨️ Hotkeys</a></li>
                    </ul>
                </aside>
                <div class="settings-panel__content">
                    <section class="settings-section" id="settings-appearance">
                        <div class="settings-section__header">
                            <h2>🎨 Appearance</h2>
                            <div class="settings-actions">
                                <button class="settings-btn" data-action="reset-appearance">Reset Appearance</button>
                            </div>
                        </div>
                        <div class="settings-controls">
                            <div class="settings-control">
                                <label for="appearance-font-family">Font Family</label>
                                <input type="text" id="appearance-font-family" list="settings-font-options" value="${escapeHtml(appearance.fontFamily || '')}" data-setting="appearance.fontFamily" placeholder="Enter CSS font-family value">
                                <datalist id="settings-font-options">
                                    ${FONT_OPTIONS.map(option => `<option value="${option}"></option>`).join('')}
                                </datalist>
                                <span class="settings-value">Applied globally to UI</span>
                            </div>
                            <div class="settings-control">
                                <label>Font Size</label>
                                <input type="range" min="10" max="28" step="1" value="${appearance.fontSize || 15}" data-setting-range="appearance.fontSize">
                                <input type="number" min="10" max="36" step="1" value="${appearance.fontSize || 15}" data-setting-number="appearance.fontSize">
                                <span class="settings-value">Current: <strong data-display="appearance.fontSize">${appearance.fontSize || 15}px</strong></span>
                            </div>
                            <div class="settings-control">
                                <label>UI Scale</label>
                                <input type="range" min="0.8" max="1.4" step="0.05" value="${appearance.uiScale || 1}" data-setting-range="appearance.uiScale">
                                <span class="settings-value">Current scale: <strong data-display="appearance.uiScale">${(appearance.uiScale || 1).toFixed(2)}x</strong></span>
                            </div>
                            <div class="settings-control">
                                <label>Accent Color</label>
                                <div class="settings-color-input">
                                    <input type="text" value="${escapeHtml((appearance.colors && appearance.colors.accent) || '')}" data-setting="appearance.colors.accent" placeholder="e.g. #00d4ff or rgba(0,212,255,1)">
                                </div>
                                <span class="settings-value">Used for highlights & primary actions</span>
                            </div>
                            <div class="settings-control">
                                <label>Background (Primary)</label>
                                <div class="settings-color-input">
                                    <input type="text" value="${escapeHtml((appearance.colors && appearance.colors.backgroundPrimary) || '')}" data-setting="appearance.colors.backgroundPrimary">
                                </div>
                            </div>
                            <div class="settings-control">
                                <label>Background (Secondary)</label>
                                <div class="settings-color-input">
                                    <input type="text" value="${escapeHtml((appearance.colors && appearance.colors.backgroundSecondary) || '')}" data-setting="appearance.colors.backgroundSecondary">
                                </div>
                            </div>
                            <div class="settings-control">
                                <label>Text Color</label>
                                <div class="settings-color-input">
                                    <input type="text" value="${escapeHtml((appearance.colors && appearance.colors.textPrimary) || '')}" data-setting="appearance.colors.textPrimary">
                                </div>
                            </div>
                        </div>
                    </section>

                    <section class="settings-section" id="settings-transparency">
                        <div class="settings-section__header">
                            <h2>🪟 Transparency & Depth</h2>
                            <div class="settings-actions">
                                <button class="settings-btn" data-action="reset-transparency">Reset Transparency</button>
                            </div>
                        </div>
                        <div class="settings-controls">
                            <div class="settings-control">
                                <label>Enable Transparency</label>
                                <label style="display:flex; align-items:center; gap:8px; font-weight:500;">
                                    <input type="checkbox" ${transparency.enabled ? 'checked' : ''} data-setting-checkbox="appearance.transparency.enabled">
                                    Allow translucent panels for multitasking
                                </label>
                            </div>
                            <div class="settings-control">
                                <label>Window Background Opacity</label>
                                <input type="range" min="0.1" max="1" step="0.05" value="${transparency.window ?? 0.95}" data-setting-range="appearance.transparency.window">
                                <span class="settings-value">Current: <strong data-display="appearance.transparency.window">${formatOpacity(transparency.window ?? 0.95)}</strong></span>
                            </div>
                            <div class="settings-control">
                                <label>Side Panels Opacity</label>
                                <input type="range" min="0.1" max="1" step="0.05" value="${transparency.sidePanels ?? 0.92}" data-setting-range="appearance.transparency.sidePanels">
                                <span class="settings-value">Current: <strong data-display="appearance.transparency.sidePanels">${formatOpacity(transparency.sidePanels ?? 0.92)}</strong></span>
                            </div>
                            <div class="settings-control">
                                <label>Chat & Floating Panels</label>
                                <input type="range" min="0.1" max="1" step="0.05" value="${transparency.chatPanels ?? 0.9}" data-setting-range="appearance.transparency.chatPanels">
                                <span class="settings-value">Current: <strong data-display="appearance.transparency.chatPanels">${formatOpacity(transparency.chatPanels ?? 0.9)}</strong></span>
                            </div>
                        </div>
                    </section>

                    <section class="settings-section" id="settings-layout">
                        <div class="settings-section__header">
                            <h2>🧩 Layout & Panels</h2>
                            <div class="settings-actions">
                                <button class="settings-btn" data-action="reset-layout">Reset Layout</button>
                            </div>
                        </div>
                        <div class="settings-controls">
                            <div class="settings-control">
                                <label>Panel Overlap</label>
                                <label style="display:flex; align-items:center; gap:8px; font-weight:500;">
                                    <input type="checkbox" ${layout.allowOverlap ? 'checked' : ''} data-setting-checkbox="layout.allowOverlap">
                                    Allow floating panels to overlap other UI
                                </label>
                                <span class="settings-value">Disable to keep every panel docked in its own space</span>
                            </div>
                            <div class="settings-control">
                                <label>Snap Panels to Edges</label>
                                <label style="display:flex; align-items:center; gap:8px; font-weight:500;">
                                    <input type="checkbox" ${layout.snapToEdges !== false ? 'checked' : ''} data-setting-checkbox="layout.snapToEdges">
                                    Automatically snap floating widgets to screen edges
                                </label>
                            </div>
                            <div class="settings-control">
                                <label>Panel Spacing</label>
                                <input type="range" min="4" max="32" step="2" value="${layout.panelSpacing ?? 12}" data-setting-range="layout.panelSpacing">
                                <span class="settings-value">Current gap: <strong data-display="layout.panelSpacing">${layout.panelSpacing ?? 12}px</strong></span>
                            </div>
                        </div>
                    </section>

                    <section class="settings-section" id="settings-hotkeys">
                        <div class="settings-section__header">
                            <h2>⌨️ Hotkeys</h2>
                            <div class="settings-actions">
                                <button class="settings-btn" data-action="reset-hotkeys">Reset Hotkeys</button>
                            </div>
                        </div>
                        <div class="settings-grid--hotkeys">
                            ${renderHotkeyGroups(hotkeys)}
                        </div>
                        <p style="font-size:12px; color: var(--cursor-text-secondary);">Tip: Enter shortcuts in the form <code>Ctrl+Shift+P</code>. Use commas to provide alternatives.</p>
                    </section>

                    <div class="settings-status" data-role="settings-status"></div>
                </div>
            </div>
        `;

        this.statusElement = this.container.querySelector('[data-role="settings-status"]');

        this.bindControls();
    }

    bindControls() {
        if (!this.container) return;

        this.container.querySelectorAll('[data-setting]').forEach((input) => {
            input.addEventListener('change', (event) => {
                const path = event.target.getAttribute('data-setting');
                const value = event.target.value;
                this.queueUpdate(path, value);
            });
        });

        this.container.querySelectorAll('[data-setting-range]').forEach((input) => {
            input.addEventListener('input', (event) => {
                const path = event.target.getAttribute('data-setting-range');
                const value = parseFloat(event.target.value);
                this.syncRelatedInputs(path, value);
                this.queueUpdate(path, value);
            });
        });

        this.container.querySelectorAll('[data-setting-number]').forEach((input) => {
            input.addEventListener('change', (event) => {
                const path = event.target.getAttribute('data-setting-number');
                const value = parseFloat(event.target.value);
                if (!Number.isFinite(value)) return;
                this.syncRelatedInputs(path, value, { skipNumber: event.target });
                this.queueUpdate(path, value);
            });
        });

        this.container.querySelectorAll('[data-setting-checkbox]').forEach((input) => {
            input.addEventListener('change', (event) => {
                const path = event.target.getAttribute('data-setting-checkbox');
                const value = event.target.checked;
                this.queueUpdate(path, value);
            });
        });

        this.container.querySelectorAll('button[data-action]').forEach((btn) => {
            btn.addEventListener('click', () => {
                const action = btn.getAttribute('data-action');
                this.handleAction(action);
            });
        });

        this.container.querySelectorAll('input[data-hotkey-action]').forEach((input) => {
            input.addEventListener('change', (event) => {
                const action = event.target.getAttribute('data-hotkey-action');
                const value = event.target.value.trim();
                this.saveHotkey(action, value, event.target);
            });
        });
    }

    syncRelatedInputs(path, value, options = {}) {
        if (!this.container) return;
        const slider = this.container.querySelector(`[data-setting-range="${path}"]`);
        const number = options.skipNumber ? null : this.container.querySelector(`[data-setting-number="${path}"]`);
        if (slider && slider.value !== String(value)) slider.value = value;
        if (number && number.value !== String(value)) number.value = value;
        const display = this.container.querySelector(`[data-display="${path}"]`);
        if (display) {
            if (path.includes('transparency')) {
                display.textContent = formatOpacity(value);
            } else if (path.includes('appearance.fontSize') || path.includes('panelSpacing')) {
                display.textContent = `${value}px`;
            } else if (path.includes('appearance.uiScale')) {
                display.textContent = `${Number(value).toFixed(2)}x`;
            } else {
                display.textContent = value;
            }
        }
    }

    handleAction(action) {
        if (!this.settingsApi) return;
        switch (action) {
            case 'reset-appearance':
                this.settingsApi.reset('appearance').then(this.handleResponse.bind(this));
                break;
            case 'reset-transparency':
                this.settingsApi.reset('appearance.transparency').then(this.handleResponse.bind(this));
                break;
            case 'reset-layout':
                this.settingsApi.reset('layout').then(this.handleResponse.bind(this));
                break;
            case 'reset-hotkeys':
                this.settingsApi.reset('hotkeys').then(this.handleResponse.bind(this));
                break;
            default:
                break;
        }
    }

    saveHotkey(action, combo, input) {
        if (!this.settingsApi) return;
        if (!combo) {
            input.value = getPath(this.currentSettings, `hotkeys.${action}.combo`) || '';
            this.showStatus('Shortcut unchanged.');
            return;
        }
        this.settingsApi.setHotkey(action, combo).then((res) => {
            if (!res?.success) {
                this.showStatus(`Failed to update hotkey: ${res?.error || 'Unknown error'}`, true);
                input.value = getPath(this.currentSettings, `hotkeys.${action}.combo`) || '';
            } else {
                this.showStatus('Hotkey updated');
            }
        }).catch((error) => {
            console.error('[SettingsPanel] Hotkey update failed:', error);
            this.showStatus('Hotkey update failed. Check console.', true);
            input.value = getPath(this.currentSettings, `hotkeys.${action}.combo`) || '';
        });
    }

    queueUpdate(path, value) {
        if (!path) return;
        if (!this.pendingPatch) this.pendingPatch = {};
        setPath(this.pendingPatch, path, value);

        if (this.patchTimer) clearTimeout(this.patchTimer);
        this.patchTimer = setTimeout(() => this.flushPatch(), 200);
    }

    flushPatch() {
        if (!this.pendingPatch || !this.settingsApi) return;
        const patch = this.pendingPatch;
        this.pendingPatch = null;
        this.settingsApi.update(patch).then(this.handleResponse.bind(this)).catch((error) => {
            console.error('[SettingsPanel] Update failed:', error);
            this.showStatus('Failed to apply changes. Check console.', true);
        });
    }

    handleResponse(res) {
        if (!res?.success) {
            this.showStatus(res?.error || 'Operation failed', true);
        } else {
            this.showStatus('Changes saved');
        }
    }

    showStatus(message, isError = false) {
        if (!this.statusElement) return;
        this.statusElement.textContent = message;
        this.statusElement.style.color = isError ? 'var(--cursor-red)' : 'var(--cursor-text-secondary)';
        if (this.statusTimer) clearTimeout(this.statusTimer);
        this.statusTimer = setTimeout(() => {
            if (this.statusElement) this.statusElement.textContent = '';
        }, 3000);
    }

    applyUpdateEvent(event) {
        if (!event) return;
        if (!this.currentSettings) {
            this.ensureSettingsLoaded().then(() => this.renderIfMounted());
            return;
        }

        switch (event.type) {
            case 'set':
                if (event.path) setPath(this.currentSettings, event.path, event.value);
                break;
            case 'update':
                if (event.changes) this.currentSettings = mergeDeep(this.currentSettings, event.changes);
                break;
            case 'reset':
                if (event.section && this.defaults) {
                    const defaultsSection = getPath(this.defaults, event.section);
                    if (defaultsSection !== undefined) {
                        setPath(this.currentSettings, event.section, clone(defaultsSection));
                    }
                } else if (this.defaults) {
                    this.currentSettings = clone(this.defaults);
                }
                break;
            case 'hotkey':
                if (event.action) {
                    const node = getPath(this.currentSettings, `hotkeys.${event.action}`) || {};
                    node.combo = event.combo;
                    setPath(this.currentSettings, `hotkeys.${event.action}`, node);
                }
                break;
            default:
                break;
        }

        this.renderIfMounted();
    }
}

function renderHotkeyGroups(hotkeys) {
    const groups = {};
    Object.entries(hotkeys).forEach(([action, config]) => {
        const category = config.category || 'General';
        if (!groups[category]) groups[category] = [];
        groups[category].push({ action, ...config });
    });

    return Object.entries(groups).map(([category, entries]) => {
        const rows = entries.map((entry) => `
            <tr>
                <td style="width: 35%;">
                    <div style="font-weight:600;">${escapeHtml(entry.description || entry.action)}</div>
                    <div style="font-size:11px; color: var(--cursor-text-secondary);">${escapeHtml(entry.action)}</div>
                </td>
                <td style="width: 30%;">${escapeHtml(entry.details || '')}</td>
                <td style="width: 35%;">
                    <input type="text" value="${escapeHtml(entry.combo || '')}" data-hotkey-action="${entry.action}">
                </td>
            </tr>
        `).join('');
        return `
            <div class="settings-hotkey-group">
                <h3>${escapeHtml(category)}</h3>
                <table class="settings-hotkey-table">
                    <thead>
                        <tr>
                            <th>Command</th>
                            <th>Notes</th>
                            <th>Shortcut</th>
                        </tr>
                    </thead>
                    <tbody>
                        ${rows}
                    </tbody>
                </table>
            </div>
        `;
    }).join('');
}

function escapeHtml(str) {
    if (str == null) return '';
    return String(str)
        .replace(/&/g, '&amp;')
        .replace(/</g, '&lt;')
        .replace(/>/g, '&gt;')
        .replace(/"/g, '&quot;')
        .replace(/'/g, '&#039;');
}

function formatOpacity(value) {
    if (value == null) return '1.00';
    return Number(value).toFixed(2);
}

function clone(value) {
    if (Array.isArray(value)) return value.map(clone);
    if (value && typeof value === 'object') {
        return Object.keys(value).reduce((acc, key) => {
            acc[key] = clone(value[key]);
            return acc;
        }, {});
    }
    return value;
}

function mergeDeep(target, source) {
    if (!source || typeof source !== 'object') {
        return target;
    }
    const output = Array.isArray(target) ? target.slice() : { ...target };
    for (const [key, value] of Object.entries(source)) {
        if (value && typeof value === 'object' && !Array.isArray(value)) {
            output[key] = mergeDeep(output[key] || {}, value);
        } else {
            output[key] = clone(value);
        }
    }
    return output;
}

function setPath(target, pathString, value) {
    const segments = pathString.split('.');
    let current = target;
    for (let i = 0; i < segments.length - 1; i++) {
        const segment = segments[i];
        if (!current[segment] || typeof current[segment] !== 'object') {
            current[segment] = {};
        }
        current = current[segment];
    }
    current[segments[segments.length - 1]] = clone(value);
}

function getPath(target, pathString) {
    const segments = pathString.split('.');
    let current = target;
    for (const segment of segments) {
        if (current == null) return undefined;
        current = current[segment];
    }
    return current;
}

const settingsPanel = new SettingsPanel();
window.settingsPanel = settingsPanel;

})();
