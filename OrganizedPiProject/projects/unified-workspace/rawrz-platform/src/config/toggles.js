/**
 * RawrZ Toggle System - Full Functionality Control
 * Allows enabling/disabling features and restrictions
 */

class ToggleManager {
    constructor() {
        this.toggles = {
            // Security & Validation Toggles
            'security.auth_required': {
                name: 'Authentication Required',
                description: 'Require authentication for API access',
                enabled: true,
                category: 'security',
                impact: 'high'
            },
            'security.algorithm_validation': {
                name: 'Algorithm Validation',
                description: 'Validate allowed algorithms for crypto operations',
                enabled: true,
                category: 'security',
                impact: 'medium'
            },
            'security.file_type_validation': {
                name: 'File Type Validation',
                description: 'Restrict file uploads to allowed MIME types',
                enabled: true,
                category: 'security',
                impact: 'medium'
            },
            'security.rate_limiting': {
                name: 'Rate Limiting',
                description: 'Apply rate limiting to API endpoints',
                enabled: true,
                category: 'security',
                impact: 'medium'
            },

            // Advanced Feature Toggles
            'features.wmic_full_access': {
                name: 'WMIC Full Access',
                description: 'Enable full WMIC system enumeration capabilities',
                enabled: true,
                category: 'features',
                impact: 'high'
            },
            'features.process_killing': {
                name: 'Process Management',
                description: 'Allow process termination capabilities',
                enabled: true,
                category: 'features',
                impact: 'high'
            },
            'features.service_control': {
                name: 'Service Control',
                description: 'Allow starting/stopping Windows services',
                enabled: true,
                category: 'features',
                impact: 'high'
            },
            'features.advanced_crypto': {
                name: 'Advanced Cryptography',
                description: 'Enable advanced encryption algorithms and modes',
                enabled: true,
                category: 'features',
                impact: 'medium'
            },

            // Anti-Analysis Toggles
            'stealth.anti_vm_detection': {
                name: 'Anti-VM Detection',
                description: 'Enable virtual machine detection and evasion',
                enabled: true,
                category: 'stealth',
                impact: 'high'
            },
            'stealth.anti_debug': {
                name: 'Anti-Debug Protection',
                description: 'Enable debugger detection and protection',
                enabled: true,
                category: 'stealth',
                impact: 'high'
            },
            'stealth.anti_sandbox': {
                name: 'Anti-Sandbox Protection',
                description: 'Enable sandbox detection and evasion',
                enabled: true,
                category: 'stealth',
                impact: 'high'
            },
            'stealth.process_hiding': {
                name: 'Process Hiding',
                description: 'Enable process hiding capabilities',
                enabled: true,
                category: 'stealth',
                impact: 'high'
            },

            // System Access Toggles
            'system.full_disk_access': {
                name: 'Full Disk Access',
                description: 'Enable access to all disk drives and partitions',
                enabled: true,
                category: 'system',
                impact: 'high'
            },
            'system.network_enumeration': {
                name: 'Network Enumeration',
                description: 'Enable comprehensive network adapter enumeration',
                enabled: true,
                category: 'system',
                impact: 'medium'
            },
            'system.hardware_enumeration': {
                name: 'Hardware Enumeration',
                description: 'Enable detailed hardware information gathering',
                enabled: true,
                category: 'system',
                impact: 'medium'
            },
            'system.user_enumeration': {
                name: 'User Enumeration',
                description: 'Enable comprehensive user account enumeration',
                enabled: true,
                category: 'system',
                impact: 'medium'
            },

            // Development & Debug Toggles
            'dev.verbose_logging': {
                name: 'Verbose Logging',
                description: 'Enable detailed logging for debugging',
                enabled: false,
                category: 'development',
                impact: 'low'
            },
            'dev.hot_reload': {
                name: 'Hot Reload',
                description: 'Enable hot reloading of engines during development',
                enabled: true,
                category: 'development',
                impact: 'low'
            },
            'dev.cors_unrestricted': {
                name: 'Unrestricted CORS',
                description: 'Allow CORS from any origin (development only)',
                enabled: false,
                category: 'development',
                impact: 'high'
            },

            // Performance Toggles
            'performance.aggressive_caching': {
                name: 'Aggressive Caching',
                description: 'Enable aggressive caching for better performance',
                enabled: true,
                category: 'performance',
                impact: 'medium'
            },
            'performance.compression': {
                name: 'Response Compression',
                description: 'Enable gzip compression for responses',
                enabled: true,
                category: 'performance',
                impact: 'low'
            }
        };

        // Load from environment variables
        this.loadFromEnv();
        
        // Load from config file if exists
        this.loadFromFile();
    }

    /**
     * Check if a toggle is enabled
     */
    isEnabled(toggleKey) {
        const toggle = this.toggles[toggleKey];
        if (!toggle) {
            console.warn(`[TOGGLE] Unknown toggle key: ${toggleKey}`);
            return false;
        }
        return toggle.enabled;
    }

    /**
     * Enable a toggle
     */
    enable(toggleKey) {
        if (this.toggles[toggleKey]) {
            this.toggles[toggleKey].enabled = true;
            console.log(`[TOGGLE] Enabled: ${toggleKey}`);
            this.saveToFile();
            return true;
        }
        return false;
    }

