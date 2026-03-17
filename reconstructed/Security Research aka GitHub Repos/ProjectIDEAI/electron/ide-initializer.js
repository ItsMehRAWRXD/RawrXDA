/**
 * BigDaddyG IDE - Initialization & Validation
 * 
 * Ensures all systems are properly initialized and integrated
 * Validates Plan of Action completion
 */

(function() {
'use strict';

class IDEInitializer {
    constructor() {
        this.systems = {
            memory: { loaded: false, functional: false },
            ollama: { loaded: false, functional: false },
            fileSystem: { loaded: false, functional: false },
            ui: { loaded: false, functional: false },
            multiAgent: { loaded: false, functional: false },
            errorCleanup: { loaded: false, functional: false }
        };
        
        this.validationResults = [];
        
        console.log('[IDEInit] 🚀 Starting BigDaddyG IDE initialization...');
        this.initialize();
    }
    
    async initialize() {
        // Wait for DOM to be ready
        if (document.readyState !== 'complete') {
            window.addEventListener('load', () => this.runInitialization());
        } else {
            await this.runInitialization();
        }
    }
    
    async runInitialization() {
        console.section('BigDaddyG IDE Initialization');
        
        // Step 1: Validate all core systems
        await this.validateMemorySystem();
        await this.validateOllamaSystem();
        await this.validateFileSystem();
        await this.validateUISystem();
        await this.validateMultiAgentSystem();
        await this.validateErrorCleanup();
        
        // Step 2: Display initialization report
        this.displayReport();
        
        // Step 3: Auto-start critical services
        await this.autoStartServices();
        
        // Step 4: Display welcome message
        this.showWelcomeMessage();
        
        console.log('[IDEInit] ✅ Initialization complete!');
    }
    
    // ========================================================================
    // SYSTEM VALIDATION
    // ========================================================================
    
    async validateMemorySystem() {
        console.log('[IDEInit] 🧠 Validating Memory System...');
        
        try {
            // Check if memory bridge exists
            if (window.memoryBridge) {
                this.systems.memory.loaded = true;
                
                // Test memory functionality
                if (window.memory && typeof window.memory.store === 'function') {
                    this.systems.memory.functional = true;
                    this.validationResults.push({
                        system: 'Memory System',
                        status: 'success',
                        message: 'OpenMemory bridge active and functional'
                    });
                } else {
                    this.validationResults.push({
                        system: 'Memory System',
                        status: 'warning',
                        message: 'Memory bridge loaded but API incomplete'
                    });
                }
            } else {
                this.validationResults.push({
                    system: 'Memory System',
                    status: 'error',
                    message: 'Memory bridge not loaded'
                });
            }
            
            // Check dashboard
            if (window.memoryDashboard) {
                console.log('[IDEInit] ✅ Memory Dashboard ready (Ctrl+Shift+M)');
            }
        } catch (error) {
            console.error('[IDEInit] ❌ Memory validation failed:', error);
            this.validationResults.push({
                system: 'Memory System',
                status: 'error',
                message: error.message
            });
        }
    }
    
    async validateOllamaSystem() {
        console.log('[IDEInit] 🦙 Validating Ollama System...');
        
        try {
            if (window.ollamaManager) {
                this.systems.ollama.loaded = true;
                
                // Test connection
                const connected = await window.ollamaManager.checkOllama();
                this.systems.ollama.functional = connected;
                
                if (connected) {
                    const models = await window.ollamaManager.getModels();
                    this.validationResults.push({
                        system: 'Ollama System',
                        status: 'success',
                        message: `Connected with ${models.total || 0} models`
                    });
                } else {
                    this.validationResults.push({
                        system: 'Ollama System',
                        status: 'warning',
                        message: 'Ollama not running (install for offline AI)'
                    });
                }
            } else {
                this.validationResults.push({
                    system: 'Ollama System',
                    status: 'error',
                    message: 'Ollama manager not loaded'
                });
            }
        } catch (error) {
            console.error('[IDEInit] ❌ Ollama validation failed:', error);
            this.validationResults.push({
                system: 'Ollama System',
                status: 'error',
                message: error.message
            });
        }
    }
    
    async validateFileSystem() {
        console.log('[IDEInit] 📁 Validating File System...');
        
        try {
            if (window.fileExplorer) {
                this.systems.fileSystem.loaded = true;
                
                // Test electron bridge
                if (window.electron && window.electron.listDrives) {
                    const drives = await window.electron.listDrives();
                    this.systems.fileSystem.functional = drives.success;
                    
                    if (drives.success) {
                        this.validationResults.push({
                            system: 'File System',
                            status: 'success',
                            message: `Full system access with ${drives.drives.length} drives`
                        });
                    } else {
                        this.validationResults.push({
                            system: 'File System',
                            status: 'warning',
                            message: 'File explorer loaded but electron bridge unavailable'
                        });
                    }
                } else {
                    this.validationResults.push({
                        system: 'File System',
                        status: 'warning',
                        message: 'Electron bridge not available'
                    });
                }
            } else {
                this.validationResults.push({
                    system: 'File System',
                    status: 'error',
                    message: 'File explorer not loaded'
                });
            }
        } catch (error) {
            console.error('[IDEInit] ❌ File system validation failed:', error);
            this.validationResults.push({
                system: 'File System',
                status: 'error',
                message: error.message
            });
        }
    }
    
    async validateUISystem() {
        console.log('[IDEInit] 🎨 Validating UI System...');
        
        try {
            if (window.uiEnhancer) {
                this.systems.ui.loaded = true;
                this.systems.ui.functional = true;
                
                this.validationResults.push({
                    system: 'UI Enhancements',
                    status: 'success',
                    message: 'Professional animations and polish active'
                });
            } else {
                this.validationResults.push({
                    system: 'UI Enhancements',
                    status: 'warning',
                    message: 'UI enhancer not loaded'
                });
            }
        } catch (error) {
            console.error('[IDEInit] ❌ UI validation failed:', error);
            this.validationResults.push({
                system: 'UI Enhancements',
                status: 'error',
                message: error.message
            });
        }
    }
    
    async validateMultiAgentSystem() {
        console.log('[IDEInit] 🤖 Validating Multi-Agent System...');
        
        try {
            if (window.multiAgentSwarm) {
                this.systems.multiAgent.loaded = true;
                this.systems.multiAgent.functional = true;
                
                this.validationResults.push({
                    system: 'Multi-Agent Swarm',
                    status: 'success',
                    message: '6 specialized agents with persistent memory'
                });
            } else if (window.MultiAgentSwarm) {
                // Class exists but not instantiated
                this.validationResults.push({
                    system: 'Multi-Agent Swarm',
                    status: 'warning',
                    message: 'Multi-agent class loaded but not initialized'
                });
            } else {
                this.validationResults.push({
                    system: 'Multi-Agent Swarm',
                    status: 'error',
                    message: 'Multi-agent system not loaded'
                });
            }
        } catch (error) {
            console.error('[IDEInit] ❌ Multi-agent validation failed:', error);
            this.validationResults.push({
                system: 'Multi-Agent Swarm',
                status: 'error',
                message: error.message
            });
        }
    }
    
    async validateErrorCleanup() {
        console.log('[IDEInit] 🧹 Validating Error Cleanup...');
        
        try {
            if (window.errorCleanup) {
                this.systems.errorCleanup.loaded = true;
                this.systems.errorCleanup.functional = true;
                
                const stats = window.errorCleanup.getStats();
                this.validationResults.push({
                    system: 'Error Cleanup',
                    status: 'success',
                    message: `Active with ${stats.errors} errors, ${stats.warnings} warnings`
                });
            } else {
                this.validationResults.push({
                    system: 'Error Cleanup',
                    status: 'warning',
                    message: 'Error cleanup not loaded'
                });
            }
        } catch (error) {
            console.error('[IDEInit] ❌ Error cleanup validation failed:', error);
            this.validationResults.push({
                system: 'Error Cleanup',
                status: 'error',
                message: error.message
            });
        }
    }
    
    // ========================================================================
    // REPORTING
    // ========================================================================
    
    displayReport() {
        console.section('System Validation Report');
        
        this.validationResults.forEach(result => {
            const icon = result.status === 'success' ? '✅' : 
                         result.status === 'warning' ? '⚠️' : '❌';
            
            const style = result.status === 'success' ? 'color: #00ff88; font-weight: bold;' :
                          result.status === 'warning' ? 'color: #ff6b35; font-weight: bold;' :
                          'color: #ff4757; font-weight: bold;';
            
            console.log(`%c${icon} ${result.system}: ${result.message}`, style);
        });
        
        // Calculate score
        const successCount = this.validationResults.filter(r => r.status === 'success').length;
        const totalCount = this.validationResults.length;
        const score = Math.round((successCount / totalCount) * 100);
        
        console.log('');
        console.log(`%c🎯 System Health: ${score}%`, 
            score >= 80 ? 'color: #00ff88; font-size: 14px; font-weight: bold;' :
            score >= 60 ? 'color: #ff6b35; font-size: 14px; font-weight: bold;' :
            'color: #ff4757; font-size: 14px; font-weight: bold;'
        );
    }
    
    // ========================================================================
    // AUTO-START SERVICES
    // ========================================================================
    
    async autoStartServices() {
        console.log('[IDEInit] 🔧 Auto-starting services...');
        
        // Auto-connect Ollama
        if (window.ollamaManager && !this.systems.ollama.functional) {
            console.log('[IDEInit] 🦙 Auto-connecting to Ollama...');
            await window.ollamaManager.autoConnect();
        }
        
        // Initialize memory stats
        if (window.memoryBridge) {
            console.log('[IDEInit] 🧠 Initializing memory stats...');
            await window.memoryBridge.updateStats();
        }
        
        console.log('[IDEInit] ✅ Services started');
    }
    
    // ========================================================================
    // WELCOME MESSAGE
    // ========================================================================
    
    showWelcomeMessage() {
        if (window.showNotification) {
            const successCount = this.validationResults.filter(r => r.status === 'success').length;
            const totalCount = this.validationResults.length;
            
            window.showNotification(
                '🌌 BigDaddyG IDE Ready!',
                `${successCount}/${totalCount} systems initialized. Press Ctrl+L to start.`,
                'success',
                5000
            );
        }
    }
    
    // ========================================================================
    // PUBLIC API
    // ========================================================================
    
    getSystemStatus() {
        return {
            systems: this.systems,
            validationResults: this.validationResults,
            health: this.calculateHealth()
        };
    }
    
    calculateHealth() {
        const successCount = this.validationResults.filter(r => r.status === 'success').length;
        const totalCount = this.validationResults.length;
        return Math.round((successCount / totalCount) * 100);
    }
    
    revalidate() {
        this.validationResults = [];
        this.runInitialization();
    }
}

// ========================================================================
// GLOBAL EXPOSURE
// ========================================================================

// Initialize after short delay to ensure all modules are loaded
setTimeout(() => {
    window.ideInitializer = new IDEInitializer();
}, 500);

// Expose helper commands
window.checkHealth = () => {
    if (window.ideInitializer) {
        const status = window.ideInitializer.getSystemStatus();
        console.section('BigDaddyG IDE Health Check');
        console.log('Health Score:', status.health + '%');
        console.log('Systems:', status.systems);
        console.table(status.validationResults);
        return status;
    } else {
        console.error('IDE Initializer not ready yet');
    }
};

window.revalidateSystems = () => {
    if (window.ideInitializer) {
        window.ideInitializer.revalidate();
    }
};

console.log('[IDEInit] 📦 IDE Initializer module loaded');

})();
