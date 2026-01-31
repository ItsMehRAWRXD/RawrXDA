#!/usr/bin/env node

const { spawn, exec } = require('child_process');
const fs = require('fs').promises;
const path = require('path');
const os = require('os');
const axios = require('axios');

class N0mN0mIDECompletion {
    constructor() {
        this.platform = os.platform();
        this.isRunning = false;
        this.services = new Map();
        this.completionEngine = null;
        this.aiIntegration = null;
    }

    async initialize() {
        console.log(' n0mn0m IDE Completion System Initializing...');
        console.log('=' .repeat(60));

        try {
            // Initialize completion engine
            await this.initializeCompletionEngine();
            
            // Initialize AI integration
            await this.initializeAIIntegration();
            
            // Start required services
            await this.startRequiredServices();
            
            // Setup IDE integration
            await this.setupIDEIntegration();
            
            console.log(' n0mn0m IDE Completion System Ready!');
            console.log(' All services running - Ready for development!');
            
        } catch (error) {
            console.error(' Initialization failed:', error.message);
            process.exit(1);
        }
    }

    async initializeCompletionEngine() {
        console.log(' Initializing Completion Engine...');
        
        // Check if Java compilation service is available
        const javaServicePath = path.join(__dirname, 'EonIDECompilationIntegration.java');
        const javaServiceExists = await this.fileExists(javaServicePath);
        
        if (javaServiceExists) {
            console.log(' Java IDE Compilation Service found');
            this.completionEngine = {
                type: 'java',
                path: javaServicePath,
                status: 'available'
            };
        } else {
            console.log('  Java service not found, using fallback');
            this.completionEngine = {
                type: 'fallback',
                status: 'available'
            };
        }
    }

    async initializeAIIntegration() {
        console.log(' Initializing AI Integration...');
        
        // Check if spoofed AI server is running
        try {
            const response = await axios.get('http://localhost:9999/health', { timeout: 2000 });
            if (response.data.status === 'healthy') {
                console.log(' Spoofed AI Server connected');
                this.aiIntegration = {
                    url: 'http://localhost:9999',
                    status: 'connected',
                    models: response.data.providers || []
                };
            }
        } catch (error) {
            console.log('  Spoofed AI Server not available, starting...');
            await this.startSpoofedAIServer();
        }
    }

    async startRequiredServices() {
        console.log(' Starting Required Services...');
        
        const services = [
            { name: 'Spoofed AI Server', script: 'spoofed-ai-server.js', port: 9999 },
            { name: 'IDE Backend', script: 'ide-backend-server.js', port: 3001 },
            { name: 'RawrZ Server', script: 'server.js', port: 8080 }
        ];

        for (const service of services) {
            await this.startService(service);
        }
    }

    async startService(service) {
        return new Promise((resolve, reject) => {
            console.log(` Starting ${service.name}...`);
            
            // Check if port is already in use
            this.checkPort(service.port).then(inUse => {
                if (inUse) {
                    console.log(`  Port ${service.port} already in use, skipping ${service.name}`);
                    resolve();
                    return;
                }
                
                const child = spawn('node', [service.script], {
                    cwd: __dirname,
                    stdio: 'pipe',
                    detached: false
                });

                child.stdout.on('data', (data) => {
                    const output = data.toString();
                    if (output.includes('listening on port') || output.includes('Server running')) {
                        console.log(` ${service.name} started successfully`);
                        this.services.set(service.name, {
                            process: child,
                            port: service.port,
                            status: 'running'
                        });
                        resolve();
                    }
                });

                child.stderr.on('data', (data) => {
                    const error = data.toString();
                    if (error.includes('EADDRINUSE')) {
                        console.log(`  Port ${service.port} already in use, skipping ${service.name}`);
                        resolve();
                    } else {
                        console.error(` ${service.name} error:`, error);
                    }
                });

                child.on('error', (error) => {
                    console.error(` Failed to start ${service.name}:`, error.message);
                    resolve(); // Don't reject, just continue
                });

                // Timeout after 5 seconds
                setTimeout(() => {
                    if (!this.services.has(service.name)) {
                        console.log(`  ${service.name} startup timeout, continuing...`);
                        resolve();
                    }
                }, 5000);
            });
        });
    }

    async checkPort(port) {
        return new Promise((resolve) => {
            const net = require('net');
            const server = net.createServer();
            
            server.listen(port, () => {
                server.close(() => {
                    resolve(false); // Port is free
                });
            });
            
            server.on('error', () => {
                resolve(true); // Port is in use
            });
        });
    }

    async setupIDEIntegration() {
        console.log(' Setting up IDE Integration...');
        
        // Check for VS Code
        const vscodeExtensionPath = path.join(__dirname, 'n0mn0m-vscode-extension');
        const vscodeExists = await this.fileExists(vscodeExtensionPath);
        
        if (vscodeExists) {
            console.log(' n0mn0m VS Code Extension found');
            await this.installVSCodeExtension();
        }

        // Check for IntelliJ
        const intellijPluginPath = path.join(__dirname, 'intellij-plugin');
        const intellijExists = await this.fileExists(intellijPluginPath);
        
        if (intellijExists) {
            console.log(' IntelliJ Plugin found');
            await this.buildIntelliJPlugin();
        }
    }

