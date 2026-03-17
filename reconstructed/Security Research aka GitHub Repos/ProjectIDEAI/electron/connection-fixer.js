/**
 * BigDaddyG IDE - Connection Fixer & Auto-Wiring
 * Ensures all systems are properly connected and working
 */

(function() {
'use strict';

class ConnectionFixer {
    constructor() {
        this.fixes = [];
        this.warnings = [];
        
        console.log('[ConnectionFixer] 🔧 Starting auto-wiring system...');
        this.init();
    }
    
    async init() {
        // Wait for DOM to be ready
        if (document.readyState === 'loading') {
            await new Promise(resolve => {
                document.addEventListener('DOMContentLoaded', resolve);
            });
        }
        
        // Wait a bit for all scripts to initialize
        await this.delay(2000);
        
        // Run all fixes
        this.fixBrowserSystems();
        this.fixTerminalSystems();
        this.fixEditorSystems();
        this.fixAISystems();
        this.fixUISystems();
        this.fixHotkeys();
        
        // Report results
        this.reportResults();
        
        // Expose globally
        window.connectionFixer = this;
    }
    
    delay(ms) {
        return new Promise(resolve => setTimeout(resolve, ms));
    }
    
    fixBrowserSystems() {
        console.log('[ConnectionFixer] 🌐 Fixing browser systems...');
        
        // Ensure browser instances are created
        if (!window.webBrowser && window.WebBrowser) {
            window.webBrowser = new window.WebBrowser();
            this.log('Created webBrowser instance');
        }
        
        if (!window.browserPanel && window.BrowserPanel) {
            window.browserPanel = new window.BrowserPanel();
            this.log('Created browserPanel instance');
        }
        
        // Wire browser button in sidebar
        const browserBtn = document.querySelector('.sidebar-quick-button.browser');
        if (browserBtn) {
            browserBtn.onclick = () => {
                if (window.browserPanel) {
                    window.browserPanel.show();
                } else if (window.webBrowser) {
                    window.webBrowser.openBrowser();
                } else {
                    this.warn('No browser system available');
                }
            };
            this.log('Wired sidebar browser button');
        }
        
        // Ensure toggle functions exist
        if (!window.toggleBrowser) {
            window.toggleBrowser = () => {
                if (window.browserPanel && typeof window.browserPanel.toggle === 'function') {
                    window.browserPanel.toggle();
                } else if (window.webBrowser && typeof window.webBrowser.toggleBrowser === 'function') {
                    window.webBrowser.toggleBrowser();
                } else {
                    console.warn('[ConnectionFixer] No browser toggle available');
                }
            };
            this.log('Created window.toggleBrowser()');
        }
    }
    
    fixTerminalSystems() {
        console.log('[ConnectionFixer] 💻 Fixing terminal systems...');
        
        // Ensure terminal panel exists
        if (!window.terminalPanelInstance && window.TerminalPanel) {
            window.terminalPanelInstance = new window.TerminalPanel();
            this.log('Created terminalPanelInstance');
        }
        
        // Wire terminal toggle if missing
        if (!window.toggleTerminalPanel) {
            window.toggleTerminalPanel = () => {
                if (window.terminalPanelInstance && typeof window.terminalPanelInstance.toggle === 'function') {
                    window.terminalPanelInstance.toggle();
                } else if (window.enhancedTerminal && typeof window.enhancedTerminal.toggle === 'function') {
                    window.enhancedTerminal.toggle();
                } else {
                    // Fallback to bottom panel
                    const bottomPanel = document.getElementById('bottom-panel');
                    if (bottomPanel) {
                        bottomPanel.style.display = bottomPanel.style.display === 'none' ? 'flex' : 'none';
                    }
                }
            };
            this.log('Created window.toggleTerminalPanel()');
        }
        
        // Ensure console panel is wired to status manager
        if (window.consolePanelInstance && window.statusManager) {
            if (!window.consolePanelInstance.statusUnsubscribe) {
                window.consolePanelInstance.statusUnsubscribe = window.statusManager.subscribe('orchestra', 
                    (running, data) => {
                        if (window.consolePanelInstance.setOrchestraRunning) {
                            window.consolePanelInstance.setOrchestraRunning(running, data);
                        }
                    });
                this.log('Connected console panel to status manager');
            }
        }
    }
    
    fixEditorSystems() {
        console.log('[ConnectionFixer] 📝 Fixing editor systems...');
        
        // Ensure tab system is initialized
        if (!window.tabSystem && window.TabSystem) {
            window.tabSystem = new window.TabSystem();
            this.log('Created tabSystem instance');
        }
        
        // Wire file explorer to editor
        if (window.enhancedFileExplorer && window.editor) {
            if (typeof window.enhancedFileExplorer.setEditor === 'function') {
                window.enhancedFileExplorer.setEditor(window.editor);
                this.log('Connected file explorer to editor');
            }
        }
        
        // Ensure agentic browser is initialized
        if (!window.agenticBrowser && window.AgenticFileBrowser) {
            try {
                window.agenticBrowser = new window.AgenticFileBrowser();
                this.log('Created agenticBrowser instance');
            } catch (error) {
                this.warn('Could not create agenticBrowser: ' + error.message);
            }
        }
    }
    
    fixAISystems() {
        console.log('[ConnectionFixer] 🤖 Fixing AI systems...');
        
        // Ensure command system is initialized
        if (!window.commandSystem && window.CommandSystem) {
            window.commandSystem = new window.CommandSystem();
            if (typeof window.commandSystem.init === 'function') {
                window.commandSystem.init();
            }
            this.log('Created commandSystem instance');
        }
        
        // Wire AI response handler
        if (!window.aiResponseHandler && window.AIResponseHandler) {
            window.aiResponseHandler = new window.AIResponseHandler();
            this.log('Created aiResponseHandler instance');
        }
        
        // Ensure unified chat is initialized
        if (!window.unifiedChat && window.UnifiedChatHandler) {
            window.unifiedChat = new window.UnifiedChatHandler();
            this.log('Created unifiedChat instance');
        }
        
        // Ensure context menu executor is initialized
        if (!window.contextMenuExecutor && window.ContextMenuExecutor) {
            window.contextMenuExecutor = new window.ContextMenuExecutor();
            this.log('Created contextMenuExecutor instance');
        }
        
        // Wire sendToAI function
        if (!window.sendToAI) {
            window.sendToAI = () => {
                if (window.unifiedChat && typeof window.unifiedChat.handleSend === 'function') {
                    window.unifiedChat.handleSend(window.unifiedChat.primaryInput || 'sidebar-chat');
                } else if (window.aiResponseHandler && typeof window.aiResponseHandler.sendMessage === 'function') {
                    const input = document.getElementById('ai-input');
                    if (input && input.value.trim()) {
                        window.aiResponseHandler.sendMessage(input.value.trim());
                        input.value = '';
                    }
                } else {
                    this.warn('No AI handler available');
                }
            };
            this.log('Created window.sendToAI()');
        }
    }
    
    fixUISystems() {
        console.log('[ConnectionFixer] 🎨 Fixing UI systems...');
        
        // Ensure marketplace is initialized
        if (!window.pluginMarketplace && window.PluginMarketplace) {
            window.pluginMarketplace = new window.PluginMarketplace();
            this.log('Created pluginMarketplace instance');
        }
        
        // Wire marketplace functions
        if (!window.openMarketplace) {
            window.openMarketplace = () => {
                if (window.pluginMarketplace && typeof window.pluginMarketplace.open === 'function') {
                    window.pluginMarketplace.open();
                } else {
                    this.warn('Plugin marketplace not available');
                }
            };
            this.log('Created window.openMarketplace()');
        }
        
        if (!window.openModelCatalog) {
            window.openModelCatalog = () => {
                if (window.pluginMarketplace) {
                    if (typeof window.pluginMarketplace.showOllamaManager === 'function') {
                        window.pluginMarketplace.showOllamaManager();
                    }
                    if (typeof window.pluginMarketplace.open === 'function') {
                        window.pluginMarketplace.open();
                    }
                } else {
                    this.warn('Plugin marketplace not available');
                }
            };
            this.log('Created window.openModelCatalog()');
        }
        
        // Wire floating chat
        if (!window.floatingChat && window.FloatingChat) {
            window.floatingChat = new window.FloatingChat();
            this.log('Created floatingChat instance');
        }
        
        // Wire command palette
        if (!window.commandPalette && window.CommandPalette) {
            window.commandPalette = new window.CommandPalette();
            this.log('Created commandPalette instance');
        }
    }
    
    fixHotkeys() {
        console.log('[ConnectionFixer] ⌨️  Fixing hotkeys...');
        
        // Ensure hotkey manager is initialized
        if (!window.hotkeyManager && window.HotkeyManager) {
            window.hotkeyManager = new window.HotkeyManager();
            if (typeof window.hotkeyManager.init === 'function') {
                window.hotkeyManager.init();
            }
            this.log('Created hotkeyManager instance');
        }
        
        // Verify critical hotkeys are bound (using actual key names from DEFAULT_HOTKEYS)
        if (window.hotkeyManager) {
            const criticalHotkeys = [
                'terminal.toggle',      // Ctrl+J
                'terminal.altToggle',   // Ctrl+`
                'browser.toggle',       // Ctrl+Shift+B
                'layout.customize',     // Ctrl+Shift+L
                'layout.reset'          // Ctrl+Alt+L
            ];
            
            criticalHotkeys.forEach(key => {
                // Check if hotkey is defined in the hotkeys object (not hotkeyConfig)
                if (window.hotkeyManager.hotkeys && !window.hotkeyManager.hotkeys[key]) {
                    this.warn(`Hotkey not configured: ${key}`);
                }
            });
        }
    }
    
    log(message) {
        this.fixes.push(message);
        console.log(`  ✅ ${message}`);
    }
    
    warn(message) {
        this.warnings.push(message);
        console.warn(`  ⚠️  ${message}`);
    }
    
    reportResults() {
        console.log('\n[ConnectionFixer] 📊 Auto-Wiring Results:');
        console.log(`  ✅ ${this.fixes.length} connections fixed`);
        console.log(`  ⚠️  ${this.warnings.length} warnings`);
        
        if (this.fixes.length > 0) {
            console.log('\n[ConnectionFixer] 🔧 Fixes Applied:');
            this.fixes.forEach((fix, i) => {
                console.log(`  ${i + 1}. ${fix}`);
            });
        }
        
        if (this.warnings.length > 0) {
            console.log('\n[ConnectionFixer] ⚠️  Warnings:');
            this.warnings.forEach((warning, i) => {
                console.log(`  ${i + 1}. ${warning}`);
            });
        }
        
        console.log('\n[ConnectionFixer] ✅ Auto-wiring complete!');
    }
    
    // Manual repair function
    repairAll() {
        console.log('[ConnectionFixer] 🔧 Running manual repair...');
        this.fixes = [];
        this.warnings = [];
        
        this.fixBrowserSystems();
        this.fixTerminalSystems();
        this.fixEditorSystems();
        this.fixAISystems();
        this.fixUISystems();
        this.fixHotkeys();
        
        this.reportResults();
    }
}

// Initialize
if (document.readyState === 'loading') {
    document.addEventListener('DOMContentLoaded', () => {
        new ConnectionFixer();
    });
} else {
    new ConnectionFixer();
}

// Expose repair function
window.repairConnections = function() {
    if (window.connectionFixer) {
        window.connectionFixer.repairAll();
    } else {
        new ConnectionFixer();
    }
};

console.log('[ConnectionFixer] 🔧 Connection fixer loaded');
console.log('[ConnectionFixer] 💡 Run: repairConnections() to manually fix all connections');

})();
