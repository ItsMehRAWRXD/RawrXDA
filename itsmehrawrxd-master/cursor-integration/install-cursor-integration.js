#!/usr/bin/env node

const fs = require('fs').promises;
const path = require('path');
const os = require('os');

class CursorIntegrationInstaller {
    constructor() {
        this.platform = os.platform();
        this.cursorConfigPath = this.getCursorConfigPath();
    }

    getCursorConfigPath() {
        const homeDir = os.homedir();
        
        switch (this.platform) {
            case 'win32':
                return path.join(homeDir, 'AppData', 'Roaming', 'Cursor', 'User', 'settings.json');
            case 'darwin':
                return path.join(homeDir, 'Library', 'Application Support', 'Cursor', 'User', 'settings.json');
            default:
                return path.join(homeDir, '.config', 'Cursor', 'User', 'settings.json');
        }
    }

    async install() {
        console.log(' Installing Airtight AI Integration for Cursor...');
        
        try {
            // Create Cursor config directory if it doesn't exist
            await fs.mkdir(path.dirname(this.cursorConfigPath), { recursive: true });
            
            // Read existing settings or create new ones
            let settings = {};
            try {
                const existingSettings = await fs.readFile(this.cursorConfigPath, 'utf8');
                settings = JSON.parse(existingSettings);
            } catch (error) {
                console.log(' Creating new Cursor settings file');
            }
            
            // Add Airtight AI configuration
            settings['airtight-ai'] = {
                enabled: true,
                backendUrl: 'http://localhost:3001',
                websocketUrl: 'ws://localhost:3002',
                defaultModel: 'gpt-5',
                autoStart: true,
                showSuggestions: true,
                maxSuggestions: 5
            };
            
            // Add keybindings
            if (!settings.keybindings) {
                settings.keybindings = [];
            }
            
            const aiKeybindings = [
                {
                    key: 'ctrl+shift+a',
                    command: 'airtight-ai.startSession',
                    when: '!aiSessionActive'
                },
                {
                    key: 'ctrl+shift+g',
                    command: 'airtight-ai.generateCode',
                    when: 'editorTextFocus'
                },
                {
                    key: 'ctrl+shift+r',
                    command: 'airtight-ai.codeReview',
                    when: 'editorTextFocus'
                },
                {
                    key: 'ctrl+shift+e',
                    command: 'airtight-ai.explainCode',
                    when: 'editorTextFocus'
                }
            ];
            
            // Add keybindings if they don't exist
            for (const binding of aiKeybindings) {
                if (!settings.keybindings.find(kb => kb.command === binding.command)) {
                    settings.keybindings.push(binding);
                }
            }
            
            // Write updated settings
            await fs.writeFile(this.cursorConfigPath, JSON.stringify(settings, null, 2));
            
            console.log(' Cursor integration installed successfully!');
            console.log(` Settings file: ${this.cursorConfigPath}`);
            
            // Create extension manifest
            await this.createExtensionManifest();
            
            console.log(' Airtight AI is now integrated with Cursor IDE');
            console.log(' Restart Cursor to activate the integration');
            
        } catch (error) {
            console.error(' Failed to install Cursor integration:', error.message);
            throw error;
        }
    }

