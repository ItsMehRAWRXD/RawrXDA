/**
 * BrowserNativeOS Kernel
 * Core system kernel providing process management, memory, and system calls
 */

export class Kernel {
    constructor() {
        this.processes = new Map();
        this.memory = new Map();
        this.syscalls = new Map();
        this.drivers = new Map();
        this.nextPID = 1;
        this.initialized = false;
    }

    async initialize() {
        console.log('🔧 Initializing kernel...');

        // Register core system calls
        this.registerSyscalls();

        // Initialize memory management
        this.initializeMemory();

        // Load device drivers
        await this.loadDrivers();

        this.initialized = true;
        console.log('✅ Kernel initialized');
    }

    registerSyscalls() {
        // File system calls
        this.syscalls.set('fs.read', async (pid, path) => {
            const proc = this.processes.get(pid);
            if (!proc || !this.checkPermission(proc, 'fs.read', path)) {
                throw new Error('Permission denied');
            }
            return this.drivers.get('vfs').readFile(path);
        });

        this.syscalls.set('fs.write', async (pid, path, data) => {
            const proc = this.processes.get(pid);
            if (!proc || !this.checkPermission(proc, 'fs.write', path)) {
                throw new Error('Permission denied');
            }
            return this.drivers.get('vfs').writeFile(path, data);
        });

        this.syscalls.set('fs.list', async (pid, path) => {
            const proc = this.processes.get(pid);
            if (!proc || !this.checkPermission(proc, 'fs.read', path)) {
                throw new Error('Permission denied');
            }
            return this.drivers.get('vfs').listDirectory(path);
        });

        // Process management calls
        this.syscalls.set('proc.spawn', async (pid, executable, args = []) => {
            return this.spawnProcess(executable, args, this.processes.get(pid));
        });

        this.syscalls.set('proc.kill', async (pid, targetPid) => {
            const proc = this.processes.get(pid);
            if (!proc || !this.checkPermission(proc, 'proc.kill', targetPid)) {
                throw new Error('Permission denied');
            }
            return this.killProcess(targetPid);
        });

        this.syscalls.set('proc.list', async (pid) => {
            const proc = this.processes.get(pid);
            if (!proc || !this.checkPermission(proc, 'proc.list')) {
                throw new Error('Permission denied');
            }
            return Array.from(this.processes.keys());
        });

        // Network calls
        this.syscalls.set('net.fetch', async (pid, url, options = {}) => {
            const proc = this.processes.get(pid);
            if (!proc || !this.checkPermission(proc, 'net.fetch', url)) {
                throw new Error('Permission denied');
            }
            return this.drivers.get('network').fetch(url, options);
        });

        // GPU calls
        this.syscalls.set('gpu.createContext', async (pid) => {
            const proc = this.processes.get(pid);
            if (!proc || !this.checkPermission(proc, 'gpu.create')) {
                throw new Error('Permission denied');
            }
            return this.drivers.get('gpu').createContext();
        });

        this.syscalls.set('gpu.dispatch', async (pid, context, shader, data) => {
            const proc = this.processes.get(pid);
            if (!proc || !this.checkPermission(proc, 'gpu.dispatch')) {
                throw new Error('Permission denied');
            }
            return this.drivers.get('gpu').dispatch(context, shader, data);
        });

        // AI/Model calls
        this.syscalls.set('ai.loadModel', async (pid, modelPath) => {
            const proc = this.processes.get(pid);
            if (!proc || !this.checkPermission(proc, 'ai.load', modelPath)) {
                throw new Error('Permission denied');
            }
            return this.drivers.get('ai').loadModel(modelPath);
        });

        this.syscalls.set('ai.infer', async (pid, model, prompt, options = {}) => {
            const proc = this.processes.get(pid);
            if (!proc || !this.checkPermission(proc, 'ai.infer')) {
                throw new Error('Permission denied');
            }
            return this.drivers.get('ai').infer(model, prompt, options);
        });

        console.log('✅ System calls registered');
    }

    initializeMemory() {
        // Initialize memory pools for processes
        this.memory.set('system', {
            heap: new ArrayBuffer(1024 * 1024), // 1MB system heap
            used: 0,
            limit: 1024 * 1024
        });

        this.memory.set('user', {
            heap: new ArrayBuffer(10 * 1024 * 1024), // 10MB user heap
            used: 0,
            limit: 10 * 1024 * 1024
        });

        console.log('✅ Memory management initialized');
    }

    async loadDrivers() {
        // Load virtual file system driver
        const { VFSDriver } = await import('./drivers/vfs.js');
        this.drivers.set('vfs', new VFSDriver());

        // Load GPU driver
        const { GPUDriver } = await import('./drivers/gpu.js');
        this.drivers.set('gpu', new GPUDriver());

        // Load network driver
        const { NetworkDriver } = await import('./drivers/network.js');
        this.drivers.set('network', new NetworkDriver());

        // Load AI driver
        const { AIDriver } = await import('./drivers/ai.js');
        this.drivers.set('ai', new AIDriver());

        console.log('✅ Device drivers loaded');
    }

