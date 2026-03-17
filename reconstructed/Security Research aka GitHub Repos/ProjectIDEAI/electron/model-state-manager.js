/**
 * BigDaddyG IDE - Unified Model State Manager
 * Single source of truth for model selection across all UI components
 */

(function() {
'use strict';

class ModelStateManager {
    constructor() {
        this.activeModel = null;
        this.availableModels = [];
        this.listeners = new Map();
        this.loading = false;
        
        console.log('[ModelState] 🎯 Initializing model state manager...');
        this.init();
    }
    
    async init() {
        // Load models from Orchestra
        await this.loadModels();
        
        // Listen for model swap events
        document.addEventListener('model-swapped', (e) => {
            this.handleModelSwap(e.detail);
        });
        
        // Expose to window
        window.modelState = this;
        
        console.log('[ModelState] ✅ Model state manager ready');
    }
    
    async loadModels() {
        if (this.loading) return;
        
        this.loading = true;
        
        try {
            // Try Orchestra health endpoint for models
            if (window.statusManager && window.statusManager.isServiceRunning('orchestra')) {
                const response = await fetch('http://localhost:11441/health');
                if (response.ok) {
                    const data = await response.json();
                    if (data.models) {
                        this.availableModels = data.models;
                        if (data.models.length > 0 && !this.activeModel) {
                            this.setActiveModel(data.models[0].id, data.models[0]);
                        }
                    }
                }
            }
            
            // Fallback: get from model hot-swap registry
            if (this.availableModels.length === 0 && window.ModelRegistry) {
                this.availableModels = Object.entries(window.ModelRegistry)
                    .filter(([key, value]) => value !== null)
                    .map(([key, value]) => ({
                        id: key,
                        ...value
                    }));
                    
                if (this.availableModels.length > 0 && !this.activeModel) {
                    const first = this.availableModels[0];
                    this.setActiveModel(first.id, first);
                }
            }
            
            console.log(`[ModelState] Loaded ${this.availableModels.length} models`);
            this.notifyListeners('models-loaded', this.availableModels);
            
        } catch (error) {
            console.warn('[ModelState] Failed to load models:', error);
        } finally {
            this.loading = false;
        }
    }
    
    setActiveModel(modelId, modelData = null) {
        const oldModel = this.activeModel;
        
        this.activeModel = {
            id: modelId,
            ...modelData
        };
        
        console.log(`[ModelState] Active model: ${modelId}`);
        
        // Notify all listeners
        this.notifyListeners('model-changed', {
            old: oldModel,
            new: this.activeModel
        });
        
        // Update all dropdowns/selectors
        this.syncAllSelectors(modelId);
    }
    
    getActiveModel() {
        return this.activeModel;
    }
    
    getAvailableModels() {
        return this.availableModels;
    }
    
    handleModelSwap(detail) {
        if (detail.to !== this.activeModel?.id) {
            this.setActiveModel(detail.to, detail.model);
        }
    }
    
    subscribe(event, callback) {
        if (!this.listeners.has(event)) {
            this.listeners.set(event, []);
        }
        
        this.listeners.get(event).push(callback);
        
        // Immediate callback with current state
        if (event === 'model-changed' && this.activeModel) {
            callback({ old: null, new: this.activeModel });
        } else if (event === 'models-loaded' && this.availableModels.length > 0) {
            callback(this.availableModels);
        }
        
        // Return unsubscribe function
        return () => {
            const listeners = this.listeners.get(event);
            if (listeners) {
                const index = listeners.indexOf(callback);
                if (index > -1) {
                    listeners.splice(index, 1);
                }
            }
        };
    }
    
    notifyListeners(event, data) {
        const listeners = this.listeners.get(event);
        if (listeners) {
            listeners.forEach(callback => {
                try {
                    callback(data);
                } catch (error) {
                    console.error(`[ModelState] Error in listener for ${event}:`, error);
                }
            });
        }
    }
    
    syncAllSelectors(modelId) {
        // Find all model selector dropdowns
        const selectors = document.querySelectorAll(
            '#agent-model-select, #model-select, .model-selector, [data-model-selector]'
        );
        
        selectors.forEach(selector => {
            if (selector.value !== modelId) {
                selector.value = modelId;
                
                // Trigger change event if needed
                const event = new Event('change', { bubbles: true });
                selector.dispatchEvent(event);
            }
        });
    }
    
    async refreshModels() {
        console.log('[ModelState] Refreshing models...');
        this.availableModels = [];
        await this.loadModels();
    }
}

// Initialize on DOM ready
if (document.readyState === 'loading') {
    document.addEventListener('DOMContentLoaded', () => {
        new ModelStateManager();
    });
} else {
    new ModelStateManager();
}

})();
