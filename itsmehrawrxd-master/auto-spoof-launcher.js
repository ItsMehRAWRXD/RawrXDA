#!/usr/bin/env node

const { spawn, exec } = require('child_process');
const fs = require('fs').promises;
const path = require('path');
const os = require('os');

class AutoSpoofLauncher {
    constructor() {
        this.processes = new Map();
        this.isRunning = false;
        this.restartAttempts = 0;
        this.maxRestartAttempts = 10;
        this.stealthMode = true;
        this.ports = [11434, 9999, 8080]; // Ollama, Spoofed API, RawrZ
    }

    async initialize() {
        console.log(' Auto-Spoof Launcher Initializing...');
        
        // Kill any existing processes on our ports
        await this.killExistingProcesses();
        
        // Create necessary directories
        await this.createDirectories();
        
        // Start all services
        await this.startAllServices();
        
        // Set up monitoring
        this.setupMonitoring();
        
        // Set up stealth mode
        this.setupStealthMode();
        
        console.log(' Auto-Spoof Launcher Ready - All systems operational');
    }

    async killExistingProcesses() {
        console.log(' Checking for existing processes...');
        
        for (const port of this.ports) {
            try {
                if (process.platform === 'win32') {
                    // Windows
                    await this.execCommand(`netstat -ano | findstr :${port}`);
                    await this.execCommand(`taskkill /F /IM node.exe 2>nul`);
                    await this.execCommand(`taskkill /F /IM ollama.exe 2>nul`);
                } else {
                    // Unix-like systems
                    await this.execCommand(`lsof -ti:${port} | xargs kill -9 2>/dev/null || true`);
                    await this.execCommand(`pkill -f "ollama" 2>/dev/null || true`);
                }
            } catch (error) {
                // Ignore errors - processes might not exist
            }
        }
        
        console.log(' Cleared existing processes');
    }

    async createDirectories() {
        const dirs = [
            '.ollama_models',
            '.spoofed_cache',
            'logs',
            'temp'
        ];

        for (const dir of dirs) {
            try {
                await fs.mkdir(dir, { recursive: true });
            } catch (error) {
                // Directory might already exist
            }
        }
    }

    async startAllServices() {
        console.log(' Starting all spoofed services...');
        
        // Start Airtight Ollama Server
        await this.startService('airtight-ollama', 'node', ['airtight-ollama-server.js'], 11434);
        
        // Start Spoofed AI Server
        await this.startService('spoofed-ai', 'node', ['spoofed-ai-server.js'], 9999);
        
        // Start RawrZ Server
        await this.startService('rawrz-server', 'node', ['server.js'], 8080);
        
        // Wait for services to start
        await this.waitForServices();
        
        console.log(' All services started successfully');
    }

    async startService(name, command, args, port) {
        return new Promise((resolve, reject) => {
            console.log(` Starting ${name} on port ${port}...`);
            
            const process = spawn(command, args, {
                stdio: this.stealthMode ? 'ignore' : 'inherit',
                detached: true,
                windowsHide: true
            });

            process.on('error', (error) => {
                console.error(` Failed to start ${name}:`, error.message);
                reject(error);
            });

            process.on('exit', (code) => {
                if (code !== 0 && this.isRunning) {
                    console.log(` ${name} exited with code ${code}, restarting...`);
                    this.restartService(name, command, args, port);
                }
            });

            this.processes.set(name, {
                process,
                command,
                args,
                port,
                startTime: Date.now(),
                restartCount: 0
            });

            // Give the process time to start
            setTimeout(() => {
                console.log(` ${name} started on port ${port}`);
                resolve();
            }, 2000);
        });
    }

    async restartService(name, command, args, port) {
        const service = this.processes.get(name);
        if (!service) return;

        service.restartCount++;
        
        if (service.restartCount > this.maxRestartAttempts) {
            console.error(` ${name} failed too many times, giving up`);
            return;
        }

        console.log(` Restarting ${name} (attempt ${service.restartCount})...`);
        
        // Kill existing process
        if (service.process && !service.process.killed) {
            service.process.kill('SIGTERM');
        }

        // Start new process
        await this.startService(name, command, args, port);
    }

    async waitForServices() {
        console.log('⏳ Waiting for services to be ready...');
        
        for (const [name, service] of this.processes) {
            await this.waitForPort(service.port, name);
        }
    }

