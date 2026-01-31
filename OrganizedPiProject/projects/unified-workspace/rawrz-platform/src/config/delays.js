/**
 * RawrZ Delay Configuration System
 * Provides configurable delays for various operations to simulate realistic timing
 */

class DelayManager {
    constructor() {
        this.defaultDelays = {
            // Assembly operations
            assembly: {
                encryption: 50,      // ms - Assembly encryption delay
                decryption: 30,      // ms - Assembly decryption delay
                compilation: 200,    // ms - Assembly compilation delay
                execution: 100,      // ms - Assembly execution delay
                fileOperations: 25   // ms - File I/O operations
            },
            
            // Crypto operations
            crypto: {
                keyGeneration: 100,  // ms - Key generation delay
                encryption: 75,      // ms - Encryption delay
                decryption: 60,      // ms - Decryption delay
                hash: 40,           // ms - Hashing delay
                signature: 80       // ms - Digital signature delay
            },
            
            // Network operations
            network: {
                connection: 500,     // ms - Network connection delay
                dataTransfer: 200,   // ms - Data transfer delay
                dnsLookup: 150,      // ms - DNS lookup delay
                ping: 50            // ms - Ping delay
            },
            
            // System operations
            system: {
                processStart: 300,   // ms - Process startup delay
                fileAccess: 20,      // ms - File access delay
                registryAccess: 15,  // ms - Registry access delay
                serviceControl: 250, // ms - Service control delay
                driverLoad: 400      // ms - Driver loading delay
            },
            
            // Anti-analysis delays
            antiAnalysis: {
                debugCheck: 10,      // ms - Debug detection delay
                vmCheck: 25,         // ms - VM detection delay
                sandboxCheck: 35,    // ms - Sandbox detection delay
                timingCheck: 5       // ms - Timing analysis delay
            },
            
            // Stealth operations
            stealth: {
                processHide: 50,     // ms - Process hiding delay
                memoryProtect: 30,   // ms - Memory protection delay
                networkStealth: 100, // ms - Network stealth delay
                hookInstall: 40      // ms - Hook installation delay
            },
            
            // Generic delays
            generic: {
                small: 10,          // ms - Small delay
                medium: 50,         // ms - Medium delay
                large: 200,         // ms - Large delay
                random: { min: 20, max: 100 } // ms - Random delay range
            }
        };
        
        this.currentDelays = { ...this.defaultDelays };
        this.enabled = true;
        this.randomVariation = 0.2; // 20% random variation
    }

    /**
     * Get delay for a specific operation
     * @param {string} category - Delay category (assembly, crypto, etc.)
     * @param {string} operation - Specific operation within category
     * @param {number} multiplier - Optional multiplier for the delay
     * @returns {number} Delay in milliseconds
     */
    getDelay(category, operation, multiplier = 1) {
        if (!this.enabled) return 0;
        
        let baseDelay = 0;
        
        if (this.currentDelays[category] && this.currentDelays[category][operation]) {
            baseDelay = this.currentDelays[category][operation];
        } else if (this.currentDelays.generic[operation]) {
            baseDelay = this.currentDelays.generic[operation];
        }
        
        // Apply multiplier
        let delay = baseDelay * multiplier;
        
        // Add random variation if enabled
        if (this.randomVariation > 0) {
            const variation = delay * this.randomVariation;
            delay += (Math.random() - 0.5) * 2 * variation;
        }
        
        // Ensure delay is not negative
        return Math.max(0, Math.round(delay));
    }

    /**
     * Get random delay within specified range
     * @param {number} min - Minimum delay in ms
     * @param {number} max - Maximum delay in ms
     * @returns {number} Random delay in milliseconds
     */
    getRandomDelay(min, max) {
        if (!this.enabled) return 0;
        
        const delay = Math.random() * (max - min) + min;
        return Math.round(delay);
    }

    /**
     * Set delay for specific operation
     * @param {string} category - Delay category
     * @param {string} operation - Operation name
     * @param {number} delay - Delay in milliseconds
     */
    setDelay(category, operation, delay) {
        if (!this.currentDelays[category]) {
            this.currentDelays[category] = {};
        }
        this.currentDelays[category][operation] = delay;
    }

    /**
     * Set multiple delays at once
     * @param {Object} delays - Object containing delay configurations
     */
    setDelays(delays) {
        Object.keys(delays).forEach(category => {
            if (!this.currentDelays[category]) {
                this.currentDelays[category] = {};
            }
            Object.assign(this.currentDelays[category], delays[category]);
        });
    }

    /**
     * Reset delays to default values
     */
    resetDelays() {
        this.currentDelays = { ...this.defaultDelays };
    }

    /**
     * Enable or disable delays
     * @param {boolean} enabled - Whether delays should be enabled
     */
    setEnabled(enabled) {
        this.enabled = enabled;
    }

