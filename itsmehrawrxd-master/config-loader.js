#!/usr/bin/env node

// ========================================
// CONFIGURATION LOADER - NO HARDCODED VALUES
// ========================================

const fs = require('fs');
const path = require('path');

class ConfigLoader {
    constructor() {
        this.config = null;
        this.configPath = path.join(process.cwd(), 'config.json');
        this.load();
    }

    load() {
        try {
            if (fs.existsSync(this.configPath)) {
                const configData = fs.readFileSync(this.configPath, 'utf8');
                this.config = JSON.parse(configData);
            } else {
                // Create default config if it doesn't exist
                this.createDefaultConfig();
            }
        } catch (error) {
            console.error('Error loading config:', error.message);
            this.createDefaultConfig();
        }
    }

    createDefaultConfig() {
        this.config = {
            server: {
                host: 'localhost',
                port: 8080,
                timeout: 1000
            },
            security_platform: {
                port: 5240
            },
            ide: {
                window: {
                    width: 1600,
                    height: 900,
                    title: 'OhGees IDE - Professional Development Environment'
                },
                panels: {
                    left_width: 250,
                    right_width: 300,
                    bottom_height: 300
                },
                colors: {
                    background: '#1E1E1E',
                    panel_background: '#252526',
                    header_background: '#2D2D30',
                    border_color: '#3E3E42'
                }
            },
            eon: {
                default_settings: {
                    debug: true,
                    optimize: true,
                    target: 'production'
                },
                project: {
                    default_name: 'New EON Project',
                    default_version: '1.0.0',
                    default_description: 'A new EON project'
                }
            },
            ai: {
                models: {
                    chatgpt: { enabled: true, api_key: '', model: 'gpt-3.5-turbo' },
                    deepseek: { enabled: true, api_key: '', model: 'deepseek-coder' },
                    microsoft_copilot: { enabled: true, api_key: '', model: 'copilot' },
                    github_copilot: { enabled: true, api_key: '', model: 'github-copilot' },
                    amazon_q: { enabled: true, api_key: '', model: 'amazon-q' }
                }
            },
            build: {
                compiler: 'dotnet',
                verbosity: 'quiet',
                target_framework: 'net6.0'
            },
            paths: {
                projects: './projects',
                temp: './temp',
                logs: './logs'
            }
        };
        
        this.save();
    }

    save() {
        try {
            fs.writeFileSync(this.configPath, JSON.stringify(this.config, null, 2));
        } catch (error) {
            console.error('Error saving config:', error.message);
        }
    }

    get(path) {
        const keys = path.split('.');
        let value = this.config;
        
        for (const key of keys) {
            if (value && typeof value === 'object' && key in value) {
                value = value[key];
            } else {
                return null;
            }
        }
        
        return value;
    }

    set(path, value) {
        const keys = path.split('.');
        let current = this.config;
        
        for (let i = 0; i < keys.length - 1; i++) {
            const key = keys[i];
            if (!(key in current) || typeof current[key] !== 'object') {
                current[key] = {};
            }
            current = current[key];
        }
        
        current[keys[keys.length - 1]] = value;
        this.save();
    }

    // Convenience methods for common config access
    getServerConfig() {
        return this.get('server');
    }

    getIDEConfig() {
        return this.get('ide');
    }

    getEONConfig() {
        return this.get('eon');
    }

    getAIConfig() {
        return this.get('ai');
    }

    getBuildConfig() {
        return this.get('build');
    }

    getPathsConfig() {
        return this.get('paths');
    }
}

// Create global config instance
const config = new ConfigLoader();

module.exports = config;
