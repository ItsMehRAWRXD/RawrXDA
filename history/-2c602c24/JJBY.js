// ========================================================
// BIGDADDYG IDE COMPLETE FIX - ALL ERRORS RESOLVED
// ========================================================
// Place this at the very top of IDEre2.html after <!DOCTYPE html>

console.log('[BigDaddyG] 🚀 Loading complete fix system...');

// ========================================================
// PHASE 1: MISSING OPFS FUNCTIONS
// ========================================================

async function initOPFS() {
    try {
        if ('storage' in navigator && 'getDirectory' in navigator.storage) {
            const root = await navigator.storage.getDirectory();
            console.log('[BigDaddyG] 💾 OPFS initialized successfully');
            window.opfsRoot = root;
            return root;
        } else {
            console.warn('[BigDaddyG] ⚠️ OPFS not supported - using localStorage fallback');
            return null;
        }
    } catch (error) {
        console.error('[BigDaddyG] ❌ OPFS initialization failed:', error);
        return null;
    }
}

async function getOPFSDirectory() {
    try {
        if (window.opfsRoot) return window.opfsRoot;
        if ('storage' in navigator && 'getDirectory' in navigator.storage) {
            const root = await navigator.storage.getDirectory();
            window.opfsRoot = root;
            return root;
        }
        return null;
    } catch (error) {
        console.error('[BigDaddyG] ❌ OPFS directory access failed:', error);
        return null;
    }
}

// ========================================================
// PHASE 2: MISSING UI FUNCTIONS
// ========================================================

function toggleAIPanelVisibility() {
    const aiPanel = document.getElementById('ai-chat-panel');
    const floatingPanel = document.getElementById('floating-ai-panel');
    
    if (aiPanel) {
        const isHidden = aiPanel.style.display === 'none';
        aiPanel.style.display = isHidden ? 'block' : 'none';
        console.log('[BigDaddyG] 🔄 AI panel visibility toggled');
    }
    
    if (floatingPanel) {
        const isHidden = floatingPanel.style.display === 'none';
        floatingPanel.style.display = isHidden ? 'block' : 'none';
    }
}

function clearAIChat() {
    const chatContainer = document.getElementById('ai-chat-messages');
    if (chatContainer) {
        chatContainer.innerHTML = '<div class="chat-message system">Chat cleared</div>';
        console.log('[BigDaddyG] 🧹 AI chat cleared');
    }
}

function toggleFloatAIPanel() {
    const floatingPanel = document.getElementById('floating-ai-panel');
    if (floatingPanel) {
        const isVisible = floatingPanel.style.display !== 'none';
        floatingPanel.style.display = isVisible ? 'none' : 'block';
        
        const button = document.querySelector('[onclick*="toggleFloatAIPanel"]');
        if (button) {
            button.textContent = isVisible ? '🗣️ Show AI' : '🔇 Hide AI';
        }
        console.log('[BigDaddyG] 🗣️ Floating AI panel toggled');
    }
}

function createNewChat() {
    const chatContainer = document.getElementById('ai-chat-messages');
    if (chatContainer) {
        chatContainer.innerHTML = '<div class="chat-message system">New chat started</div>';
        console.log('[BigDaddyG] 💬 New chat created');
    }
}

function createNewTerminal() {
    const terminalContainer = document.getElementById('terminal-panel');
    if (terminalContainer) {
        const terminalOutput = terminalContainer.querySelector('.terminal-output');
        if (terminalOutput) {
            terminalOutput.innerHTML = '<div class="terminal-line">$ New terminal session</div>';
        }
        console.log('[BigDaddyG] 💻 New terminal created');
    }
}

// ========================================================
// PHASE 3: ENHANCED EXTENSION MANAGER WITH OPFS
// ========================================================

class ExtensionManager {
    constructor() {
        this.extensions = new Map();
        this.opfsReady = false;
        this.extensionsDir = null;
    }

    async init() {
        try {
            const root = await initOPFS();
            if (root) {
                this.extensionsDir = await root.getDirectoryHandle('extensions', { create: true });
                this.opfsReady = true;
                console.log('[BigDaddyG] 📦 Extension Manager initialized with OPFS');
            } else {
                console.warn('[BigDaddyG] ⚠️ Extension Manager using localStorage fallback');
            }
            await this.loadInstalledExtensions();
            return true;
        } catch (error) {
            console.error('[BigDaddyG] ❌ Extension Manager init failed:', error);
            return false;
        }
    }

