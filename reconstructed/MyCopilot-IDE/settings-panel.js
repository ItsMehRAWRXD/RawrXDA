// Settings Panel Manager for MyCopilot IDE
class SettingsPanel {
    constructor() {
        this.currentTab = 'general';
        this.settings = this.loadSettings();
        this.init();
    }

    loadSettings() {
        const defaultSettings = {
            general: {
                theme: 'dark',
                fontSize: 14,
                fontFamily: 'Consolas, Monaco, monospace',
                autoSave: true,
                autoSaveDelay: 1000,
                tabSize: 4,
                insertSpaces: true
            },
            editor: {
                lineNumbers: true,
                wordWrap: false,
                minimap: true,
                formatOnSave: false,
                autoCloseBrackets: true,
                autoCloseQuotes: true,
                highlightActiveLine: true,
                renderWhitespace: 'none'
            },
            compiler: {
                defaultCompiler: 'auto',
                autoCompile: false,
                showCompilerOutput: true,
                optimizationLevel: 'O2',
                enableWarnings: true,
                warningsAsErrors: false,
                parallelCompilation: true
            },
            ai: {
                enableAI: true,
                aiProvider: 'local',
                modelEndpoint: 'http://localhost:3000',
                maxTokens: 2048,
                temperature: 0.7,
                autoSuggest: true,
                suggestionDelay: 500
            },
            terminal: {
                defaultShell: 'pwsh',
                fontSize: 12,
                cursorStyle: 'block',
                cursorBlink: true,
                scrollback: 1000,
                copyOnSelect: false
            },
            git: {
                enableGit: true,
                autoFetch: false,
                autoStage: false,
                showInlineBlame: true,
                confirmSync: true
            }
        };

        try {
            const saved = localStorage.getItem('mycopilot-settings');
            return saved ? { ...defaultSettings, ...JSON.parse(saved) } : defaultSettings;
        } catch (e) {
            console.error('Failed to load settings:', e);
            return defaultSettings;
        }
    }

    saveSettings() {
        try {
            localStorage.setItem('mycopilot-settings', JSON.stringify(this.settings));
            this.applySettings();
            this.showNotification('Settings saved successfully', 'success');
        } catch (e) {
            console.error('Failed to save settings:', e);
            this.showNotification('Failed to save settings', 'error');
        }
    }

    applySettings() {
        // Apply theme
        document.documentElement.setAttribute('data-theme', this.settings.general.theme);
        
        // Apply editor settings
        if (window.editor) {
            window.editor.updateOptions({
                fontSize: this.settings.general.fontSize,
                fontFamily: this.settings.general.fontFamily,
                tabSize: this.settings.general.tabSize,
                insertSpaces: this.settings.editor.insertSpaces,
                lineNumbers: this.settings.editor.lineNumbers ? 'on' : 'off',
                wordWrap: this.settings.editor.wordWrap ? 'on' : 'off',
                minimap: { enabled: this.settings.editor.minimap },
                renderWhitespace: this.settings.editor.renderWhitespace
            });
        }

        // Broadcast settings change
        window.dispatchEvent(new CustomEvent('settings-changed', { detail: this.settings }));
    }

    init() {
        this.createSettingsPanel();
        this.attachEventListeners();
        this.applySettings();
    }

