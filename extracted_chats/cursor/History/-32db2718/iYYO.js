/**
 * IndexedDB Storage Manager
 * 
 * High-performance storage for large data (files, chat history, memory)
 * localStorage has 5-10MB limit - IndexedDB has virtually unlimited storage
 */

(function() {
'use strict';

class IndexedDBStorage {
    constructor() {
        this.db = null;
        this.dbName = 'BigDaddyG-IDE';
        this.version = 1;
        this.ready = false;
        
        this.init();
    }
    
    async init() {
        try {
            this.db = await this.openDatabase();
            this.ready = true;
            console.log('[IndexedDB] ✅ Database ready');
            
            // Migrate from localStorage if needed
            await this.migrateFromLocalStorage();
        } catch (error) {
            console.error('[IndexedDB] ❌ Initialization failed:', error);
            console.warn('[IndexedDB] ⚠️ Falling back to localStorage');
        }
    }
    
    openDatabase() {
        return new Promise((resolve, reject) => {
            const request = indexedDB.open(this.dbName, this.version);
            
            request.onerror = () => reject(request.error);
            request.onsuccess = () => resolve(request.result);
            
            request.onupgradeneeded = (event) => {
                const db = event.target.result;
                
                // Create object stores
                if (!db.objectStoreNames.contains('tabs')) {
                    db.createObjectStore('tabs', { keyPath: 'id' });
                }
                
                if (!db.objectStoreNames.contains('chatHistory')) {
                    const chatStore = db.createObjectStore('chatHistory', { keyPath: 'id', autoIncrement: true });
                    chatStore.createIndex('timestamp', 'timestamp', { unique: false });
                    chatStore.createIndex('sessionId', 'sessionId', { unique: false });
                }
                
                if (!db.objectStoreNames.contains('fileCache')) {
                    const fileStore = db.createObjectStore('fileCache', { keyPath: 'path' });
                    fileStore.createIndex('lastAccess', 'lastAccess', { unique: false });
                }
                
                if (!db.objectStoreNames.contains('memory')) {
                    const memStore = db.createObjectStore('memory', { keyPath: 'id', autoIncrement: true });
                    memStore.createIndex('timestamp', 'timestamp', { unique: false });
                    memStore.createIndex('type', 'type', { unique: false });
                }
                
                console.log('[IndexedDB] 📦 Object stores created');
            };
        });
    }
    
    // ========================================================================
    // TAB STORAGE (Replaces localStorage for recovery)
    // ========================================================================
    
    async saveTabs(tabState) {
        if (!this.ready) {
            console.warn('[IndexedDB] Not ready, using localStorage fallback');
            localStorage.setItem('bigdaddyg-tab-recovery', JSON.stringify(tabState));
            return;
        }
        
        try {
            const tx = this.db.transaction(['tabs'], 'readwrite');
            const store = tx.objectStore('tabs');
            
            await store.put({
                id: 'current-session',
                tabs: tabState,
                timestamp: Date.now()
            });
            
            console.log('[IndexedDB] 💾 Tab state saved');
        } catch (error) {
            console.error('[IndexedDB] ❌ Failed to save tabs:', error);
        }
    }
    
    async loadTabs() {
        if (!this.ready) {
            const saved = localStorage.getItem('bigdaddyg-tab-recovery');
            return saved ? JSON.parse(saved) : null;
        }
        
        try {
            const tx = this.db.transaction(['tabs'], 'readonly');
            const store = tx.objectStore('tabs');
            const request = store.get('current-session');
            
            return new Promise((resolve, reject) => {
                request.onsuccess = () => {
                    const data = request.result;
                    resolve(data ? data.tabs : null);
                };
                request.onerror = () => reject(request.error);
            });
        } catch (error) {
            console.error('[IndexedDB] ❌ Failed to load tabs:', error);
            return null;
        }
    }
    
    // ========================================================================
    // CHAT HISTORY (Unlimited storage)
    // ========================================================================
    
    async saveChatMessage(message) {
        if (!this.ready) return false;
        
        try {
            const tx = this.db.transaction(['chatHistory'], 'readwrite');
            const store = tx.objectStore('chatHistory');
            
            await store.add({
                ...message,
                timestamp: Date.now()
            });
            
            return true;
        } catch (error) {
            console.error('[IndexedDB] ❌ Failed to save chat message:', error);
            return false;
        }
    }
    
    async getChatHistory(sessionId = null, limit = 100) {
        if (!this.ready) return [];
        
        try {
            const tx = this.db.transaction(['chatHistory'], 'readonly');
            const store = tx.objectStore('chatHistory');
            
            let request;
            if (sessionId) {
                const index = store.index('sessionId');
                request = index.getAll(sessionId);
            } else {
                request = store.getAll();
            }
            
            return new Promise((resolve, reject) => {
                request.onsuccess = () => {
                    const messages = request.result || [];
                    // Return latest messages first, limited
                    resolve(messages.reverse().slice(0, limit));
                };
                request.onerror = () => reject(request.error);
            });
        } catch (error) {
            console.error('[IndexedDB] ❌ Failed to get chat history:', error);
            return [];
        }
    }
    
    async clearOldChatHistory(daysToKeep = 30) {
        if (!this.ready) return 0;
        
        const cutoffTime = Date.now() - (daysToKeep * 24 * 60 * 60 * 1000);
        
        try {
            const tx = this.db.transaction(['chatHistory'], 'readwrite');
            const store = tx.objectStore('chatHistory');
            const index = store.index('timestamp');
            const request = index.openCursor();
            
            let deleted = 0;
            
            return new Promise((resolve, reject) => {
                request.onsuccess = (event) => {
                    const cursor = event.target.result;
                    if (cursor) {
                        if (cursor.value.timestamp < cutoffTime) {
                            cursor.delete();
                            deleted++;
                        }
                        cursor.continue();
                    } else {
                        console.log(`[IndexedDB] 🧹 Deleted ${deleted} old messages`);
                        resolve(deleted);
                    }
                };
                request.onerror = () => reject(request.error);
            });
        } catch (error) {
            console.error('[IndexedDB] ❌ Failed to clear history:', error);
            return 0;
        }
    }
    
    // ========================================================================
    // FILE CACHE (Cache opened files for quick re-open)
    // ========================================================================
    
    async cacheFile(filePath, content, metadata = {}) {
        if (!this.ready) return false;
        
        try {
            const tx = this.db.transaction(['fileCache'], 'readwrite');
            const store = tx.objectStore('fileCache');
            
            await store.put({
                path: filePath,
                content,
                metadata,
                lastAccess: Date.now(),
                size: content.length
            });
            
            return true;
        } catch (error) {
            console.error('[IndexedDB] ❌ Failed to cache file:', error);
            return false;
        }
    }
    
    async getCachedFile(filePath) {
        if (!this.ready) return null;
        
        try {
            const tx = this.db.transaction(['fileCache'], 'readwrite');
            const store = tx.objectStore('fileCache');
            
            const request = store.get(filePath);
            
            return new Promise((resolve, reject) => {
                request.onsuccess = () => {
                    const data = request.result;
                    if (data) {
                        // Update last access time
                        data.lastAccess = Date.now();
                        store.put(data);
                    }
                    resolve(data);
                };
                request.onerror = () => reject(request.error);
            });
        } catch (error) {
            console.error('[IndexedDB] ❌ Failed to get cached file:', error);
            return null;
        }
    }
    
    async clearFileCache() {
        if (!this.ready) return false;
        
        try {
            const tx = this.db.transaction(['fileCache'], 'readwrite');
            const store = tx.objectStore('fileCache');
            await store.clear();
            console.log('[IndexedDB] 🧹 File cache cleared');
            return true;
        } catch (error) {
            console.error('[IndexedDB] ❌ Failed to clear cache:', error);
            return false;
        }
    }
    
    // ========================================================================
    // MEMORY STORAGE (Persistent AI memory)
    // ========================================================================
    
    async storeMemory(content, metadata = {}) {
        if (!this.ready) return false;
        
        try {
            const tx = this.db.transaction(['memory'], 'readwrite');
            const store = tx.objectStore('memory');
            
            await store.add({
                content,
                metadata,
                timestamp: Date.now()
            });
            
            return true;
        } catch (error) {
            console.error('[IndexedDB] ❌ Failed to store memory:', error);
            return false;
        }
    }
    
    async searchMemory(query, limit = 20) {
        if (!this.ready) return [];
        
        try {
            const tx = this.db.transaction(['memory'], 'readonly');
            const store = tx.objectStore('memory');
            const request = store.getAll();
            
            return new Promise((resolve, reject) => {
                request.onsuccess = () => {
                    const memories = request.result || [];
                    
                    // Simple keyword search (can be enhanced with fuzzy search)
                    const results = memories.filter(mem => 
                        mem.content.toLowerCase().includes(query.toLowerCase())
                    ).slice(0, limit);
                    
                    resolve(results);
                };
                request.onerror = () => reject(request.error);
            });
        } catch (error) {
            console.error('[IndexedDB] ❌ Failed to search memory:', error);
            return [];
        }
    }
    
    // ========================================================================
    // UTILITY METHODS
    // ========================================================================
    
    async getStorageUsage() {
        if (!this.ready) return { used: 0, quota: 0 };
        
        try {
            if (navigator.storage && navigator.storage.estimate) {
                const estimate = await navigator.storage.estimate();
                return {
                    used: estimate.usage,
                    quota: estimate.quota,
                    usedMB: Math.round(estimate.usage / 1024 / 1024),
                    quotaMB: Math.round(estimate.quota / 1024 / 1024),
                    percentage: Math.round((estimate.usage / estimate.quota) * 100)
                };
            }
        } catch (error) {
            console.error('[IndexedDB] ❌ Failed to get storage usage:', error);
        }
        
        return { used: 0, quota: 0 };
    }
    
    async migrateFromLocalStorage() {
        try {
            // Migrate tab recovery data
            const tabData = localStorage.getItem('bigdaddyg-tab-recovery');
            if (tabData) {
                const parsed = JSON.parse(tabData);
                await this.saveTabs(parsed);
                console.log('[IndexedDB] ✅ Migrated tab data from localStorage');
            }
        } catch (error) {
            console.warn('[IndexedDB] ⚠️ Migration failed:', error);
        }
    }
}

// ============================================================================
// GLOBAL EXPOSURE
// ============================================================================

window.idbStorage = new IndexedDBStorage();

// Expose convenient API
window.storage = {
    // Tabs
    saveTabs: (tabState) => window.idbStorage.saveTabs(tabState),
    loadTabs: () => window.idbStorage.loadTabs(),
    
    // Chat
    saveChat: (message) => window.idbStorage.saveChatMessage(message),
    getChatHistory: (sessionId, limit) => window.idbStorage.getChatHistory(sessionId, limit),
    clearOldChat: (days) => window.idbStorage.clearOldChatHistory(days),
    
    // Files
    cacheFile: (path, content, metadata) => window.idbStorage.cacheFile(path, content, metadata),
    getCachedFile: (path) => window.idbStorage.getCachedFile(path),
    clearCache: () => window.idbStorage.clearFileCache(),
    
    // Memory
    storeMemory: (content, metadata) => window.idbStorage.storeMemory(content, metadata),
    searchMemory: (query, limit) => window.idbStorage.searchMemory(query, limit),
    
    // Utilities
    getUsage: () => window.idbStorage.getStorageUsage(),
    isReady: () => window.idbStorage.ready
};

console.log('[IndexedDB] 📦 IndexedDB storage module loaded');
console.log('[IndexedDB] 💡 Use storage.getUsage() to check storage limits');

})();

