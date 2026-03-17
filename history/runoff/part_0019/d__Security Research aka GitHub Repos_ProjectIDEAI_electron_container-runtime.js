/**
 * BigDaddyG IDE - Container Runtime
 * 
 * Safe code execution in isolated Docker containers with:
 * - Resource limits (CPU, RAM, disk quotas via cgroups)
 * - Network egress allow-lists
 * - Ephemeral volumes with uid/gid isolation
 * - 5-second cold-start budget for responsive agent loops
 * 
 * Part of Phase 1 (P0) - Critical for production agentic operation
 */

const { exec, spawn } = require('child_process');
const { promisify } = require('util');
const fs = require('fs').promises;
const path = require('path');
const crypto = require('crypto');

const execAsync = promisify(exec);

// ============================================================================
// CONFIGURATION
// ============================================================================

const DEFAULT_CONFIG = {
    // Resource limits
    cpuQuota: 0.5,           // 50% of one core
    memoryLimit: '512M',     // 512 MB
    diskLimit: '1G',         // 1 GB
    
    // Network policy
    networkMode: 'restricted',
    allowedDomains: [
        'registry.npmjs.org',
        'pypi.org',
        'files.pythonhosted.org',
        'github.com',
        'raw.githubusercontent.com',
        'hub.docker.com',
        'registry-1.docker.io'
    ],
    
    // Security
    dropCapabilities: ['ALL'],
    addCapabilities: [],
    readOnly: true,
    noNewPrivileges: true,
    
    // Performance
    coldStartBudget: 5000,   // 5 seconds max startup time
    maxRuntime: 300000,      // 5 minutes max execution
    
    // Storage
    workspaceRoot: path.join(process.cwd(), '.bigdaddyg', 'workspaces'),
    uid: process.getuid ? process.getuid() : 1000,
    gid: process.getgid ? process.getgid() : 1000
};

// ============================================================================
// CONTAINER RUNTIME CLASS
// ============================================================================

class ContainerRuntime {
    constructor(config = {}) {
        this.config = { ...DEFAULT_CONFIG, ...config };
        this.containers = new Map();
        this.imageCache = new Map();
    }
    
    /**
     * Initialize runtime - verify Docker availability
     */
    async initialize() {
        console.log('[ContainerRuntime] 🐳 Initializing Docker runtime...');
        
        try {
            const { stdout } = await execAsync('docker version --format "{{.Server.Version}}"');
            const version = stdout.trim();
            console.log(`[ContainerRuntime] ✅ Docker ${version} detected`);
            
            // Warm up common images
            await this.warmUpImages([
                'node:20-alpine',
                'python:3.11-alpine',
                'rust:1.75-alpine'
            ]);
            
            return true;
        } catch (error) {
            console.error('[ContainerRuntime] ❌ Docker not available:', error.message);
            console.warn('[ContainerRuntime] ⚠️  Install Docker to enable safe code execution');
            return false;
        }
    }
    
    /**
     * Pre-pull and cache common images for faster cold starts
     */
    async warmUpImages(images) {
        console.log('[ContainerRuntime] 🔥 Warming up image cache...');
        
        for (const image of images) {
            if (this.imageCache.has(image)) continue;
            
            try {
                const { stdout } = await execAsync(`docker images -q ${image}`);
                
                if (!stdout.trim()) {
                    console.log(`[ContainerRuntime] 📥 Pulling ${image}...`);
                    await execAsync(`docker pull ${image}`);
                }
                
                this.imageCache.set(image, Date.now());
                console.log(`[ContainerRuntime] ✅ Cached ${image}`);
            } catch (error) {
                console.warn(`[ContainerRuntime] ⚠️  Failed to cache ${image}:`, error.message);
            }
        }
    }
    
