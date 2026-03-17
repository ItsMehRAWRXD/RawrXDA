/**
 * BigDaddyG IDE - Advanced Model Browser & Selector
 * Full support for 60+ models with search, filtering, and favorites
 * Seamlessly integrates with model-hotswap.js
 */

class ModelBrowser {
    constructor() {
        this.allModels = new Map();
        this.favorites = this.loadFavorites();
        this.recentlyUsed = this.loadRecentlyUsed();
        this.searchQuery = '';
        this.currentFilter = 'all'; // all, favorites, recent, built-in, ollama, local
        this.modelCache = new Map();
        this.isLoading = false;

        console.log('[ModelBrowser] 🎯 Advanced model browser initialized');
        this.loadAllModels();
    }

    async loadAllModels() {
        if (this.isLoading) return;
        this.isLoading = true;

        try {
            // Load built-in models from model-hotswap.js
            if (typeof ModelRegistry !== 'undefined') {
                Object.entries(ModelRegistry).forEach(([key, model]) => {
                    if (model && !key.startsWith('plugin:')) {
                        this.allModels.set(key, {
                            id: key,
                            name: model.name,
                            icon: model.icon,
                            type: 'built-in',
                            description: model.description,
                            context: model.context_size,
                            temperature: model.temperature,
                            specialization: model.specialization,
                            color: model.color,
                            hotkey: model.hotkey,
                            favorite: this.favorites.has(key)
                        });
                    }
                });
            }

            // Load Ollama models
            try {
                const response = await fetch('http://localhost:11441/ollama/api/tags');
                const data = await response.json();

                if (data.models && Array.isArray(data.models)) {
                    data.models.forEach(model => {
                        const modelId = `ollama:${model.name}`;
                        const sizeGB = (model.size / (1024 * 1024 * 1024)).toFixed(2);

                        this.allModels.set(modelId, {
                            id: modelId,
                            name: model.name,
                            icon: '🦙',
                            type: 'ollama',
                            description: `Ollama model (${sizeGB} GB)`,
                            context: model.context || 4096,
                            temperature: 0.7,
                            specialization: 'External Model',
                            color: '#a855f7',
                            size: model.size,
                            sizeGB: sizeGB,
                            favorite: this.favorites.has(modelId)
                        });
                    });

                    console.log(`[ModelBrowser] ✅ Loaded ${data.models.length} Ollama models`);
                }
            } catch (e) {
                console.warn('[ModelBrowser] ⚠️ Ollama not available:', e.message);
            }

            // Load local models (via Orchestra server)
            try {
                const response = await fetch('http://localhost:11441/api/tags');
                const data = await response.json();

                if (data.models && Array.isArray(data.models)) {
                    data.models.forEach(model => {
                        const modelId = `local:${model.name}`;

                        // Skip if already loaded from Ollama
                        if (!this.allModels.has(modelId)) {
                            this.allModels.set(modelId, {
                                id: modelId,
                                name: model.name,
                                icon: '💾',
                                type: 'local',
                                description: model.description || 'Local model',
                                context: model.context_size || 2048,
                                temperature: 0.7,
                                specialization: 'Local Model',
                                color: '#00ff88',
                                favorite: this.favorites.has(modelId)
                            });
                        }
                    });

                    console.log(`[ModelBrowser] ✅ Loaded ${data.models.length} local models`);
                }
            } catch (e) {
                console.warn('[ModelBrowser] ⚠️ Local models not available:', e.message);
            }

            console.log(`[ModelBrowser] 🎯 Total models loaded: ${this.allModels.size}`);
            this.isLoading = false;

            return Array.from(this.allModels.values());
        } catch (error) {
            console.error('[ModelBrowser] Error loading models:', error);
            this.isLoading = false;
            return [];
        }
    }