    /**
     * Disable a toggle
     */
    disable(toggleKey) {
        if (this.toggles[toggleKey]) {
            this.toggles[toggleKey].enabled = false;
            console.log(`[TOGGLE] Disabled: ${toggleKey}`);
            this.saveToFile();
            return true;
        }
        return false;
    }

    /**
     * Toggle a toggle (flip state)
     */
    toggle(toggleKey) {
        if (this.toggles[toggleKey]) {
            this.toggles[toggleKey].enabled = !this.toggles[toggleKey].enabled;
            console.log(`[TOGGLE] ${this.toggles[toggleKey].enabled ? 'Enabled' : 'Disabled'}: ${toggleKey}`);
            this.saveToFile();
            return this.toggles[toggleKey].enabled;
        }
        return false;
    }

    /**
     * Get all toggles by category
     */
    getByCategory(category) {
        return Object.entries(this.toggles)
            .filter(([key, toggle]) => toggle.category === category)
            .reduce((acc, [key, toggle]) => {
                acc[key] = toggle;
                return acc;
            }, {});
    }

    /**
     * Get all toggles
     */
    getAll() {
        return this.toggles;
    }

    /**
     * Set multiple toggles at once
     */
    setBulk(toggles) {
        let changed = 0;
        for (const [key, enabled] of Object.entries(toggles)) {
            if (this.toggles[key]) {
                this.toggles[key].enabled = Boolean(enabled);
                changed++;
            }
        }
        if (changed > 0) {
            console.log(`[TOGGLE] Updated ${changed} toggles`);
            this.saveToFile();
        }
        return changed;
    }

    /**
     * Enable all toggles for full functionality
     */
    enableFullFunctionality() {
        let enabled = 0;
        for (const [key, toggle] of Object.entries(this.toggles)) {
            if (!toggle.enabled) {
                toggle.enabled = true;
                enabled++;
            }
        }
        console.log(`[TOGGLE] Enabled ${enabled} toggles for full functionality`);
        this.saveToFile();
        return enabled;
    }

    /**
     * Disable all security toggles (dangerous mode)
     */
    disableAllSecurity() {
        const securityToggles = this.getByCategory('security');
        let disabled = 0;
        for (const [key, toggle] of Object.entries(securityToggles)) {
            toggle.enabled = false;
            disabled++;
        }
        console.log(`[TOGGLE] Disabled ${disabled} security toggles - DANGEROUS MODE`);
        this.saveToFile();
        return disabled;
    }

    /**
     * Load toggles from environment variables
     */
    loadFromEnv() {
        for (const [key, toggle] of Object.entries(this.toggles)) {
            const envKey = `TOGGLE_${key.replace(/\./g, '_').toUpperCase()}`;
            const envValue = process.env[envKey];
            if (envValue !== undefined) {
                toggle.enabled = envValue.toLowerCase() === 'true';
                console.log(`[TOGGLE] Loaded from env: ${key} = ${toggle.enabled}`);
            }
        }
    }

    /**
     * Load toggles from config file
     */
    loadFromFile() {
        try {
            const fs = require('fs');
            const path = require('path');
            const configFile = path.join(__dirname, '..', '..', 'toggles.json');
            
            if (fs.existsSync(configFile)) {
                const data = fs.readFileSync(configFile, 'utf8');
                const config = JSON.parse(data);
                
                for (const [key, enabled] of Object.entries(config)) {
                    if (this.toggles[key]) {
                        this.toggles[key].enabled = Boolean(enabled);
                    }
                }
                console.log('[TOGGLE] Loaded configuration from file');
            }
        } catch (e) {
            console.warn('[TOGGLE] Could not load config file:', e.message);
        }
    }

    /**
     * Save toggles to config file
     */
    saveToFile() {
        try {
            const fs = require('fs');
            const path = require('path');
            const configFile = path.join(__dirname, '..', '..', 'toggles.json');
            
            const config = {};
            for (const [key, toggle] of Object.entries(this.toggles)) {
                config[key] = toggle.enabled;
            }
            
            fs.writeFileSync(configFile, JSON.stringify(config, null, 2));
            console.log('[TOGGLE] Saved configuration to file');
        } catch (e) {
            console.warn('[TOGGLE] Could not save config file:', e.message);
        }
    }

    /**
     * Get toggle status for API
     */
    getStatus() {
        const status = {
            total: Object.keys(this.toggles).length,
            enabled: 0,
            disabled: 0,
            categories: {}
        };

        for (const [key, toggle] of Object.entries(this.toggles)) {
            if (toggle.enabled) {
                status.enabled++;
            } else {
                status.disabled++;
            }

            if (!status.categories[toggle.category]) {
                status.categories[toggle.category] = { enabled: 0, disabled: 0 };
            }

            if (toggle.enabled) {
                status.categories[toggle.category].enabled++;
            } else {
                status.categories[toggle.category].disabled++;
            }
        }

        return status;
    }
}

// Create singleton instance
const toggleManager = new ToggleManager();

module.exports = toggleManager;
