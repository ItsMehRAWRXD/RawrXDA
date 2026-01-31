#!/usr/bin/env node

const fs = require('fs').promises;
const path = require('path');
const { spawn } = require('child_process');
const os = require('os');

class N0mN0mExtensionInstaller {
    constructor() {
        this.platform = os.platform();
        this.extensionPath = path.join(__dirname, 'n0mn0m-vscode-extension');
        this.vscodeExtensionsPath = this.getVSCodeExtensionsPath();
    }

    getVSCodeExtensionsPath() {
        const homeDir = os.homedir();
        
        switch (this.platform) {
            case 'win32':
                return path.join(homeDir, '.vscode', 'extensions');
            case 'darwin':
                return path.join(homeDir, '.vscode', 'extensions');
            default:
                return path.join(homeDir, '.vscode', 'extensions');
        }
    }

    async install() {
        console.log(' Installing n0mn0m AI Extension...');
        
        try {
            // Check if extension directory exists
            await this.checkExtensionDirectory();
            
            // Install dependencies
            await this.installDependencies();
            
            // Compile TypeScript
            await this.compileExtension();
            
            // Package extension
            await this.packageExtension();
            
            // Install to VS Code
            await this.installToVSCode();
            
            // Configure VS Code settings
            await this.configureVSCodeSettings();
            
            console.log(' n0mn0m AI Extension installed successfully!');
            console.log(' Extension is now available in VS Code');
            console.log(' Use Ctrl+Shift+P and search for "n0mn0m" to get started');
            
        } catch (error) {
            console.error(' Installation failed:', error.message);
            throw error;
        }
    }

    async checkExtensionDirectory() {
        try {
            await fs.access(this.extensionPath);
            console.log(' Extension directory found');
        } catch (error) {
            throw new Error('Extension directory not found. Please ensure n0mn0m-vscode-extension exists.');
        }
    }

    async installDependencies() {
        console.log(' Installing dependencies...');
        
        return new Promise((resolve, reject) => {
            const npm = spawn('npm', ['install'], {
                cwd: this.extensionPath,
                stdio: 'inherit'
            });

            npm.on('close', (code) => {
                if (code === 0) {
                    console.log(' Dependencies installed');
                    resolve();
                } else {
                    reject(new Error('Failed to install dependencies'));
                }
            });

            npm.on('error', (error) => {
                reject(new Error(`Failed to run npm install: ${error.message}`));
            });
        });
    }

    async compileExtension() {
        console.log(' Compiling TypeScript...');
        
        return new Promise((resolve, reject) => {
            const tsc = spawn('npx', ['tsc', '-p', '.'], {
                cwd: this.extensionPath,
                stdio: 'inherit'
            });

            tsc.on('close', (code) => {
                if (code === 0) {
                    console.log(' TypeScript compiled');
                    resolve();
                } else {
                    reject(new Error('Failed to compile TypeScript'));
                }
            });

            tsc.on('error', (error) => {
                reject(new Error(`Failed to compile: ${error.message}`));
            });
        });
    }

    async packageExtension() {
        console.log(' Packaging extension...');
        
        return new Promise((resolve, reject) => {
            const vsce = spawn('npx', ['vsce', 'package'], {
                cwd: this.extensionPath,
                stdio: 'inherit'
            });

            vsce.on('close', (code) => {
                if (code === 0) {
                    console.log(' Extension packaged');
                    resolve();
                } else {
                    reject(new Error('Failed to package extension'));
                }
            });

            vsce.on('error', (error) => {
                reject(new Error(`Failed to package: ${error.message}`));
            });
        });
    }

    async installToVSCode() {
        console.log(' Installing to VS Code...');
        
        // Find the packaged extension file
        const files = await fs.readdir(this.extensionPath);
        const vsixFile = files.find(file => file.endsWith('.vsix'));
        
        if (!vsixFile) {
            throw new Error('Extension package not found');
        }

        const vsixPath = path.join(this.extensionPath, vsixFile);

        return new Promise((resolve, reject) => {
            const code = spawn('code', ['--install-extension', vsixPath], {
                stdio: 'inherit'
            });

            code.on('close', (code) => {
                if (code === 0) {
                    console.log(' Extension installed to VS Code');
                    resolve();
                } else {
                    reject(new Error('Failed to install extension to VS Code'));
                }
            });

            code.on('error', (error) => {
                reject(new Error(`Failed to install to VS Code: ${error.message}`));
            });
        });
    }

    async configureVSCodeSettings() {
        console.log(' Configuring VS Code settings...');
        
        const settingsPath = this.getVSCodeSettingsPath();
        
        try {
            // Read existing settings
            let settings = {};
            try {
                const existingSettings = await fs.readFile(settingsPath, 'utf8');
                settings = JSON.parse(existingSettings);
            } catch (error) {
                // Settings file doesn't exist, create new one
            }

            // Add n0mn0m configuration
            settings['n0mn0m.backendUrl'] = 'http://localhost:9999';
            settings['n0mn0m.defaultModel'] = 'gpt-5';
            settings['n0mn0m.autoConnect'] = true;
            settings['n0mn0m.projectPath'] = __dirname;
            settings['n0mn0m.autoInstallServices'] = true;
            settings['n0mn0m.enableStealth'] = true;

            // Write updated settings
            await fs.writeFile(settingsPath, JSON.stringify(settings, null, 2));
            
            console.log(' VS Code settings configured');
            
        } catch (error) {
            console.warn(' Failed to configure VS Code settings:', error.message);
        }
    }

    getVSCodeSettingsPath() {
        const homeDir = os.homedir();
        
        switch (this.platform) {
            case 'win32':
                return path.join(homeDir, 'AppData', 'Roaming', 'Code', 'User', 'settings.json');
            case 'darwin':
                return path.join(homeDir, 'Library', 'Application Support', 'Code', 'User', 'settings.json');
            default:
                return path.join(homeDir, '.config', 'Code', 'User', 'settings.json');
        }
    }

    async uninstall() {
        console.log(' Uninstalling n0mn0m AI Extension...');
        
        try {
            // Uninstall from VS Code
            return new Promise((resolve, reject) => {
                const code = spawn('code', ['--uninstall-extension', 'n0mn0m.n0mn0m-ai-extension'], {
                    stdio: 'inherit'
                });

                code.on('close', (code) => {
                    if (code === 0) {
                        console.log(' Extension uninstalled from VS Code');
                        resolve();
                    } else {
                        reject(new Error('Failed to uninstall extension from VS Code'));
                    }
                });

                code.on('error', (error) => {
                    reject(new Error(`Failed to uninstall: ${error.message}`));
                });
            });
            
        } catch (error) {
            console.error(' Uninstall failed:', error.message);
            throw error;
        }
    }
}

// CLI interface
if (require.main === module) {
    const args = process.argv.slice(2);
    const installer = new N0mN0mExtensionInstaller();
    
    if (args.includes('--uninstall')) {
        installer.uninstall().catch(console.error);
    } else {
        installer.install().catch(console.error);
    }
}

module.exports = N0mN0mExtensionInstaller;