    createSettingsPanel() {
        const settingsHTML = `
            <div id="settings-panel" class="settings-panel hidden">
                <div class="settings-header">
                    <h2>Settings</h2>
                    <button class="close-btn" onclick="settingsPanel.close()">×</button>
                </div>
                <div class="settings-container">
                    <div class="settings-tabs">
                        <button class="tab-btn active" data-tab="general">
                            <i class="icon">⚙️</i>
                            <span>General</span>
                        </button>
                        <button class="tab-btn" data-tab="editor">
                            <i class="icon">✏️</i>
                            <span>Editor</span>
                        </button>
                        <button class="tab-btn" data-tab="compiler">
                            <i class="icon">🔧</i>
                            <span>Compiler</span>
                        </button>
                        <button class="tab-btn" data-tab="ai">
                            <i class="icon">🤖</i>
                            <span>AI Assistant</span>
                        </button>
                        <button class="tab-btn" data-tab="terminal">
                            <i class="icon">💻</i>
                            <span>Terminal</span>
                        </button>
                        <button class="tab-btn" data-tab="git">
                            <i class="icon">📦</i>
                            <span>Git</span>
                        </button>
                    </div>
                    <div class="settings-content">
                        ${this.createGeneralTab()}
                        ${this.createEditorTab()}
                        ${this.createCompilerTab()}
                        ${this.createAITab()}
                        ${this.createTerminalTab()}
                        ${this.createGitTab()}
                    </div>
                </div>
                <div class="settings-footer">
                    <button class="btn btn-secondary" onclick="settingsPanel.reset()">Reset to Defaults</button>
                    <button class="btn btn-primary" onclick="settingsPanel.save()">Save Changes</button>
                </div>
            </div>
        `;

        const overlay = document.createElement('div');
        overlay.id = 'settings-overlay';
        overlay.className = 'settings-overlay hidden';
        overlay.onclick = () => this.close();
        document.body.appendChild(overlay);

        const container = document.createElement('div');
        container.innerHTML = settingsHTML;
        document.body.appendChild(container.firstElementChild);
    }

    createGeneralTab() {
        return `
            <div class="tab-content active" data-tab="general">
                <h3>General Settings</h3>
                
                <div class="setting-group">
                    <label>Theme</label>
                    <select id="setting-theme" value="${this.settings.general.theme}">
                        <option value="dark">Dark</option>
                        <option value="light">Light</option>
                        <option value="high-contrast">High Contrast</option>
                    </select>
                </div>

                <div class="setting-group">
                    <label>Font Size</label>
                    <input type="number" id="setting-fontSize" value="${this.settings.general.fontSize}" min="8" max="32">
                    <span class="unit">px</span>
                </div>

                <div class="setting-group">
                    <label>Font Family</label>
                    <input type="text" id="setting-fontFamily" value="${this.settings.general.fontFamily}">
                </div>

                <div class="setting-group">
                    <label>
                        <input type="checkbox" id="setting-autoSave" ${this.settings.general.autoSave ? 'checked' : ''}>
                        Auto Save
                    </label>
                </div>

                <div class="setting-group">
                    <label>Auto Save Delay</label>
                    <input type="number" id="setting-autoSaveDelay" value="${this.settings.general.autoSaveDelay}" min="100" max="5000" step="100">
                    <span class="unit">ms</span>
                </div>

                <div class="setting-group">
                    <label>Tab Size</label>
                    <input type="number" id="setting-tabSize" value="${this.settings.general.tabSize}" min="2" max="8">
                </div>

                <div class="setting-group">
                    <label>
                        <input type="checkbox" id="setting-insertSpaces" ${this.settings.general.insertSpaces ? 'checked' : ''}>
                        Insert Spaces (instead of tabs)
                    </label>
                </div>
            </div>
        `;
    }

    createEditorTab() {
        return `
            <div class="tab-content" data-tab="editor">
                <h3>Editor Settings</h3>
                
                <div class="setting-group">
                    <label>
                        <input type="checkbox" id="setting-lineNumbers" ${this.settings.editor.lineNumbers ? 'checked' : ''}>
                        Show Line Numbers
                    </label>
                </div>

                <div class="setting-group">
                    <label>
                        <input type="checkbox" id="setting-wordWrap" ${this.settings.editor.wordWrap ? 'checked' : ''}>
                        Word Wrap
                    </label>
                </div>

                <div class="setting-group">
                    <label>
                        <input type="checkbox" id="setting-minimap" ${this.settings.editor.minimap ? 'checked' : ''}>
                        Show Minimap
                    </label>
                </div>

                <div class="setting-group">
                    <label>
                        <input type="checkbox" id="setting-formatOnSave" ${this.settings.editor.formatOnSave ? 'checked' : ''}>
                        Format on Save
                    </label>
                </div>

                <div class="setting-group">
                    <label>
                        <input type="checkbox" id="setting-autoCloseBrackets" ${this.settings.editor.autoCloseBrackets ? 'checked' : ''}>
                        Auto Close Brackets
                    </label>
                </div>

                <div class="setting-group">
                    <label>
                        <input type="checkbox" id="setting-autoCloseQuotes" ${this.settings.editor.autoCloseQuotes ? 'checked' : ''}>
                        Auto Close Quotes
                    </label>
                </div>

                <div class="setting-group">
                    <label>
                        <input type="checkbox" id="setting-highlightActiveLine" ${this.settings.editor.highlightActiveLine ? 'checked' : ''}>
                        Highlight Active Line
                    </label>
                </div>

                <div class="setting-group">
                    <label>Render Whitespace</label>
                    <select id="setting-renderWhitespace" value="${this.settings.editor.renderWhitespace}">
                        <option value="none">None</option>
                        <option value="boundary">Boundary</option>
                        <option value="selection">Selection</option>
                        <option value="all">All</option>
                    </select>
                </div>
            </div>
        `;
    }