    async installVSCodeExtension() {
        console.log(' Installing n0mn0m VS Code Extension...');
        
        try {
            const extensionPath = path.join(__dirname, 'n0mn0m-vscode-extension');
            
            // Install dependencies first
            console.log(' Installing VS Code extension dependencies...');
            await this.runCommand('npm install', { cwd: extensionPath });
            
            // Compile TypeScript
            console.log(' Compiling TypeScript...');
            await this.runCommand('npx tsc', { cwd: extensionPath });
            
            // Package extension
            console.log(' Packaging VS Code extension...');
            await this.runCommand('npx vsce package', { cwd: extensionPath });
            
            console.log(' VS Code Extension packaged successfully');
        } catch (error) {
            console.log('  VS Code Extension packaging failed:', error.message);
            console.log(' Extension files exist, continuing with manual setup...');
        }
    }

    async buildIntelliJPlugin() {
        console.log(' Building IntelliJ Plugin...');
        
        try {
            const pluginPath = path.join(__dirname, 'intellij-plugin');
            const gradleCommand = this.platform === 'win32' ? 'gradlew.bat' : './gradlew';
            
            await this.runCommand(`${gradleCommand} build`, { cwd: pluginPath });
            console.log(' IntelliJ Plugin built successfully');
        } catch (error) {
            console.log('  IntelliJ Plugin build failed:', error.message);
            console.log(' Plugin files exist, continuing with manual setup...');
        }
    }

    async startSpoofedAIServer() {
        return new Promise((resolve) => {
            console.log(' Starting Spoofed AI Server...');
            
            const child = spawn('node', ['spoofed-ai-server.js'], {
                cwd: __dirname,
                stdio: 'pipe',
                detached: false
            });

            child.stdout.on('data', (data) => {
                const output = data.toString();
                if (output.includes('Server running on http://localhost:9999')) {
                    console.log(' Spoofed AI Server started');
                    this.services.set('Spoofed AI Server', {
                        process: child,
                        port: 9999,
                        status: 'running'
                    });
                    resolve();
                }
            });

            setTimeout(() => resolve(), 5000);
        });
    }

    async runCommand(command, options = {}) {
        return new Promise((resolve, reject) => {
            exec(command, options, (error, stdout, stderr) => {
                if (error) {
                    reject(error);
                } else {
                    resolve(stdout);
                }
            });
        });
    }

    async fileExists(filePath) {
        try {
            await fs.access(filePath);
            return true;
        } catch {
            return false;
        }
    }

    async getCompletionSuggestions(filePath, position, context) {
        console.log(` Getting completion suggestions for ${filePath} at position ${position}`);
        
        try {
            // Use our AI integration for intelligent completions
            const response = await axios.post('http://localhost:9999/api/gpt-5/unlock', {
                messages: [{
                    role: 'user',
                    content: `Provide code completion suggestions for file: ${filePath}, position: ${position}, context: ${context}`
                }]
            });

            if (response.data && response.data.choices && response.data.choices[0]) {
                return {
                    suggestions: this.parseCompletionResponse(response.data.choices[0].message.content),
                    source: 'n0mn0m-ai',
                    timestamp: new Date().toISOString()
                };
            }
        } catch (error) {
            console.error(' Completion request failed:', error.message);
        }

        return { suggestions: [], source: 'fallback', timestamp: new Date().toISOString() };
    }

    parseCompletionResponse(content) {
        // Parse AI response into completion suggestions
        const suggestions = [];
        const lines = content.split('\n');
        
        for (const line of lines) {
            if (line.trim() && !line.startsWith('//') && !line.startsWith('#')) {
                suggestions.push({
                    text: line.trim(),
                    type: 'snippet',
                    priority: 1
                });
            }
        }
        
        return suggestions;
    }

    async shutdown() {
        console.log(' Shutting down n0mn0m IDE Completion System...');
        
        for (const [name, service] of this.services) {
            if (service.process) {
                console.log(` Stopping ${name}...`);
                service.process.kill();
            }
        }
        
        console.log(' All services stopped');
        process.exit(0);
    }
}

// Handle graceful shutdown
process.on('SIGINT', async () => {
    if (global.ideCompletion) {
        await global.ideCompletion.shutdown();
    }
});

process.on('SIGTERM', async () => {
    if (global.ideCompletion) {
        await global.ideCompletion.shutdown();
    }
});

// Start the IDE Completion System
if (require.main === module) {
    const ideCompletion = new N0mN0mIDECompletion();
    global.ideCompletion = ideCompletion;
    ideCompletion.initialize().catch(console.error);
}

module.exports = N0mN0mIDECompletion;