    getFilteredModels() {
        let filtered = Array.from(this.allModels.values());

        // Apply type filter
        if (this.currentFilter !== 'all') {
            switch (this.currentFilter) {
                case 'favorites':
                    filtered = filtered.filter(m => m.favorite);
                    break;
                case 'recent':
                    filtered = filtered.filter(m => this.recentlyUsed.has(m.id));
                    break;
                case 'built-in':
                    filtered = filtered.filter(m => m.type === 'built-in');
                    break;
                case 'ollama':
                    filtered = filtered.filter(m => m.type === 'ollama');
                    break;
                case 'local':
                    filtered = filtered.filter(m => m.type === 'local');
                    break;
            }
        }

        // Apply search filter
        if (this.searchQuery) {
            const query = this.searchQuery.toLowerCase();
            filtered = filtered.filter(m =>
                m.name.toLowerCase().includes(query) ||
                m.description.toLowerCase().includes(query) ||
                m.specialization.toLowerCase().includes(query)
            );
        }

        // Sort: favorites first, then recently used, then alphabetically
        filtered.sort((a, b) => {
            if (a.favorite !== b.favorite) return b.favorite ? 1 : -1;
            if (this.recentlyUsed.has(a.id) !== this.recentlyUsed.has(b.id)) {
                return this.recentlyUsed.has(b.id) ? 1 : -1;
            }
            return a.name.localeCompare(b.name);
        });

        return filtered;
    }

    toggleFavorite(modelId) {
        if (this.favorites.has(modelId)) {
            this.favorites.delete(modelId);
        } else {
            this.favorites.add(modelId);
        }

        this.saveFavorites();

        const model = this.allModels.get(modelId);
        if (model) {
            model.favorite = this.favorites.has(modelId);
        }

        console.log(`[ModelBrowser] ⭐ Toggled favorite: ${modelId}`);
    }

    markAsUsed(modelId) {
        this.recentlyUsed.add(modelId);
        if (this.recentlyUsed.size > 20) {
            const arr = Array.from(this.recentlyUsed);
            this.recentlyUsed = new Set(arr.slice(-20));
        }
        this.saveRecentlyUsed();
    }

    saveFavorites() {
        localStorage.setItem('modelBrowser:favorites', JSON.stringify(Array.from(this.favorites)));
    }

    loadFavorites() {
        try {
            const stored = localStorage.getItem('modelBrowser:favorites');
            return new Set(stored ? JSON.parse(stored) : []);
        } catch (e) {
            return new Set();
        }
    }

    saveRecentlyUsed() {
        localStorage.setItem('modelBrowser:recently_used', JSON.stringify(Array.from(this.recentlyUsed)));
    }

    loadRecentlyUsed() {
        try {
            const stored = localStorage.getItem('modelBrowser:recently_used');
            return new Set(stored ? JSON.parse(stored) : []);
        } catch (e) {
            return new Set();
        }
    }