    createCompilerTab() {
        return `
            <div class="tab-content" data-tab="compiler">
                <h3>Compiler Settings</h3>
                
                <div class="setting-group">
                    <label>Default Compiler</label>
                    <select id="setting-defaultCompiler" value="${this.settings.compiler.defaultCompiler}">
                        <option value="auto">Auto-detect</option>
                        <option value="gcc">GCC</option>
                        <option value="clang">Clang</option>
                        <option value="msvc">MSVC</option>
                        <option value="rustc">Rust</option>
                        <option value="go">Go</option>
                        <option value="javac">Java</option>
                        <option value="powershell">PowerShell</option>
                    </select>
                </div>

                <div class="setting-group">
                    <label>
                        <input type="checkbox" id="setting-autoCompile" ${this.settings.compiler.autoCompile ? 'checked' : ''}>
                        Auto Compile on Save
                    </label>
                </div>

                <div class="setting-group">
                    <label>
                        <input type="checkbox" id="setting-showCompilerOutput" ${this.settings.compiler.showCompilerOutput ? 'checked' : ''}>
                        Show Compiler Output
                    </label>
                </div>

                <div class="setting-group">
                    <label>Optimization Level</label>
                    <select id="setting-optimizationLevel" value="${this.settings.compiler.optimizationLevel}">
                        <option value="O0">O0 - No optimization</option>
                        <option value="O1">O1 - Basic optimization</option>
                        <option value="O2">O2 - Moderate optimization</option>
                        <option value="O3">O3 - Aggressive optimization</option>
                    </select>
                </div>

                <div class="setting-group">
                    <label>
                        <input type="checkbox" id="setting-enableWarnings" ${this.settings.compiler.enableWarnings ? 'checked' : ''}>
                        Enable Compiler Warnings
                    </label>
                </div>

                <div class="setting-group">
                    <label>
                        <input type="checkbox" id="setting-warningsAsErrors" ${this.settings.compiler.warningsAsErrors ? 'checked' : ''}>
                        Treat Warnings as Errors
                    </label>
                </div>

                <div class="setting-group">
                    <label>
                        <input type="checkbox" id="setting-parallelCompilation" ${this.settings.compiler.parallelCompilation ? 'checked' : ''}>
                        Parallel Compilation
                    </label>
                </div>
            </div>
        `;
    }

