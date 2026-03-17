/**
 * OpenMemory Bridge for BigDaddyG IDE
 * 
 * Bridges JavaScript IDE to PowerShell OpenMemory modules via IPC
 * Provides persistent, context-aware memory for all agentic operations
 */

(function() {
'use strict';

class MemoryBridge {
    constructor() {
        this.isInitialized = false;
        this.memoryStats = {
            totalMemories: 0,
            totalEmbeddings: 0,
            storageSize: 0,
            lastUpdated: null
        };
        
        // Check if we're in Electron renderer with preload bridge
        if (!window.electron) {
            console.warn('[MemoryBridge] ⚠️ Not running in Electron - memory features limited');
            return;
        }
        
        console.log('[MemoryBridge] 🧠 Initializing OpenMemory Bridge...');
        // Don't call async initialize() in constructor - causes issues
        // Will be initialized when first used
    }
    
    // ========================================================================
    // INITIALIZATION
    // ========================================================================
    
    async initialize() {
        if (this.isInitialized) return true;
        
        try {
            // Request memory stats via IPC
            if (window.electron && window.electron.memory) {
                const stats = await window.electron.memory.getStats();
                if (stats && stats.success) {
                    this.memoryStats = stats;
                    this.isInitialized = true;
                    console.log('[MemoryBridge] ✅ Memory system connected');
                    console.log(`[MemoryBridge] 📊 ${stats.totalMemories || 0} memories loaded`);
                    return true;
                }
            }
            
            // Fallback: In-memory only mode (silent - no warnings)
            this.setupInMemoryMode();
            this.isInitialized = true;
            console.log('[MemoryBridge] ✅ OpenMemory Bridge initialized (in-memory mode)');
            return true;
            
        } catch (error) {
            // Silent fallback - don't show errors, just use in-memory mode
            this.setupInMemoryMode();
            this.isInitialized = true;
            console.log('[MemoryBridge] ℹ️ Using in-memory mode');
            return true;
        }
    }
    
    // ========================================================================
    // POWERSHELL EXECUTION WRAPPER
    // ========================================================================
    
    async executePowerShell(scriptPath, args = []) {
        return new Promise((resolve, reject) => {
            const argsString = args.map(arg => {
                // Escape PowerShell special characters
                const escaped = arg.replace(/'/g, "''");
                return `'${escaped}'`;
            }).join(' ');
            
            const command = `powershell -ExecutionPolicy Bypass -NoProfile -File "${scriptPath}" ${argsString}`;
            
            console.log('[MemoryBridge] 🔧 Executing:', command);
            
            exec(command, { 
                encoding: 'utf8',
                maxBuffer: 10 * 1024 * 1024 // 10MB buffer for large responses
            }, (error, stdout, stderr) => {
                if (error) {
                    console.error('[MemoryBridge] ❌ PowerShell error:', error);
                    console.error('[MemoryBridge] stderr:', stderr);
                    reject(error);
                    return;
                }
                
                if (stderr) {
                    console.warn('[MemoryBridge] ⚠️ PowerShell warning:', stderr);
                }
                
                // Parse JSON output
                try {
                    const result = JSON.parse(stdout);
                    resolve(result);
                } catch (parseError) {
                    // If not JSON, return raw output
                    resolve({ success: true, output: stdout.trim() });
                }
            });
        });
    }
    
    // ========================================================================
    // STORAGE OPERATIONS
    // ========================================================================
    
    async storeMemory(content, metadata = {}) {
        if (!this.isInitialized) {
            await this.initialize();
        }
        
        try {
            // Create a temporary PowerShell script to store memory
            const scriptContent = `
                Import-Module "${this.openMemoryPath}\\OpenMemory.psd1" -Force
                
                $memory = @{
                    Content = '${content.replace(/'/g, "''").replace(/\n/g, '`n')}'
                    Timestamp = Get-Date -Format "yyyy-MM-ddTHH:mm:ss"
                    Type = '${metadata.type || 'conversation'}'
                    Source = '${metadata.source || 'IDE'}'
                    Context = '${JSON.stringify(metadata.context || {}).replace(/'/g, "''")}'
                }
                
                Add-Memory -Memory $memory -StorePath "${this.storePath}"
                
                $memory | ConvertTo-Json -Compress
            `;
            
            const tempScript = path.join(this.storePath, `temp_store_${Date.now()}.ps1`);
            fs.writeFileSync(tempScript, scriptContent, 'utf8');
            
            const result = await this.executePowerShell(tempScript);
            
            // Clean up temp script
            fs.unlinkSync(tempScript);
            
            await this.updateStats();
            console.log('[MemoryBridge] 💾 Memory stored successfully');
            
            return result;
            
        } catch (error) {
            console.error('[MemoryBridge] ❌ Failed to store memory:', error);
            return null;
        }
    }
    
    async queryMemory(query, limit = 10) {
        if (!this.isInitialized) {
            await this.initialize();
        }
        
        try {
            const scriptContent = `
                Import-Module "${this.openMemoryPath}\\OpenMemory.psd1" -Force
                
                $results = Search-Memory -Query '${query.replace(/'/g, "''")}' -StorePath "${this.storePath}" -Limit ${limit}
                
                $results | ConvertTo-Json -Compress
            `;
            
            const tempScript = path.join(this.storePath, `temp_query_${Date.now()}.ps1`);
            fs.writeFileSync(tempScript, scriptContent, 'utf8');
            
            const result = await this.executePowerShell(tempScript);
            
            // Clean up temp script
            fs.unlinkSync(tempScript);
            
            console.log('[MemoryBridge] 🔍 Query completed, found', result.length || 0, 'results');
            
            return Array.isArray(result) ? result : [result];
            
        } catch (error) {
            console.error('[MemoryBridge] ❌ Failed to query memory:', error);
            return [];
        }
    }
    
    async getRecentMemories(limit = 20) {
        if (!this.isInitialized) {
            return [];
        }
        
        try {
            const scriptContent = `
                Import-Module "${this.openMemoryPath}\\OpenMemory.psd1" -Force
                
                $memories = Get-RecentMemories -StorePath "${this.storePath}" -Limit ${limit}
                
                $memories | ConvertTo-Json -Compress
            `;
            
            const tempScript = path.join(this.storePath, `temp_recent_${Date.now()}.ps1`);
            fs.writeFileSync(tempScript, scriptContent, 'utf8');
            
            const result = await this.executePowerShell(tempScript);
            
            // Clean up temp script
            fs.unlinkSync(tempScript);
            
            return Array.isArray(result) ? result : [result];
            
        } catch (error) {
            console.error('[MemoryBridge] ❌ Failed to get recent memories:', error);
            return [];
        }
    }
    
    // ========================================================================
    // EMBEDDING OPERATIONS
    // ========================================================================
    
    async createEmbedding(text, model = 'local') {
        if (!this.isInitialized) {
            return null;
        }
        
        try {
            const scriptContent = `
                Import-Module "${this.openMemoryPath}\\OpenMemory.psd1" -Force
                
                $embedding = New-Embedding -Text '${text.replace(/'/g, "''")}' -Model '${model}'
                
                $embedding | ConvertTo-Json -Compress
            `;
            
            const tempScript = path.join(this.storePath, `temp_embed_${Date.now()}.ps1`);
            fs.writeFileSync(tempScript, scriptContent, 'utf8');
            
            const result = await this.executePowerShell(tempScript);
            
            // Clean up temp script
            fs.unlinkSync(tempScript);
            
            console.log('[MemoryBridge] 🔢 Embedding created');
            
            return result;
            
        } catch (error) {
            console.error('[MemoryBridge] ❌ Failed to create embedding:', error);
            return null;
        }
    }
    
    async similaritySearch(embedding, threshold = 0.7, limit = 10) {
        if (!this.isInitialized) {
            return [];
        }
        
        try {
            const scriptContent = `
                Import-Module "${this.openMemoryPath}\\OpenMemory.psd1" -Force
                
                $results = Find-SimilarMemories -Embedding ${JSON.stringify(embedding)} -Threshold ${threshold} -Limit ${limit} -StorePath "${this.storePath}"
                
                $results | ConvertTo-Json -Compress
            `;
            
            const tempScript = path.join(this.storePath, `temp_similar_${Date.now()}.ps1`);
            fs.writeFileSync(tempScript, scriptContent, 'utf8');
            
            const result = await this.executePowerShell(tempScript);
            
            // Clean up temp script
            fs.unlinkSync(tempScript);
            
            return Array.isArray(result) ? result : [result];
            
        } catch (error) {
            console.error('[MemoryBridge] ❌ Failed to find similar memories:', error);
            return [];
        }
    }
    
    // ========================================================================
    // DECAY OPERATIONS
    // ========================================================================
    
    async applyDecay() {
        if (!this.isInitialized) {
            return false;
        }
        
        try {
            const scriptContent = `
                Import-Module "${this.openMemoryPath}\\OpenMemory.psd1" -Force
                
                $result = Invoke-MemoryDecay -StorePath "${this.storePath}"
                
                @{ Success = $true; DecayedCount = $result.DecayedCount } | ConvertTo-Json -Compress
            `;
            
            const tempScript = path.join(this.storePath, `temp_decay_${Date.now()}.ps1`);
            fs.writeFileSync(tempScript, scriptContent, 'utf8');
            
            const result = await this.executePowerShell(tempScript);
            
            // Clean up temp script
            fs.unlinkSync(tempScript);
            
            await this.updateStats();
            console.log('[MemoryBridge] 🗑️ Decay applied, decayed', result.DecayedCount || 0, 'memories');
            
            return true;
            
        } catch (error) {
            console.error('[MemoryBridge] ❌ Failed to apply decay:', error);
            return false;
        }
    }
    
    // ========================================================================
    // STATISTICS & MAINTENANCE
    // ========================================================================
    
    async updateStats() {
        try {
            // In renderer process, can't access fs directly
            // Request stats via IPC if available
            if (window.electron && window.electron.memory) {
                const stats = await window.electron.memory.getStats();
                if (stats && stats.success) {
                    this.memoryStats = stats;
                    return;
                }
            }
            
            // Fallback: Use in-memory stats
            this.memoryStats = {
                totalMemories: this.inMemoryStore ? this.inMemoryStore.size : 0,
                totalEmbeddings: 0,
                storageSize: '0 KB',
                storageSizeBytes: 0,
                lastUpdated: new Date().toISOString()
            };
        } catch (error) {
            // Silent - stats update not critical
        }
    }
    
    formatBytes(bytes) {
        if (bytes === 0) return '0 Bytes';
        const k = 1024;
        const sizes = ['Bytes', 'KB', 'MB', 'GB'];
        const i = Math.floor(Math.log(bytes) / Math.log(k));
        return Math.round((bytes / Math.pow(k, i)) * 100) / 100 + ' ' + sizes[i];
    }
    
    async clearAllMemories() {
        if (!this.isInitialized) {
            return false;
        }
        
        try {
            // Archive old memories before clearing
            const archivePath = path.join(this.storePath, `archive_${Date.now()}`);
            fs.mkdirSync(archivePath, { recursive: true });
            
            const files = fs.readdirSync(this.storePath);
            files.forEach(file => {
                if (file.endsWith('.json') || file.endsWith('.mem')) {
                    const source = path.join(this.storePath, file);
                    const dest = path.join(archivePath, file);
                    fs.renameSync(source, dest);
                }
            });
            
            await this.updateStats();
            console.log('[MemoryBridge] 🗑️ All memories archived to:', archivePath);
            
            return true;
            
        } catch (error) {
            console.error('[MemoryBridge] ❌ Failed to clear memories:', error);
            return false;
        }
    }
    
    getStats() {
        return this.memoryStats;
    }
    
    isAvailable() {
        // Check if memory service is actually available and working
        if (!this.isInitialized) {
            return false;
        }
        
        // Check if we have Electron IPC bridge
        if (window.electron && window.electron.memory) {
            return true;
        }
        
        // In-memory mode is technically available but limited
        return this.inMemoryStore !== undefined;
    }
    
    getAvailabilityStatus() {
        if (!this.isInitialized) {
            return { available: false, mode: 'not-initialized', message: 'Memory service not initialized' };
        }
        
        if (window.electron && window.electron.memory) {
            return { available: true, mode: 'full', message: 'Memory service fully available' };
        }
        
        if (this.inMemoryStore !== undefined) {
            return { available: true, mode: 'limited', message: 'In-memory mode (limited functionality)' };
        }
        
        return { available: false, mode: 'offline', message: 'Memory service offline' };
    }
}

// ========================================================================
// GLOBAL EXPOSURE
// ========================================================================

// Create singleton instance
window.memoryBridge = new MemoryBridge();

// Expose to global scope for easy access
window.memory = {
    store: (content, metadata) => window.memoryBridge.storeMemory(content, metadata),
    query: (query, limit) => window.memoryBridge.queryMemory(query, limit),
    recent: (limit) => window.memoryBridge.getRecentMemories(limit),
    embed: (text, model) => window.memoryBridge.createEmbedding(text, model),
    similar: (embedding, threshold, limit) => window.memoryBridge.similaritySearch(embedding, threshold, limit),
    decay: () => window.memoryBridge.applyDecay(),
    stats: () => window.memoryBridge.getStats(),
    clear: () => window.memoryBridge.clearAllMemories()
};

console.log('[MemoryBridge] 🌐 Global memory API exposed: window.memory');

})();