    openBrowser() {
        if (document.getElementById('model-browser-modal')) {
            document.getElementById('model-browser-modal').remove();
            return;
        }

        const modal = document.createElement('div');
        modal.id = 'model-browser-modal';
        modal.style.cssText = `
            position: fixed;
            top: 0;
            left: 0;
            right: 0;
            bottom: 0;
            background: rgba(0, 0, 0, 0.7);
            z-index: 10000;
            display: flex;
            align-items: center;
            justify-content: center;
            backdrop-filter: blur(5px);
        `;

        const container = document.createElement('div');
        container.style.cssText = `
            background: rgba(10, 10, 30, 0.98);
            border: 3px solid var(--cyan);
            border-radius: 20px;
            padding: 30px;
            width: 90%;
            max-width: 1000px;
            max-height: 90vh;
            display: flex;
            flex-direction: column;
            box-shadow: 0 20px 60px rgba(0,212,255,0.6);
            overflow: hidden;
        `;

        container.innerHTML = `
            <!-- Header -->
            <div style="display: flex; justify-content: space-between; align-items: center; margin-bottom: 20px; padding-bottom: 15px; border-bottom: 2px solid var(--cyan);">
                <div>
                    <h2 style="color: var(--cyan); margin: 0; font-size: 24px;">🎯 Model Browser</h2>
                    <div style="font-size: 12px; color: #888; margin-top: 5px;">Search and select from ${this.allModels.size}+ available models</div>
                </div>
                <button id="close-model-browser" style="background: var(--red); color: white; border: none; padding: 10px 20px; border-radius: 8px; cursor: pointer; font-weight: bold; font-size: 14px;">✕ Close</button>
            </div>

            <!-- Search & Filter Bar -->
            <div style="display: flex; gap: 15px; margin-bottom: 20px; flex-wrap: wrap;">
                <input
                    type="text"
                    id="model-search"
                    placeholder="Search models by name, description, or specialization..."
                    style="
                        flex: 1;
                        min-width: 300px;
                        padding: 12px 15px;
                        background: rgba(0,0,0,0.5);
                        border: 1px solid rgba(0,212,255,0.3);
                        border-radius: 8px;
                        color: white;
                        font-size: 13px;
                    "
                />
                <button id="filter-all" class="model-filter-btn" data-filter="all" style="padding: 10px 15px; background: rgba(0,212,255,0.2); border: 1px solid var(--cyan); border-radius: 8px; color: var(--cyan); cursor: pointer; font-weight: bold; font-size: 12px;">All (${this.allModels.size})</button>
                <button id="filter-favorites" class="model-filter-btn" data-filter="favorites" style="padding: 10px 15px; background: rgba(0,0,0,0.5); border: 1px solid rgba(255,255,255,0.2); border-radius: 8px; color: #888; cursor: pointer; font-weight: bold; font-size: 12px;">⭐ Favorites (${this.favorites.size})</button>
                <button id="filter-recent" class="model-filter-btn" data-filter="recent" style="padding: 10px 15px; background: rgba(0,0,0,0.5); border: 1px solid rgba(255,255,255,0.2); border-radius: 8px; color: #888; cursor: pointer; font-weight: bold; font-size: 12px;">📝 Recent</button>
                <button id="filter-ollama" class="model-filter-btn" data-filter="ollama" style="padding: 10px 15px; background: rgba(0,0,0,0.5); border: 1px solid rgba(255,255,255,0.2); border-radius: 8px; color: #888; cursor: pointer; font-weight: bold; font-size: 12px;">🦙 Ollama</button>
            </div>

            <!-- Models Grid -->
            <div id="models-grid" style="flex: 1; overflow-y: auto; display: grid; grid-template-columns: repeat(auto-fill, minmax(280px, 1fr)); gap: 15px; padding-right: 10px;">
                <!-- Models will be inserted here -->
            </div>

            <!-- Footer -->
            <div style="margin-top: 20px; padding-top: 15px; border-top: 1px solid rgba(0,212,255,0.2); font-size: 11px; color: #888;">
                💡 <strong>Tips:</strong> Click to select, star icon to favorite, Ctrl+M to open this browser anytime
            </div>
        `;

        modal.appendChild(container);
        document.body.appendChild(modal);

        // Close button
        document.getElementById('close-model-browser').onclick = () => modal.remove();

        // Filter buttons
        document.querySelectorAll('.model-filter-btn').forEach(btn => {
            btn.onclick = (e) => {
                document.querySelectorAll('.model-filter-btn').forEach(b => {
                    b.style.background = 'rgba(0,0,0,0.5)';
                    b.style.borderColor = 'rgba(255,255,255,0.2)';
                    b.style.color = '#888';
                });
                e.target.style.background = 'rgba(0,212,255,0.2)';
                e.target.style.borderColor = 'var(--cyan)';
                e.target.style.color = 'var(--cyan)';

                this.currentFilter = e.target.dataset.filter;
                this.renderModels();
            };
        });

        // Search input
        document.getElementById('model-search').oninput = (e) => {
            this.searchQuery = e.target.value;
            this.renderModels();
        };

        // Close on backdrop click
        modal.onclick = (e) => {
            if (e.target === modal) modal.remove();
        };

        this.renderModels();
    }