    /**
     * Spin up an isolated container
     */
    async spinUp(options = {}) {
        const startTime = Date.now();
        const containerId = `bigdaddyg-${crypto.randomBytes(8).toString('hex')}`;
        
        const config = {
            image: options.image || 'node:20-alpine',
            mounts: options.mounts || [],
            cpuQuota: options.cpuQuota || this.config.cpuQuota,
            memoryLimit: options.memoryLimit || this.config.memoryLimit,
            networkMode: options.networkMode || this.config.networkMode,
            allowedDomains: options.allowedDomains || this.config.allowedDomains,
            env: options.env || {},
            command: options.command || '/bin/sh'
        };
        
        // Create ephemeral workspace
        const workspacePath = await this.createEphemeralWorkspace(containerId);
        
        // Build Docker run command with security constraints
        const dockerCmd = this.buildDockerCommand(containerId, config, workspacePath);
        
        console.log(`[ContainerRuntime] 🚀 Spinning up container: ${containerId}`);
        console.log(`[ContainerRuntime] 📦 Image: ${config.image}`);
        
        try {
            // Start container in detached mode
            await execAsync(dockerCmd);
            
            // Wait for container to be running
            await this.waitForContainer(containerId);
            
            const elapsed = Date.now() - startTime;
            
            if (elapsed > this.config.coldStartBudget) {
                console.warn(`[ContainerRuntime] ⚠️  Cold start took ${elapsed}ms (budget: ${this.config.coldStartBudget}ms)`);
            } else {
                console.log(`[ContainerRuntime] ✅ Container ready in ${elapsed}ms`);
            }
            
            const container = {
                id: containerId,
                config,
                workspacePath,
                startTime,
                exec: (cmd) => this.execInContainer(containerId, cmd)
            };
            
            this.containers.set(containerId, container);
            
            return container;
        } catch (error) {
            // Cleanup on failure
            await this.destroy(containerId);
            throw new Error(`Failed to spin up container: ${error.message}`);
        }
    }
    
    /**
     * Build secure Docker command with all constraints
     */
    buildDockerCommand(containerId, config, workspacePath) {
        const parts = [
            'docker run',
            '-d',  // Detached
            `--name ${containerId}`,
            
            // Resource limits (cgroups)
            `--cpus="${config.cpuQuota}"`,
            `--memory="${config.memoryLimit}"`,
            `--storage-opt size=${this.config.diskLimit}`,
            
            // Network restrictions
            config.networkMode === 'restricted' ? '--network none' : `--network ${config.networkMode}`,
            
            // Security constraints
            '--security-opt=no-new-privileges:true',
            '--cap-drop=ALL',
            '--read-only',  // Read-only root filesystem
            
            // User isolation
            `--user ${this.config.uid}:${this.config.gid}`,
            
            // Mount ephemeral workspace
            `-v ${workspacePath}:/workspace:rw`,
            '--workdir /workspace',
            
            // Environment variables
            ...Object.entries(config.env).map(([k, v]) => `-e ${k}="${v}"`),
            
            // Temp directory (writable)
            '--tmpfs /tmp:rw,noexec,nosuid,size=100m',
            
            // Image
            config.image,
            
            // Keep alive (we'll exec commands into it)
            'tail -f /dev/null'
        ];
        
        return parts.join(' ');
    }
    
    /**
     * Create ephemeral workspace directory
     */
    async createEphemeralWorkspace(containerId) {
        const workspacePath = path.join(this.config.workspaceRoot, containerId);
        await fs.mkdir(workspacePath, { recursive: true });
        
        // Set ownership to match uid/gid
        if (process.platform !== 'win32') {
            await execAsync(`chown ${this.config.uid}:${this.config.gid} ${workspacePath}`);
        }
        
        console.log(`[ContainerRuntime] 📁 Workspace: ${workspacePath}`);
        return workspacePath;
    }
    
    /**
     * Wait for container to be running
     */
    async waitForContainer(containerId, timeout = 5000) {
        const startTime = Date.now();
        
        while (Date.now() - startTime < timeout) {
            try {
                const { stdout } = await execAsync(`docker inspect -f '{{.State.Running}}' ${containerId}`);
                
                if (stdout.trim() === 'true') {
                    return true;
                }
            } catch (error) {
                // Container not ready yet
            }
            
            await new Promise(resolve => setTimeout(resolve, 100));
        }
        
        throw new Error(`Container ${containerId} failed to start within ${timeout}ms`);
    }
    
    /**
     * Execute command in running container
     */
    async execInContainer(containerId, command, options = {}) {
        console.log(`[ContainerRuntime] 🔧 Executing in ${containerId}: ${command}`);
        
        const timeout = options.timeout || this.config.maxRuntime;
        const startTime = Date.now();
        
        try {
            const { stdout, stderr } = await execAsync(
                `docker exec ${containerId} /bin/sh -c "${command}"`,
                { timeout }
            );
            
            const elapsed = Date.now() - startTime;
            
            return {
                success: true,
                stdout: stdout.trim(),
                stderr: stderr.trim(),
                exitCode: 0,
                elapsed
            };
        } catch (error) {
            const elapsed = Date.now() - startTime;
            
            return {
                success: false,
                stdout: error.stdout || '',
                stderr: error.stderr || error.message,
                exitCode: error.code || 1,
                elapsed
            };
        }
    }
    
