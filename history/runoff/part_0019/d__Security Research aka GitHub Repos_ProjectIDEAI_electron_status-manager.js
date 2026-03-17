/**
 * BigDaddyG IDE - Centralized Status Manager
 * Single source of truth for Orchestra, Ollama, and service health
 */

(function() {
'use strict';

class StatusManager {
    constructor() {
        this.status = {
            orchestra: {
                running: null,
                data: null,
                lastCheck: null
            },
            ollama: {
                running: null,
                models: [],
                lastCheck: null
            },
            memory: {
                available: false,
                lastCheck: null
            }
        };
        
        this.listeners = new Map();
        this.pollingInterval = null;
        this.pollingRate = 3000; // 3 seconds
        
        console.log('[StatusManager] 🔄 Initializing status manager...');
        this.init();
    }
    
    init() {
        // Start polling
        this.startPolling();
        
        // Expose to window
        window.statusManager = this;
        
        console.log('[StatusManager] ✅ Status manager ready');
    }
    
    startPolling() {
        if (this.pollingInterval) {
            return;
        }
        
        // Initial check delayed to let services start
        setTimeout(() => this.checkAllStatus(), 2000);
        
        // Regular polling
        this.pollingInterval = setInterval(() => {
            this.checkAllStatus();
        }, this.pollingRate);
        
        console.log('[StatusManager] 🔄 Polling started');
    }
    
    stopPolling() {
        if (this.pollingInterval) {
            clearInterval(this.pollingInterval);
            this.pollingInterval = null;
            console.log('[StatusManager] ⏸️ Polling stopped');
        }
    }
    
    async checkAllStatus() {
        await Promise.all([
            this.checkOrchestraStatus(),
            this.checkOllamaStatus(),
            this.checkMemoryStatus()
        ]);
    }
    
    async checkOrchestraStatus() {
        try {
            const response = await fetch('http://localhost:11441/health', {
                method: 'GET',
                signal: AbortSignal.timeout(3000)
            });
            
            if (response.ok) {
                const data = await response.json();
                this.updateStatus('orchestra', true, data);
            } else {
                this.updateStatus('orchestra', false, null);
            }
        } catch (error) {
            this.updateStatus('orchestra', false, null);
        }
    }
    
    async checkOllamaStatus() {
        try {
            const response = await fetch('http://localhost:11434/api/tags', {
                method: 'GET',
                signal: AbortSignal.timeout(3000)
            });
            
            if (response.ok) {
                const data = await response.json();
                this.updateStatus('ollama', true, data);
            } else {
                this.updateStatus('ollama', false, null);
            }
        } catch (error) {
            this.updateStatus('ollama', false, null);
        }
    }
    
    async checkMemoryStatus() {
        const available = !!(window.memory && typeof window.memory.getStats === 'function');
        this.updateStatus('memory', available, null);
    }
    
    updateStatus(service, running, data) {
        const oldStatus = this.status[service].running;
        
        this.status[service] = {
            running: running,
            data: data,
            lastCheck: new Date()
        };
        
        // Notify listeners only if status changed
        if (oldStatus !== running) {
            this.notifyListeners(service, running, data);
        }
    }
    
    subscribe(service, callback) {
        if (!this.listeners.has(service)) {
            this.listeners.set(service, []);
        }
        
        this.listeners.get(service).push(callback);
        
        // Immediately call with current status
        const status = this.status[service];
        if (status.lastCheck) {
            callback(status.running, status.data);
        }
        
        // Return unsubscribe function
        return () => {
            const listeners = this.listeners.get(service);
            if (listeners) {
                const index = listeners.indexOf(callback);
                if (index > -1) {
                    listeners.splice(index, 1);
                }
            }
        };
    }
    
    notifyListeners(service, running, data) {
        const listeners = this.listeners.get(service);
        if (listeners) {
            listeners.forEach(callback => {
                try {
                    callback(running, data);
                } catch (error) {
                    console.error(`[StatusManager] Error in listener for ${service}:`, error);
                }
            });
        }
    }
    
    getStatus(service) {
        return this.status[service];
    }
    
    isServiceRunning(service) {
        return this.status[service]?.running === true;
    }
    
    cleanup() {
        this.stopPolling();
        this.listeners.clear();
        console.log('[StatusManager] 🧹 Cleaned up');
    }
}

// Initialize on DOM ready
if (document.readyState === 'loading') {
    document.addEventListener('DOMContentLoaded', () => {
        new StatusManager();
    });
} else {
    new StatusManager();
}

})();