    async createExtensionManifest() {
        const extensionPath = path.join(__dirname, 'cursor-extension');
        await fs.mkdir(extensionPath, { recursive: true });
        
        const manifest = {
            name: 'airtight-ai-assistant',
            displayName: 'Airtight AI Assistant',
            description: 'Complete AI coding assistant with unlimited access to all models',
            version: '1.0.0',
            publisher: 'airtight-ai',
            engines: {
                cursor: '^1.0.0'
            },
            categories: ['Other', 'Machine Learning', 'Snippets'],
            activationEvents: [
                'onCommand:airtight-ai.startSession',
                'onCommand:airtight-ai.generateCode',
                'onCommand:airtight-ai.codeReview',
                'onCommand:airtight-ai.debugCode',
                'onCommand:airtight-ai.refactorCode',
                'onCommand:airtight-ai.explainCode',
                'onCommand:airtight-ai.generateTests',
                'onCommand:airtight-ai.optimizeCode'
            ],
            main: './out/extension.js',
            contributes: {
                commands: [
                    {
                        command: 'airtight-ai.startSession',
                        title: 'Start AI Session',
                        category: 'Airtight AI'
                    },
                    {
                        command: 'airtight-ai.generateCode',
                        title: 'Generate Code',
                        category: 'Airtight AI'
                    },
                    {
                        command: 'airtight-ai.codeReview',
                        title: 'Code Review',
                        category: 'Airtight AI'
                    },
                    {
                        command: 'airtight-ai.debugCode',
                        title: 'Debug Code',
                        category: 'Airtight AI'
                    },
                    {
                        command: 'airtight-ai.refactorCode',
                        title: 'Refactor Code',
                        category: 'Airtight AI'
                    },
                    {
                        command: 'airtight-ai.explainCode',
                        title: 'Explain Code',
                        category: 'Airtight AI'
                    },
                    {
                        command: 'airtight-ai.generateTests',
                        title: 'Generate Tests',
                        category: 'Airtight AI'
                    },
                    {
                        command: 'airtight-ai.optimizeCode',
                        title: 'Optimize Code',
                        category: 'Airtight AI'
                    }
                ],
                menus: {
                    'editor/context': [
                        {
                            command: 'airtight-ai.generateCode',
                            group: 'airtight-ai',
                            when: 'editorHasSelection'
                        },
                        {
                            command: 'airtight-ai.codeReview',
                            group: 'airtight-ai',
                            when: 'editorHasSelection'
                        },
                        {
                            command: 'airtight-ai.explainCode',
                            group: 'airtight-ai',
                            when: 'editorHasSelection'
                        },
                        {
                            command: 'airtight-ai.refactorCode',
                            group: 'airtight-ai',
                            when: 'editorHasSelection'
                        }
                    ]
                },
                configuration: {
                    title: 'Airtight AI',
                    properties: {
                        'airtight-ai.enabled': {
                            type: 'boolean',
                            default: true,
                            description: 'Enable Airtight AI Assistant'
                        },
                        'airtight-ai.defaultModel': {
                            type: 'string',
                            default: 'gpt-5',
                            enum: ['gpt-5', 'claude-3.5', 'gemini-2.0', 'copilot'],
                            description: 'Default AI model to use'
                        },
                        'airtight-ai.backendUrl': {
                            type: 'string',
                            default: 'http://localhost:3001',
                            description: 'Backend server URL'
                        },
                        'airtight-ai.websocketUrl': {
                            type: 'string',
                            default: 'ws://localhost:3002',
                            description: 'WebSocket server URL'
                        },
                        'airtight-ai.autoStart': {
                            type: 'boolean',
                            default: true,
                            description: 'Automatically start AI session on IDE launch'
                        }
                    }
                }
            }
        };
        
        await fs.writeFile(
            path.join(extensionPath, 'package.json'),
            JSON.stringify(manifest, null, 2)
        );
        
        console.log(' Extension manifest created');
    }

    async uninstall() {
        console.log(' Uninstalling Airtight AI Integration from Cursor...');
        
        try {
            // Read existing settings
            const existingSettings = await fs.readFile(this.cursorConfigPath, 'utf8');
            const settings = JSON.parse(existingSettings);
            
            // Remove Airtight AI configuration
            delete settings['airtight-ai'];
            
            // Remove AI keybindings
            if (settings.keybindings) {
                settings.keybindings = settings.keybindings.filter(
                    kb => !kb.command.startsWith('airtight-ai.')
                );
            }
            
            // Write updated settings
            await fs.writeFile(this.cursorConfigPath, JSON.stringify(settings, null, 2));
            
            console.log(' Cursor integration uninstalled successfully!');
            
        } catch (error) {
            console.error(' Failed to uninstall Cursor integration:', error.message);
            throw error;
        }
    }
}

// CLI interface
if (require.main === module) {
    const args = process.argv.slice(2);
    const installer = new CursorIntegrationInstaller();
    
    if (args.includes('--uninstall')) {
        installer.uninstall().catch(console.error);
    } else {
        installer.install().catch(console.error);
    }
}

module.exports = CursorIntegrationInstaller;