    createAITab() {
        return `
            <div class="tab-content" data-tab="ai">
                <h3>AI Assistant Settings</h3>
                
                <div class="setting-group">
                    <label>
                        <input type="checkbox" id="setting-enableAI" ${this.settings.ai.enableAI ? 'checked' : ''}>
                        Enable AI Assistant
                    </label>
                </div>

                <div class="setting-group">
                    <label>AI Provider</label>
                    <select id="setting-aiProvider" value="${this.settings.ai.aiProvider}">
                        <option value="local">Local (PowerShell)</option>
                        <option value="ollama">Ollama</option>
                        <option value="openai">OpenAI</option>
                        <option value="anthropic">Anthropic (Claude)</option>
                        <option value="amazonq">Amazon Q</option>
                    </select>
                </div>

                <div class="setting-group">
                    <label>Model Endpoint</label>
                    <input type="text" id="setting-modelEndpoint" value="${this.settings.ai.modelEndpoint}">
                </div>

                <div class="setting-group">
                    <label>Max Tokens</label>
                    <input type="number" id="setting-maxTokens" value="${this.settings.ai.maxTokens}" min="128" max="8192">
                </div>

                <div class="setting-group">
                    <label>Temperature</label>
                    <input type="range" id="setting-temperature" value="${this.settings.ai.temperature}" min="0" max="1" step="0.1">
                    <span class="value">${this.settings.ai.temperature}</span>
                </div>

                <div class="setting-group">
                    <label>
                        <input type="checkbox" id="setting-autoSuggest" ${this.settings.ai.autoSuggest ? 'checked' : ''}>
                        Auto-suggest Code Completions
                    </label>
                </div>

                <div class="setting-group">
                    <label>Suggestion Delay</label>
                    <input type="number" id="setting-suggestionDelay" value="${this.settings.ai.suggestionDelay}" min="100" max="2000" step="100">
                    <span class="unit">ms</span>
                </div>
            </div>
        `;
    }

    createTerminalTab() {
        return `
            <div class="tab-content" data-tab="terminal">
                <h3>Terminal Settings</h3>
                
                <div class="setting-group">
                    <label>Default Shell</label>
                    <select id="setting-defaultShell" value="${this.settings.terminal.defaultShell}">
                        <option value="pwsh">PowerShell 7</option>
                        <option value="powershell">Windows PowerShell</option>
                        <option value="cmd">Command Prompt</option>
                        <option value="bash">Git Bash</option>
                    </select>
                </div>

                <div class="setting-group">
                    <label>Terminal Font Size</label>
                    <input type="number" id="setting-terminal-fontSize" value="${this.settings.terminal.fontSize}" min="8" max="24">
                    <span class="unit">px</span>
                </div>

                <div class="setting-group">
                    <label>Cursor Style</label>
                    <select id="setting-cursorStyle" value="${this.settings.terminal.cursorStyle}">
                        <option value="block">Block</option>
                        <option value="underline">Underline</option>
                        <option value="bar">Bar</option>
                    </select>
                </div>

                <div class="setting-group">
                    <label>
                        <input type="checkbox" id="setting-cursorBlink" ${this.settings.terminal.cursorBlink ? 'checked' : ''}>
                        Cursor Blink
                    </label>
                </div>

                <div class="setting-group">
                    <label>Scrollback Lines</label>
                    <input type="number" id="setting-scrollback" value="${this.settings.terminal.scrollback}" min="100" max="10000" step="100">
                </div>

                <div class="setting-group">
                    <label>
                        <input type="checkbox" id="setting-copyOnSelect" ${this.settings.terminal.copyOnSelect ? 'checked' : ''}>
                        Copy on Select
                    </label>
                </div>
            </div>
        `;
    }

    createGitTab() {
        return `
            <div class="tab-content" data-tab="git">
                <h3>Git Settings</h3>
                
                <div class="setting-group">
                    <label>
                        <input type="checkbox" id="setting-enableGit" ${this.settings.git.enableGit ? 'checked' : ''}>
                        Enable Git Integration
                    </label>
                </div>

                <div class="setting-group">
                    <label>
                        <input type="checkbox" id="setting-autoFetch" ${this.settings.git.autoFetch ? 'checked' : ''}>
                        Auto Fetch
                    </label>
                </div>

                <div class="setting-group">
                    <label>
                        <input type="checkbox" id="setting-autoStage" ${this.settings.git.autoStage ? 'checked' : ''}>
                        Auto Stage Modified Files
                    </label>
                </div>

                <div class="setting-group">
                    <label>
                        <input type="checkbox" id="setting-showInlineBlame" ${this.settings.git.showInlineBlame ? 'checked' : ''}>
                        Show Inline Git Blame
                    </label>
                </div>

                <div class="setting-group">
                    <label>
                        <input type="checkbox" id="setting-confirmSync" ${this.settings.git.confirmSync ? 'checked' : ''}>
                        Confirm Before Sync
                    </label>
                </div>
            </div>
        `;
    }

