#!/usr/bin/env node

const { spawn } = require('child_process');
const fs = require('fs').promises;
const path = require('path');
const os = require('os');

class N0mN0mMasterLauncher {
    constructor() {
        this.platform = os.platform();
        this.services = {
            spoofedAI: { port: 9999, script: 'spoofed-ai-server.js', name: 'Spoofed AI Server' },
            airtightOllama: { port: 11434, script: 'airtight-ollama-server.js', name: 'Airtight Ollama Server' },
            ideBackend: { port: 3001, script: 'ide-backend-server.js', name: 'IDE Backend Server' },
            rawrzServer: { port: 8080, script: 'server.js', name: 'RawrZ Server' }
        };
        this.runningServices = new Map();
    }

    async launchAll() {
        console.log(' n0mn0m Master Launcher Starting...');
        console.log('=' .repeat(60));
        
        try {
            // Check if all required files exist
            await this.checkRequiredFiles();
            
            // Install VS Code extension if not already installed
            await this.installVSCodeExtension();
            
            // Start all services
            await this.startAllServices();
            
            // Open Project Hub
            await this.openProjectHub();
            
            console.log('\n n0mn0m Master System Launched Successfully!');
            console.log('=' .repeat(60));
            console.log(' Service Status:');
            this.runningServices.forEach((process, service) => {
                console.log(`   ${service}: Running (PID: ${process.pid})`);
            });
            console.log('\n Next Steps:');
            console.log('  1. Open VS Code and use Ctrl+Shift+P → "n0mn0m: Open Project Hub"');
            console.log('  2. Or open http://localhost:9999 for direct AI access');
            console.log('  3. Use the Project Hub to manage all n0mn0m projects');
            console.log('\n To stop all services: Ctrl+C');
            
            // Keep the process alive
            process.on('SIGINT', () => this.shutdown());
            process.on('SIGTERM', () => this.shutdown());
            
            // Keep alive
            setInterval(() => {
                // Check if services are still running
                this.checkServiceHealth();
            }, 30000); // Check every 30 seconds
            
        } catch (error) {
            console.error(' Failed to launch n0mn0m system:', error.message);
            await this.shutdown();
            process.exit(1);
        }
    }

    async checkRequiredFiles() {
        console.log(' Checking required files...');
        
        const requiredFiles = [
            'spoofed-ai-server.js',
            'airtight-ollama-server.js',
            'ide-backend-server.js',
            'server.js',
            'ollama-chat-panel.html',
            'ai-terminal.js',
            'test-complete-system.js',
            'n0mn0m-vscode-extension/package.json'
        ];

        for (const file of requiredFiles) {
            try {
                await fs.access(file);
                console.log(`   ${file}`);
            } catch (error) {
                throw new Error(`Required file not found: ${file}`);
            }
        }
    }

    async installVSCodeExtension() {
        console.log(' Installing VS Code extension...');
        
        try {
            // Check if extension is already installed
            const result = await this.runCommand('code', ['--list-extensions', '--show-versions']);
            if (result.includes('n0mn0m.n0mn0m-ai-extension')) {
                console.log('   VS Code extension already installed');
                return;
            }
            
            // Install extension
            await this.runCommand('node', ['install-n0mn0m-extension.js']);
            console.log('   VS Code extension installed');
            
        } catch (error) {
            console.warn('   Failed to install VS Code extension:', error.message);
            console.log('   You can install it manually later using the Project Hub');
        }
    }

    async startAllServices() {
        console.log(' Starting all n0mn0m services...');
        
        // Start services in order
        const serviceOrder = ['spoofedAI', 'airtightOllama', 'ideBackend', 'rawrzServer'];
        
        for (const serviceName of serviceOrder) {
            const service = this.services[serviceName];
            await this.startService(serviceName, service);
            
            // Wait a bit between services
            await this.sleep(2000);
        }
    }

