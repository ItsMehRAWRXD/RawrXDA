/**
 * Global Functions
 * 
 * Utility functions used across the IDE
 */

(function() {
'use strict';

// Expose globally for index.html onclick handlers
window.globalFunctions = {
    // Safe function caller - prevents "is not a function" errors
    safeCall: (obj, method, ...args) => {
        try {
            if (obj && typeof obj[method] === 'function') {
                return obj[method](...args);
            } else {
                console.warn(`[GlobalFunctions] ⚠️ ${method} is not a function on`, obj);
                return null;
            }
        } catch (error) {
            console.error(`[GlobalFunctions] ❌ Error calling ${method}:`, error);
            return null;
        }
    },
    
    // Check if function exists
    functionExists: (obj, method) => {
        return obj && typeof obj[method] === 'function';
    }
};

// Expose common missing functions as no-ops until they're properly loaded
window.ensureFunctionExists = (name, fallback = () => {}) => {
    if (typeof window[name] !== 'function') {
        window[name] = fallback;
        console.log(`[GlobalFunctions] 📝 Created placeholder for ${name}`);
    }
};

// CTRL+L Chat Box Handler
window.handleCtrlL = () => {
    if (window.floatingChat) {
        window.floatingChat.toggle();
        return true;
    }
    console.warn('[GlobalFunctions] ⚠️ FloatingChat not available');
    return false;
};

// Context Management (1M tokens)
window.contextManager = {
    maxTokens: 1000000, // 1M context window
    currentTokens: 0,
    
    addToContext: (content, role = 'user') => {
        const tokens = content.length; // Simple token estimation
        window.contextManager.currentTokens += tokens;
        
        // Trim if over limit
        if (window.contextManager.currentTokens > window.contextManager.maxTokens) {
            window.contextManager.trimContext();
        }
        
        console.log(`[Context] Added ${tokens} tokens (${window.contextManager.currentTokens}/${window.contextManager.maxTokens})`);
    },
    
    trimContext: () => {
        const targetTokens = window.contextManager.maxTokens * 0.8; // Keep 80%
        const tokensToRemove = window.contextManager.currentTokens - targetTokens;
        window.contextManager.currentTokens = targetTokens;
        console.log(`[Context] Trimmed ${tokensToRemove} tokens`);
    },
    
    clearContext: () => {
        window.contextManager.currentTokens = 0;
        console.log('[Context] Cleared all context');
    },
    
    getStatus: () => {
        const usage = (window.contextManager.currentTokens / window.contextManager.maxTokens * 100).toFixed(1);
        return {
            current: window.contextManager.currentTokens,
            max: window.contextManager.maxTokens,
            usage: `${usage}%`,
            available: window.contextManager.maxTokens - window.contextManager.currentTokens
        };
    }
};

// Ollama Connection Manager
window.ollamaManager = {
    baseUrl: 'http://localhost:11434',
    orchestraUrl: 'http://localhost:11441',
    availableModels: [],
    isConnected: false,
    
    async checkOllama() {
        try {
            const response = await fetch(`${this.baseUrl}/api/tags`, { 
                method: 'GET',
                timeout: 3000 
            });
            this.isConnected = response.ok;
            if (this.isConnected) {
                console.log('[Ollama] ✅ Connected to local Ollama instance');
                await this.loadModels();
            }
            return response.ok;
        } catch (error) {
            console.log('[Ollama] Not available:', error.message);
            this.isConnected = false;
            return false;
        }
    },
    
    async loadModels() {
        try {
            const response = await fetch(`${this.baseUrl}/api/tags`);
            if (response.ok) {
                const data = await response.json();
                this.availableModels = data.models || [];
                console.log('[Ollama] 📦 Loaded', this.availableModels.length, 'models');
                return this.availableModels;
            }
        } catch (error) {
            console.error('[Ollama] Error loading models:', error);
        }
        return [];
    },
    
    async getModels() {
        if (this.availableModels.length === 0) {
            await this.loadModels();
        }
        return { 
            ollama: this.availableModels, 
            discovered: this.availableModels,
            total: this.availableModels.length 
        };
    },
    
    async sendMessage(message, model = 'auto') {
        // Store in memory before sending
        if (window.memory) {
            await window.memory.store(message, {
                type: 'user_message',
                source: 'chat',
                context: { model, timestamp: new Date().toISOString() }
            });
        }
        
        try {
            // Try Orchestra first, fallback to direct Ollama
            let response = await fetch(`${this.orchestraUrl}/api/chat`, {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ message, model })
            }).catch(() => null);
            
            // If Orchestra fails, try direct Ollama
            if (!response || !response.ok) {
                const selectedModel = model === 'auto' && this.availableModels.length > 0 
                    ? this.availableModels[0].name 
                    : model;
                
                response = await fetch(`${this.baseUrl}/api/generate`, {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify({ 
                        model: selectedModel,
                        prompt: message,
                        stream: false
                    })
                });
            }
            
            if (response && response.ok) {
                const data = await response.json();
                const aiResponse = data.response || data.text || '';
                
                // Store AI response in memory
                if (window.memory && aiResponse) {
                    await window.memory.store(aiResponse, {
                        type: 'ai_response',
                        source: 'chat',
                        context: { model, timestamp: new Date().toISOString() }
                    });
                }
                
                window.contextManager.addToContext(message, 'user');
                window.contextManager.addToContext(aiResponse, 'assistant');
                
                return { ...data, response: aiResponse };
            }
        } catch (error) {
            console.error('[Ollama] Error sending message:', error);
        }
        
        return null;
    },
    
    async autoConnect() {
        console.log('[Ollama] 🔌 Auto-connecting to local models...');
        const connected = await this.checkOllama();
        if (connected) {
            console.log('[Ollama] ✅ Auto-connected successfully');
        } else {
            console.log('[Ollama] ⚠️ No local models detected. Install Ollama for offline AI.');
        }
        return connected;
    }
};

// Auto-connect on startup
setTimeout(() => {
    window.ollamaManager.autoConnect();
}, 1000);

console.log('[GlobalFunctions] 📦 Global functions loaded');

})();