    attachEventListeners() {
        // Tab switching
        document.addEventListener('click', (e) => {
            if (e.target.closest('.tab-btn')) {
                const btn = e.target.closest('.tab-btn');
                const tab = btn.dataset.tab;
                this.switchTab(tab);
            }
        });

        // Temperature slider
        document.addEventListener('input', (e) => {
            if (e.target.id === 'setting-temperature') {
                const valueDisplay = e.target.nextElementSibling;
                if (valueDisplay) {
                    valueDisplay.textContent = e.target.value;
                }
            }
        });
    }

    switchTab(tabName) {
        this.currentTab = tabName;

        // Update tab buttons
        document.querySelectorAll('.tab-btn').forEach(btn => {
            btn.classList.toggle('active', btn.dataset.tab === tabName);
        });

        // Update tab content
        document.querySelectorAll('.tab-content').forEach(content => {
            content.classList.toggle('active', content.dataset.tab === tabName);
        });
    }

    open() {
        const panel = document.getElementById('settings-panel');
        const overlay = document.getElementById('settings-overlay');
        
        if (panel && overlay) {
            panel.classList.remove('hidden');
            overlay.classList.remove('hidden');
            this.populateCurrentValues();
        }
    }

    close() {
        const panel = document.getElementById('settings-panel');
        const overlay = document.getElementById('settings-overlay');
        
        if (panel && overlay) {
            panel.classList.add('hidden');
            overlay.classList.add('hidden');
        }
    }

    populateCurrentValues() {
        // Populate all form fields with current settings
        Object.entries(this.settings).forEach(([category, values]) => {
            Object.entries(values).forEach(([key, value]) => {
                const element = document.getElementById(`setting-${key}`);
                if (element) {
                    if (element.type === 'checkbox') {
                        element.checked = value;
                    } else {
                        element.value = value;
                    }
                }
            });
        });
    }

    save() {
        // Collect all form values
        const formElements = document.querySelectorAll('[id^="setting-"]');
        formElements.forEach(element => {
            const key = element.id.replace('setting-', '').replace('terminal-', '');
            let category = 'general';
            
            if (element.closest('[data-tab="editor"]')) category = 'editor';
            else if (element.closest('[data-tab="compiler"]')) category = 'compiler';
            else if (element.closest('[data-tab="ai"]')) category = 'ai';
            else if (element.closest('[data-tab="terminal"]')) category = 'terminal';
            else if (element.closest('[data-tab="git"]')) category = 'git';

            if (this.settings[category] && key in this.settings[category]) {
                if (element.type === 'checkbox') {
                    this.settings[category][key] = element.checked;
                } else if (element.type === 'number' || element.type === 'range') {
                    this.settings[category][key] = parseFloat(element.value);
                } else {
                    this.settings[category][key] = element.value;
                }
            }
        });

        this.saveSettings();
        this.close();
    }

    reset() {
        if (confirm('Reset all settings to defaults? This cannot be undone.')) {
            localStorage.removeItem('mycopilot-settings');
            this.settings = this.loadSettings();
            this.populateCurrentValues();
            this.applySettings();
            this.showNotification('Settings reset to defaults', 'info');
        }
    }

    showNotification(message, type = 'info') {
        console.log(`[Settings] ${type.toUpperCase()}: ${message}`);
        
        // Create notification element
        const notification = document.createElement('div');
        notification.className = `notification notification-${type}`;
        notification.textContent = message;
        document.body.appendChild(notification);

        setTimeout(() => notification.classList.add('show'), 10);
        setTimeout(() => {
            notification.classList.remove('show');
            setTimeout(() => notification.remove(), 300);
        }, 3000);
    }

    getSetting(category, key) {
        return this.settings[category]?.[key];
    }

    setSetting(category, key, value) {
        if (this.settings[category]) {
            this.settings[category][key] = value;
            this.saveSettings();
        }
    }
}

// Initialize settings panel when DOM is ready
if (typeof window !== 'undefined') {
    window.addEventListener('DOMContentLoaded', () => {
        window.settingsPanel = new SettingsPanel();
    });
}

// Export for use in other modules
if (typeof module !== 'undefined' && module.exports) {
    module.exports = SettingsPanel;
}