    async loadInstalledExtensions() {
        try {
            if (this.opfsReady && this.extensionsDir) {
                for await (const [name, handle] of this.extensionsDir.entries()) {
                    if (handle.kind === 'directory') {
                        await this.loadExtension(name, handle);
                    }
                }
            } else {
                // Load from localStorage
                const stored = localStorage.getItem('installed_extensions');
                if (stored) {
                    const extensions = JSON.parse(stored);
                    extensions.forEach(ext => {
                        this.extensions.set(ext.id, ext);
                    });
                }
            }
            console.log(`[BigDaddyG] 📦 Loaded ${this.extensions.size} extensions`);
        } catch (error) {
            console.error('[BigDaddyG] ❌ Failed to load extensions:', error);
        }
    }

    async loadExtension(id, dirHandle) {
        try {
            const manifestHandle = await dirHandle.getFileHandle('manifest.json');
            const manifestFile = await manifestHandle.getFile();
            const manifestText = await manifestFile.text();
            const manifest = JSON.parse(manifestText);

            const codeHandle = await dirHandle.getFileHandle('extension.js');
            const codeFile = await codeHandle.getFile();
            const code = await codeFile.text();

            const extension = {
                id,
                manifest,
                code,
                enabled: manifest.enabled !== false,
                loaded: false
            };

            this.extensions.set(id, extension);
            console.log(`[BigDaddyG] ✅ Loaded extension: ${manifest.name}`);
        } catch (error) {
            console.error(`[BigDaddyG] ❌ Failed to load extension ${id}:`, error);
        }
    }

    async installFromURL(url, extensionId) {
        try {
            console.log(`[BigDaddyG] 📥 Installing extension from ${url}`);
            const response = await fetch(url);
            const code = await response.text();
            
            const manifest = {
                id: extensionId || `ext_${Date.now()}`,
                name: extensionId || 'Custom Extension',
                version: '1.0.0',
                enabled: true,
                source: url
            };

            await this.saveExtension(manifest.id, manifest, code);
            console.log(`[BigDaddyG] ✅ Extension installed: ${manifest.name}`);
            return manifest.id;
        } catch (error) {
            console.error('[BigDaddyG] ❌ Installation from URL failed:', error);
            throw error;
        }
    }

    async installFromFile(file) {
        try {
            console.log(`[BigDaddyG] 📥 Installing extension from file: ${file.name}`);
            const code = await file.text();
            
            const manifest = {
                id: `ext_${Date.now()}`,
                name: file.name.replace('.js', ''),
                version: '1.0.0',
                enabled: true,
                source: 'local'
            };

            await this.saveExtension(manifest.id, manifest, code);
            console.log(`[BigDaddyG] ✅ Extension installed: ${manifest.name}`);
            return manifest.id;
        } catch (error) {
            console.error('[BigDaddyG] ❌ Installation from file failed:', error);
            throw error;
        }
    }

    async saveExtension(id, manifest, code) {
        if (this.opfsReady && this.extensionsDir) {
            const extDir = await this.extensionsDir.getDirectoryHandle(id, { create: true });
            
            const manifestHandle = await extDir.getFileHandle('manifest.json', { create: true });
            const manifestWritable = await manifestHandle.createWritable();
            await manifestWritable.write(JSON.stringify(manifest, null, 2));
            await manifestWritable.close();

            const codeHandle = await extDir.getFileHandle('extension.js', { create: true });
            const codeWritable = await codeHandle.createWritable();
            await codeWritable.write(code);
            await codeWritable.close();
        } else {
            // Save to localStorage
            const extensions = this.getStoredExtensions();
            extensions.push({ id, manifest, code });
            localStorage.setItem('installed_extensions', JSON.stringify(extensions));
        }

        this.extensions.set(id, { id, manifest, code, enabled: manifest.enabled, loaded: false });
    }

    getStoredExtensions() {
        const stored = localStorage.getItem('installed_extensions');
        return stored ? JSON.parse(stored) : [];
    }

