#!/usr/bin/env node

const { spawn, exec } = require('child_process');
const fs = require('fs').promises;
const path = require('path');
const os = require('os');

class MasterSpoofLauncher {
    constructor() {
        this.platform = os.platform();
        this.isInstalled = false;
    }

    async initialize() {
        console.log(' Master Spoof Launcher Starting...');
        console.log(` Platform: ${this.platform}`);
        
        // Check if already installed
        await this.checkInstallation();
        
        if (!this.isInstalled) {
            await this.installService();
        }
        
        await this.startServices();
        
        console.log(' Master Spoof Launcher Ready');
    }

    async checkInstallation() {
        try {
            if (this.platform === 'win32') {
                // Check Windows registry
                const { exec } = require('child_process');
                exec('reg query "HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run" /v AutoSpoofService', (error, stdout) => {
                    if (!error && stdout.includes('AutoSpoofService')) {
                        this.isInstalled = true;
                    }
                });
            } else {
                // Check systemd service
                const { exec } = require('child_process');
                exec('systemctl --user is-enabled auto-spoof.service', (error, stdout) => {
                    if (!error) {
                        this.isInstalled = true;
                    }
                });
            }
        } catch (error) {
            this.isInstalled = false;
        }
    }

    async installService() {
        console.log(' Installing Auto-Spoof Service...');
        
        if (this.platform === 'win32') {
            await this.installWindowsService();
        } else {
            await this.installUnixService();
        }
    }

    async installWindowsService() {
        return new Promise((resolve) => {
            const installer = spawn('install-spoof-service.bat', [], {
                stdio: 'inherit',
                shell: true
            });
            
            installer.on('close', (code) => {
                console.log(` Windows service installation completed with code ${code}`);
                resolve();
            });
        });
    }

    async installUnixService() {
        return new Promise((resolve) => {
            const installer = spawn('bash', ['install-spoof-service.sh'], {
                stdio: 'inherit'
            });
            
            installer.on('close', (code) => {
                console.log(` Unix service installation completed with code ${code}`);
                resolve();
            });
        });
    }

    async startServices() {
        console.log(' Starting all spoofed services...');
        
        // Start the auto-spoof launcher
        const launcher = spawn('node', ['auto-spoof-launcher.js'], {
            stdio: 'inherit',
            detached: true
        });
        
        launcher.unref();
        
        console.log(' All services started');
    }
}

// Start the master launcher
const launcher = new MasterSpoofLauncher();
launcher.initialize().catch(console.error);
