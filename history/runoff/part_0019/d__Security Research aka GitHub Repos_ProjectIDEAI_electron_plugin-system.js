/**
 * BigDaddyG IDE - Plugin System
 * Extensible architecture for adding features without modifying core
 */

(function() {
'use strict';

class PluginSystem {
    constructor() {
        this.plugins = new Map();
        this.hooks = new Map();
        this.apis = new Map();
        this.pluginDir = null;
        this.initialized = false;
        
        // Plugin API version
        this.API_VERSION = '1.0.0';
        
        console.log('[PluginSystem] 🔌 Initializing plugin system...');
    }
    
    /**
     * Initialize the plugin system
     */
    async init() {
        if (this.initialized) return;
        
        try {
            // Setup plugin directory
            if (window.electron && window.electron.getPluginDir) {
                this.pluginDir = await window.electron.getPluginDir();
                console.log(`[PluginSystem] 📁 Plugin directory: ${this.pluginDir}`);
            }
            
            // Register core APIs
            this.registerCoreAPIs();
            
            // Register core hooks
            this.registerCoreHooks();
            
            // Load installed plugins
            await this.loadAllPlugins();
            
            this.initialized = true;
            console.log('[PluginSystem] ✅ Plugin system initialized');
            
            // Trigger hook
            await this.trigger('pluginSystem:ready');
            
        } catch (error) {
            console.error('[PluginSystem] ❌ Failed to initialize:', error);
        }
    }
    
    /**
     * Register core APIs that plugins can use
     */
    registerCoreAPIs() {
        // Editor API
        this.apis.set('editor', {
            getActiveEditor: () => window.editor,
            getValue: () => window.editor?.getValue() || '',
            setValue: (value) => window.editor?.setValue(value),
            getSelection: () => {
                const selection = window.editor?.getSelection();
                return window.editor?.getModel()?.getValueInRange(selection) || '';
            },
            replaceSelection: (text) => {
                const selection = window.editor?.getSelection();
                if (selection) {
                    window.editor?.executeEdits('plugin', [{
                        range: selection,
                        text: text
                    }]);
                }
            },
            insertText: (text) => {
                const position = window.editor?.getPosition();
                if (position) {
                    window.editor?.executeEdits('plugin', [{
                        range: new monaco.Range(position.lineNumber, position.column, position.lineNumber, position.column),
                        text: text
                    }]);
                }
            },
            getCursorPosition: () => window.editor?.getPosition(),
            setCursorPosition: (line, column) => {
                window.editor?.setPosition({ lineNumber: line, column: column });
            }
        });
        
        // File System API
        this.apis.set('fs', {
            readFile: async (path) => {
                if (window.electron && window.electron.readFile) {
                    return await window.electron.readFile(path);
                }
                throw new Error('File system not available');
            },
            writeFile: async (path, content) => {
                if (window.electron && window.electron.writeFile) {
                    return await window.electron.writeFile(path, content);
                }
                throw new Error('File system not available');
            },
            readDir: async (path) => {
                if (window.electron && window.electron.readDir) {
                    return await window.electron.readDir(path);
                }
                throw new Error('File system not available');
            },
            exists: async (path) => {
                if (window.electron && window.electron.fileExists) {
                    return await window.electron.fileExists(path);
                }
                throw new Error('File system not available');
            }
        });
        
        // UI API
        this.apis.set('ui', {
            showNotification: (message, type = 'info') => {
                this.showNotification(message, type);
            },
            addMenuItem: (menu, label, callback) => {
                this.addMenuItem(menu, label, callback);
            },
            addPanel: (id, title, content) => {
                this.addPanel(id, title, content);
            },
            addStatusBarItem: (id, content) => {
                this.addStatusBarItem(id, content);
            },
            showDialog: (title, message, buttons) => {
                return this.showDialog(title, message, buttons);
            }
        });
        
        // AI API
        this.apis.set('ai', {
            sendMessage: async (message, options = {}) => {
                try {
                    const response = await fetch('http://localhost:11441/api/chat', {
                        method: 'POST',
                        headers: { 'Content-Type': 'application/json' },
                        body: JSON.stringify({
                            message,
                            model: options.model || 'BigDaddyG:Latest',
                            parameters: options.parameters || {}
                        })
                    });
                    
                    if (!response.ok) {
                        throw new Error(`Server returned ${response.status}`);
                    }
                    
                    const data = await response.json();
                    return data.response || data.message;
                } catch (error) {
                    console.error('[PluginSystem] AI API error:', error);
                    throw error;
                }
            },
            analyzeCode: async (code, prompt) => {
                const fullPrompt = `${prompt}\n\nCode to analyze:\n\`\`\`\n${code}\n\`\`\``;
                return await this.apis.get('ai').sendMessage(fullPrompt);
            },
            generateCode: async (description, language = 'javascript') => {
                const prompt = `Generate ${language} code for: ${description}`;
                return await this.apis.get('ai').sendMessage(prompt);
            }
        });
        
        // HTTP API (for web plugins)
        this.apis.set('http', {
            get: async (url, options = {}) => {
                const response = await fetch(url, { method: 'GET', ...options });
                return await response.json();
            },
            post: async (url, data, options = {}) => {
                const response = await fetch(url, {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/json', ...options.headers },
                    body: JSON.stringify(data),
                    ...options
                });
                return await response.json();
            }
        });
        
        console.log('[PluginSystem] ✅ Core APIs registered');
    }
    
    /**
     * Register core hooks
     */
    registerCoreHooks() {
        // File hooks
        this.registerHook('file:open');
        this.registerHook('file:save');
        this.registerHook('file:close');
        this.registerHook('file:change');
        
        // Editor hooks
        this.registerHook('editor:selection:change');
        this.registerHook('editor:cursor:change');
        this.registerHook('editor:content:change');
        
        // AI hooks
        this.registerHook('ai:message:sent');
        this.registerHook('ai:message:received');
        
        // Plugin hooks
        this.registerHook('plugin:loaded');
        this.registerHook('plugin:unloaded');
        this.registerHook('pluginSystem:ready');
        
        console.log('[PluginSystem] ✅ Core hooks registered');
    }
    
    /**
     * Register a hook
     */
    registerHook(hookName) {
        if (!this.hooks.has(hookName)) {
            this.hooks.set(hookName, new Set());
        }
    }
    
    /**
     * Subscribe to a hook
     */
    on(hookName, callback) {
        if (!this.hooks.has(hookName)) {
            this.registerHook(hookName);
        }
        this.hooks.get(hookName).add(callback);
        
        return () => {
            this.hooks.get(hookName).delete(callback);
        };
    }
    
    /**
     * Trigger a hook
     */
    async trigger(hookName, data = {}) {
        if (!this.hooks.has(hookName)) {
            return;
        }
        
        const callbacks = this.hooks.get(hookName);
        const results = [];
        
        for (const callback of callbacks) {
            try {
                const result = await callback(data);
                results.push(result);
            } catch (error) {
                console.error(`[PluginSystem] Error in hook ${hookName}:`, error);
            }
        }
        
        return results;
    }
    
    /**
     * Load a plugin from manifest
     */
    async loadPlugin(manifest) {
        try {
            const pluginId = manifest.id;
            
            if (this.plugins.has(pluginId)) {
                console.warn(`[PluginSystem] Plugin ${pluginId} already loaded`);
                return;
            }
            
            // Validate manifest
            if (!this.validateManifest(manifest)) {
                throw new Error('Invalid plugin manifest');
            }
            
            // Create plugin instance
            const plugin = {
                id: pluginId,
                name: manifest.name,
                version: manifest.version,
                author: manifest.author,
                description: manifest.description,
                manifest: manifest,
                enabled: true,
                apis: {},
                unsubscribers: []
            };
            
            // Provide APIs to plugin
            plugin.apis = this.getPluginAPIs(plugin);
            
            // Load plugin code
            if (manifest.main) {
                await this.loadPluginCode(plugin, manifest.main);
            }
            
            // Register plugin
            this.plugins.set(pluginId, plugin);
            
            console.log(`[PluginSystem] ✅ Loaded plugin: ${manifest.name} v${manifest.version}`);
            
            // Trigger hook
            await this.trigger('plugin:loaded', { plugin });
            
            return plugin;
            
        } catch (error) {
            console.error('[PluginSystem] Failed to load plugin:', error);
            throw error;
        }
    }
    
    /**
     * Validate plugin manifest
     */
    validateManifest(manifest) {
        const required = ['id', 'name', 'version', 'author'];
        
        for (const field of required) {
            if (!manifest[field]) {
                console.error(`[PluginSystem] Missing required field: ${field}`);
                return false;
            }
        }
        
        // Check API version compatibility
        if (manifest.apiVersion && manifest.apiVersion !== this.API_VERSION) {
            console.warn(`[PluginSystem] API version mismatch: ${manifest.apiVersion} vs ${this.API_VERSION}`);
        }
        
        return true;
    }
    
    /**
     * Get APIs available to plugin
     */
    getPluginAPIs(plugin) {
        const apis = {};
        
        // Expose all registered APIs
        for (const [name, api] of this.apis.entries()) {
            apis[name] = api;
        }
        
        // Add plugin-specific APIs
        apis.plugin = {
            id: plugin.id,
            name: plugin.name,
            version: plugin.version,
            on: (hookName, callback) => {
                const unsubscribe = this.on(hookName, callback);
                plugin.unsubscribers.push(unsubscribe);
                return unsubscribe;
            },
            trigger: (hookName, data) => {
                return this.trigger(hookName, data);
            },
            getConfig: (key, defaultValue) => {
                return this.getPluginConfig(plugin.id, key, defaultValue);
            },
            setConfig: (key, value) => {
                return this.setPluginConfig(plugin.id, key, value);
            }
        };
        
        return apis;
    }
    
    /**
     * Load plugin code
     */
    async loadPluginCode(plugin, mainFile) {
        try {
            // For now, plugins must be manually added to index.html
            // In future, we can use dynamic import()
            console.log(`[PluginSystem] Loading plugin code from: ${mainFile}`);
            
            // Plugin code should call window.pluginSystem.registerPluginCode()
            
        } catch (error) {
            console.error('[PluginSystem] Failed to load plugin code:', error);
            throw error;
        }
    }
    
    /**
     * Register plugin code (called by plugin)
     */
    registerPluginCode(pluginId, activateFn) {
        const plugin = this.plugins.get(pluginId);
        
        if (!plugin) {
            console.error(`[PluginSystem] Plugin ${pluginId} not found`);
            return;
        }
        
        try {
            // Call plugin's activate function
            activateFn(plugin.apis);
            
            console.log(`[PluginSystem] ✅ Activated plugin: ${plugin.name}`);
            
        } catch (error) {
            console.error(`[PluginSystem] Failed to activate plugin ${pluginId}:`, error);
        }
    }
    
    /**
     * Unload a plugin
     */
    async unloadPlugin(pluginId) {
        const plugin = this.plugins.get(pluginId);
        
        if (!plugin) {
            console.warn(`[PluginSystem] Plugin ${pluginId} not found`);
            return;
        }
        
        try {
            // Unsubscribe from all hooks
            for (const unsubscribe of plugin.unsubscribers) {
                unsubscribe();
            }
            
            // Remove plugin
            this.plugins.delete(pluginId);
            
            console.log(`[PluginSystem] ✅ Unloaded plugin: ${plugin.name}`);
            
            // Trigger hook
            await this.trigger('plugin:unloaded', { plugin });
            
        } catch (error) {
            console.error('[PluginSystem] Failed to unload plugin:', error);
        }
    }
    
    /**
     * Load all plugins
     */
    async loadAllPlugins() {
        try {
            // For now, load sample plugins
            // In future, scan plugin directory
            
            console.log('[PluginSystem] Loading installed plugins...');
            
            // Load sample plugin
            await this.loadSamplePlugins();
            
        } catch (error) {
            console.error('[PluginSystem] Failed to load plugins:', error);
        }
    }
    
    /**
     * Load sample plugins for demonstration
     */
    async loadSamplePlugins() {
        // Sample plugin: Code Statistics
        await this.loadPlugin({
            id: 'code-stats',
            name: 'Code Statistics',
            version: '1.0.0',
            author: 'BigDaddyG Team',
            description: 'Show code statistics (lines, characters, words)',
            apiVersion: '1.0.0',
            main: 'plugins/code-stats.js'
        });
        
        // Sample plugin: Web Search (coming soon)
        await this.loadPlugin({
            id: 'web-search',
            name: 'Web Search',
            version: '1.0.0',
            author: 'BigDaddyG Team',
            description: 'Search Stack Overflow, MDN, GitHub from IDE',
            apiVersion: '1.0.0',
            main: 'plugins/web-search.js',
            enabled: false // Not yet implemented
        });
        
        // Sample plugin: Package Manager (coming soon)
        await this.loadPlugin({
            id: 'package-manager',
            name: 'Package Manager',
            version: '1.0.0',
            author: 'BigDaddyG Team',
            description: 'Search and install npm/PyPI packages',
            apiVersion: '1.0.0',
            main: 'plugins/package-manager.js',
            enabled: false // Not yet implemented
        });
    }
    
    /**
     * Get plugin config
     */
    getPluginConfig(pluginId, key, defaultValue) {
        try {
            const config = localStorage.getItem(`plugin:${pluginId}:${key}`);
            return config ? JSON.parse(config) : defaultValue;
        } catch {
            return defaultValue;
        }
    }
    
    /**
     * Set plugin config
     */
    setPluginConfig(pluginId, key, value) {
        try {
            localStorage.setItem(`plugin:${pluginId}:${key}`, JSON.stringify(value));
        } catch (error) {
            console.error('[PluginSystem] Failed to set config:', error);
        }
    }
    
    /**
     * Helper: Show notification
     */
    showNotification(message, type = 'info') {
        console.log(`[Plugin Notification] ${type.toUpperCase()}: ${message}`);
        
        // Create notification element
        const notification = document.createElement('div');
        notification.style.cssText = `
            position: fixed;
            top: 20px;
            right: 20px;
            background: ${type === 'error' ? '#ff4757' : type === 'success' ? '#77ddbe' : '#4a90e2'};
            color: white;
            padding: 12px 20px;
            border-radius: 8px;
            box-shadow: 0 4px 12px rgba(0,0,0,0.3);
            z-index: 20000;
            font-size: 13px;
            font-weight: 600;
            animation: slideInRight 0.3s ease-out;
        `;
        notification.textContent = message;
        document.body.appendChild(notification);
        
        setTimeout(() => notification.remove(), 3000);
    }
    
    /**
     * Helper: Add menu item
     */
    addMenuItem(menu, label, callback) {
        console.log(`[PluginSystem] Adding menu item: ${menu} → ${label}`);
        
        // Implement menu system
        if (!window.customMenuItems) {
            window.customMenuItems = {};
        }
        
        if (!window.customMenuItems[menu]) {
            window.customMenuItems[menu] = [];
        }
        
        window.customMenuItems[menu].push({
            label,
            callback
        });
        
        // Trigger menu rebuild if menu system exists
        if (window.rebuildMenus) {
            window.rebuildMenus();
        }
    }
    
    /**
     * Helper: Add panel
     */
    addPanel(id, title, content) {
        console.log(`[PluginSystem] Adding panel: ${id} (${title})`);
        
        // Implement panel system
        const container = document.getElementById('center-tabs-content') || document.body;
        
        const panel = document.createElement('div');
        panel.id = `plugin-panel-${id}`;
        panel.className = 'tab-content-panel';
        panel.style.cssText = `
            display: none;
            flex-direction: column;
            height: 100%;
            padding: 20px;
            overflow-y: auto;
        `;
        
        panel.innerHTML = content;
        container.appendChild(panel);
        
        // Add tab if tab system exists
        if (window.addCenterTab) {
            window.addCenterTab(title, () => {
                document.querySelectorAll('.tab-content-panel').forEach(p => p.style.display = 'none');
                panel.style.display = 'flex';
            });
        }
        
        return panel;
    }
    
    /**
     * Helper: Add status bar item
     */
    addStatusBarItem(id, content) {
        console.log(`[PluginSystem] Adding status bar item: ${id}`);
        
        // Implement status bar system
        let statusBar = document.getElementById('status-bar');
        
        if (!statusBar) {
            // Create status bar if it doesn't exist
            statusBar = document.createElement('div');
            statusBar.id = 'status-bar';
            statusBar.style.cssText = `
                position: fixed;
                bottom: 0;
                left: 0;
                right: 0;
                height: 24px;
                background: rgba(0, 0, 0, 0.8);
                border-top: 1px solid rgba(0, 212, 255, 0.2);
                display: flex;
                align-items: center;
                padding: 0 12px;
                gap: 16px;
                font-size: 11px;
                color: #fff;
                z-index: 100;
            `;
            document.body.appendChild(statusBar);
        }
        
        const item = document.createElement('div');
        item.id = `status-item-${id}`;
        item.innerHTML = content;
        item.style.cssText = `
            display: flex;
            align-items: center;
            gap: 6px;
        `;
        
        statusBar.appendChild(item);
        return item;
    }
    
    /**
     * Helper: Show dialog
     */
    async showDialog(title, message, buttons = ['OK']) {
        console.log(`[PluginSystem] Showing dialog: ${title}`);
        
        return new Promise((resolve) => {
            const modal = document.createElement('div');
            modal.style.cssText = `
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
                align-items: center;
                animation: fadeIn 0.2s ease-out;
            `;
            
            const dialog = document.createElement('div');
            dialog.style.cssText = `
                background: rgba(10, 10, 30, 0.98);
                border: 1px solid var(--cyan);
                border-radius: 12px;
                padding: 30px;
                min-width: 400px;
                max-width: 600px;
                box-shadow: 0 20px 60px rgba(0, 0, 0, 0.5);
                animation: scaleIn 0.3s ease-out;
            `;
            
            dialog.innerHTML = `
                <h3 style="margin: 0 0 16px 0; color: var(--cyan); font-size: 18px;">${title}</h3>
                <p style="margin: 0 0 24px 0; color: #ccc; line-height: 1.6;">${message}</p>
                <div style="display: flex; gap: 12px; justify-content: flex-end;">
                    ${buttons.map((btn, idx) => `
                        <button 
                            class="dialog-btn-${idx}" 
                            style="padding: 10px 20px; background: ${idx === 0 ? 'var(--cyan)' : 'transparent'}; color: ${idx === 0 ? '#000' : 'var(--cyan)'}; border: 1px solid var(--cyan); border-radius: 6px; cursor: pointer; font-weight: bold;"
                        >
                            ${btn}
                        </button>
                    `).join('')}
                </div>
            `;
            
            modal.appendChild(dialog);
            document.body.appendChild(modal);
            
            buttons.forEach((btn, idx) => {
                const btnEl = dialog.querySelector(`.dialog-btn-${idx}`);
                btnEl.onclick = () => {
                    modal.remove();
                    resolve(btn);
                };
            });
            
            modal.onclick = (e) => {
                if (e.target === modal) {
                    modal.remove();
                    resolve(null);
                }
            };
        });
    }
    
    /**
     * Get all loaded plugins
     */
    getPlugins() {
        return Array.from(this.plugins.values());
    }
    
    /**
     * Get plugin by ID
     */
    getPlugin(pluginId) {
        return this.plugins.get(pluginId);
    }
}

// Initialize
window.pluginSystem = null;

if (document.readyState === 'loading') {
    document.addEventListener('DOMContentLoaded', () => {
        window.pluginSystem = new PluginSystem();
        window.pluginSystem.init();
    });
} else {
    window.pluginSystem = new PluginSystem();
    window.pluginSystem.init();
}

// Export
window.PluginSystem = PluginSystem;

if (typeof module !== 'undefined' && module.exports) {
    module.exports = PluginSystem;
}

console.log('[PluginSystem] 📦 Plugin system module loaded');

})(); // End IIFE

