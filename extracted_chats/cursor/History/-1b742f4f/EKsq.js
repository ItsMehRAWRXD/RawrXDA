#!/usr/bin/env node
/**
 * BigDaddyG Auto-Launcher (No Auto-Injection)
 * Starts services but doesn't automatically inject connections
 */

const { spawn, exec } = require('child_process');
const path = require('path');
const fs = require('fs');
const http = require('http');
const WebSocket = require('ws');

class BigDaddyGAutoLauncherNoInject {
    constructor() {
        this.baseDir = 'D:\\';
        this.services = new Map();
        this.isRunning = false;
        this.fallbackMode = false;
        this.ports = {
            main: 3000,
            websocket: 8001,
            backend: 3001,
            live: 3002,
            ollama: 11434,
            proxy: 11441
        };
    }

    async start() {
        console.log('🚀 BigDaddyG Auto-Launcher starting (No Auto-Injection)...');
        
        try {
            // Check if services are already running
            if (await this.checkServicesRunning()) {
                console.log('✅ Services already running, skipping startup');
                this.isRunning = true;
                return true;
            }

            // Start everything in the background
            await this.startAllServices();
            
            // Create fallback system for offline mode
            await this.createFallbackSystem();
            
            // DON'T inject connections automatically
            console.log('💡 Connection injection disabled - use BigDaddyG-Connection-Manager.js to manage injections');
            
            console.log('🎉 BigDaddyG Auto-Launcher ready! Services running without auto-injection.');
            this.isRunning = true;
            return true;
            
        } catch (error) {
            console.error('❌ Auto-launcher failed:', error);
            return false;
        }
    }

    async checkServicesRunning() {
        const checks = [
            this.checkPort(this.ports.main),
            this.checkPort(this.ports.websocket),
            this.checkPort(this.ports.proxy)
        ];
        
        const results = await Promise.all(checks);
        return results.every(result => result);
    }

    async startAllServices() {
        console.log('🔄 Starting all services in background...');
        
        // Start Puppeteer Agent (main system)
        await this.startPuppeteerAgent();
        
        // Wait a bit for services to initialize
        await this.sleep(3000);
        
        // Start additional services if needed
        await this.startAdditionalServices();
    }

    async startPuppeteerAgent() {
        return new Promise((resolve) => {
            console.log('🚀 Starting Puppeteer Agent...');
            
            const agentDir = path.join(this.baseDir, 'puppeteer-agent');
            const agent = spawn('npm', ['start'], {
                cwd: agentDir,
                stdio: 'pipe',
                shell: true,
                detached: true
            });

            // Don't wait for the process to finish
            agent.unref();
            
            // Give it time to start
            setTimeout(() => {
                console.log('✅ Puppeteer Agent started in background');
                resolve();
            }, 2000);
        });
    }

    async startAdditionalServices() {
        // Start BigDaddyG Backend if it exists
        const backendDir = path.join(this.baseDir, 'bigdaddyg-backend');
        if (fs.existsSync(backendDir)) {
            console.log('🔄 Starting BigDaddyG Backend...');
            const backend = spawn('node', ['micro-server.js'], {
                cwd: backendDir,
                stdio: 'pipe',
                detached: true
            });
            backend.unref();
        }

        // Start BigDaddyG Live if it exists
        const liveDir = path.join(this.baseDir, 'bigdaddyg-live');
        if (fs.existsSync(liveDir)) {
            console.log('🔄 Starting BigDaddyG Live...');
            const live = spawn('node', ['daemon.js'], {
                cwd: liveDir,
                stdio: 'pipe',
                detached: true
            });
            live.unref();
        }
    }

    async createFallbackSystem() {
        console.log('🛡️ Creating fallback system for offline mode...');
        
        // Create a fallback server that handles offline scenarios
        const fallbackServer = http.createServer((req, res) => {
            res.setHeader('Access-Control-Allow-Origin', '*');
            res.setHeader('Content-Type', 'application/json');
            
            if (req.url === '/api/fallback/status') {
                res.end(JSON.stringify({
                    status: 'offline-mode',
                    message: 'Running in fallback mode - some features may be limited',
                    services: {
                        ollama: false,
                        cloud: false,
                        local: true
                    }
                }));
            } else if (req.url === '/api/fallback/chat') {
                // Simple fallback chat response
                res.end(JSON.stringify({
                    response: 'I\'m running in offline mode. Some features may be limited, but I can still help with basic tasks!',
                    mode: 'fallback'
                }));
            } else {
                res.statusCode = 404;
                res.end(JSON.stringify({ error: 'Not found' }));
            }
        });

        // Start fallback server on port 3003
        fallbackServer.listen(3003, () => {
            console.log('✅ Fallback server running on port 3003');
        });
    }

    async checkPort(port) {
        return new Promise((resolve) => {
            const server = http.createServer();
            server.listen(port, () => {
                server.close(() => resolve(false));
            });
            server.on('error', () => resolve(true));
        });
    }

    sleep(ms) {
        return new Promise(resolve => setTimeout(resolve, ms));
    }

    getStatus() {
        return {
            running: this.isRunning,
            fallbackMode: this.fallbackMode,
            ports: this.ports
        };
    }
}

// Auto-start when this script is run
async function main() {
    const launcher = new BigDaddyGAutoLauncherNoInject();
    
    const command = process.argv[2];
    
    if (command === 'status') {
        console.log('📊 BigDaddyG Auto-Launcher Status:');
        console.log(JSON.stringify(launcher.getStatus(), null, 2));
    } else {
        await launcher.start();
        
        // Keep the process running
        console.log('🔄 Auto-launcher running in background...');
        console.log('💡 Use BigDaddyG-Connection-Manager.js to manage injections');
        console.log('💡 Press Ctrl+C to stop');
        
        // Keep alive
        setInterval(() => {
            // Just keep the process alive
        }, 60000);
    }
}

// Handle graceful shutdown
process.on('SIGINT', () => {
    console.log('\n🛑 BigDaddyG Auto-Launcher stopping...');
    process.exit(0);
});

if (require.main === module) {
    main().catch(console.error);
}

module.exports = BigDaddyGAutoLauncherNoInject;