    renderModels() {
        const grid = document.getElementById('models-grid');
        if (!grid) return;

        const models = this.getFilteredModels();
        grid.innerHTML = '';

        if (models.length === 0) {
            grid.innerHTML = `<div style="grid-column: 1/-1; padding: 40px; text-align: center; color: #888;">No models found matching your search</div>`;
            return;
        }

        models.forEach(model => {
            const card = document.createElement('div');
            card.style.cssText = `
                padding: 15px;
                background: rgba(0,0,0,0.5);
                border: 1px solid rgba(255,255,255,0.1);
                border-radius: 12px;
                cursor: pointer;
                transition: all 0.3s;
                display: flex;
                flex-direction: column;
            `;

            card.onmouseover = () => {
                card.style.background = 'rgba(0,0,0,0.8)';
                card.style.borderColor = model.color;
                card.style.boxShadow = `0 0 20px ${model.color}40`;
            };

            card.onmouseout = () => {
                card.style.background = 'rgba(0,0,0,0.5)';
                card.style.borderColor = 'rgba(255,255,255,0.1)';
                card.style.boxShadow = 'none';
            };

            card.onclick = () => {
                this.selectModel(model);
            };

            const favoriteBtn = document.createElement('button');
            favoriteBtn.style.cssText = `
                position: absolute;
                top: 10px;
                right: 10px;
                background: none;
                border: none;
                font-size: 18px;
                cursor: pointer;
                padding: 5px;
            `;
            favoriteBtn.textContent = model.favorite ? '⭐' : '☆';
            favoriteBtn.onclick = (e) => {
                e.stopPropagation();
                this.toggleFavorite(model.id);
                favoriteBtn.textContent = this.favorites.has(model.id) ? '⭐' : '☆';
            };

            card.innerHTML = `
                <div style="display: flex; align-items: center; gap: 10px; margin-bottom: 10px; position: relative;">
                    <div style="font-size: 32px;">${model.icon}</div>
                    <div style="flex: 1;">
                        <div style="font-size: 14px; font-weight: bold; color: ${model.color};">${model.name}</div>
                        <div style="font-size: 10px; color: #888;">${model.type}</div>
                    </div>
                </div>
                <div style="font-size: 11px; color: #aaa; margin-bottom: 8px; line-height: 1.4;">${model.description}</div>
                <div style="display: flex; gap: 10px; margin-bottom: 8px; flex-wrap: wrap; font-size: 9px; color: #666;">
                    <div style="background: rgba(0,212,255,0.2); padding: 3px 8px; border-radius: 4px; color: var(--cyan);">📊 ${(model.context / 1024).toFixed(0)}K ctx</div>
                    ${model.temperature ? `<div style="background: rgba(255,107,53,0.2); padding: 3px 8px; border-radius: 4px; color: var(--orange);">🌡️ ${model.temperature}</div>` : ''}
                    ${model.sizeGB ? `<div style="background: rgba(168,85,247,0.2); padding: 3px 8px; border-radius: 4px; color: var(--purple);">💾 ${model.sizeGB}GB</div>` : ''}
                </div>
                <div style="font-size: 10px; color: #666; padding-top: 8px; border-top: 1px solid rgba(255,255,255,0.1);">
                    ${model.specialization}
                </div>
            `;

            card.appendChild(favoriteBtn);
            grid.appendChild(card);
        });

        console.log(`[ModelBrowser] Rendered ${models.length} models`);
    }

    selectModel(model) {
        console.log(`[ModelBrowser] 🎯 Selected model: ${model.name}`);
        this.markAsUsed(model.id);

        // Close modal
        const modal = document.getElementById('model-browser-modal');
        if (modal) modal.remove();

        // Swap model using hotswap system if available
        if (typeof swapModel === 'function') {
            swapModel(model.id);
        } else if (model.type === 'ollama' && typeof assignModelToSlot === 'function') {
            // Try to find an empty plugin slot
            for (let i = 1; i <= 6; i++) {
                const slotKey = `plugin:${i}`;
                if (!ModelRegistry[slotKey]) {
                    assignModelToSlot(slotKey, model.name);
                    break;
                }
            }
        }

        // Dispatch event for other components
        document.dispatchEvent(new CustomEvent('model-selected', {
            detail: { model: model }
        }));
    }
}

// Initialize on page load
let modelBrowser;
if (document.readyState === 'loading') {
    document.addEventListener('DOMContentLoaded', () => {
        modelBrowser = new ModelBrowser();
    });
} else {
    modelBrowser = new ModelBrowser();
}

// Register hotkey (Ctrl+M)
document.addEventListener('keydown', (e) => {
    if (e.ctrlKey && e.key === 'm') {
        e.preventDefault();
        if (modelBrowser) modelBrowser.openBrowser();
    }
});

console.log('[ModelBrowser] ✅ Model browser loaded - Press Ctrl+M to open');