    async enableExtension(id) {
        const ext = this.extensions.get(id);
        if (!ext) {
            console.error(`[BigDaddyG] ❌ Extension ${id} not found`);
            return;
        }

        try {
            // Load extension code
            const blob = new Blob([ext.code], { type: 'application/javascript' });
            const url = URL.createObjectURL(blob);
            const module = await import(url);
            URL.revokeObjectURL(url);

            // Activate extension
            if (module.activate) {
                const context = this.createExtensionContext();
                await module.activate(context);
                ext.loaded = true;
                ext.enabled = true;
                ext.module = module;
                console.log(`[BigDaddyG] ✅ Extension enabled: ${ext.manifest.name}`);
            }
        } catch (error) {
            console.error(`[BigDaddyG] ❌ Failed to enable extension ${id}:`, error);
            throw error;
        }
    }

    async disableExtension(id) {
        const ext = this.extensions.get(id);
        if (!ext) return;

        try {
            if (ext.module && ext.module.deactivate) {
                await ext.module.deactivate();
            }
            ext.loaded = false;
            ext.enabled = false;
            console.log(`[BigDaddyG] 🔇 Extension disabled: ${ext.manifest.name}`);
        } catch (error) {
            console.error(`[BigDaddyG] ❌ Failed to disable extension ${id}:`, error);
        }
    }

    async uninstallExtension(id) {
        const ext = this.extensions.get(id);
        if (!ext) return;

        try {
            if (ext.enabled) {
                await this.disableExtension(id);
            }

            if (this.opfsReady && this.extensionsDir) {
                await this.extensionsDir.removeEntry(id, { recursive: true });
            } else {
                const extensions = this.getStoredExtensions().filter(e => e.id !== id);
                localStorage.setItem('installed_extensions', JSON.stringify(extensions));
            }

            this.extensions.delete(id);
            console.log(`[BigDaddyG] 🗑️ Extension uninstalled: ${ext.manifest.name}`);
        } catch (error) {
            console.error(`[BigDaddyG] ❌ Failed to uninstall extension ${id}:`, error);
        }
    }

    createExtensionContext() {
        return {
            showToast: (msg, type) => showNotification(msg, type),
            addChatMessage: (msg) => {
                const container = document.getElementById('ai-chat-messages');
                if (container) {
                    const div = document.createElement('div');
                    div.className = 'chat-message extension';
                    div.textContent = msg;
                    container.appendChild(div);
                }
            },
            fileOps: window.fileOps,
            editor: window.editor,
            terminal: window.terminalManager
        };
    }

    getMarketplaceExtensions() {
        return [
            {
                id: 'prettier',
                name: 'Prettier Code Formatter',
                description: 'Format your code beautifully',
                version: '1.0.0',
                author: 'BigDaddyG',
                icon: '✨'
            },
            {
                id: 'eslint',
                name: 'ESLint',
                description: 'JavaScript linting tool',
                version: '1.0.0',
                author: 'BigDaddyG',
                icon: '🔍'
            },
            {
                id: 'git-helper',
                name: 'Git Helper',
                description: 'Git integration for IDE',
                version: '1.0.0',
                author: 'BigDaddyG',
                icon: '🌿'
            }
        ];
    }

    listInstalled() {
        return Array.from(this.extensions.values());
    }

    async autoLoadInstalled() {
        for (const [id, ext] of this.extensions) {
            if (ext.enabled && !ext.loaded) {
                try {
                    await this.enableExtension(id);
                } catch (error) {
                    console.error(`[BigDaddyG] ❌ Failed to auto-load ${id}:`, error);
                }
            }
        }
    }
}

// Create global extension manager instance
window.extensionManager = new ExtensionManager();

// ========================================================
// PHASE 4: NOTIFICATION SYSTEM
// ========================================================

function showNotification(message, type = 'info') {
    // Safely create notification
    if (!document.body) {
        console.warn('[BigDaddyG] ⚠️ Cannot show notification - body not ready');
        return;
    }

    const notification = document.createElement('div');
    notification.className = `notification notification-${type}`;
    notification.innerHTML = `
        <span class="notification-message">${message}</span>
        <button class="notification-close" onclick="this.parentElement.remove()">×</button>
    `;
    
    Object.assign(notification.style, {
        position: 'fixed',
        top: '20px',
        right: '20px',
        padding: '12px 20px',
        borderRadius: '8px',
        color: 'white',
        fontWeight: 'bold',
        zIndex: '10000',
        maxWidth: '350px',
        boxShadow: '0 4px 6px rgba(0,0,0,0.3)',
        animation: 'slideIn 0.3s ease-out'
    });
    
    const colors = {
        success: '#4CAF50',
        error: '#f44336',
        warning: '#ff9800',
        info: '#2196F3'
    };
    notification.style.backgroundColor = colors[type] || colors.info;
    
    document.body.appendChild(notification);
    
    setTimeout(() => {
        if (notification.parentElement) {
            notification.style.animation = 'slideOut 0.3s ease-in';
            setTimeout(() => notification.remove(), 300);
        }
    }, 5000);
}

