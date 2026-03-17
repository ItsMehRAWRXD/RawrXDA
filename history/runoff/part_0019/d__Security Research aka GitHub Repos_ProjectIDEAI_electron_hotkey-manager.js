/**
 * BigDaddyG IDE - Comprehensive Hotkey Manager
 * Centralizes ALL keyboard shortcuts for proper conflict resolution
 */

(function() {
'use strict';

const DEFAULT_HOTKEYS = {
    'ide.newFile':      { combo: 'Ctrl+N',                description: 'New File',                 category: 'File' },
    'ide.openFile':     { combo: 'Ctrl+O',                description: 'Open File',                category: 'File' },
    'ide.saveFile':     { combo: 'Ctrl+S',                description: 'Save File',                category: 'File' },
    'ide.saveFileAs':   { combo: 'Ctrl+Shift+S',          description: 'Save As',                  category: 'File' },
    // 'ide.saveAll': Removed - chord hotkeys not fully supported yet
    'ide.saveAll':      { combo: 'Ctrl+Alt+S',            description: 'Save All Files',           category: 'File' },

    'tabs.next':        { combo: 'Ctrl+Tab',              description: 'Next Tab',                 category: 'Tabs' },
    'tabs.previous':    { combo: 'Ctrl+Shift+Tab',        description: 'Previous Tab',             category: 'Tabs' },
    'tabs.close':       { combo: 'Ctrl+W',                description: 'Close Tab',                category: 'Tabs' },
    'tabs.closeAll':    { combo: 'Ctrl+Shift+W',          description: 'Close All Tabs',           category: 'Tabs' },
    'tabs.switch.1':    { combo: 'Ctrl+1',                description: 'Switch to Tab 1',          category: 'Tabs' },
    'tabs.switch.2':    { combo: 'Ctrl+2',                description: 'Switch to Tab 2',          category: 'Tabs' },
    'tabs.switch.3':    { combo: 'Ctrl+3',                description: 'Switch to Tab 3',          category: 'Tabs' },
    'tabs.switch.4':    { combo: 'Ctrl+4',                description: 'Switch to Tab 4',          category: 'Tabs' },
    'tabs.switch.5':    { combo: 'Ctrl+5',                description: 'Switch to Tab 5',          category: 'Tabs' },
    'tabs.switch.6':    { combo: 'Ctrl+6',                description: 'Switch to Tab 6',          category: 'Tabs' },
    'tabs.switch.7':    { combo: 'Ctrl+7',                description: 'Switch to Tab 7',          category: 'Tabs' },
    'tabs.switch.8':    { combo: 'Ctrl+8',                description: 'Switch to Tab 8',          category: 'Tabs' },
    'tabs.switch.9':    { combo: 'Ctrl+9',                description: 'Switch to Tab 9',          category: 'Tabs' },
    'tabs.previousAlt': { combo: 'Alt+ArrowLeft',         description: 'Previous Tab (Alt)',       category: 'Tabs' },
    'tabs.nextAlt':     { combo: 'Alt+ArrowRight',        description: 'Next Tab (Alt)',           category: 'Tabs' },

    'center.chat':      { combo: 'Ctrl+Shift+C',          description: 'Open Chat Tab',            category: 'Center Tabs' },
    'center.explorer':  { combo: 'Ctrl+Shift+E',          description: 'Open Explorer Tab',        category: 'Center Tabs' },
    'center.github':    { combo: 'Ctrl+Shift+G',          description: 'Open GitHub Tab',          category: 'Center Tabs' },
    'center.agents':    { combo: 'Ctrl+Shift+A',          description: 'Open Agents Tab',          category: 'Center Tabs' },
    'center.team':      { combo: 'Ctrl+Shift+T',          description: 'Open Team Tab',            category: 'Center Tabs' },
    'center.settings':  { combo: 'Ctrl+,',                description: 'Open Settings Tab',        category: 'Center Tabs' },

    'chat.toggleFloating': { combo: 'Ctrl+L',             description: 'Toggle Floating Chat',     category: 'Chat' },
    'chat.sendMessage':    { combo: 'Ctrl+Enter',         description: 'Send AI Message',          category: 'Chat' },
    'chat.stop':           { combo: 'Ctrl+Shift+X',       description: 'Stop AI Execution',        category: 'Chat' },

    'terminal.toggle':  { combo: 'Ctrl+J',                description: 'Toggle Terminal',          category: 'Terminal' },
    'terminal.altToggle': { combo: 'Ctrl+`',              description: 'Toggle Terminal (Alt)',    category: 'Terminal' },
    'console.toggle':   { combo: 'Ctrl+Shift+U',          description: 'Toggle Console Panel',     category: 'Terminal' },
    'browser.toggle':   { combo: 'Ctrl+Shift+B',          description: 'Toggle Browser',           category: 'Terminal' },
    'memory.dashboard': { combo: 'Ctrl+Shift+M',          description: 'Memory Dashboard',         category: 'Tools' },
    'swarm.engine':     { combo: 'Ctrl+Alt+S',            description: 'Swarm Engine',             category: 'Tools' },
    'layout.customize': { combo: 'Ctrl+Shift+L',          description: 'Customize Layout',         category: 'View' },
    'layout.reset':     { combo: 'Ctrl+Alt+L',            description: 'Reset Layout',             category: 'View' },

    'voice.start':      { combo: 'Ctrl+Shift+V',          description: 'Start Voice Coding',       category: 'Voice' },
    'palette.open':     { combo: 'Ctrl+Shift+P',          description: 'Enhanced Command Palette', category: 'Productivity' },

    'modals.close':     { combo: 'Escape',                description: 'Close Modals',             category: 'General' },

    'editor.find':      { combo: 'Ctrl+F',                description: 'Find',                     category: 'Editor' },
    'editor.replace':   { combo: 'Ctrl+H',                description: 'Find & Replace',           category: 'Editor' },
    'editor.toggleComment': { combo: 'Ctrl+/',            description: 'Toggle Comment',           category: 'Editor' }
};

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

class HotkeyManager {
    constructor() {
        this.shortcuts = new Map();
        this.priority = [];
        this.hotkeyConfig = clone(DEFAULT_HOTKEYS);
        this.settingsApi = window.electron?.settings || null;
        this.refreshTimer = null;
        this.ready = false;

        this.init().catch((error) => {
            console.error('[HotkeyManager] ❌ Failed to initialize:', error);
        });
    }
    
    async init() {
        console.log('[HotkeyManager] ⌨️ Initializing hotkey manager...');

        await this.reloadHotkeys();
        this.registerAllShortcuts();
        
        document.addEventListener('keydown', (e) => this.handleKeyPress(e), true);
        
        this.ready = true;
        console.log('[HotkeyManager] ✅ Hotkey manager ready');
        this.listAllShortcuts();

        if (this.settingsApi) {
            this.settingsApi.onBootstrap((snapshot) => {
                if (snapshot?.hotkeys) {
                    this.hotkeyConfig = clone(snapshot.hotkeys);
                    this.registerAllShortcuts();
                    this.listAllShortcuts();
                }
            });

            this.settingsApi.onDidChange((event) => this.onSettingsChanged(event));
        }
    }
    
    async reloadHotkeys() {
        if (this.settingsApi) {
            try {
                const res = await this.settingsApi.getHotkeys();
                if (res?.success && res.hotkeys) {
                    this.hotkeyConfig = clone(res.hotkeys);
                    return;
                }
            } catch (error) {
                console.warn('[HotkeyManager] ⚠️ Failed to fetch hotkeys from settings service:', error);
            }
        }
        this.hotkeyConfig = clone(DEFAULT_HOTKEYS);
    }

    scheduleRefreshHotkeys() {
        if (this.refreshTimer) return;
        this.refreshTimer = setTimeout(async () => {
            this.refreshTimer = null;
            await this.reloadHotkeys();
            this.registerAllShortcuts();
            this.listAllShortcuts();
        }, 100);
    }

    onSettingsChanged(event) {
        if (!event) return;
        if (event.type === 'hotkey') {
            this.scheduleRefreshHotkeys();
            return;
        }

        if (event.type === 'reset') {
            if (!event.section || event.section.startsWith('hotkeys')) {
                this.scheduleRefreshHotkeys();
            }
            return;
        }

        if (event.type === 'update' && event.changes && event.changes.hotkeys) {
            this.scheduleRefreshHotkeys();
        }
    }
    
    registerTabShortcuts() {
        this.bindHotkey('center.chat', () => {
            if (window.tabSystem && typeof window.tabSystem.openChatTab === 'function') {
                window.tabSystem.openChatTab();
            } else {
                console.warn('[HotkeyManager] Tab system not ready yet');
            }
        }, 'Open Chat Tab');
        
        this.bindHotkey('center.explorer', () => {
            if (window.tabSystem && typeof window.tabSystem.openExplorerTab === 'function') {
                window.tabSystem.openExplorerTab();
            } else {
                console.warn('[HotkeyManager] Tab system not ready yet');
            }
        }, 'Open Explorer Tab');
        
        this.bindHotkey('center.github', () => {
            if (window.tabSystem && typeof window.tabSystem.openGitHubTab === 'function') {
                window.tabSystem.openGitHubTab();
            } else {
                console.warn('[HotkeyManager] Tab system not ready yet');
            }
        }, 'Open GitHub Tab');
        
        this.bindHotkey('center.agents', () => {
            if (window.tabSystem && typeof window.tabSystem.openAgentsTab === 'function') {
                window.tabSystem.openAgentsTab();
            } else {
                console.warn('[HotkeyManager] Tab system not ready yet');
            }
        }, 'Open Agents Tab');
        
        this.bindHotkey('center.team', () => {
            if (window.tabSystem && typeof window.tabSystem.openTeamTab === 'function') {
                window.tabSystem.openTeamTab();
            } else {
                console.warn('[HotkeyManager] Tab system not ready yet');
            }
        }, 'Open Team Tab');
        
        this.bindHotkey('center.settings', () => {
            if (window.tabSystem && typeof window.tabSystem.openSettingsTab === 'function') {
                window.tabSystem.openSettingsTab();
            } else {
                console.warn('[HotkeyManager] Tab system not ready yet');
            }
        }, 'Open Settings Tab');
    }
    
    registerAllShortcuts() {
        this.shortcuts.clear();

        // ========================================================================
        // FILE OPERATIONS
        // ========================================================================
        this.bindHotkey('ide.newFile', () => {
            if (typeof createNewFile === 'function') {
                createNewFile();
            }
        }, 'New File');
        
        this.bindHotkey('ide.openFile', () => {
            if (typeof openFileDialog === 'function') {
                openFileDialog();
            }
        }, 'Open File');
        
        this.bindHotkey('ide.saveFile', () => {
            if (typeof saveCurrentFile === 'function') {
                saveCurrentFile();
            }
        }, 'Save File');
        
        this.bindHotkey('ide.saveFileAs', () => {
            if (typeof saveFileAs === 'function') {
                saveFileAs();
            }
        }, 'Save As');
        
        this.bindHotkey('ide.saveAll', () => {
            if (typeof saveAllFiles === 'function') {
                saveAllFiles();
            }
        }, 'Save All Files');

        // ========================================================================
        // TAB OPERATIONS
        // ========================================================================
        this.bindHotkey('tabs.next', () => {
            if (typeof nextTab === 'function') nextTab();
        }, 'Next Tab');
        
        this.bindHotkey('tabs.previous', () => {
            if (typeof previousTab === 'function') previousTab();
        }, 'Previous Tab');
        
        this.bindHotkey('tabs.close', () => {
            if (typeof closeTab === 'function' && window.activeTab) {
                closeTab({ stopPropagation: () => {} }, window.activeTab);
            }
        }, 'Close Tab');
        
        this.bindHotkey('tabs.closeAll', () => {
            if (typeof closeAllTabs === 'function') closeAllTabs();
        }, 'Close All Tabs');
        
        for (let i = 1; i <= 9; i++) {
            this.bindHotkey(`tabs.switch.${i}`, () => {
                const tabs = Object.keys(window.openTabs || {});
                if (tabs[i - 1]) {
                    if (typeof switchTab === 'function') switchTab(tabs[i - 1]);
                }
            }, `Switch to Tab ${i}`);
        }
        
        this.bindHotkey('tabs.previousAlt', () => {
            if (typeof previousTab === 'function') previousTab();
        }, 'Previous Tab (Alt)');
        
        this.bindHotkey('tabs.nextAlt', () => {
            if (typeof nextTab === 'function') nextTab();
        }, 'Next Tab (Alt)');
        
        // ========================================================================
        // CENTER TABS (Tab System)
        // ========================================================================
        this.registerTabShortcuts();
        
        // ========================================================================
        // FLOATING CHAT
        // ========================================================================
        this.bindHotkey('chat.toggleFloating', (e) => {
            if (e) {
                e.preventDefault();
                e.stopPropagation();
            }
            
            if (window.floatingChat && typeof window.floatingChat.toggle === 'function') {
                window.floatingChat.toggle();
                console.log('[HotkeyManager] 💬 Floating chat toggled');
            } else if (window.handleCtrlL && typeof window.handleCtrlL === 'function') {
                window.handleCtrlL();
            } else {
                console.warn('[HotkeyManager] ⚠️ Floating chat not available - initializing...');
                if (typeof FloatingChat !== 'undefined') {
                    window.floatingChat = new FloatingChat();
                    setTimeout(() => {
                        if (window.floatingChat) window.floatingChat.toggle();
                    }, 100);
                }
            }
        }, 'Toggle Floating Chat (CTRL+L)');
        
        // ========================================================================
        // AI CHAT
        // ========================================================================
        this.bindHotkey('chat.sendMessage', (e) => {
            const activeEl = document.activeElement;
            const isAIInput = activeEl && (
                activeEl.id === 'ai-input' ||
                activeEl.id === 'floating-chat-input' ||
                activeEl.id === 'center-chat-input' ||
                activeEl.classList.contains('ai-input') ||
                activeEl.closest('#ai-chat-inputs') ||
                activeEl.closest('.floating-chat-input')
            );
            
            if (isAIInput) {
                if (e) {
                    e.preventDefault();
                    e.stopPropagation();
                }
                if (typeof sendToAI === 'function') {
                    sendToAI();
                } else if (window.floatingChat && activeEl.id === 'floating-chat-input') {
                    const sendBtn = document.querySelector('#floating-chat-send');
                    if (sendBtn) sendBtn.click();
                }
            }
        }, 'Send AI Message');
        
        this.bindHotkey('chat.stop', () => {
            if (window.aiResponseHandler) {
                window.aiResponseHandler.stopCurrentExecution();
            }
        }, 'Stop AI Execution');
        
        // ========================================================================
        // TERMINAL & CONSOLE - Unified terminal toggle
        // ========================================================================
        this.bindHotkey('terminal.toggle', () => {
            this.toggleUnifiedTerminal();
        }, 'Toggle Terminal');

        // Alt keybinding for terminal (same as Ctrl+J)
        this.bindHotkey('terminal.altToggle', () => {
            this.toggleUnifiedTerminal();
        }, 'Toggle Terminal (Alt)');

        this.bindHotkey('console.toggle', () => {
            if (typeof window.toggleConsolePanel === 'function') {
                window.toggleConsolePanel();
            } else if (window.consolePanelInstance) {
                window.consolePanelInstance.toggle();
            } else {
                console.warn('[HotkeyManager] Console panel not ready yet');
            }
        }, 'Toggle Console Panel');

        this.bindHotkey('browser.toggle', () => {
            if (window.webBrowser && typeof window.webBrowser.toggleBrowser === 'function') {
                window.webBrowser.toggleBrowser();
            } else if (window.browserPanel && typeof window.browserPanel.toggle === 'function') {
                window.browserPanel.toggle();
            } else {
                console.warn('[HotkeyManager] Browser panel not ready yet');
            }
        }, 'Toggle Browser');
        
        // ========================================================================
        // MEMORY DASHBOARD
        // ========================================================================
        this.bindHotkey('memory.dashboard', () => {
            if (window.memoryBridge && window.memoryBridge.isAvailable()) {
                if (window.tabSystem && typeof window.tabSystem.openMemoryTab === 'function') {
                    window.tabSystem.openMemoryTab();
                } else {
                    console.warn('[HotkeyManager] Memory tab system not ready');
                }
            } else {
                console.warn('[HotkeyManager] Memory service not available');
                window.showNotification?.('Memory Service Offline', 'Please start the memory service first', 'warning', 3000);
            }
        }, 'Memory Dashboard');
        
        // ========================================================================
        // SWARM ENGINE
        // ========================================================================
        this.bindHotkey('swarm.engine', () => {
            if (window.swarmEngine) {
                window.swarmEngine.toggle();
            } else if (window.tabSystem && typeof window.tabSystem.openSwarmTab === 'function') {
                window.tabSystem.openSwarmTab();
            } else {
                console.warn('[HotkeyManager] Swarm engine not ready');
                window.showNotification?.('Swarm Engine', 'Feature coming soon', 'info', 2000);
            }
        }, 'Swarm Engine');
        
        // ========================================================================
        // FLEXIBLE LAYOUT
        // ========================================================================
        this.bindHotkey('layout.customize', () => {
            if (window.flexibleLayout) {
                window.flexibleLayout.showPanelSelector((type) => {
                    window.flexibleLayout.addPanel(type, 'root');
                });
            } else {
                window.showNotification?.('Layout System', 'Loading...', 'info', 2000);
            }
        }, 'Customize Layout');
        
        this.bindHotkey('layout.reset', () => {
            if (window.flexibleLayout) {
                if (confirm('Reset layout to default?')) {
                    window.flexibleLayout.createDefaultLayout();
                    window.showNotification?.('Layout Reset', 'Back to default!', 'success', 2000);
                }
            }
        }, 'Reset Layout');
        
        // ========================================================================
        // VOICE CODING & COMMAND PALETTE
        // ========================================================================
        this.bindHotkey('voice.start', () => {
            if (typeof startVoiceCoding === 'function') {
                startVoiceCoding();
            } else if (window.voiceCodingEngine) {
                window.voiceCodingEngine.start();
            }
        }, 'Start Voice Coding');
        
        this.bindHotkey('palette.open', () => {
            console.log('[HotkeyManager] 💡 Opening enhanced command palette...');
            if (window.commandPalette) {
                window.commandPalette.show();
            } else {
                this.showCommandPalette();
            }
        }, 'Enhanced Command Palette');
        
        // ========================================================================
        // ESCAPE KEY (Close modals, etc.)
        // ========================================================================
        this.bindHotkey('modals.close', () => {
            if (window.floatingChat && window.floatingChat.isOpen) {
                window.floatingChat.close();
            }
            
            const errorModal = document.getElementById('error-log-modal');
            if (errorModal) errorModal.remove();
        }, 'Close Modals');
        
        // ========================================================================
        // FIND & REPLACE
        // ========================================================================
        this.bindHotkey('editor.find', () => {
            if (window.editor) {
                window.editor.getAction('actions.find').run();
            }
        }, 'Find');
        
        this.bindHotkey('editor.replace', () => {
            if (window.editor) {
                window.editor.getAction('editor.action.startFindReplaceAction').run();
            }
        }, 'Find & Replace');
        
        // ========================================================================
        // COMMENT TOGGLE
        // ========================================================================
        this.bindHotkey('editor.toggleComment', () => {
            if (window.editor) {
                window.editor.getAction('editor.action.commentLine').run();
            }
        }, 'Toggle Comment');
    }
    
    register(combo, handler, description, action) {
        if (!combo) return;
        const normalized = this.normalizeCombo(combo);
        if (!normalized) return;
        this.shortcuts.set(normalized, {
            handler,
            description,
            combo,
            action: action || null
        });
    }
    
    normalizeCombo(combo) {
        if (!combo) return null;
        let normalizedCombo = combo.trim();
        if (normalizedCombo.includes(' ')) {
            // Basic support: use the last chord of the sequence
            const parts = normalizedCombo.split(' ');
            normalizedCombo = parts[parts.length - 1];
        }

        const parts = normalizedCombo.split('+').map(p => p.trim());
        const key = parts.pop().toLowerCase();
        const modifiers = parts.map(p => p.toLowerCase()).sort().join('+');
        return `${modifiers ? modifiers + '+' : ''}${key}`;
    }
    
    handleKeyPress(e) {
        // Build key identifier
        const parts = [];
        if (e.ctrlKey) parts.push('ctrl');
        if (e.altKey) parts.push('alt');
        if (e.shiftKey) parts.push('shift');
        if (e.metaKey) parts.push('meta');
        
        let key = e.key.toLowerCase();
        
        // Normalize special keys
        if (key === 'arrowleft') key = 'arrowleft';
        else if (key === 'arrowright') key = 'arrowright';
        else if (key === 'arrowup') key = 'arrowup';
        else if (key === 'arrowdown') key = 'arrowdown';
        else if (key === ' ') key = 'space';
        else if (key.length === 1) key = key.toLowerCase();
        
        parts.push(key);
        const identifier = parts.join('+');
        
        // Check if shortcut exists
        const shortcut = this.shortcuts.get(identifier);
        
        if (shortcut) {
            // Check if we're in an input field (some shortcuts should work even in inputs)
            const shouldPreventDefault = !this.isInputField(e.target) || 
                                       ['ctrl+enter', 'escape'].includes(identifier) ||
                                       identifier.startsWith('ctrl+shift+');
            
            if (shouldPreventDefault) {
                e.preventDefault();
                e.stopPropagation();
            }
            
            console.log(`[HotkeyManager] ⌨️ ${shortcut.description} (${shortcut.combo})`);
            shortcut.handler(e);
            return true;
        }
        
        return false;
    }
    
    isInputField(element) {
        if (!element) return false;
        const tag = typeof element.tagName === 'string' ? element.tagName.toLowerCase() : '';
        return tag === 'input' || tag === 'textarea' || element.contentEditable === 'true';
    }
    
    listAllShortcuts() {
        console.log('[HotkeyManager] 📋 Registered shortcuts:');
        console.log(`[HotkeyManager] 📊 Total: ${this.shortcuts.size} shortcuts`);

        const categories = {};

        for (const [, shortcut] of this.shortcuts) {
            const action = shortcut.action;
            const config = (action && (this.hotkeyConfig[action] || DEFAULT_HOTKEYS[action])) || {};
            const category = config.category || 'Other';
            const label = shortcut.description || config.description || action || shortcut.combo;

            if (!categories[category]) categories[category] = [];
            categories[category].push(`  ${String(shortcut.combo).padEnd(20)} → ${label}`);
        }

        for (const [cat, list] of Object.entries(categories)) {
            if (list.length > 0) {
                console.log(`\n${cat}:`);
                list.forEach(item => console.log(item));
            }
        }
    }
    
    // ========================================================================
    // COMMAND PALETTE
    // ========================================================================
    
    async showCommandPalette() {
        // Remove existing palette
        const existing = document.getElementById('command-palette');
        if (existing) {
            existing.remove();
            return; // Toggle off
        }
        
        // Create command palette overlay
        const overlay = document.createElement('div');
        overlay.id = 'command-palette';
        overlay.style.cssText = `
            position: fixed;
            top: 0;
            left: 0;
            right: 0;
            bottom: 0;
            background: rgba(0, 0, 0, 0.8);
            backdrop-filter: blur(5px);
            z-index: 10000;
            display: flex;
            justify-content: center;
            padding-top: 100px;
            animation: fadeIn 0.2s ease-out;
        `;
        
        // Add CSS animations if not already present
        if (!document.getElementById('hotkey-animations')) {
            const style = document.createElement('style');
            style.id = 'hotkey-animations';
            style.textContent = `
                @keyframes fadeIn {
                    from { opacity: 0; }
                    to { opacity: 1; }
                }
                @keyframes slideUp {
                    from { transform: translateY(20px); opacity: 0; }
                    to { transform: translateY(0); opacity: 1; }
                }
            `;
            document.head.appendChild(style);
        }
        
        // Create palette container
        const palette = document.createElement('div');
        palette.style.cssText = `
            width: 600px;
            max-width: 90%;
            height: fit-content;
            max-height: 500px;
            background: rgba(10, 10, 30, 0.98);
            border: 1px solid var(--cyan);
            border-radius: 12px;
            box-shadow: 0 20px 60px rgba(0, 0, 0, 0.5);
            display: flex;
            flex-direction: column;
            animation: slideUp 0.3s ease-out;
        `;
        
        // Search input
        const searchBox = document.createElement('input');
        searchBox.type = 'text';
        searchBox.placeholder = 'Type a command or search...';
        searchBox.style.cssText = `
            padding: 16px 20px;
            background: rgba(0, 0, 0, 0.5);
            border: none;
            border-bottom: 1px solid rgba(0, 212, 255, 0.2);
            color: #fff;
            font-size: 14px;
            outline: none;
            border-radius: 12px 12px 0 0;
        `;
        
        // Commands list
        const commandsList = document.createElement('div');
        commandsList.style.cssText = `
            flex: 1;
            overflow-y: auto;
            padding: 8px 0;
        `;
        
        // Get all commands
        const commands = await this.getAllCommands();
        
        // Render commands
        const renderCommands = async (filter = '') => {
            commandsList.innerHTML = '<div style="padding: 20px; text-align: center; color: #666;">Searching...</div>';
            
            // Get fresh commands (includes file search)
            const allCommands = await this.getAllCommands();
            
            const filtered = allCommands.filter(cmd => 
                cmd.name.toLowerCase().includes(filter.toLowerCase()) ||
                (cmd.description && cmd.description.toLowerCase().includes(filter.toLowerCase())) ||
                (cmd.path && cmd.path.toLowerCase().includes(filter.toLowerCase()))
            );
            
            commandsList.innerHTML = '';
            
            if (filtered.length === 0) {
                commandsList.innerHTML = '<div style="padding: 20px; text-align: center; color: #666;">No commands or files found</div>';
                return;
            }
            
            // Limit results to prevent UI lag
            const displayCommands = filtered.slice(0, 50);
            
            displayCommands.forEach((cmd, index) => {
                const item = document.createElement('div');
                item.className = 'command-palette-item';
                item.style.cssText = `
                    padding: 12px 20px;
                    cursor: pointer;
                    display: flex;
                    justify-content: space-between;
                    align-items: center;
                    transition: background 0.2s;
                    ${index === 0 ? 'background: rgba(0, 212, 255, 0.1);' : ''}
                `;
                
                const sanitize = (str) => String(str || '').replace(/[<>"'&]/g, (m) => ({'<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;','&':'&amp;'}[m]));
                const icon = cmd.type === 'file' ? (cmd.isDirectory ? '📁' : '📄') : '⚡';
                const pathInfo = cmd.path ? `<div style="color: #666; font-size: 10px; margin-top: 2px;">${sanitize(cmd.path)}</div>` : '';
                
                item.innerHTML = `
                    <div style="flex: 1; min-width: 0;">
                        <div style="display: flex; align-items: center; gap: 8px;">
                            <span style="font-size: 14px;">${icon}</span>
                            <div style="color: #fff; font-size: 13px; font-weight: ${cmd.type === 'file' ? '500' : '600'};">${sanitize(cmd.name)}</div>
                        </div>
                        ${cmd.description ? `<div style="color: #888; font-size: 11px; margin-top: 2px;">${sanitize(cmd.description)}</div>` : ''}
                        ${pathInfo}
                    </div>
                    ${cmd.shortcut ? `<kbd style="background: rgba(0, 0, 0, 0.5); padding: 4px 8px; border-radius: 4px; font-size: 11px; color: var(--cyan);">${sanitize(cmd.shortcut)}</kbd>` : ''}
                `;
                
                item.onclick = () => {
                    cmd.action();
                    overlay.remove();
                };
                
                item.onmouseenter = () => {
                    document.querySelectorAll('.command-palette-item').forEach(i => {
                        i.style.background = 'transparent';
                    });
                    item.style.background = 'rgba(0, 212, 255, 0.1)';
                };
                
                commandsList.appendChild(item);
            });
            
            if (filtered.length > 50) {
                const moreItem = document.createElement('div');
                moreItem.style.cssText = 'padding: 8px 20px; color: #666; font-size: 11px; text-align: center; font-style: italic;';
                moreItem.textContent = `... and ${filtered.length - 50} more results. Refine your search.`;
                commandsList.appendChild(moreItem);
            }
        };
        
        // Search handler with debounce
        let searchTimeout;
        searchBox.oninput = (e) => {
            clearTimeout(searchTimeout);
            searchTimeout = setTimeout(() => {
                renderCommands(e.target.value);
            }, 200);
        };
        
        // Keyboard navigation
        searchBox.onkeydown = (e) => {
            if (e.key === 'Escape') {
                overlay.remove();
            } else if (e.key === 'Enter') {
                const selected = commandsList.querySelector('.command-palette-item');
                if (selected) {
                    selected.click();
                }
            } else if (e.key === 'ArrowDown' || e.key === 'ArrowUp') {
                e.preventDefault();
                const items = Array.from(commandsList.querySelectorAll('.command-palette-item'));
                const current = items.findIndex(i => i.style.background !== 'transparent');
                
                if (e.key === 'ArrowDown') {
                    const next = (current + 1) % items.length;
                    items.forEach((i, idx) => {
                        i.style.background = idx === next ? 'rgba(0, 212, 255, 0.1)' : 'transparent';
                    });
                } else {
                    const prev = (current - 1 + items.length) % items.length;
                    items.forEach((i, idx) => {
                        i.style.background = idx === prev ? 'rgba(0, 212, 255, 0.1)' : 'transparent';
                    });
                }
            }
        };
        
        // Initial render
        await renderCommands();
        
        // Assemble
        palette.appendChild(searchBox);
        palette.appendChild(commandsList);
        overlay.appendChild(palette);
        
        // Close on overlay click
        overlay.onclick = (e) => {
            if (e.target === overlay) {
                overlay.remove();
            }
        };
        
        document.body.appendChild(overlay);
        searchBox.focus();
    }
    
    async getAllCommands() {
        const baseCommands = [
            { name: 'Open File', description: 'Open a file from disk', shortcut: 'Ctrl+O', action: () => window.electron && window.electron.openFileDialog && window.electron.openFileDialog() },
            { name: 'Save File', description: 'Save current file', shortcut: 'Ctrl+S', action: () => window.saveFile && window.saveFile() },
            { name: 'New File', description: 'Create a new file', shortcut: 'Ctrl+N', action: () => window.createNewTab && window.createNewTab('untitled', 'plaintext', '', null) },
            { name: 'AI Chat', description: 'Open floating AI chat', shortcut: 'Ctrl+L', action: () => window.floatingChat && window.floatingChat.toggle() },
            { name: 'Memory Dashboard', description: 'View memory statistics', shortcut: 'Ctrl+Shift+M', action: () => window.memoryDashboard && window.memoryDashboard.toggle() },
            { name: 'File Explorer', description: 'Focus file explorer', shortcut: 'Ctrl+Shift+E', action: () => switchTab('explorer') },
            
            // Terminal Commands
            { name: 'Terminal: Toggle', description: 'Open/close integrated terminal', shortcut: 'Ctrl+`', action: () => this.toggleTerminal() },
            { name: 'Terminal: PowerShell', description: 'Open PowerShell terminal', action: () => this.openTerminal('powershell') },
            { name: 'Terminal: Command Prompt', description: 'Open Command Prompt', action: () => this.openTerminal('cmd') },
            { name: 'Terminal: Git Bash', description: 'Open Git Bash terminal', action: () => this.openTerminal('bash') },
            { name: 'Terminal: WSL', description: 'Open WSL terminal', action: () => this.openTerminal('wsl') },
            { name: 'Terminal: Clear', description: 'Clear terminal output', action: () => this.clearTerminal() },
            
            // Build & Run Commands
            { name: 'Run: npm install', description: 'Install npm dependencies', action: () => this.runCommand('npm install') },
            { name: 'Run: npm start', description: 'Start npm dev server', action: () => this.runCommand('npm start') },
            { name: 'Run: npm run build', description: 'Build project', action: () => this.runCommand('npm run build') },
            { name: 'Run: npm test', description: 'Run tests', action: () => this.runCommand('npm test') },
            { name: 'Run: git status', description: 'Check git status', action: () => this.runCommand('git status') },
            { name: 'Run: git pull', description: 'Pull latest changes', action: () => this.runCommand('git pull') },
            { name: 'Run: git push', description: 'Push changes', action: () => this.runCommand('git push') },
            
            { name: 'Multi-Agent Swarm', description: 'Open agent collaboration', action: () => window.showSwarmEngine && window.showSwarmEngine() },
            { name: 'Check System Health', description: 'Run system diagnostics', action: () => window.checkHealth && window.checkHealth() },
            { name: 'Reload IDE', description: 'Reload the application', shortcut: 'Ctrl+R', action: () => location.reload() },
            { name: 'Toggle Sidebar', description: 'Show/hide left sidebar', action: () => { const sb = document.getElementById('sidebar'); if (sb) sb.style.display = sb.style.display === 'none' ? 'flex' : 'none'; } },
            { name: 'Refresh File Explorer', description: 'Reload drives and files', action: () => window.enhancedFileExplorer && window.enhancedFileExplorer.refresh() },
            { name: 'Close All Editors', description: 'Close all open files', action: () => window.enhancedFileExplorer && window.enhancedFileExplorer.closeAllEditors() },
            { name: 'Settings', description: 'Open IDE settings', shortcut: 'Ctrl+,', action: () => alert('Settings coming soon!') },
            { name: 'Keyboard Shortcuts', description: 'View all shortcuts', action: () => this.showShortcuts() }
        ];
        
        // Add file search results
        const fileCommands = await this.getFileCommands();
        
        return [...baseCommands, ...fileCommands];
    }
    
    showShortcuts() {
        const sanitize = (str) => String(str || '').replace(/[<>"'&]/g, (m) => ({'<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;','&':'&amp;'}[m]));
        const shortcuts = Array.from(this.shortcuts.entries()).map(([key, data]) => 
            `<tr><td style="padding: 8px; border: 1px solid rgba(0, 212, 255, 0.2);">${sanitize(data.combo)}</td><td style="padding: 8px; border: 1px solid rgba(0, 212, 255, 0.2);">${sanitize(data.description || 'No description')}</td></tr>`
        ).join('');
        
        const modal = document.createElement('div');
        modal.style.cssText = `
            position: fixed;
            top: 0;
            left: 0;
            right: 0;
            bottom: 0;
            background: rgba(0, 0, 0, 0.9);
            z-index: 10001;
            display: flex;
            justify-content: center;
            align-items: center;
            padding: 20px;
        `;
        
        const container = document.createElement('div');
        container.style.cssText = 'background: rgba(10, 10, 30, 0.98); border: 1px solid var(--cyan); border-radius: 12px; padding: 30px; max-width: 800px; width: 100%; max-height: 80vh; overflow-y: auto;';
        
        const title = document.createElement('h2');
        title.style.cssText = 'margin: 0 0 20px 0; color: var(--cyan);';
        title.textContent = '⌨️ Keyboard Shortcuts';
        
        const table = document.createElement('table');
        table.style.cssText = 'width: 100%; border-collapse: collapse; color: #fff; font-size: 13px;';
        table.innerHTML = `
            <thead>
                <tr>
                    <th style="padding: 8px; border: 1px solid rgba(0, 212, 255, 0.2); text-align: left; background: rgba(0, 0, 0, 0.3);">Shortcut</th>
                    <th style="padding: 8px; border: 1px solid rgba(0, 212, 255, 0.2); text-align: left; background: rgba(0, 0, 0, 0.3);">Description</th>
                </tr>
            </thead>
            <tbody>
                ${shortcuts}
            </tbody>
        `;
        
        const closeBtn = document.createElement('button');
        closeBtn.style.cssText = 'margin-top: 20px; padding: 10px 20px; background: var(--cyan); color: #000; border: none; border-radius: 6px; cursor: pointer; font-weight: bold;';
        closeBtn.textContent = 'Close';
        closeBtn.onclick = () => modal.remove();
        
        container.appendChild(title);
        container.appendChild(table);
        container.appendChild(closeBtn);
        modal.appendChild(container);
        
        modal.onclick = (e) => {
            if (e.target === modal) {
                modal.remove();
            }
        };
        
        document.body.appendChild(modal);
    }
    
    // ========================================================================
    // MISSING METHOD IMPLEMENTATIONS
    // ========================================================================
    
    async getFileCommands() {
        try {
            // Scan workspace for files
            if (window.electron && window.electron.scanWorkspace) {
                const files = await window.electron.scanWorkspace();
                return files.map(file => ({
                    name: file.name,
                    path: file.path,
                    type: 'file',
                    isDirectory: file.isDirectory,
                    action: () => {
                        if (window.electron && window.electron.openFile) {
                            window.electron.openFile(file.path);
                        }
                    }
                }));
            }
        } catch (error) {
            console.warn('[HotkeyManager] File scan not available:', error);
        }
        return [];
    }
    
    toggleUnifiedTerminal() {
        // Unified terminal toggle - works for both Ctrl+J and Ctrl+`
        if (typeof window.toggleTerminalPanel === 'function') {
            window.toggleTerminalPanel();
        } else if (window.terminalPanelInstance) {
            window.terminalPanelInstance.toggle();
        } else if (window.enhancedTerminal) {
            window.enhancedTerminal.toggle();
        } else {
            // Fallback to bottom panel
            const panel = document.getElementById('bottom-panel') || document.getElementById('enhanced-terminal');
            if (panel) {
                if (panel.style.display === 'none') {
                    panel.style.display = 'flex';
                } else {
                    panel.style.display = 'none';
                }
            } else {
                console.warn('[HotkeyManager] Terminal panel not ready yet');
            }
        }
    }
    
    toggleTerminal() {
        // Deprecated - use toggleUnifiedTerminal instead
        this.toggleUnifiedTerminal();
    }
    
    openTerminal(shell = 'powershell') {
        // Use enhanced terminal if available
        if (window.enhancedTerminal) {
            window.enhancedTerminal.open();
            if (shell && window.enhancedTerminal.switchShell) {
                window.enhancedTerminal.switchShell(shell);
            }
            return;
        }
        
        // Fallback: just toggle terminal
        this.toggleTerminal();
    }
    
    clearTerminal() {
        // Use enhanced terminal if available
        if (window.enhancedTerminal && window.enhancedTerminal.clearTerminal) {
            window.enhancedTerminal.clearTerminal();
            return;
        }
        
        // Fallback: clear output element
        const output = document.getElementById('terminal-output');
        if (output) {
            output.innerHTML = '<div style="color: var(--cursor-jade-dark);">Terminal cleared</div>';
        }
    }
    
    runCommand(command) {
        // Use enhanced terminal if available
        if (window.enhancedTerminal && window.enhancedTerminal.run) {
            window.enhancedTerminal.run(command);
            return;
        }
        
        // Fallback: log command
        console.log(`[HotkeyManager] Command: ${command}`);
        
        // Try to open terminal and show command
        this.toggleTerminal();
        setTimeout(() => {
            const input = document.getElementById('terminal-input');
            if (input) {
                input.value = String(command).replace(/[<>"'&]/g, '');
                input.focus();
            }
        }, 100);
    }

    bindHotkey(action, handler, description) {
        const config = this.hotkeyConfig[action] || DEFAULT_HOTKEYS[action];
        if (!config || !config.combo) {
            return;
        }

        const combos = String(config.combo)
            .split(',')
            .map((value) => value.trim())
            .filter(Boolean);

        const label = description || config.description || action;

        combos.forEach((combo) => {
            if (!combo) return;
            if (combo.includes(' ')) {
                console.warn(`[HotkeyManager] ⚠️ Chord hotkeys (“${combo}”) are not fully supported yet.`);
            }
            this.register(combo, handler, label, action);
        });
    }
    
    destroy() {
        if (this.refreshTimer) {
            clearTimeout(this.refreshTimer);
            this.refreshTimer = null;
        }
        this.shortcuts.clear();
        this.priority = [];
        document.removeEventListener('keydown', this.handleKeyPress, true);
        const animStyle = document.getElementById('hotkey-animations');
        if (animStyle) animStyle.remove();
        console.log('[HotkeyManager] 🧹 Cleaned up');
    }
    
    getHotkeyConfig() {
        return clone(this.hotkeyConfig);
    }
    
    updateHotkey(action, newCombo) {
        if (!action || !newCombo) return false;
        if (this.hotkeyConfig[action]) {
            this.hotkeyConfig[action].combo = newCombo;
            this.registerAllShortcuts();
            console.log(`[HotkeyManager] ✅ Updated ${action} to ${newCombo}`);
            return true;
        }
        return false;
    }
}

// Initialize
window.hotkeyManager = null;

if (document.readyState === 'loading') {
    document.addEventListener('DOMContentLoaded', () => {
        window.hotkeyManager = new HotkeyManager();
    });
} else {
    window.hotkeyManager = new HotkeyManager();
}

// Export
window.HotkeyManager = HotkeyManager;

if (typeof module !== 'undefined' && module.exports) {
    module.exports = HotkeyManager;
}

console.log('[HotkeyManager] 📦 Hotkey manager module loaded');

})(); // End IIFE

// Cleanup on page unload
window.addEventListener('beforeunload', () => {
    if (window.hotkeyManager && typeof window.hotkeyManager.destroy === 'function') {
        window.hotkeyManager.destroy();
    }
});

