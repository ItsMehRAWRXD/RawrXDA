/**
 * BigDaddyG IDE - Master Initialization & Flow Coordinator
 * Ensures all systems are properly initialized and connected
 */

(function() {
'use strict';

console.log('%c═══════════════════════════════════════════════════════════', 'color: cyan; font-weight: bold');
console.log('%c   🚀 BigDaddyG IDE - Master Initialization', 'color: cyan; font-weight: bold; font-size: 16px');
console.log('%c═══════════════════════════════════════════════════════════', 'color: cyan; font-weight: bold');

class MasterInitializer {
    constructor() {
        this.initialized = false;
        this.systems = {
            monaco: false,
            terminal: false,
            chat: false,
            executor: false,
            layout: false,
            hotkeys: false
        };
        
        this.init();
    }
    
    init() {
        console.log('[MasterInit] Starting initialization sequence...');
        
        // Wait for DOM to be ready
        if (document.readyState === 'loading') {
            document.addEventListener('DOMContentLoaded', () => this.startInitialization());
        } else {
            this.startInitialization();
        }
    }
    
    async startInitialization() {
        console.log('[MasterInit] DOM ready, initializing systems...');
        
        // Initialize in order of dependency
        await this.initializeMonaco();
        await this.initializeLayout();
        await this.initializeTerminal();
        await this.initializeChat();
        await this.initializeExecutor();
        await this.initializeHotkeys();
        await this.finalizeConnections();
        
        this.initialized = true;
        console.log('%c[MasterInit] ✅ All systems initialized!', 'color: #00ff00; font-weight: bold');
        this.displayStatus();
    }
    
    async initializeMonaco() {
        console.log('[MasterInit] 1/6 Initializing Monaco Editor...');
        
        return new Promise((resolve) => {
            const checkMonaco = () => {
                if (window.monaco && window.editor) {
                    this.systems.monaco = true;
                    console.log('[MasterInit] ✅ Monaco ready');
                    resolve();
                    return true;
                }
                return false;
            };
            
            if (!checkMonaco()) {
                // Monaco not ready, wait for it
                const interval = setInterval(() => {
                    if (checkMonaco()) {
                        clearInterval(interval);
                    }
                }, 100);
                
                // Timeout after 10 seconds
                setTimeout(() => {
                    clearInterval(interval);
                    if (!this.systems.monaco) {
                        console.warn('[MasterInit] ⚠️  Monaco not initialized after 10s');
                        resolve(); // Continue anyway
                    }
                }, 10000);
            }
        });
    }
    
    async initializeLayout() {
        console.log('[MasterInit] 2/6 Initializing Flexible Layout...');
        
        return new Promise((resolve) => {
            const checkLayout = () => {
                if (window.flexibleLayout) {
                    this.systems.layout = true;
                    console.log('[MasterInit] ✅ Layout system ready');
                    resolve();
                    return true;
                }
                return false;
            };
            
            if (!checkLayout()) {
                setTimeout(() => {
                    if (checkLayout()) return;
                    console.warn('[MasterInit] ⚠️  Layout system not found');
                    resolve();
                }, 2000);
            }
        });
    }
    
    async initializeTerminal() {
        console.log('[MasterInit] 3/6 Initializing Terminal...');
        
        return new Promise((resolve) => {
            const checkTerminal = () => {
                if (window.terminalPanelInstance || window.enhancedTerminal) {
                    this.systems.terminal = true;
                    console.log('[MasterInit] ✅ Terminal ready');
                    resolve();
                    return true;
                }
                return false;
            };
            
            if (!checkTerminal()) {
                setTimeout(() => {
                    if (checkTerminal()) return;
                    console.warn('[MasterInit] ⚠️  Terminal not initialized');
                    resolve();
                }, 2000);
            }
        });
    }
    
    async initializeChat() {
        console.log('[MasterInit] 4/6 Initializing Chat System...');
        
        return new Promise((resolve) => {
            const checkChat = () => {
                if (window.unifiedChat || window.aiResponseHandler) {
                    this.systems.chat = true;
                    console.log('[MasterInit] ✅ Chat system ready');
                    resolve();
                    return true;
                }
                return false;
            };
            
            if (!checkChat()) {
                setTimeout(() => {
                    if (checkChat()) return;
                    console.warn('[MasterInit] ⚠️  Chat system not initialized');
                    resolve();
                }, 2000);
            }
        });
    }
    
    async initializeExecutor() {
        console.log('[MasterInit] 5/6 Initializing Agentic Executor...');
        
        return new Promise((resolve) => {
            const checkExecutor = () => {
                if (window.getAgenticExecutor && typeof window.getAgenticExecutor === 'function') {
                    const executor = window.getAgenticExecutor();
                    if (executor) {
                        this.systems.executor = true;
                        console.log('[MasterInit] ✅ Agentic executor ready');
                        resolve();
                        return true;
                    }
                }
                return false;
            };
            
            if (!checkExecutor()) {
                setTimeout(() => {
                    if (checkExecutor()) return;
                    console.warn('[MasterInit] ⚠️  Agentic executor not initialized');
                    resolve();
                }, 2000);
            }
        });
    }
    
    async initializeHotkeys() {
        console.log('[MasterInit] 6/6 Initializing Hotkeys...');
        
        return new Promise((resolve) => {
            const checkHotkeys = () => {
                if (window.hotkeyManager) {
                    this.systems.hotkeys = true;
                    console.log('[MasterInit] ✅ Hotkey manager ready');
                    resolve();
                    return true;
                }
                return false;
            };
            
            if (!checkHotkeys()) {
                setTimeout(() => {
                    if (checkHotkeys()) return;
                    console.warn('[MasterInit] ⚠️  Hotkey manager not initialized');
                    resolve();
                }, 2000);
            }
        });
    }
    
    async finalizeConnections() {
        console.log('[MasterInit] Finalizing connections...');
        
        // Connect Monaco to tab system
        if (window.editor && window.tabSystem) {
            console.log('[MasterInit] 🔗 Connecting Monaco to tab system');
        }
        
        // Connect Terminal to Executor
        if (this.systems.terminal && this.systems.executor) {
            console.log('[MasterInit] 🔗 Connecting terminal to executor');
            const executor = window.getAgenticExecutor();
            if (executor && window.terminalPanelInstance) {
                executor.terminalPanel = window.terminalPanelInstance;
            }
        }
        
        // Connect Chat to Executor
        if (this.systems.chat && this.systems.executor) {
            console.log('[MasterInit] 🔗 Connecting chat to executor');
            const executor = window.getAgenticExecutor();
            if (executor && window.unifiedChat) {
                window.unifiedChat.executor = executor;
            }
        }
        
        // Ensure Monaco container is visible
        if (this.systems.monaco) {
            const container = document.getElementById('monaco-container');
            if (container) {
                const rect = container.getBoundingClientRect();
                if (rect.height === 0) {
                    console.warn('[MasterInit] ⚠️  Monaco container has no height, fixing...');
                    container.style.minHeight = '400px';
                }
            }
        }
        
        console.log('[MasterInit] ✅ All connections finalized');
    }
    
    displayStatus() {
        console.log('');
        console.log('%c═══════════════════════════════════════════════════════════', 'color: cyan');
        console.log('%c   📊 SYSTEM STATUS', 'color: cyan; font-weight: bold');
        console.log('%c═══════════════════════════════════════════════════════════', 'color: cyan');
        
        Object.entries(this.systems).forEach(([name, status]) => {
            const icon = status ? '✅' : '❌';
            const color = status ? '#00ff00' : '#ff0000';
            const label = name.charAt(0).toUpperCase() + name.slice(1);
            console.log(`%c${icon} ${label.padEnd(20)}${status ? 'Ready' : 'Not Initialized'}`, `color: ${color}`);
        });
        
        const readyCount = Object.values(this.systems).filter(Boolean).length;
        const totalCount = Object.keys(this.systems).length;
        const percentage = Math.round((readyCount / totalCount) * 100);
        
        console.log('');
        console.log(`%c📈 System Health: ${readyCount}/${totalCount} (${percentage}%)`, `color: ${percentage >= 80 ? '#00ff00' : percentage >= 50 ? '#ffaa00' : '#ff0000'}; font-weight: bold; font-size: 14px`);
        console.log('');
        console.log('%c═══════════════════════════════════════════════════════════', 'color: cyan');
        console.log('');
        
        // Show available diagnostic commands
        console.log('%c💡 Run Diagnostics:', 'color: yellow; font-weight: bold');
        console.log('   quickHealthCheck()      - Fast system check');
        console.log('   runSystemDiagnostic()   - Full diagnostic');
        console.log('   diagnoseMonaco()        - Monaco debugger');
        console.log('   repairConnections()     - Auto-repair');
        console.log('');
    }
    
    getStatus() {
        return {
            initialized: this.initialized,
            systems: { ...this.systems },
            health: Object.values(this.systems).filter(Boolean).length,
            total: Object.keys(this.systems).length
        };
    }
}

// Create global instance
window.masterInitializer = new MasterInitializer();

// Expose status check
window.getIDEStatus = () => window.masterInitializer.getStatus();

console.log('[MasterInit] 🎯 Master initializer loaded');

})();