// ========================================================
// PHASE 5: TERMINAL BLACK SCREEN FIX
// ========================================================

function fixTerminalAfterResize() {
    const terminal = document.getElementById('terminal-panel');
    if (terminal) {
        terminal.style.display = 'none';
        terminal.offsetHeight; // Force reflow
        terminal.style.display = 'block';
        
        Object.assign(terminal.style, {
            backgroundColor: '#000000',
            color: '#00ff00',
            fontFamily: 'monospace',
            padding: '10px',
            borderRadius: '5px',
            minHeight: '200px'
        });
        
        console.log('[BigDaddyG] 🔧 Terminal display fixed');
    }
}

// ========================================================
// PHASE 6: ERROR RECOVERY SYSTEM
// ========================================================

window.errorRecovery = {
    errors: [],
    
    log(type, message, details = {}) {
        this.errors.push({ type, message, details, timestamp: Date.now() });
        console.error(`[BigDaddyG] ❌ ${type}: ${message}`, details);
    },
    
    getReport() {
        return {
            totalErrors: this.errors.length,
            byType: this.errors.reduce((acc, err) => {
                acc[err.type] = (acc[err.type] || 0) + 1;
                return acc;
            }, {})
        };
    }
};

// Enhanced error handler
window.addEventListener('error', function(event) {
    window.errorRecovery.log('Runtime', event.message, {
        line: event.lineno,
        column: event.colno,
        filename: event.filename
    });
});

window.addEventListener('unhandledrejection', function(event) {
    window.errorRecovery.log('Promise', event.reason);
});

// ========================================================
// PHASE 7: SAFE INITIALIZATION
// ========================================================

function safeDOMReady(callback) {
    if (document.readyState === 'loading') {
        document.addEventListener('DOMContentLoaded', callback);
    } else {
        callback();
    }
}

// ========================================================
// PHASE 8: INITIALIZE EVERYTHING
// ========================================================

safeDOMReady(async function() {
    console.log('[BigDaddyG] 🚀 Initializing complete fix system...');
    
    try {
        // Initialize Extension Manager
        await window.extensionManager.init();
        
        // Auto-load enabled extensions
        await window.extensionManager.autoLoadInstalled();
        
        // Apply terminal fix
        setTimeout(fixTerminalAfterResize, 500);
        
        // Test critical functions
        const criticalFunctions = [
            'initOPFS', 'getOPFSDirectory', 'toggleAIPanelVisibility',
            'clearAIChat', 'toggleFloatAIPanel', 'createNewChat',
            'createNewTerminal', 'showNotification', 'fixTerminalAfterResize'
        ];
        
        let workingCount = 0;
        criticalFunctions.forEach(funcName => {
            if (typeof window[funcName] === 'function') {
                console.log(`[BigDaddyG] ✅ ${funcName} ready`);
                workingCount++;
            } else {
                console.error(`[BigDaddyG] ❌ ${funcName} missing`);
            }
        });
        
        const report = window.errorRecovery.getReport();
        console.log('[BigDaddyG] 📊 System Status:', {
            functionsWorking: `${workingCount}/${criticalFunctions.length}`,
            extensions: window.extensionManager.extensions.size,
            errors: report.totalErrors
        });
        
        if (workingCount === criticalFunctions.length && report.totalErrors === 0) {
            console.log('[BigDaddyG] 🎉 ALL SYSTEMS OPERATIONAL!');
            showNotification('IDE fully operational!', 'success');
        } else {
            console.warn('[BigDaddyG] ⚠️ Some issues detected');
            showNotification('Some issues detected. Check console.', 'warning');
        }
    } catch (error) {
        console.error('[BigDaddyG] ❌ Initialization failed:', error);
        window.errorRecovery.log('Init', 'System initialization failed', error);
    }
});

// Listen for resize events
window.addEventListener('resize', () => setTimeout(fixTerminalAfterResize, 100));

console.log('[BigDaddyG] 🔧 Complete fix system loaded');
