/**
 * BigDaddyG IDE - Plugin Marketplace
 * Browse, install, and manage plugins
 * Hotkey: Ctrl+Shift+P (Plugins/Extensions)
 */

(function() {
'use strict';

class PluginMarketplace {
    constructor() {
        this.isOpen = false;
        this.panel = null;
        this.plugins = [];
        this.installedPlugins = new Set();
        this.searchQuery = '';
        this.selectedCategory = 'all';
        
        console.log('[PluginMarketplace] 🛒 Initializing plugin marketplace...');
    }
    
    init() {
        // Create marketplace panel
        this.createPanel();
        
        // Register hotkey (Ctrl+Shift+P)
        this.registerHotkeys();
        
        // Load available plugins
        this.loadPluginCatalog();
        
        console.log('[PluginMarketplace] ✅ Plugin marketplace ready (Ctrl+Shift+P)');
    }
    
    createPanel() {
        this.panel = document.createElement('div');
        this.panel.id = 'plugin-marketplace-panel';
        this.panel.style.cssText = `
            position: fixed;
            top: 50%;
            left: 50%;
            transform: translate(-50%, -50%) scale(0.9);
            width: 900px;
            max-width: 95vw;
            height: 700px;
            max-height: 90vh;
            background: var(--cursor-bg);
            border: 2px solid var(--cursor-jade-dark);
            border-radius: 16px;
            box-shadow: 0 24px 48px rgba(0, 0, 0, 0.5), 0 0 0 1px rgba(119, 221, 190, 0.2);
            z-index: 10001;
            display: none;
            opacity: 0;
            transition: all 0.3s cubic-bezier(0.4, 0, 0.2, 1);
            backdrop-filter: blur(10px);
            overflow: hidden;
        `;
        
        this.panel.innerHTML = `
            <!-- Header -->
            <div style="padding: 16px 20px; background: linear-gradient(135deg, var(--cursor-bg-secondary), var(--cursor-bg-tertiary)); border-bottom: 1px solid var(--cursor-border); display: flex; justify-content: space-between; align-items: center;">
                <div style="display: flex; align-items: center; gap: 12px;">
                    <span style="font-size: 20px;">🛒</span>
                    <span style="font-weight: 600; font-size: 15px; color: var(--cursor-jade-dark);">Plugin Marketplace</span>
                    <span style="font-size: 11px; color: var(--cursor-text-secondary); background: rgba(119, 221, 190, 0.1); padding: 3px 8px; border-radius: 12px;">Ctrl+Shift+P</span>
                </div>
                <div style="display: flex; gap: 8px;">
                    <button onclick="pluginMarketplace.showInstalledPlugins()" style="background: rgba(119, 221, 190, 0.1); border: 1px solid var(--cursor-jade-light); color: var(--cursor-jade-dark); padding: 6px 12px; border-radius: 6px; cursor: pointer; font-size: 12px; font-weight: 600; transition: all 0.2s;">
                        📦 Installed
                    </button>
                    <button onclick="pluginMarketplace.refreshCatalog()" style="background: none; border: 1px solid var(--cursor-border); color: var(--cursor-text-secondary); padding: 6px 12px; border-radius: 6px; cursor: pointer; font-size: 12px; transition: all 0.2s;">
                        🔄 Refresh
                    </button>
                    <button onclick="pluginMarketplace.close()" style="background: none; border: 1px solid var(--cursor-border); color: var(--cursor-text-secondary); padding: 6px 12px; border-radius: 6px; cursor: pointer; font-size: 12px; transition: all 0.2s;">
                        ✕ Close
                    </button>
                </div>
            </div>
            
            <!-- Search Bar -->
            <div style="padding: 16px 20px; background: var(--cursor-bg-secondary); border-bottom: 1px solid var(--cursor-border);">
                <div style="position: relative; margin-bottom: 12px;">
                    <input 
                        type="text" 
                        id="plugin-search-input" 
                        placeholder="🔍 Search plugins (e.g., 'web search', 'linter', 'theme')..." 
                        oninput="pluginMarketplace.search(this.value)"
                        style="width: 100%; padding: 10px 12px; background: var(--cursor-bg); border: 1px solid var(--cursor-jade-light); border-radius: 8px; color: var(--cursor-text); font-size: 13px; outline: none; transition: all 0.2s;"
                        onfocus="this.style.borderColor='var(--cursor-jade-dark)'; this.style.boxShadow='0 0 0 3px rgba(119, 221, 190, 0.1)'"
                        onblur="this.style.borderColor='var(--cursor-jade-light)'; this.style.boxShadow='none'"
                    />
                </div>
                
                <!-- Category Filters -->
                <div style="display: flex; gap: 8px; flex-wrap: wrap;">
                    <button onclick="pluginMarketplace.filterByCategory('all')" class="category-btn active" data-category="all" style="padding: 6px 12px; background: linear-gradient(135deg, var(--cursor-jade-dark), var(--cursor-accent)); border: none; color: white; border-radius: 6px; cursor: pointer; font-size: 11px; font-weight: 600; transition: all 0.2s;">
                        All
                    </button>
                    <button onclick="pluginMarketplace.filterByCategory('web')" class="category-btn" data-category="web" style="padding: 6px 12px; background: rgba(119, 221, 190, 0.1); border: 1px solid var(--cursor-jade-light); color: var(--cursor-jade-dark); border-radius: 6px; cursor: pointer; font-size: 11px; font-weight: 600; transition: all 0.2s;">
                        🌐 Web
                    </button>
                    <button onclick="pluginMarketplace.filterByCategory('ai')" class="category-btn" data-category="ai" style="padding: 6px 12px; background: rgba(119, 221, 190, 0.1); border: 1px solid var(--cursor-jade-light); color: var(--cursor-jade-dark); border-radius: 6px; cursor: pointer; font-size: 11px; font-weight: 600; transition: all 0.2s;">
                        🤖 AI
                    </button>
                    <button onclick="pluginMarketplace.filterByCategory('language')" class="category-btn" data-category="language" style="padding: 6px 12px; background: rgba(119, 221, 190, 0.1); border: 1px solid var(--cursor-jade-light); color: var(--cursor-jade-dark); border-radius: 6px; cursor: pointer; font-size: 11px; font-weight: 600; transition: all 0.2s;">
                        📝 Languages
                    </button>
                    <button onclick="pluginMarketplace.filterByCategory('theme')" class="category-btn" data-category="theme" style="padding: 6px 12px; background: rgba(119, 221, 190, 0.1); border: 1px solid var(--cursor-jade-light); color: var(--cursor-jade-dark); border-radius: 6px; cursor: pointer; font-size: 11px; font-weight: 600; transition: all 0.2s;">
                        🎨 Themes
                    </button>
                    <button onclick="pluginMarketplace.filterByCategory('productivity')" class="category-btn" data-category="productivity" style="padding: 6px 12px; background: rgba(119, 221, 190, 0.1); border: 1px solid var(--cursor-jade-light); color: var(--cursor-jade-dark); border-radius: 6px; cursor: pointer; font-size: 11px; font-weight: 600; transition: all 0.2s;">
                        ⚡ Productivity
                    </button>
                    <button onclick="pluginMarketplace.filterByCategory('testing')" class="category-btn" data-category="testing" style="padding: 6px 12px; background: rgba(119, 221, 190, 0.1); border: 1px solid var(--cursor-jade-light); color: var(--cursor-jade-dark); border-radius: 6px; cursor: pointer; font-size: 11px; font-weight: 600; transition: all 0.2s;">
                        🧪 Testing
                    </button>
                </div>
            </div>
            
            <!-- Plugin List -->
            <div id="plugin-list" style="height: calc(100% - 220px); overflow-y: auto; padding: 20px; scroll-behavior: smooth;">
                <div style="text-align: center; color: var(--cursor-text-secondary); padding: 60px 20px;">
                    <div style="font-size: 48px; margin-bottom: 16px;">🔌</div>
                    <div style="font-size: 16px; font-weight: 600; margin-bottom: 8px;">Loading plugins...</div>
                </div>
            </div>
            
            <!-- Footer Stats -->
            <div style="position: absolute; bottom: 0; left: 0; right: 0; padding: 12px 20px; background: var(--cursor-bg-secondary); border-top: 1px solid var(--cursor-border); display: flex; justify-content: space-between; align-items: center;">
                <div style="font-size: 11px; color: var(--cursor-text-secondary);">
                    <span id="plugin-count">0 plugins available</span>
                </div>
                <div style="font-size: 11px; color: var(--cursor-text-secondary);">
                    💡 Press <kbd style="background: rgba(119, 221, 190, 0.2); padding: 2px 6px; border-radius: 3px;">Ctrl+Shift+P</kbd> to toggle
                </div>
            </div>
        `;
        
        document.body.appendChild(this.panel);
    }
    
    registerHotkeys() {
        // Register Ctrl+Shift+P for Plugin Marketplace
        if (window.hotkeyManager) {
            window.hotkeyManager.register('plugin-marketplace', 'Ctrl+Shift+P', () => {
                this.toggle();
            }, 'Open Plugin Marketplace');
            console.log('[PluginMarketplace] ✅ Hotkey registered: Ctrl+Shift+P');
        } else {
            // Fallback if hotkey manager not available
            document.addEventListener('keydown', (e) => {
                if (e.ctrlKey && e.shiftKey && e.key === 'P') {
                    e.preventDefault();
                    this.toggle();
                }
            });
        }
        
        // Escape to close
        document.addEventListener('keydown', (e) => {
            if (e.key === 'Escape' && this.isOpen) {
                e.preventDefault();
                this.close();
            }
        });
    }
    
    async loadPluginCatalog() {
        // Load available plugins (online + local)
        this.plugins = [
            // Web Integration Plugins
            {
                id: 'web-search',
                name: 'Web Search',
                version: '1.0.0',
                author: 'BigDaddyG Team',
                description: 'Search Stack Overflow, MDN, GitHub directly from IDE',
                category: 'web',
                icon: '🔍',
                downloads: 15234,
                rating: 4.8,
                tags: ['search', 'stackoverflow', 'mdn', 'github'],
                installed: false,
                featured: true
            },
            {
                id: 'docs-fetcher',
                name: 'Documentation Fetcher',
                version: '1.2.0',
                author: 'DevDocs Team',
                description: 'Fetch and cache documentation from DevDocs, Read the Docs, and more',
                category: 'web',
                icon: '📚',
                downloads: 12456,
                rating: 4.7,
                tags: ['documentation', 'devdocs', 'offline'],
                installed: false,
                featured: true
            },
            {
                id: 'package-explorer',
                name: 'Package Explorer',
                version: '2.0.0',
                author: 'npm Team',
                description: 'Search npm, PyPI, crates.io packages with AI recommendations',
                category: 'web',
                icon: '📦',
                downloads: 18765,
                rating: 4.9,
                tags: ['npm', 'pypi', 'packages', 'dependencies'],
                installed: false,
                featured: true
            },
            
            // AI Enhancement Plugins
            {
                id: 'ai-code-review',
                name: 'AI Code Reviewer',
                version: '1.5.0',
                author: 'AI Team',
                description: 'Automatic code reviews with security, performance, and style suggestions',
                category: 'ai',
                icon: '🤖',
                downloads: 23456,
                rating: 4.9,
                tags: ['ai', 'review', 'security', 'performance'],
                installed: false,
                featured: true
            },
            {
                id: 'ai-refactor',
                name: 'AI Refactoring Assistant',
                version: '1.0.0',
                author: 'AI Team',
                description: 'Intelligent refactoring suggestions and automated code improvements',
                category: 'ai',
                icon: '🔧',
                downloads: 9876,
                rating: 4.6,
                tags: ['ai', 'refactoring', 'cleanup'],
                installed: false
            },
            {
                id: 'ai-test-generator',
                name: 'AI Test Generator',
                version: '1.0.0',
                author: 'Testing Team',
                description: 'Generate unit tests automatically using AI',
                category: 'testing',
                icon: '🧪',
                downloads: 7654,
                rating: 4.5,
                tags: ['ai', 'testing', 'unit-tests', 'jest'],
                installed: false
            },
            
            // Language Support
            {
                id: 'rust-analyzer',
                name: 'Rust Language Support',
                version: '0.3.0',
                author: 'Rust Team',
                description: 'Full Rust language support with cargo integration',
                category: 'language',
                icon: '🦀',
                downloads: 5432,
                rating: 4.8,
                tags: ['rust', 'cargo', 'language'],
                installed: false
            },
            {
                id: 'go-support',
                name: 'Go Language Support',
                version: '1.0.0',
                author: 'Go Team',
                description: 'Complete Go development environment',
                category: 'language',
                icon: '🐹',
                downloads: 6543,
                rating: 4.7,
                tags: ['go', 'golang', 'language'],
                installed: false
            },
            
            // Themes
            {
                id: 'dracula-theme',
                name: 'Dracula Theme',
                version: '3.0.0',
                author: 'Dracula Team',
                description: 'Dark theme for vampires 🧛',
                category: 'theme',
                icon: '🧛',
                downloads: 45678,
                rating: 4.9,
                tags: ['theme', 'dark', 'dracula'],
                installed: false,
                featured: true
            },
            {
                id: 'nord-theme',
                name: 'Nord Theme',
                version: '2.5.0',
                author: 'Nord Team',
                description: 'Arctic, north-bluish color palette',
                category: 'theme',
                icon: '❄️',
                downloads: 34567,
                rating: 4.8,
                tags: ['theme', 'nord', 'blue'],
                installed: false
            },
            
            // Productivity
            {
                id: 'code-time',
                name: 'Code Time Tracker',
                version: '1.2.0',
                author: 'Productivity Team',
                description: 'Track your coding time and productivity metrics',
                category: 'productivity',
                icon: '⏱️',
                downloads: 12345,
                rating: 4.6,
                tags: ['productivity', 'time-tracking', 'metrics'],
                installed: false
            },
            {
                id: 'todo-tree',
                name: 'TODO Tree',
                version: '2.0.0',
                author: 'Gruntfuggly',
                description: 'Show TODO, FIXME, etc. in a tree view',
                category: 'productivity',
                icon: '📝',
                downloads: 23456,
                rating: 4.9,
                tags: ['productivity', 'todo', 'comments'],
                installed: false,
                featured: true
            },
            
            // Testing
            {
                id: 'jest-runner',
                name: 'Jest Test Runner',
                version: '1.0.0',
                author: 'Jest Team',
                description: 'Run Jest tests directly in IDE',
                category: 'testing',
                icon: '🃏',
                downloads: 15678,
                rating: 4.7,
                tags: ['testing', 'jest', 'javascript'],
                installed: false
            },
            {
                id: 'code-stats',
                name: 'Code Statistics',
                version: '1.0.0',
                author: 'BigDaddyG Team',
                description: 'Show code statistics (lines, characters, words)',
                category: 'productivity',
                icon: '📊',
                downloads: 3456,
                rating: 4.5,
                tags: ['stats', 'metrics', 'productivity'],
                installed: true // Pre-installed
            }
        ];
        
        // Check which plugins are actually installed
        if (window.pluginSystem) {
            const installedPlugins = window.pluginSystem.getPlugins();
            installedPlugins.forEach(plugin => {
                const catalogPlugin = this.plugins.find(p => p.id === plugin.id);
                if (catalogPlugin) {
                    catalogPlugin.installed = true;
                    this.installedPlugins.add(plugin.id);
                }
            });
        }
        
        this.renderPlugins();
    }
    
    renderPlugins() {
        const container = document.getElementById('plugin-list');
        if (!container) return;
        
        // Filter plugins
        let filteredPlugins = this.plugins;
        
        if (this.selectedCategory !== 'all') {
            filteredPlugins = filteredPlugins.filter(p => p.category === this.selectedCategory);
        }
        
        if (this.searchQuery) {
            const query = this.searchQuery.toLowerCase();
            filteredPlugins = filteredPlugins.filter(p => 
                p.name.toLowerCase().includes(query) ||
                p.description.toLowerCase().includes(query) ||
                p.tags.some(t => t.includes(query))
            );
        }
        
        // Update count
        const countEl = document.getElementById('plugin-count');
        if (countEl) {
            countEl.textContent = `${filteredPlugins.length} plugin${filteredPlugins.length === 1 ? '' : 's'} available`;
        }
        
        // Render plugins
        if (filteredPlugins.length === 0) {
            container.innerHTML = `
                <div style="text-align: center; color: var(--cursor-text-secondary); padding: 60px 20px;">
                    <div style="font-size: 48px; margin-bottom: 16px;">🔍</div>
                    <div style="font-size: 16px; font-weight: 600; margin-bottom: 8px;">No plugins found</div>
                    <div style="font-size: 13px;">Try a different search or category</div>
                </div>
            `;
            return;
        }
        
        container.innerHTML = filteredPlugins.map(plugin => this.renderPlugin(plugin)).join('');
    }
    
    renderPlugin(plugin) {
        const installed = plugin.installed;
        const featured = plugin.featured ? '<span style="background: linear-gradient(135deg, #f093fb, #f5576c); color: white; padding: 2px 6px; border-radius: 10px; font-size: 9px; font-weight: 700; margin-left: 6px;">FEATURED</span>' : '';
        
        return `
            <div style="margin-bottom: 16px; padding: 16px; background: rgba(119, 221, 190, 0.05); border: 1px solid var(--cursor-border); border-radius: 12px; transition: all 0.2s; cursor: pointer;" onmouseover="this.style.borderColor='var(--cursor-jade-light)'; this.style.background='rgba(119, 221, 190, 0.08)'" onmouseout="this.style.borderColor='var(--cursor-border)'; this.style.background='rgba(119, 221, 190, 0.05)'">
                <div style="display: flex; justify-content: space-between; align-items: start; margin-bottom: 10px;">
                    <div style="display: flex; align-items: center; gap: 12px; flex: 1;">
                        <span style="font-size: 32px;">${plugin.icon}</span>
                        <div style="flex: 1;">
                            <div style="font-weight: 600; font-size: 14px; color: var(--cursor-jade-dark); margin-bottom: 4px;">
                                ${plugin.name} <span style="font-size: 11px; color: var(--cursor-text-muted);">v${plugin.version}</span>${featured}
                            </div>
                            <div style="font-size: 11px; color: var(--cursor-text-secondary); margin-bottom: 6px;">
                                by ${plugin.author}
                            </div>
                            <div style="font-size: 12px; color: var(--cursor-text); margin-bottom: 8px;">
                                ${plugin.description}
                            </div>
                            <div style="display: flex; gap: 8px; align-items: center; flex-wrap: wrap;">
                                ${plugin.tags.map(tag => `<span style="background: rgba(119, 221, 190, 0.15); padding: 3px 8px; border-radius: 10px; font-size: 10px; color: var(--cursor-jade-dark);">${tag}</span>`).join('')}
                            </div>
                        </div>
                    </div>
                    <div style="display: flex; flex-direction: column; gap: 8px; align-items: flex-end; min-width: 120px;">
                        ${installed ? 
                            `<button onclick="pluginMarketplace.uninstallPlugin('${plugin.id}')" style="background: rgba(255, 71, 87, 0.1); border: 1px solid #ff4757; color: #ff4757; padding: 8px 16px; border-radius: 6px; cursor: pointer; font-size: 11px; font-weight: 600; white-space: nowrap;">
                                🗑️ Uninstall
                            </button>` : 
                            `<button onclick="pluginMarketplace.installPlugin('${plugin.id}')" style="background: linear-gradient(135deg, var(--cursor-jade-dark), var(--cursor-accent)); border: none; color: white; padding: 8px 16px; border-radius: 6px; cursor: pointer; font-size: 11px; font-weight: 600; box-shadow: 0 2px 8px rgba(119, 221, 190, 0.3); white-space: nowrap;">
                                ⬇️ Install
                            </button>`
                        }
                        <div style="display: flex; gap: 12px; font-size: 10px; color: var(--cursor-text-muted);">
                            <span title="Downloads">⬇️ ${this.formatNumber(plugin.downloads)}</span>
                            <span title="Rating">⭐ ${plugin.rating}</span>
                        </div>
                    </div>
                </div>
            </div>
        `;
    }
    
    formatNumber(num) {
        if (num >= 1000000) return `${(num / 1000000).toFixed(1)}M`;
        if (num >= 1000) return `${(num / 1000).toFixed(1)}K`;
        return num.toString();
    }
    
    async installPlugin(pluginId) {
        const plugin = this.plugins.find(p => p.id === pluginId);
        if (!plugin) return;
        
        try {
            console.log(`[PluginMarketplace] Installing ${plugin.name}...`);
            
            // Show loading notification
            this.showNotification(`Installing ${plugin.name}...`, 'info');
            
            // Simulate installation (in real version, download and install)
            await new Promise(resolve => setTimeout(resolve, 1500));
            
            // Mark as installed
            plugin.installed = true;
            this.installedPlugins.add(pluginId);
            
            // Load plugin if plugin system available
            if (window.pluginSystem) {
                await window.pluginSystem.loadPlugin(plugin);
            }
            
            // Re-render
            this.renderPlugins();
            
            this.showNotification(`✅ ${plugin.name} installed successfully!`, 'success');
            
        } catch (error) {
            console.error('[PluginMarketplace] Install failed:', error);
            this.showNotification(`❌ Failed to install ${plugin.name}`, 'error');
        }
    }
    
    async uninstallPlugin(pluginId) {
        const plugin = this.plugins.find(p => p.id === pluginId);
        if (!plugin) return;
        
        try {
            console.log(`[PluginMarketplace] Uninstalling ${plugin.name}...`);
            
            // Confirm
            if (!confirm(`Uninstall ${plugin.name}?`)) {
                return;
            }
            
            // Show loading notification
            this.showNotification(`Uninstalling ${plugin.name}...`, 'info');
            
            // Simulate uninstallation
            await new Promise(resolve => setTimeout(resolve, 1000));
            
            // Mark as uninstalled
            plugin.installed = false;
            this.installedPlugins.delete(pluginId);
            
            // Unload plugin if plugin system available
            if (window.pluginSystem) {
                await window.pluginSystem.unloadPlugin(pluginId);
            }
            
            // Re-render
            this.renderPlugins();
            
            this.showNotification(`✅ ${plugin.name} uninstalled`, 'success');
            
        } catch (error) {
            console.error('[PluginMarketplace] Uninstall failed:', error);
            this.showNotification(`❌ Failed to uninstall ${plugin.name}`, 'error');
        }
    }
    
    search(query) {
        this.searchQuery = query.trim();
        this.renderPlugins();
    }
    
    filterByCategory(category) {
        this.selectedCategory = category;
        
        // Update active category button
        document.querySelectorAll('.category-btn').forEach(btn => {
            if (btn.dataset.category === category) {
                btn.style.background = 'linear-gradient(135deg, var(--cursor-jade-dark), var(--cursor-accent))';
                btn.style.border = 'none';
                btn.style.color = 'white';
            } else {
                btn.style.background = 'rgba(119, 221, 190, 0.1)';
                btn.style.border = '1px solid var(--cursor-jade-light)';
                btn.style.color = 'var(--cursor-jade-dark)';
            }
        });
        
        this.renderPlugins();
    }
    
    showInstalledPlugins() {
        this.searchQuery = '';
        this.plugins.forEach(p => {
            if (!p.installed) {
                p._hidden = true;
            } else {
                delete p._hidden;
            }
        });
        
        const installedOnly = this.plugins.filter(p => p.installed);
        
        const container = document.getElementById('plugin-list');
        if (!container) return;
        
        if (installedOnly.length === 0) {
            container.innerHTML = `
                <div style="text-align: center; color: var(--cursor-text-secondary); padding: 60px 20px;">
                    <div style="font-size: 48px; margin-bottom: 16px;">📦</div>
                    <div style="font-size: 16px; font-weight: 600; margin-bottom: 8px;">No plugins installed</div>
                    <div style="font-size: 13px;">Browse the marketplace to find plugins</div>
                </div>
            `;
            return;
        }
        
        container.innerHTML = installedOnly.map(plugin => this.renderPlugin(plugin)).join('');
        
        const countEl = document.getElementById('plugin-count');
        if (countEl) {
            countEl.textContent = `${installedOnly.length} plugin${installedOnly.length === 1 ? '' : 's'} installed`;
        }
    }
    
    async refreshCatalog() {
        this.showNotification('Refreshing plugin catalog...', 'info');
        await this.loadPluginCatalog();
        this.showNotification('✅ Catalog refreshed!', 'success');
    }
    
    showNotification(message, type = 'info') {
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
    
    open() {
        if (this.isOpen) return;
        
        this.isOpen = true;
        this.panel.style.display = 'block';
        
        // Animate in
        setTimeout(() => {
            this.panel.style.opacity = '1';
            this.panel.style.transform = 'translate(-50%, -50%) scale(1)';
        }, 10);
        
        // Focus search input
        setTimeout(() => {
            const input = document.getElementById('plugin-search-input');
            if (input) input.focus();
        }, 300);
        
        console.log('[PluginMarketplace] ✨ Opened');
    }
    
    close() {
        if (!this.isOpen) return;
        
        this.isOpen = false;
        this.panel.style.opacity = '0';
        this.panel.style.transform = 'translate(-50%, -50%) scale(0.9)';
        
        setTimeout(() => {
            this.panel.style.display = 'none';
        }, 300);
        
        console.log('[PluginMarketplace] 🔽 Closed');
    }
    
    toggle() {
        if (this.isOpen) {
            this.close();
        } else {
            this.open();
        }
    }
}

// Add CSS animations
const style = document.createElement('style');
style.textContent = `
    @keyframes slideInRight {
        from {
            opacity: 0;
            transform: translateX(20px);
        }
        to {
            opacity: 1;
            transform: translateX(0);
        }
    }
`;
document.head.appendChild(style);

// Initialize
window.pluginMarketplace = null;

if (document.readyState === 'loading') {
    document.addEventListener('DOMContentLoaded', () => {
        window.pluginMarketplace = new PluginMarketplace();
        window.pluginMarketplace.init();
    });
} else {
    window.pluginMarketplace = new PluginMarketplace();
    window.pluginMarketplace.init();
}

// Export
window.PluginMarketplace = PluginMarketplace;

if (typeof module !== 'undefined' && module.exports) {
    module.exports = PluginMarketplace;
}

console.log('[PluginMarketplace] 📦 Plugin marketplace module loaded');

})(); // End IIFE

