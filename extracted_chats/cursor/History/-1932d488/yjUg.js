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
        this.searchQuery = '';
        this.selectedCategory = 'all';
        this.marketplaceAPI = window.electron?.marketplace || null;
        this.plugins = [];
        this.apiKeysAPI = window.electron?.apiKeys || null;
        this.ollamaAPI = window.electron?.ollama || null;
        this.backendAvailable = !!this.marketplaceAPI;
        this.catalog = [];
        this.searchResults = null;
        this.installedExtensions = new Map();
        this.stateById = new Map();
        this.loading = false;
        this.searchTimeout = null;
        this.lastQuery = '';
        this.unsubscribeMarketplaceEvents = null;
        this.activeOverlay = null;
        this.mode = 'catalog';
        this.apiKeyPresets = [
            {
                id: 'openai',
                label: 'ChatGPT / OpenAI',
                provider: 'openai',
                baseUrl: 'https://api.openai.com/v1/chat/completions',
                docs: 'https://platform.openai.com/api-keys',
                defaultModel: 'gpt-4o-mini',
                description: 'Use your OpenAI Platform API key. Supports ChatGPT, GPT-4o, GPT-4o-mini, etc.'
            },
            {
                id: 'deepseek',
                label: 'DeepSeek',
                provider: 'deepseek',
                baseUrl: 'https://api.deepseek.com/v1/chat/completions',
                docs: 'https://platform.deepseek.com/',
                defaultModel: 'deepseek-chat',
                description: 'Paste your DeepSeek API key. Fully OpenAI-compatible endpoint.'
            },
            {
                id: 'kimi',
                label: 'Kimi / Moonshot',
                provider: 'kimi',
                baseUrl: 'https://api.moonshot.cn/v1/chat/completions',
                docs: 'https://platform.moonshot.cn/',
                defaultModel: 'moonshot-v1-8k',
                description: 'Moonshot (Kimi) uses an OpenAI-compatible API. Requires a paid account to generate an API key.'
            },
            {
                id: 'cursor',
                label: 'Cursor (Bring Your Own Backend)',
                provider: 'cursor',
                baseUrl: '',
                docs: 'https://www.cursor.sh/',
                defaultModel: '',
                description: 'Cursor does not expose a public API. If you have a compatible endpoint, set provider to "cursor" and fill in the Base URL manually.'
            },
            {
                id: 'custom-openai',
                label: 'Custom OpenAI-Compatible',
                provider: 'custom_openai',
                baseUrl: '',
                docs: '',
                defaultModel: '',
                description: 'Use any self-hosted or third-party OpenAI-compatible API (e.g., Azure OpenAI, Together, Groq). Provide the base URL and optional custom headers.'
            }
        ];
        this.selectedApiPreset = null;
        
        console.log('[PluginMarketplace] 🛒 Initializing plugin marketplace...');
    }
    
    async init() {
        // Create marketplace panel
        this.createPanel();
        
        // Register hotkey (Ctrl+Shift+P)
        this.registerHotkeys();
        
        if (this.backendAvailable && window.electron?.marketplace?.onEvent) {
            this.unsubscribeMarketplaceEvents = window.electron.marketplace.onEvent((event) => {
                this.handleMarketplaceEvent(event);
            });
        }
        
        // Load available plugins
        try {
            await this.loadPluginCatalog();
        } catch (error) {
            console.error('[PluginMarketplace] Failed to load catalog on init:', error);
        }
        
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
                <div style="display: flex; gap: 8px; flex-wrap: wrap; justify-content: flex-end;">
                    <button onclick="pluginMarketplace.showApiKeyManager()" style="background: rgba(119, 221, 190, 0.12); border: 1px solid var(--cursor-jade-light); color: var(--cursor-jade-dark); padding: 6px 12px; border-radius: 6px; cursor: pointer; font-size: 12px; font-weight: 600; transition: all 0.2s;">
                        🔑 API Keys
                    </button>
                    <button onclick="pluginMarketplace.showOllamaManager()" style="background: rgba(119, 221, 190, 0.12); border: 1px solid var(--cursor-jade-light); color: var(--cursor-jade-dark); padding: 6px 12px; border-radius: 6px; cursor: pointer; font-size: 12px; font-weight: 600; transition: all 0.2s;">
                        🧠 Ollama Models
                    </button>
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
                    <button onclick="pluginMarketplace.filterByCategory('other')" class="category-btn" data-category="other" style="padding: 6px 12px; background: rgba(119, 221, 190, 0.1); border: 1px solid var(--cursor-jade-light); color: var(--cursor-jade-dark); border-radius: 6px; cursor: pointer; font-size: 11px; font-weight: 600; transition: all 0.2s;">
                        🧩 Other
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
    
    async loadPluginCatalog(query = '') {
        this.loading = true;
        this.renderLoading('Loading marketplace catalog...');
        const trimmedQuery = (query || '').trim();
        this.lastQuery = trimmedQuery;
        
        if (this.backendAvailable && this.marketplaceAPI) {
            try {
                const status = await this.marketplaceAPI.status();
                if (!status.success) {
                    throw new Error(status.error || 'Marketplace status unavailable');
                }

                this.syncMarketplaceState(status);

                let sourceExtensions = [];
                if (trimmedQuery) {
                    const searchResponse = await this.marketplaceAPI.search({
                        query: trimmedQuery,
                        pageNumber: 1,
                        pageSize: 50
                    });

                    if (!searchResponse.success) {
                        throw new Error(searchResponse.error || 'Marketplace search failed');
                    }

                    sourceExtensions = searchResponse.results || [];
                } else {
                    const featuredResponse = await this.marketplaceAPI.featured();
                    if (!featuredResponse.success) {
                        throw new Error(featuredResponse.error || 'Marketplace featured fetch failed');
                    }

                    if (Array.isArray(featuredResponse.installed)) {
                        this.syncInstalledList(featuredResponse.installed);
                    }

                    sourceExtensions = featuredResponse.extensions || [];
                }

                this.plugins = sourceExtensions.map((ext) => {
                    const plugin = this.transformMarketplaceExtension(ext);
                    plugin.installed = this.installedPlugins.has(plugin.id);
                    plugin.state = this.stateById.get(plugin.id) || null;
                    return plugin;
                });

                this.loading = false;
                this.renderPlugins();
                return;
            } catch (error) {
                console.error('[PluginMarketplace] Marketplace fetch failed:', error);
                this.backendAvailable = false;
                this.showNotification('Marketplace service unavailable. Showing offline catalog.', 'error');
            }
        }

        this.plugins = this.getFallbackPlugins();
        this.installedPlugins = new Set(this.plugins.filter(p => p.installed).map(p => p.id));

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

        this.loading = false;
        this.renderPlugins();
    }

    getFallbackPlugins() {
        return [
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
    }
    
    renderLoading(message = 'Loading...') {
        const container = document.getElementById('plugin-list');
        if (!container) return;
        container.innerHTML = `
            <div style="text-align: center; color: var(--cursor-text-secondary); padding: 60px 20px;">
                <div style="font-size: 48px; margin-bottom: 16px;">⏳</div>
                <div style="font-size: 16px; font-weight: 600; margin-bottom: 8px;">${message}</div>
            </div>
        `;
    }

    getFilteredPlugins() {
        let filtered = Array.from(this.plugins);
        
        if (this.selectedCategory !== 'all') {
            filtered = filtered.filter(p => p.category === this.selectedCategory);
        }
        
        if (this.searchQuery) {
            const query = this.searchQuery.toLowerCase();
            filtered = filtered.filter(p =>
                p.name.toLowerCase().includes(query) ||
                (p.description && p.description.toLowerCase().includes(query)) ||
                (p.tags && p.tags.some(tag => tag.toLowerCase().includes(query)))
            );
        }
        
        return filtered;
    }

    renderPlugins(overrideList = null, options = {}) {
        const container = document.getElementById('plugin-list');
        if (!container) return 0;
        
        if (this.loading) {
            this.renderLoading(options.loadingMessage || 'Loading marketplace...');
            return 0;
        }
        
        const plugins = overrideList ?? this.getFilteredPlugins();
        
        const countEl = document.getElementById('plugin-count');
        if (countEl) {
            const countLabel = options.countLabel || `${plugins.length} plugin${plugins.length === 1 ? '' : 's'} available`;
            countEl.textContent = countLabel;
        }
        
        if (!plugins.length) {
            container.innerHTML = `
                <div style="text-align: center; color: var(--cursor-text-secondary); padding: 60px 20px;">
                    <div style="font-size: 48px; margin-bottom: 16px;">🔍</div>
                    <div style="font-size: 16px; font-weight: 600; margin-bottom: 8px;">${options.emptyTitle || 'No plugins found'}</div>
                    <div style="font-size: 13px;">${options.emptyMessage || 'Try a different search or category'}</div>
                </div>
            `;
            return 0;
        }
        
        container.innerHTML = plugins.map(plugin => this.renderPlugin(plugin)).join('');
        return plugins.length;
    }
    
    renderPlugin(plugin) {
        const installed = plugin.installed;
        const featured = plugin.featured ? '<span style="background: linear-gradient(135deg, #f093fb, #f5576c); color: white; padding: 2px 6px; border-radius: 10px; font-size: 9px; font-weight: 700; margin-left: 6px;">FEATURED</span>' : '';
        const iconContent = plugin.iconUrl
            ? `<img src="${plugin.iconUrl}" alt="${plugin.name}" style="width: 32px; height: 32px; border-radius: 6px; object-fit: cover;">`
            : `<span style="font-size: 32px;">${plugin.icon || '🧩'}</span>`;
        const downloadsDisplay = this.formatNumber(plugin.downloads || 0);
        const ratingDisplay = plugin.rating ? plugin.rating.toFixed(1) : 'New';
        const stateBadge = plugin.state && plugin.state.enabled === false
            ? '<span style="margin-left: 8px; font-size: 10px; color: #ff4757; background: rgba(255, 71, 87, 0.1); padding: 2px 6px; border-radius: 6px;">Disabled</span>'
            : '';
        
        return `
            <div style="margin-bottom: 16px; padding: 16px; background: rgba(119, 221, 190, 0.05); border: 1px solid var(--cursor-border); border-radius: 12px; transition: all 0.2s; cursor: pointer;" onmouseover="this.style.borderColor='var(--cursor-jade-light)'; this.style.background='rgba(119, 221, 190, 0.08)'" onmouseout="this.style.borderColor='var(--cursor-border)'; this.style.background='rgba(119, 221, 190, 0.05)'">
                <div style="display: flex; justify-content: space-between; align-items: start; margin-bottom: 10px;">
                    <div style="display: flex; align-items: center; gap: 12px; flex: 1;">
                        ${iconContent}
                        <div style="flex: 1;">
                            <div style="font-weight: 600; font-size: 14px; color: var(--cursor-jade-dark); margin-bottom: 4px;">
                                ${plugin.name} <span style="font-size: 11px; color: var(--cursor-text-muted);">v${plugin.version}</span>${featured}${stateBadge}
                            </div>
                            <div style="font-size: 11px; color: var(--cursor-text-secondary); margin-bottom: 6px;">
                                by ${plugin.author}
                            </div>
                            <div style="font-size: 12px; color: var(--cursor-text); margin-bottom: 8px;">
                                ${plugin.description}
                            </div>
                            <div style="display: flex; gap: 8px; align-items: center; flex-wrap: wrap;">
                                ${(plugin.tags || []).map(tag => `<span style="background: rgba(119, 221, 190, 0.15); padding: 3px 8px; border-radius: 10px; font-size: 10px; color: var(--cursor-jade-dark);">${tag}</span>`).join('')}
                            </div>
                        </div>
                    </div>
                    <div style="display: flex; flex-direction: column; gap: 8px; align-items: flex-end; min-width: 140px;">
                        ${installed 
                            ? `<button onclick="pluginMarketplace.uninstallPlugin('${plugin.id}')" style="background: rgba(255, 71, 87, 0.1); border: 1px solid #ff4757; color: #ff4757; padding: 8px 16px; border-radius: 6px; cursor: pointer; font-size: 11px; font-weight: 600; white-space: nowrap;">🗑️ Uninstall</button>`
                            : `<button onclick="pluginMarketplace.installPlugin('${plugin.id}')" style="background: linear-gradient(135deg, var(--cursor-jade-dark), var(--cursor-accent)); border: none; color: white; padding: 8px 16px; border-radius: 6px; cursor: pointer; font-size: 11px; font-weight: 600; box-shadow: 0 2px 8px rgba(119, 221, 190, 0.3); white-space: nowrap;">⬇️ Install</button>`
                        }
                        <div style="display: flex; gap: 12px; font-size: 10px; color: var(--cursor-text-muted);">
                            ${downloadsDisplay !== '0' ? `<span title="Downloads">⬇️ ${downloadsDisplay}</span>` : ''}
                            <span title="Rating">⭐ ${ratingDisplay}</span>
                        </div>
                    </div>
                </div>
            </div>
        `;
    }

    transformMarketplaceExtension(ext) {
        const id = `${ext.publisher}.${ext.name}`;
        const categories = (ext.raw?.categories || []).map(cat => String(cat).toLowerCase());
        return {
            id,
            name: ext.displayName || ext.name || id,
            version: ext.version || '0.0.0',
            author: ext.publisher || 'Unknown',
            description: ext.shortDescription || '',
            category: this.mapExtensionCategory(categories, ext),
            icon: '🧩',
            iconUrl: ext.icon || '',
            downloads: ext.installCount || 0,
            rating: ext.averageRating || 0,
            ratingCount: ext.ratingCount || 0,
            tags: this.extractTags(ext),
            installed: false,
            featured: categories.includes('featured'),
            state: null,
            marketplace: {
                categories,
                raw: ext.raw || ext
            }
        };
    }

    mapExtensionCategory(categories, ext) {
        const lowerName = `${ext.displayName || ext.name || ''}`.toLowerCase();
        if (categories.some(c => c.includes('theme')) || lowerName.includes('theme')) {
            return 'theme';
        }
        if (categories.some(c => c.includes('language'))) {
            return 'language';
        }
        if (categories.some(c => c.includes('ai') || c.includes('machine learning') || c.includes('data science'))) {
            return 'ai';
        }
        if (categories.some(c => c.includes('testing') || c.includes('test'))) {
            return 'testing';
        }
        if (categories.some(c => c.includes('debug'))) {
            return 'testing';
        }
        if (categories.some(c => c.includes('productivity') || c.includes('format') || c.includes('lint'))) {
            return 'productivity';
        }
        if (categories.some(c => c.includes('visualization'))) {
            return 'web';
        }
        if (lowerName.includes('theme')) {
            return 'theme';
        }
        return 'other';
    }

    extractTags(ext) {
        const tags = new Set();
        if (Array.isArray(ext.raw?.tags)) {
            ext.raw.tags.forEach(tag => tags.add(String(tag)));
        }
        if (Array.isArray(ext.raw?.categories)) {
            ext.raw.categories.forEach(cat => tags.add(String(cat)));
        }
        if (ext.shortDescription) {
            ext.shortDescription.split(/[,;/]/).forEach(part => {
                const trimmed = part.trim();
                if (trimmed.length > 2 && trimmed.length < 32) {
                    tags.add(trimmed);
                }
            });
        }
        return Array.from(tags).slice(0, 8);
    }

    resolveExtensionId(ext) {
        if (!ext) return null;
        if (typeof ext === 'string') return ext;
        if (ext.id) return ext.id;
        if (ext.extensionId) return ext.extensionId;
        if (ext.publisher && ext.name) return `${ext.publisher}.${ext.name}`;
        if (ext.manifest?.publisher && ext.manifest?.name) {
            return `${ext.manifest.publisher}.${ext.manifest.name}`;
        }
        return null;
    }

    syncInstalledList(list) {
        if (!Array.isArray(list)) {
            return;
        }
        if (!this.installedPlugins) {
            this.installedPlugins = new Set();
        }
        list.forEach(ext => {
            const extId = this.resolveExtensionId(ext);
            if (extId) {
                this.installedPlugins.add(extId);
            }
        });
    }

    syncMarketplaceState(status) {
        if (!status) return;

        const installed = Array.isArray(status.installed) ? status.installed : [];
        this.installedPlugins = new Set();
        installed.forEach(ext => {
            const extId = this.resolveExtensionId(ext);
            if (extId) {
                this.installedPlugins.add(extId);
            }
        });

        this.stateById = new Map(Object.entries(status.states || {}));
    }

    async updateMarketplaceState() {
        if (!this.backendAvailable || !window.electron?.marketplace) {
            return;
        }
        try {
            const status = await window.electron.marketplace.status();
            if (!status.success) {
                return;
            }
            this.syncMarketplaceState(status);
            this.plugins.forEach(plugin => {
                plugin.installed = this.installedPlugins.has(plugin.id);
                plugin.state = this.stateById.get(plugin.id) || plugin.state || null;
            });
        } catch (error) {
            console.warn('[PluginMarketplace] Failed to refresh marketplace state', error);
        }
    }

    async handleMarketplaceEvent(event) {
        if (!event) return;
        await this.updateMarketplaceState();
        this.renderPlugins();
    }
    
    formatNumber(num) {
        if (num >= 1000000) return `${(num / 1000000).toFixed(1)}M`;
        if (num >= 1000) return `${(num / 1000).toFixed(1)}K`;
        return num.toString();
    }
    
    async installPlugin(pluginId) {
        const plugin = this.plugins.find(p => p.id === pluginId);
        if (!plugin && !this.backendAvailable) {
            return;
        }
        
        try {
            const displayName = plugin?.name || pluginId;
            console.log(`[PluginMarketplace] Installing ${displayName}...`);
            this.showNotification(`Installing ${displayName}...`, 'info');
            
            if (this.backendAvailable && window.electron?.marketplace) {
                const result = await window.electron.marketplace.install(pluginId);
                if (!result.success) {
                    throw new Error(result.error || 'Marketplace install failed');
                }
            } else if (window.pluginSystem && plugin) {
                await window.pluginSystem.loadPlugin(plugin);
            }
            
            await this.updateMarketplaceState();
            if (plugin) {
                plugin.installed = true;
                plugin.state = this.stateById.get(plugin.id) || plugin.state || null;
            }
            this.renderPlugins();
            
            this.showNotification(`✅ ${displayName} installed successfully!`, 'success');
            
        } catch (error) {
            console.error('[PluginMarketplace] Install failed:', error);
            this.showNotification(`❌ Failed to install ${plugin?.name || pluginId}: ${error.message}`, 'error');
        }
    }
    
    async uninstallPlugin(pluginId) {
        const plugin = this.plugins.find(p => p.id === pluginId);
        if (!plugin && !this.backendAvailable) {
            return;
        }
        
        try {
            const displayName = plugin?.name || pluginId;
            console.log(`[PluginMarketplace] Uninstalling ${displayName}...`);
            
            if (!confirm(`Uninstall ${displayName}?`)) {
                return;
            }
            
            this.showNotification(`Uninstalling ${displayName}...`, 'info');
            
            if (this.backendAvailable && window.electron?.marketplace) {
                const result = await window.electron.marketplace.uninstall(pluginId);
                if (!result.success) {
                    throw new Error(result.error || 'Marketplace uninstall failed');
                }
            } else if (window.pluginSystem) {
                await window.pluginSystem.unloadPlugin(pluginId);
            }
            
            await this.updateMarketplaceState();
            if (plugin) {
                plugin.installed = false;
                plugin.state = this.stateById.get(plugin.id) || null;
            }
            this.renderPlugins();
            
            this.showNotification(`✅ ${displayName} uninstalled`, 'success');
            
        } catch (error) {
            console.error('[PluginMarketplace] Uninstall failed:', error);
            this.showNotification(`❌ Failed to uninstall ${plugin?.name || pluginId}: ${error.message}`, 'error');
        }
    }
    
    search(query) {
        const trimmed = (query || '').trim();
        this.searchQuery = trimmed;
        if (this.backendAvailable && this.marketplaceAPI) {
            clearTimeout(this.searchDebounce);
            this.searchDebounce = setTimeout(() => {
                const effectiveQuery = trimmed.length >= 2 ? trimmed : '';
                this.loadPluginCatalog(effectiveQuery).catch(error => {
                    console.error('[PluginMarketplace] Search refresh failed:', error);
                });
            }, 300);
        } else {
            this.renderPlugins();
        }
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
    
    async showInstalledPlugins() {
        if (this.backendAvailable) {
            await this.updateMarketplaceState();
        }
        
        const installedOnly = this.plugins.filter(p => p.installed);
        this.renderPlugins(installedOnly, {
            countLabel: `${installedOnly.length} plugin${installedOnly.length === 1 ? '' : 's'} installed`,
            emptyTitle: 'No plugins installed',
            emptyMessage: 'Browse the marketplace to find plugins'
        });
    }
    
    async refreshCatalog() {
        this.showNotification('Refreshing plugin catalog...', 'info');
        await this.loadPluginCatalog(this.searchQuery);
        this.showNotification('✅ Catalog refreshed!', 'success');
    }

    closeOverlay() {
        if (this.activeOverlay) {
            this.activeOverlay.remove();
            this.activeOverlay = null;
        }
    }

    openOverlay(title, contentHtml) {
        this.closeOverlay();

        const overlay = document.createElement('div');
        overlay.style.cssText = `
            position: fixed;
            top: 0;
            left: 0;
            right: 0;
            bottom: 0;
            background: rgba(8, 12, 22, 0.75);
            backdrop-filter: blur(4px);
            display: flex;
            align-items: center;
            justify-content: center;
            z-index: 20050;
            padding: 20px;
        `;

        const container = document.createElement('div');
        container.style.cssText = `
            background: var(--cursor-bg);
            border: 1px solid var(--cursor-border);
            border-radius: 12px;
            padding: 20px;
            width: 520px;
            max-width: 95vw;
            max-height: 80vh;
            overflow-y: auto;
            box-shadow: 0 20px 60px rgba(0, 0, 0, 0.5);
            position: relative;
        `;

        container.innerHTML = `
            <div style="display: flex; justify-content: space-between; align-items: center; margin-bottom: 16px;">
                <div style="font-size: 18px; font-weight: 600; color: var(--cursor-jade-dark);">${title}</div>
                <button onclick="pluginMarketplace.closeOverlay()" style="background: none; border: 1px solid var(--cursor-border); color: var(--cursor-text-secondary); padding: 4px 10px; border-radius: 6px; cursor: pointer; font-size: 12px;">✕</button>
            </div>
            <div id="plugin-marketplace-overlay-body">${contentHtml}</div>
        `;

        overlay.appendChild(container);
        overlay.addEventListener('click', (event) => {
            if (event.target === overlay) {
                this.closeOverlay();
            }
        });

        document.body.appendChild(overlay);
        this.activeOverlay = overlay;
        return container.querySelector('#plugin-marketplace-overlay-body');
    }

    getApiKeyPreset(id) {
        return this.apiKeyPresets.find(preset => preset.id === id) || null;
    }

    selectApiKeyPreset(id) {
        const preset = this.getApiKeyPreset(id);
        this.selectedApiPreset = preset;

        const providerInput = document.getElementById('api-key-provider-input');
        const baseUrlInput = document.getElementById('api-key-base-url-input');
        const modelInput = document.getElementById('api-key-model-input');
        const helper = document.getElementById('api-key-helper');
        const docsLink = document.getElementById('api-key-docs-link');

        if (preset) {
            if (providerInput) {
                providerInput.value = preset.provider || preset.id;
            }
            if (baseUrlInput) {
                baseUrlInput.placeholder = preset.baseUrl || 'https://your-host/v1/chat/completions';
                if (preset.baseUrl) {
                    baseUrlInput.value = preset.baseUrl;
                }
            }
            if (modelInput) {
                modelInput.placeholder = preset.defaultModel || 'e.g., gpt-4o-mini';
                if (preset.defaultModel) {
                    modelInput.value = preset.defaultModel;
                }
            }
            if (helper) {
                helper.innerHTML = `
                    <div style="font-weight: 600; color: var(--cursor-jade-dark);">${this.escapeHtml(preset.label)}</div>
                    <div style="font-size: 12px; color: var(--cursor-text-secondary); margin-top: 6px;">${this.escapeHtml(preset.description)}</div>
                `;
            }
            if (docsLink) {
                if (preset.docs) {
                    docsLink.href = preset.docs;
                    docsLink.style.display = 'inline-flex';
                } else {
                    docsLink.style.display = 'none';
                }
            }
        } else {
            if (helper) {
                helper.innerHTML = '<div style="font-size: 12px; color: var(--cursor-text-secondary);">Select a provider to see setup instructions.</div>';
            }
            if (docsLink) {
                docsLink.style.display = 'none';
            }
        }
    }

    renderApiKeyPresets() {
        if (!Array.isArray(this.apiKeyPresets) || this.apiKeyPresets.length === 0) {
            return '';
        }
        return `
            <div style="display: flex; flex-direction: column; gap: 6px;">
                ${this.apiKeyPresets.map(preset => `
                    <button class="api-key-preset-btn" data-preset="${this.escapeHtml(preset.id)}"
                        onclick="pluginMarketplace.selectApiKeyPreset('${preset.id}')"
                        style="display: flex; align-items: center; justify-content: space-between; gap: 12px; padding: 8px 12px; background: rgba(119, 221, 190, 0.08); border: 1px solid var(--cursor-border); border-radius: 6px; cursor: pointer; font-size: 12px; color: var(--cursor-text); text-align: left;">
                        <span>${this.escapeHtml(preset.label)}</span>
                        <span style="font-size: 11px; color: var(--cursor-text-secondary);">${this.escapeHtml(preset.provider || preset.id)}</span>
                    </button>
                `).join('')}
            </div>
        `;
    }

    async showApiKeyManager() {
        if (!this.apiKeysAPI) {
            this.showNotification('API key storage not available on this build.', 'error');
            return;
        }

        this.openOverlay('API Key Manager', '<div id="api-key-manager-content" style="font-size: 13px; color: var(--cursor-text);">Loading API keys...</div>');
        await this.refreshApiKeyOverlay();
    }

    async refreshApiKeyOverlay() {
        const container = document.getElementById('api-key-manager-content');
        if (!container || !this.apiKeysAPI) {
            return;
        }

        try {
            const response = await this.apiKeysAPI.list();
            if (!response?.success) {
                throw new Error(response?.error || 'Unable to load API keys');
            }

            const keys = response.keys || {};
            const entries = Object.entries(keys);

            const listHtml = entries.length
                ? entries.map(([provider, info]) => {
                    const masked = info?.masked || '********';
                    const updated = info?.updatedAt ? new Date(info.updatedAt).toLocaleString() : 'Unknown';
                    const providerLabel = this.escapeHtml(provider);
                    const providerAttr = provider.replace(/'/g, "\\'");
                    const metaDetails = [];
                    if (info?.providerLabel) {
                        metaDetails.push(`<div><strong>Preset:</strong> ${this.escapeHtml(info.providerLabel)}</div>`);
                    }
                    if (info?.baseUrl) {
                        metaDetails.push(`<div><strong>Endpoint:</strong> ${this.escapeHtml(info.baseUrl)}</div>`);
                    }
                    if (info?.defaultModel) {
                        metaDetails.push(`<div><strong>Default Model:</strong> ${this.escapeHtml(info.defaultModel)}</div>`);
                    }
                    if (info?.organization) {
                        metaDetails.push(`<div><strong>Organization:</strong> ${this.escapeHtml(info.organization)}</div>`);
                    }
                    if (info?.notes) {
                        metaDetails.push(`<div><strong>Notes:</strong> ${this.escapeHtml(info.notes)}</div>`);
                    }
                    const metaHtml = metaDetails.length
                        ? `<div style="font-size: 11px; color: var(--cursor-text-secondary); margin-top: 8px; line-height: 1.5;">${metaDetails.join('')}</div>`
                        : '';
                    return `
                        <div style="border: 1px solid var(--cursor-border); border-radius: 8px; padding: 12px 14px; margin-bottom: 10px; background: rgba(119, 221, 190, 0.05);">
                            <div style="display: flex; justify-content: space-between; align-items: center; gap: 12px;">
                                <div style="flex: 1;">
                                    <div style="font-weight: 600; font-size: 13px; color: var(--cursor-jade-dark); text-transform: uppercase;">${providerLabel}</div>
                                    <div style="font-size: 12px; color: var(--cursor-text-secondary); margin-top: 4px;">${masked}</div>
                                    <div style="font-size: 11px; color: var(--cursor-text-muted); margin-top: 4px;">Updated: ${updated}</div>
                                    ${metaHtml}
                                </div>
                                <button onclick="pluginMarketplace.deleteApiKey('${providerAttr}')" style="background: rgba(255, 71, 87, 0.12); border: 1px solid #ff4757; color: #ff4757; padding: 6px 12px; border-radius: 6px; cursor: pointer; font-size: 11px; font-weight: 600;">Remove</button>
                            </div>
                        </div>
                    `;
                }).join('')
                : '<div style="font-size: 13px; color: var(--cursor-text-secondary);">No API keys stored yet.</div>';

            const presetsHtml = this.renderApiKeyPresets();

            container.innerHTML = `
                <div style="display: flex; flex-direction: column; gap: 20px;">
                    <div>
                        ${listHtml}
                    </div>
                    <div style="border-top: 1px solid var(--cursor-border); padding-top: 16px; display: grid; gap: 16px; grid-template-columns: minmax(200px, 240px) 1fr; align-items: start;">
                        <div style="display: flex; flex-direction: column; gap: 10px;">
                            <div style="font-weight: 600; font-size: 13px; color: var(--cursor-jade-dark);">Quick Provider Presets</div>
                            ${presetsHtml || '<div style="font-size: 12px; color: var(--cursor-text-secondary);">No presets available.</div>'}
                            <a id="api-key-docs-link" href="#" target="_blank" rel="noopener" style="display: none; font-size: 12px; color: var(--cursor-accent); text-decoration: none; gap: 6px; align-items: center;">Provider Documentation ↗</a>
                            <div id="api-key-helper" style="font-size: 12px; color: var(--cursor-text-secondary); line-height: 1.5;">
                                Select a provider to see setup instructions.
                            </div>
                        </div>
                        <div>
                            <div style="font-weight: 600; font-size: 13px; color: var(--cursor-jade-dark); margin-bottom: 10px;">Add or Update API Key</div>
                            <div style="display: flex; flex-direction: column; gap: 10px;">
                                <div>
                                    <label style="display: block; font-size: 11px; color: var(--cursor-text-muted); margin-bottom: 4px;">Provider</label>
                                    <input id="api-key-provider-input" placeholder="e.g., openai" style="width: 100%; padding: 8px; border-radius: 6px; border: 1px solid var(--cursor-border); background: var(--cursor-bg-secondary); color: var(--cursor-text);" />
                                </div>
                                <div>
                                    <label style="display: block; font-size: 11px; color: var(--cursor-text-muted); margin-bottom: 4px;">API Key</label>
                                    <textarea id="api-key-value-input" rows="3" placeholder="Paste API key" style="width: 100%; padding: 8px; border-radius: 6px; border: 1px solid var(--cursor-border); background: var(--cursor-bg-secondary); color: var(--cursor-text);"></textarea>
                                </div>
                                <div>
                                    <label style="display: block; font-size: 11px; color: var(--cursor-text-muted); margin-bottom: 4px;">Base URL (OpenAI-compatible)</label>
                                    <input id="api-key-base-url-input" placeholder="https://your-host/v1/chat/completions" style="width: 100%; padding: 8px; border-radius: 6px; border: 1px solid var(--cursor-border); background: var(--cursor-bg-secondary); color: var(--cursor-text);" />
                                </div>
                                <div style="display: grid; gap: 10px; grid-template-columns: repeat(auto-fit, minmax(140px, 1fr));">
                                    <div>
                                        <label style="display: block; font-size: 11px; color: var(--cursor-text-muted); margin-bottom: 4px;">Default Model (optional)</label>
                                        <input id="api-key-model-input" placeholder="e.g., gpt-4o-mini" style="width: 100%; padding: 8px; border-radius: 6px; border: 1px solid var(--cursor-border); background: var(--cursor-bg-secondary); color: var(--cursor-text);" />
                                    </div>
                                    <div>
                                        <label style="display: block; font-size: 11px; color: var(--cursor-text-muted); margin-bottom: 4px;">Organization / Tenant (optional)</label>
                                        <input id="api-key-org-input" placeholder="Only for OpenAI enterprise/azure" style="width: 100%; padding: 8px; border-radius: 6px; border: 1px solid var(--cursor-border); background: var(--cursor-bg-secondary); color: var(--cursor-text);" />
                                    </div>
                                </div>
                                <div>
                                    <label style="display: block; font-size: 11px; color: var(--cursor-text-muted); margin-bottom: 4px;">Custom Headers (JSON)</label>
                                    <textarea id="api-key-headers-input" rows="2" placeholder='{"X-Example-Header": "value"}' style="width: 100%; padding: 8px; border-radius: 6px; border: 1px solid var(--cursor-border); background: var(--cursor-bg-secondary); color: var(--cursor-text);"></textarea>
                                </div>
                                <div>
                                    <label style="display: block; font-size: 11px; color: var(--cursor-text-muted); margin-bottom: 4px;">Notes (optional)</label>
                                    <textarea id="api-key-notes-input" rows="2" placeholder="Describe how this key should be used" style="width: 100%; padding: 8px; border-radius: 6px; border: 1px solid var(--cursor-border); background: var(--cursor-bg-secondary); color: var(--cursor-text);"></textarea>
                                </div>
                                <button onclick="pluginMarketplace.saveApiKey()" style="align-self: flex-start; background: linear-gradient(135deg, var(--cursor-jade-dark), var(--cursor-accent)); border: none; color: white; padding: 8px 16px; border-radius: 6px; cursor: pointer; font-size: 12px; font-weight: 600;">Save API Key</button>
                            </div>
                        </div>
                    </div>
                </div>
            `;

            this.selectApiKeyPreset(this.selectedApiPreset?.id || null);
        } catch (error) {
            console.error('[PluginMarketplace] Failed to load API keys:', error);
            container.innerHTML = `<div style="color: #ff4757; font-size: 13px;">Failed to load API keys: ${error.message}</div>`;
        }
    }

    async saveApiKey() {
        if (!this.apiKeysAPI) {
            return;
        }

        const providerInput = document.getElementById('api-key-provider-input');
        const keyInput = document.getElementById('api-key-value-input');
        const baseUrlInput = document.getElementById('api-key-base-url-input');
        const modelInput = document.getElementById('api-key-model-input');
        const orgInput = document.getElementById('api-key-org-input');
        const headersInput = document.getElementById('api-key-headers-input');
        const notesInput = document.getElementById('api-key-notes-input');
        const provider = providerInput?.value.trim();
        const key = keyInput?.value.trim();
        const baseUrl = baseUrlInput?.value.trim();
        const defaultModel = modelInput?.value.trim();
        const organization = orgInput?.value.trim();
        const headersRaw = headersInput?.value.trim();
        const notes = notesInput?.value.trim();

        if (!provider || !key) {
            this.showNotification('Provider and key are required', 'error');
            return;
        }

        let customHeaders = undefined;
        if (headersRaw) {
            try {
                const parsed = JSON.parse(headersRaw);
                if (parsed && typeof parsed === 'object' && !Array.isArray(parsed)) {
                    customHeaders = parsed;
                } else {
                    throw new Error('Headers must be a JSON object');
                }
            } catch (error) {
                this.showNotification(`Invalid custom headers JSON: ${error.message}`, 'error');
                return;
            }
        }

        const metadata = {};
        if (baseUrl) metadata.baseUrl = baseUrl;
        if (defaultModel) metadata.defaultModel = defaultModel;
        if (organization) metadata.organization = organization;
        if (notes) metadata.notes = notes;
        if (customHeaders) metadata.customHeaders = customHeaders;
        if (this.selectedApiPreset) {
            metadata.presetId = this.selectedApiPreset.id;
            metadata.providerLabel = this.selectedApiPreset.label;
        }

        try {
            const response = await this.apiKeysAPI.set(provider, key, metadata);
            if (response?.success === false) {
                throw new Error(response?.error || 'Failed to save key');
            }
            if (providerInput) providerInput.value = '';
            if (keyInput) keyInput.value = '';
            if (baseUrlInput) baseUrlInput.value = '';
            if (modelInput) modelInput.value = '';
            if (orgInput) orgInput.value = '';
            if (headersInput) headersInput.value = '';
            if (notesInput) notesInput.value = '';
            this.showNotification(`Saved API key for ${provider}`, 'success');
            await this.refreshApiKeyOverlay();
        } catch (error) {
            console.error('[PluginMarketplace] Save API key failed:', error);
            this.showNotification(`Failed to save API key: ${error.message}`, 'error');
        }
    }

    async deleteApiKey(provider) {
        if (!this.apiKeysAPI) {
            return;
        }

        const normalized = provider?.trim();
        if (!normalized) {
            return;
        }

        try {
            const response = await this.apiKeysAPI.delete(normalized);
            if (response?.success === false) {
                throw new Error(response?.error || 'Failed to delete key');
            }
            this.showNotification(`Removed API key for ${normalized}`, 'success');
            await this.refreshApiKeyOverlay();
        } catch (error) {
            console.error('[PluginMarketplace] Delete API key failed:', error);
            this.showNotification(`Failed to delete API key: ${error.message}`, 'error');
        }
    }

    async showOllamaManager() {
        if (!this.ollamaAPI) {
            this.showNotification('Ollama integration not available.', 'error');
            return;
        }

        this.openOverlay('Ollama Models', `
            <div id="ollama-models-content" style="font-size: 13px; color: var(--cursor-text);">
                Loading Ollama models...
            </div>
        `);

        await this.refreshOllamaModels();
    }

    async refreshOllamaModels() {
        const container = document.getElementById('ollama-models-content');
        if (!container || !this.ollamaAPI) {
            return;
        }

        try {
            const status = await this.ollamaAPI.status();
            const available = status?.success && status?.status?.available;

            if (!available) {
                container.innerHTML = `
                    <div style="color: #ff4757; font-size: 13px;">Ollama service is not reachable on localhost:11434.</div>
                    <div style="font-size: 12px; color: var(--cursor-text-secondary); margin-top: 6px;">Start Ollama (\`ollama serve\`) and refresh.</div>
                `;
                return;
            }

            const response = await this.ollamaAPI.listModels();
            if (!response?.success) {
                throw new Error(response?.error || 'Failed to list models');
            }

            const models = response.models || [];
            const listHtml = models.length
                ? models.map(model => {
                    const name = model.name || model.model || 'unknown';
                    const size = model.size ? this.formatFileSize(model.size) : 'Unknown size';
                    const updated = model.modified_at ? new Date(model.modified_at).toLocaleString() : 'Unknown time';
                    const nameLabel = this.escapeHtml(name);
                    const nameAttr = name.replace(/'/g, "\\'");
                    return `
                        <div style="border: 1px solid var(--cursor-border); border-radius: 8px; padding: 12px 14px; margin-bottom: 10px; background: rgba(119, 221, 190, 0.05);">
                            <div style="display: flex; justify-content: space-between; align-items: center;">
                                <div>
                                    <div style="font-weight: 600; font-size: 13px; color: var(--cursor-jade-dark);">${nameLabel}</div>
                                    <div style="font-size: 11px; color: var(--cursor-text-muted); margin-top: 4px;">${size} • Updated ${updated}</div>
                                </div>
                                <div style="display: flex; gap: 8px;">
                                    <button onclick="pluginMarketplace.showOllamaModelInfo('${nameAttr}')" style="background: rgba(119, 221, 190, 0.12); border: 1px solid var(--cursor-jade-light); color: var(--cursor-jade-dark); padding: 6px 12px; border-radius: 6px; cursor: pointer; font-size: 11px; font-weight: 600;">Details</button>
                                    <button onclick="pluginMarketplace.deleteOllamaModel('${nameAttr}')" style="background: rgba(255, 71, 87, 0.12); border: 1px solid #ff4757; color: #ff4757; padding: 6px 12px; border-radius: 6px; cursor: pointer; font-size: 11px; font-weight: 600;">Remove</button>
                                </div>
                            </div>
                        </div>
                    `;
                }).join('')
                : '<div style="font-size: 13px; color: var(--cursor-text-secondary);">No local Ollama models found.</div>';

            container.innerHTML = `
                <div style="margin-bottom: 16px;">
                    <div style="font-weight: 600; font-size: 13px; color: var(--cursor-jade-dark); margin-bottom: 8px;">Installed Models</div>
                    ${listHtml}
                </div>
                <div style="border-top: 1px solid var(--cursor-border); padding-top: 16px;">
                    <div style="font-weight: 600; font-size: 13px; color: var(--cursor-jade-dark); margin-bottom: 8px;">Pull New Model</div>
                    <div style="display: flex; gap: 10px; align-items: center;">
                        <input id="ollama-model-input" placeholder="e.g., llama3:8b" style="flex: 1; padding: 8px; border-radius: 6px; border: 1px solid var(--cursor-border); background: var(--cursor-bg-secondary); color: var(--cursor-text);" />
                        <button onclick="pluginMarketplace.pullOllamaModel()" style="background: linear-gradient(135deg, var(--cursor-jade-dark), var(--cursor-accent)); border: none; color: white; padding: 8px 16px; border-radius: 6px; cursor: pointer; font-size: 12px; font-weight: 600;">Pull</button>
                    </div>
                    <div style="font-size: 11px; color: var(--cursor-text-muted); margin-top: 6px;">Requires Ollama CLI installed locally.</div>
                </div>
            `;
        } catch (error) {
            console.error('[PluginMarketplace] Failed to load Ollama models:', error);
            container.innerHTML = `<div style="color: #ff4757; font-size: 13px;">Failed to load Ollama models: ${error.message}</div>`;
        }
    }

    async pullOllamaModel() {
        if (!this.ollamaAPI) {
            return;
        }

        const input = document.getElementById('ollama-model-input');
        const model = input?.value.trim();
        if (!model) {
            this.showNotification('Model name required (e.g., llama3:8b)', 'error');
            return;
        }

        this.showNotification(`Pulling ${model}...`, 'info');

        try {
            const result = await this.ollamaAPI.pullModel(model);
            if (result?.success === false) {
                throw new Error(result?.error || 'Failed to pull model');
            }
            if (input) input.value = '';
            this.showNotification(`Pulled ${model}`, 'success');
            await this.refreshOllamaModels();
        } catch (error) {
            console.error('[PluginMarketplace] Pull model failed:', error);
            this.showNotification(`Failed to pull model: ${error.message}`, 'error');
        }
    }

    async deleteOllamaModel(model) {
        if (!this.ollamaAPI) {
            return;
        }

        const trimmed = model?.trim();
        if (!trimmed) {
            return;
        }

        if (!confirm(`Remove Ollama model "${trimmed}"?`)) {
            return;
        }

        try {
            const result = await this.ollamaAPI.deleteModel(trimmed);
            if (result?.success === false) {
                throw new Error(result?.error || 'Failed to delete model');
            }
            this.showNotification(`Removed ${trimmed}`, 'success');
            await this.refreshOllamaModels();
        } catch (error) {
            console.error('[PluginMarketplace] Delete model failed:', error);
            this.showNotification(`Failed to remove model: ${error.message}`, 'error');
        }
    }

    async showOllamaModelInfo(model) {
        if (!this.ollamaAPI) {
            return;
        }

        try {
            const result = await this.ollamaAPI.showModel(model);
            if (result?.success === false) {
                throw new Error(result?.error || 'Failed to inspect model');
            }

            const output = (result?.output || '').trim() || 'No additional metadata available.';
            this.openOverlay(`Model: ${model}`, `
                <pre style="background: rgba(0,0,0,0.35); padding: 12px; border-radius: 8px; white-space: pre-wrap; font-size: 12px; color: var(--cursor-text);">${this.escapeHtml(output)}</pre>
            `);
        } catch (error) {
            console.error('[PluginMarketplace] Show model info failed:', error);
            this.showNotification(`Failed to inspect model: ${error.message}`, 'error');
        }
    }

    formatFileSize(bytes) {
        if (typeof bytes !== 'number' || Number.isNaN(bytes)) {
            return 'Unknown size';
        }
        if (bytes === 0) return '0 B';
        const units = ['B', 'KB', 'MB', 'GB', 'TB'];
        const idx = Math.floor(Math.log(bytes) / Math.log(1024));
        const value = bytes / Math.pow(1024, idx);
        return `${value.toFixed(2)} ${units[idx]}`;
    }

    escapeHtml(text) {
        if (typeof text !== 'string') return '';
        return text.replace(/[&<>"']/g, (char) => {
            const map = {
                '&': '&amp;',
                '<': '&lt;',
                '>': '&gt;',
                '"': '&quot;',
                "'": '&#39;'
            };
            return map[char] || char;
        });
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
        
        this.closeOverlay();
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