    /**
     * Set random variation percentage
     * @param {number} variation - Variation percentage (0-1)
     */
    setRandomVariation(variation) {
        this.randomVariation = Math.max(0, Math.min(1, variation));
    }

    /**
     * Get all current delay configurations
     * @returns {Object} Current delay configurations
     */
    getAllDelays() {
        return { ...this.currentDelays };
    }

    /**
     * Get delay configuration for a specific category
     * @param {string} category - Category name
     * @returns {Object} Category delay configurations
     */
    getCategoryDelays(category) {
        return this.currentDelays[category] ? { ...this.currentDelays[category] } : {};
    }

    /**
     * Apply delay preset
     * @param {string} preset - Preset name (fast, normal, slow, realistic, stealth)
     */
    applyPreset(preset) {
        const presets = {
            fast: {
                assembly: { encryption: 10, decryption: 5, compilation: 50, execution: 20, fileOperations: 5 },
                crypto: { keyGeneration: 20, encryption: 15, decryption: 10, hash: 5, signature: 15 },
                network: { connection: 100, dataTransfer: 50, dnsLookup: 30, ping: 10 },
                system: { processStart: 50, fileAccess: 5, registryAccess: 3, serviceControl: 50, driverLoad: 100 },
                antiAnalysis: { debugCheck: 2, vmCheck: 5, sandboxCheck: 7, timingCheck: 1 },
                stealth: { processHide: 10, memoryProtect: 5, networkStealth: 20, hookInstall: 8 }
            },
            
            normal: {
                // Uses default delays
            },
            
            slow: {
                assembly: { encryption: 200, decryption: 150, compilation: 1000, execution: 500, fileOperations: 100 },
                crypto: { keyGeneration: 500, encryption: 300, decryption: 250, hash: 150, signature: 400 },
                network: { connection: 2000, dataTransfer: 1000, dnsLookup: 800, ping: 200 },
                system: { processStart: 1000, fileAccess: 100, registryAccess: 50, serviceControl: 800, driverLoad: 1500 },
                antiAnalysis: { debugCheck: 50, vmCheck: 100, sandboxCheck: 150, timingCheck: 20 },
                stealth: { processHide: 200, memoryProtect: 100, networkStealth: 400, hookInstall: 150 }
            },
            
            realistic: {
                assembly: { encryption: 100, decryption: 80, compilation: 500, execution: 200, fileOperations: 50 },
                crypto: { keyGeneration: 250, encryption: 150, decryption: 120, hash: 80, signature: 200 },
                network: { connection: 1000, dataTransfer: 500, dnsLookup: 300, ping: 100 },
                system: { processStart: 500, fileAccess: 30, registryAccess: 20, serviceControl: 400, driverLoad: 800 },
                antiAnalysis: { debugCheck: 25, vmCheck: 50, sandboxCheck: 75, timingCheck: 10 },
                stealth: { processHide: 100, memoryProtect: 60, networkStealth: 200, hookInstall: 80 }
            },
            
            stealth: {
                assembly: { encryption: 300, decryption: 250, compilation: 800, execution: 400, fileOperations: 100 },
                crypto: { keyGeneration: 400, encryption: 300, decryption: 250, hash: 150, signature: 350 },
                network: { connection: 1500, dataTransfer: 800, dnsLookup: 500, ping: 200 },
                system: { processStart: 800, fileAccess: 80, registryAccess: 60, serviceControl: 600, driverLoad: 1200 },
                antiAnalysis: { debugCheck: 80, vmCheck: 150, sandboxCheck: 200, timingCheck: 30 },
                stealth: { processHide: 200, memoryProtect: 120, networkStealth: 300, hookInstall: 150 }
            }
        };
        
        if (presets[preset]) {
            this.setDelays(presets[preset]);
            this.setRandomVariation(preset === 'realistic' ? 0.3 : 0.2);
        }
    }

    /**
     * Wait for specified delay
     * @param {number} delay - Delay in milliseconds
     * @returns {Promise} Promise that resolves after delay
     */
    async wait(delay) {
        if (delay > 0) {
            return new Promise(resolve => setTimeout(resolve, delay));
        }
    }

    /**
     * Wait for delay with progress callback
     * @param {number} delay - Delay in milliseconds
     * @param {Function} progressCallback - Callback for progress updates
     * @returns {Promise} Promise that resolves after delay
     */
    async waitWithProgress(delay, progressCallback) {
        if (delay <= 0) return;
        
        const interval = Math.min(100, delay / 10); // Update every 100ms or 10% of delay
        const steps = Math.ceil(delay / interval);
        
        for (let i = 0; i < steps; i++) {
            await new Promise(resolve => setTimeout(resolve, interval));
            if (progressCallback) {
                progressCallback(Math.min(100, ((i + 1) / steps) * 100));
            }
        }
    }
}

// Create singleton instance
const delayManager = new DelayManager();

module.exports = delayManager;
module.exports.default = delayManager;