    async startService(serviceName, service) {
        console.log(`   Starting ${service.name}...`);
        
        try {
            const process = spawn('node', [service.script], {
                stdio: 'pipe',
                detached: false
            });

            // Store process reference
            this.runningServices.set(serviceName, process);

            // Handle process events
            process.stdout.on('data', (data) => {
                const output = data.toString().trim();
                if (output) {
                    console.log(`    [${service.name}] ${output}`);
                }
            });

            process.stderr.on('data', (data) => {
                const error = data.toString().trim();
                if (error && !error.includes('DeprecationWarning')) {
                    console.log(`    [${service.name}] ${error}`);
                }
            });

            process.on('close', (code) => {
                console.log(`    [${service.name}] Process exited with code ${code}`);
                this.runningServices.delete(serviceName);
            });

            process.on('error', (error) => {
                console.error(`    [${service.name}] Error: ${error.message}`);
                this.runningServices.delete(serviceName);
            });

            // Wait for service to start
            await this.waitForService(service.port, service.name);
            
        } catch (error) {
            console.error(`     Failed to start ${service.name}: ${error.message}`);
            throw error;
        }
    }

    async waitForService(port, serviceName, maxAttempts = 30) {
        const axios = require('axios');
        
        for (let attempt = 1; attempt <= maxAttempts; attempt++) {
            try {
                await axios.get(`http://localhost:${port}/health`, { timeout: 1000 });
                console.log(`     ${serviceName} is ready`);
                return;
            } catch (error) {
                if (attempt === maxAttempts) {
                    throw new Error(`${serviceName} failed to start after ${maxAttempts} attempts`);
                }
                await this.sleep(1000);
            }
        }
    }

    async openProjectHub() {
        console.log(' Opening Project Hub...');
        
        try {
            // Try to open in VS Code first
            await this.runCommand('code', ['--new-window', '.'], { silent: true });
            console.log('   Opened in VS Code');
            
            // Also open the web panel
            await this.sleep(3000); // Wait for VS Code to open
            await this.runCommand('start', ['ollama-chat-panel.html'], { silent: true });
            console.log('   Opened Ollama Chat Panel');
            
        } catch (error) {
            console.warn('   Failed to open Project Hub automatically');
            console.log('   You can open it manually:');
            console.log('     - VS Code: Ctrl+Shift+P → "n0mn0m: Open Project Hub"');
            console.log('     - Web: http://localhost:9999');
        }
    }

    async checkServiceHealth() {
        const axios = require('axios');
        
        for (const [serviceName, service] of Object.entries(this.services)) {
            if (this.runningServices.has(serviceName)) {
                try {
                    await axios.get(`http://localhost:${service.port}/health`, { timeout: 5000 });
                } catch (error) {
                    console.log(` ${service.name} is not responding, restarting...`);
                    this.runningServices.delete(serviceName);
                    await this.startService(serviceName, service);
                }
            }
        }
    }

    async shutdown() {
        console.log('\n Shutting down n0mn0m services...');
        
        for (const [serviceName, process] of this.runningServices) {
            console.log(`   Stopping ${serviceName}...`);
            process.kill('SIGTERM');
        }
        
        // Wait for processes to exit
        await this.sleep(3000);
        
        // Force kill if still running
        for (const [serviceName, process] of this.runningServices) {
            if (!process.killed) {
                console.log(`   Force killing ${serviceName}...`);
                process.kill('SIGKILL');
            }
        }
        
        console.log(' All services stopped');
        process.exit(0);
    }

    async runCommand(command, args = [], options = {}) {
        return new Promise((resolve, reject) => {
            const process = spawn(command, args, {
                stdio: options.silent ? 'ignore' : 'inherit',
                shell: true
            });

            process.on('close', (code) => {
                if (code === 0) {
                    resolve();
                } else {
                    reject(new Error(`Command failed with code ${code}`));
                }
            });

            process.on('error', (error) => {
                reject(error);
            });
        });
    }

    sleep(ms) {
        return new Promise(resolve => setTimeout(resolve, ms));
    }
}

// CLI interface
if (require.main === module) {
    const launcher = new N0mN0mMasterLauncher();
    launcher.launchAll().catch(console.error);
}

module.exports = N0mN0mMasterLauncher;