    spawnProcess(executable, args = [], parent = null) {
        const pid = this.nextPID++;
        const process = {
            pid,
            executable,
            args,
            parent,
            permissions: this.inheritPermissions(parent),
            memory: this.allocateMemory(pid),
            state: 'running',
            created: Date.now(),
            channels: new Map()
        };

        this.processes.set(pid, process);

        // Execute the process
        if (typeof executable === 'function') {
            executable(pid, args, this.createProcessAPI(pid));
        } else if (typeof executable === 'string') {
            // Load WASM module or script
            this.loadExecutable(pid, executable, args);
        }

        console.log(`✅ Process ${pid} spawned`);
        return pid;
    }

    async loadExecutable(pid, path, args) {
        const process = this.processes.get(pid);
        if (!process) return;

        try {
            // Load from VFS
            const code = await this.drivers.get('vfs').readFile(path);
            const module = new Function('pid', 'args', 'api', code);
            module(pid, args, this.createProcessAPI(pid));
        } catch (error) {
            console.error(`❌ Failed to load executable ${path}:`, error);
            this.killProcess(pid);
        }
    }

    createProcessAPI(pid) {
        const process = this.processes.get(pid);
        return {
            syscall: async (name, ...args) => {
                return this.syscalls.get(name)?.(pid, ...args);
            },
            send: (targetPid, message) => {
                this.sendMessage(pid, targetPid, message);
            },
            receive: (callback) => {
                const channel = process.channels.get('inbox') || [];
                channel.push(callback);
                process.channels.set('inbox', channel);
            },
            exit: (code = 0) => {
                this.killProcess(pid);
            }
        };
    }

    killProcess(pid) {
        const process = this.processes.get(pid);
        if (!process) return false;

        // Clean up resources
        this.memory.delete(pid);
        this.processes.delete(pid);

        console.log(`✅ Process ${pid} terminated`);
        return true;
    }

    sendMessage(fromPid, toPid, message) {
        const targetProcess = this.processes.get(toPid);
        if (!targetProcess) return false;

        const inbox = targetProcess.channels.get('inbox') || [];
        inbox.forEach(callback => callback({ from: fromPid, message }));
        targetProcess.channels.set('inbox', inbox);

        return true;
    }

    checkPermission(process, capability, resource) {
        const permissions = process.permissions || {};
        const capabilityRules = permissions[capability] || [];

        // Check if permission is granted
        if (capabilityRules === true) return true;
        if (capabilityRules === false) return false;
        if (Array.isArray(capabilityRules)) {
            return capabilityRules.some(rule => this.matchResource(rule, resource));
        }

        return false;
    }

    matchResource(rule, resource) {
        // Simple pattern matching for resources
        if (rule === '*') return true;
        if (rule.startsWith('*') && resource.endsWith(rule.slice(1))) return true;
        if (rule.endsWith('*') && resource.startsWith(rule.slice(0, -1))) return true;
        return rule === resource;
    }

    inheritPermissions(parent) {
        if (!parent) {
            return {
                'fs.read': ['/usr/*', '/home/*'],
                'fs.write': ['/tmp/*', '/home/*'],
                'proc.spawn': true,
                'proc.kill': false,
                'proc.list': true,
                'net.fetch': false,
                'gpu.create': true,
                'gpu.dispatch': true,
                'ai.load': true,
                'ai.infer': true
            };
        }
        return { ...parent.permissions };
    }

    allocateMemory(pid) {
        const userMemory = this.memory.get('user');
        const allocation = {
            heap: userMemory.heap.slice(0),
            used: 0,
            limit: 1024 * 1024 // 1MB per process
        };

        this.memory.set(pid, allocation);
        return allocation;
    }

    // Public API
    async syscall(name, ...args) {
        const syscall = this.syscalls.get(name);
        if (!syscall) {
            throw new Error(`Unknown syscall: ${name}`);
        }
        return syscall(0, ...args); // System calls from kernel have PID 0
    }

    getProcess(pid) {
        return this.processes.get(pid);
    }

    getAllProcesses() {
        return Array.from(this.processes.entries());
    }

    getDriver(name) {
        return this.drivers.get(name);
    }

    getMemoryUsage() {
        const systemMemory = this.memory.get('system');
        const userMemory = this.memory.get('user');

        return {
            system: systemMemory,
            user: userMemory,
            processes: Array.from(this.memory.entries()).filter(([key]) => typeof key === 'number')
        };
    }

    getSystemInfo() {
        return {
            initialized: this.initialized,
            processCount: this.processes.size,
            syscallCount: this.syscalls.size,
            driverCount: this.drivers.size,
            memory: this.getMemoryUsage()
        };
    }
}

// Global kernel instance
export const kernel = new Kernel();