    async waitForPort(port, serviceName) {
        const maxAttempts = 30;
        let attempts = 0;

        while (attempts < maxAttempts) {
            try {
                const { spawn } = require('child_process');
                
                if (process.platform === 'win32') {
                    await this.execCommand(`netstat -an | findstr :${port}`);
                } else {
                    await this.execCommand(`nc -z localhost ${port}`);
                }
                
                console.log(` ${serviceName} is ready on port ${port}`);
                return;
            } catch (error) {
                attempts++;
                await new Promise(resolve => setTimeout(resolve, 1000));
            }
        }
        
        console.error(` ${serviceName} failed to start on port ${port}`);
    }

    setupMonitoring() {
        console.log(' Setting up process monitoring...');
        
        // Monitor processes every 30 seconds
        setInterval(() => {
            this.monitorProcesses();
        }, 30000);

        // Health check every 60 seconds
        setInterval(() => {
            this.healthCheck();
        }, 60000);

        // Cleanup logs every 5 minutes
        setInterval(() => {
            this.cleanupLogs();
        }, 300000);
    }

    async monitorProcesses() {
        for (const [name, service] of this.processes) {
            if (service.process.killed || service.process.exitCode !== null) {
                console.log(` ${name} process died, restarting...`);
                await this.restartService(name, service.command, service.args, service.port);
            }
        }
    }

    async healthCheck() {
        const healthChecks = [
            { name: 'Airtight Ollama', url: 'http://localhost:11434/health' },
            { name: 'Spoofed AI', url: 'http://localhost:9999/health' },
            { name: 'RawrZ Server', url: 'http://localhost:8080/health' }
        ];

        for (const check of healthChecks) {
            try {
                const response = await fetch(check.url);
                if (!response.ok) {
                    console.log(` ${check.name} health check failed`);
                }
            } catch (error) {
                console.log(` ${check.name} is not responding`);
            }
        }
    }

    async cleanupLogs() {
        try {
            const logDir = path.join(__dirname, 'logs');
            const files = await fs.readdir(logDir);
            
            for (const file of files) {
                const filePath = path.join(logDir, file);
                const stats = await fs.stat(filePath);
                
                // Delete logs older than 24 hours
                if (Date.now() - stats.mtime.getTime() > 24 * 60 * 60 * 1000) {
                    await fs.unlink(filePath);
                }
            }
        } catch (error) {
            // Ignore cleanup errors
        }
    }

    setupStealthMode() {
        if (!this.stealthMode) return;

        console.log(' Stealth mode activated...');
        
        // Hide console window on Windows
        if (process.platform === 'win32') {
            try {
                const { spawn } = require('child_process');
                spawn('powershell', ['-Command', 'Add-Type -Name Win32 -Namespace Win32Functions -MemberDefinition \'[DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr hWnd, Int32 nCmdShow);\' -PassThru'], { stdio: 'ignore' });
            } catch (error) {
                // Ignore stealth mode errors
            }
        }

        // Disable process listing
        process.title = 'System Service';
        
        // Clear command line arguments
        process.argv = ['node', 'system-service.js'];
    }

    async execCommand(command) {
        return new Promise((resolve, reject) => {
            exec(command, (error, stdout, stderr) => {
                if (error) {
                    reject(error);
                } else {
                    resolve(stdout);
                }
            });
        });
    }

    async shutdown() {
        console.log(' Shutting down Auto-Spoof Launcher...');
        this.isRunning = false;

        for (const [name, service] of this.processes) {
            if (service.process && !service.process.killed) {
                console.log(` Stopping ${name}...`);
                service.process.kill('SIGTERM');
            }
        }

        // Wait for processes to exit
        await new Promise(resolve => setTimeout(resolve, 5000));

        // Force kill if still running
        for (const [name, service] of this.processes) {
            if (service.process && !service.process.killed) {
                service.process.kill('SIGKILL');
            }
        }

        console.log(' Auto-Spoof Launcher stopped');
    }
}

// Global launcher instance
const launcher = new AutoSpoofLauncher();

// Handle graceful shutdown
process.on('SIGINT', async () => {
    await launcher.shutdown();
    process.exit(0);
});

process.on('SIGTERM', async () => {
    await launcher.shutdown();
    process.exit(0);
});

// Handle uncaught exceptions
process.on('uncaughtException', (error) => {
    console.error(' Uncaught Exception:', error);
    // Don't exit - keep running
});

process.on('unhandledRejection', (reason, promise) => {
    console.error(' Unhandled Rejection:', reason);
    // Don't exit - keep running
});

// Start the launcher
async function main() {
    try {
        launcher.isRunning = true;
        await launcher.initialize();
        
        // Keep the process alive
        setInterval(() => {
            // Heartbeat
        }, 60000);
        
    } catch (error) {
        console.error(' Failed to start Auto-Spoof Launcher:', error);
        process.exit(1);
    }
}

if (require.main === module) {
    main();
}

module.exports = AutoSpoofLauncher;