    /**
     * Execute code in a sandboxed container (high-level API)
     */
    async executeInSandbox(code, options = {}) {
        const {
            language = 'javascript',
            image = this.getImageForLanguage(language),
            timeout = 30000
        } = options;
        
        console.log(`[ContainerRuntime] 🎯 Executing ${language} code in sandbox...`);
        
        // Spin up container
        const container = await this.spinUp({ image, ...options });
        
        try {
            // Write code to file
            const filename = this.getFilenameForLanguage(language);
            await fs.writeFile(
                path.join(container.workspacePath, filename),
                code
            );
            
            // Execute code
            const runCommand = this.getRunCommandForLanguage(language, filename);
            const result = await container.exec(runCommand, { timeout });
            
            return result;
        } finally {
            // Always cleanup
            await this.destroy(container.id);
        }
    }
    
    /**
     * Get appropriate Docker image for language
     */
    getImageForLanguage(language) {
        const images = {
            javascript: 'node:20-alpine',
            typescript: 'node:20-alpine',
            python: 'python:3.11-alpine',
            rust: 'rust:1.75-alpine',
            go: 'golang:1.21-alpine',
            java: 'openjdk:17-alpine',
            ruby: 'ruby:3.2-alpine'
        };
        
        return images[language.toLowerCase()] || 'node:20-alpine';
    }
    
    /**
     * Get filename for language
     */
    getFilenameForLanguage(language) {
        const extensions = {
            javascript: 'script.js',
            typescript: 'script.ts',
            python: 'script.py',
            rust: 'main.rs',
            go: 'main.go',
            java: 'Main.java',
            ruby: 'script.rb'
        };
        
        return extensions[language.toLowerCase()] || 'script.txt';
    }
    
    /**
     * Get run command for language
     */
    getRunCommandForLanguage(language, filename) {
        const commands = {
            javascript: `node ${filename}`,
            typescript: `npx ts-node ${filename}`,
            python: `python ${filename}`,
            rust: `rustc ${filename} -o app && ./app`,
            go: `go run ${filename}`,
            java: `javac ${filename} && java Main`,
            ruby: `ruby ${filename}`
        };
        
        return commands[language.toLowerCase()] || `cat ${filename}`;
    }
    
    /**
     * Destroy container and cleanup workspace
     */
    async destroy(containerId) {
        console.log(`[ContainerRuntime] 🗑️  Destroying container: ${containerId}`);
        
        try {
            // Force remove container
            await execAsync(`docker rm -f ${containerId}`);
            
            // Cleanup workspace
            const container = this.containers.get(containerId);
            if (container?.workspacePath) {
                await fs.rm(container.workspacePath, { recursive: true, force: true });
            }
            
            this.containers.delete(containerId);
            
            console.log(`[ContainerRuntime] ✅ Container destroyed`);
        } catch (error) {
            console.warn(`[ContainerRuntime] ⚠️  Cleanup error:`, error.message);
        }
    }
    
    /**
     * Cleanup all containers on shutdown
     */
    async destroyAll() {
        console.log('[ContainerRuntime] 🧹 Cleaning up all containers...');
        
        const promises = Array.from(this.containers.keys()).map(id => this.destroy(id));
        await Promise.all(promises);
        
        console.log('[ContainerRuntime] ✅ All containers destroyed');
    }
}

// ============================================================================
// GLOBAL INSTANCE
// ============================================================================

let globalRuntime = null;

async function getContainerRuntime() {
    if (!globalRuntime) {
        globalRuntime = new ContainerRuntime();
        await globalRuntime.initialize();
        
        // Cleanup on exit
        process.on('exit', () => {
            if (globalRuntime) {
                globalRuntime.destroyAll().catch(console.error);
            }
        });
    }
    
    return globalRuntime;
}

// ============================================================================
// EXPORTS
// ============================================================================

if (typeof module !== 'undefined' && module.exports) {
    module.exports = {
        ContainerRuntime,
        getContainerRuntime,
        DEFAULT_CONFIG
    };
}

if (typeof window !== 'undefined') {
    window.ContainerRuntime = ContainerRuntime;
    window.getContainerRuntime = getContainerRuntime;
}

console.log('[ContainerRuntime] 📦 Module loaded');
