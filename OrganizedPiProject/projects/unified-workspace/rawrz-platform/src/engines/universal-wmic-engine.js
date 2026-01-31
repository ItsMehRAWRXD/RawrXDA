// Universal WMIC Engine - Cross-platform system information and management
// Provides WMIC-equivalent functionality for Windows, Linux, and macOS
'use strict';

const { exec } = require('child_process');
const { promisify } = require('util');
const fs = require('fs').promises;
const os = require('os');
const path = require('path');

const execAsync = promisify(exec);

class UniversalWMICEngine {
    constructor() {
        this.platform = process.platform;
        this.isWindows = this.platform === 'win32';
        this.isLinux = this.platform === 'linux';
        this.isMacOS = this.platform === 'darwin';
        
        // Cache for frequently accessed data
        this.cache = new Map();
        this.cacheTimeout = 30000; // 30 seconds
        
        // Self-sufficiency detection
        this.selfSufficient = true; // JavaScript system info is self-sufficient
        this.externalDependencies = [];
        this.requiredDependencies = [];
        
        // WMIC command mappings for cross-platform compatibility
        this.wmicCommands = {
            // System Information
            'computersystem': {
                windows: 'wmic computersystem get /format:csv',
                linux: this.getLinuxSystemInfo.bind(this),
                darwin: this.getMacSystemInfo.bind(this)
            },
            
            // Processor Information
            'cpu': {
                windows: 'wmic cpu get /format:csv',
                linux: 'lscpu',
                darwin: 'sysctl -n machdep.cpu.brand_string && sysctl -n hw.ncpu'
            },
            
            // Memory Information
            'memorychip': {
                windows: 'wmic memorychip get /format:csv',
                linux: 'cat /proc/meminfo',
                darwin: 'vm_stat && sysctl hw.memsize'
            },
            
            // Disk Information
            'logicaldisk': {
                windows: 'wmic logicaldisk get /format:csv',
                linux: 'df -h && lsblk',
                darwin: 'df -h && diskutil list'
            },
            
            // Network Adapters
            'networkadapter': {
                windows: 'wmic path win32_networkadapter get /format:csv',
                linux: 'ip link show && cat /proc/net/dev',
                darwin: 'ifconfig && networksetup -listallhardwareports'
            },
            
            // Running Processes
            'process': {
                windows: 'wmic process get /format:csv',
                linux: 'ps aux',
                darwin: 'ps aux'
            },
            
            // Services
            'service': {
                windows: 'wmic service get /format:csv',
                linux: 'systemctl list-units --type=service',
                darwin: 'launchctl list'
            },
            
            // Installed Software
            'product': {
                windows: 'wmic product get /format:csv',
                linux: 'dpkg -l 2>/dev/null || rpm -qa 2>/dev/null || pacman -Q 2>/dev/null',
                darwin: 'system_profiler SPApplicationsDataType'
            },
            
            // User Accounts
            'useraccount': {
                windows: 'wmic useraccount get /format:csv',
                linux: 'cat /etc/passwd',
                darwin: 'dscl . list /Users'
            },
            
            // BIOS Information
            'bios': {
                windows: 'wmic bios get /format:csv',
                linux: 'cat /sys/class/dmi/id/bios_* 2>/dev/null || dmidecode -t bios',
                darwin: 'system_profiler SPHardwareDataType'
            },
            
            // Startup Programs
            'startup': {
                windows: 'wmic startup get /format:csv',
                linux: 'ls /etc/init.d/ && ls ~/.config/autostart/ 2>/dev/null',
                darwin: 'ls ~/Library/LaunchAgents/ && ls /Library/LaunchDaemons/'
            },
            
            // Environment Variables
            'environment': {
                windows: 'wmic environment get /format:csv',
                linux: 'env',
                darwin: 'env'
            }
        };
    }

    // Main WMIC interface - mimics Windows WMIC command structure
    async wmic(className, properties = [], whereClause = '', format = 'csv') {
        const cacheKey = `${className}_${properties.join(',')}_${whereClause}_${format}`;
        
        // Check cache first
        if (this.cache.has(cacheKey)) {
            const cached = this.cache.get(cacheKey);
            if (Date.now() - cached.timestamp < this.cacheTimeout) {
                return cached.data;
            }
        }

        let result;
        const command = this.wmicCommands[className];
        
        if (!command) {
            throw new Error(`WMIC class '${className}' not supported`);
        }

        try {
            if (this.isWindows) {
                result = await this.executeWindowsWMIC(className, properties, whereClause, format);
            } else if (this.isLinux) {
                result = await this.executeLinuxCommand(command.linux, className, properties);
            } else if (this.isMacOS) {
                result = await this.executeMacCommand(command.darwin, className, properties);
            }
            
            // Cache the result
            this.cache.set(cacheKey, {
                data: result,
                timestamp: Date.now()
            });
            
            return result;
        } catch (error) {
            console.error(`WMIC Error for ${className}:`, error.message);
            throw error;
        }
    }

    // Windows WMIC execution
    async executeWindowsWMIC(className, properties, whereClause, format) {
        let command = `wmic ${className} get`;
        
        if (properties.length > 0) {
            command += ` ${properties.join(',')}`;
        } else {
            command += ` /format:${format}`;
        }
        
        if (whereClause) {
            command += ` where ${whereClause}`;
        }

        const { stdout, stderr } = await execAsync(command);
        if (stderr) {
            throw new Error(`WMIC Error: ${stderr}`);
        }
        
        return this.parseWindowsOutput(stdout, format);
    }

    // Linux command execution with parsing
    async executeLinuxCommand(command, className, properties) {
        if (typeof command === 'function') {
            return await command(className, properties);
        }
        
        const { stdout, stderr } = await execAsync(command);
        if (stderr) {
            throw new Error(`Linux Command Error: ${stderr}`);
        }
        
        return this.parseLinuxOutput(stdout, className);
    }

    // macOS command execution with parsing
    async executeMacCommand(command, className, properties) {
        if (typeof command === 'function') {
            return await command(className, properties);
        }
        
        const { stdout, stderr } = await execAsync(command);
        if (stderr) {
            throw new Error(`macOS Command Error: ${stderr}`);
        }
        
        return this.parseMacOutput(stdout, className);
    }

    // Linux-specific system information
    async getLinuxSystemInfo() {
        try {
            const manufacturer = await this.readFileSafe('/sys/class/dmi/id/sys_vendor');
            const productName = await this.readFileSafe('/sys/class/dmi/id/product_name');
            const version = await this.readFileSafe('/sys/class/dmi/id/product_version');
            
            return [{
                manufacturer: manufacturer.trim(),
                model: productName.trim(),
                version: version.trim(),
                platform: 'Linux'
            }];
        } catch (error) {
            // Fallback to basic info
            return [{
                manufacturer: 'Unknown',
                model: os.hostname(),
                version: os.release(),
                platform: 'Linux'
            }];
        }
    }

    // macOS-specific system information
    async getMacSystemInfo() {
        try {
            const { stdout } = await execAsync('system_profiler SPHardwareDataType');
            const lines = stdout.split('\n');
            
            const info = {};
            for (const line of lines) {
                if (line.includes(':')) {
                    const [key, value] = line.split(':').map(s => s.trim());
                    info[key.toLowerCase().replace(/\s+/g, '')] = value;
                }
            }
            
            return [{
                manufacturer: info.modelname || 'Apple',
                model: info.modelname || 'Mac',
                version: info.modelidentifier || '',
                platform: 'macOS'
            }];
        } catch (error) {
            return [{
                manufacturer: 'Apple',
                model: os.hostname(),
                version: os.release(),
                platform: 'macOS'
            }];
        }
    }

    // Safe file reading
    async readFileSafe(filePath) {
        try {
            return await fs.readFile(filePath, 'utf8');
        } catch (error) {
            return 'Unknown';
        }
    }

    // Windows output parsing
    parseWindowsOutput(output, format) {
        if (format === 'csv') {
            const lines = output.split('\n').filter(line => line.trim());
            if (lines.length < 2) return [];
            
            const headers = lines[0].split(',').map(h => h.trim());
            const results = [];
            
            for (let i = 1; i < lines.length; i++) {
                const values = lines[i].split(',');
                const obj = {};
                headers.forEach((header, index) => {
                    obj[header] = values[index] ? values[index].trim() : '';
                });
                results.push(obj);
            }
            
            return results;
        }
        return output;
    }

    // Linux output parsing
    parseLinuxOutput(output, className) {
        switch (className) {
            case 'cpu':
                return this.parseLinuxCPU(output);
            case 'memorychip':
                return this.parseLinuxMemory(output);
            case 'logicaldisk':
                return this.parseLinuxDisk(output);
            case 'networkadapter':
                return this.parseLinuxNetwork(output);
            case 'process':
                return this.parseLinuxProcesses(output);
            case 'service':
                return this.parseLinuxServices(output);
            case 'useraccount':
                return this.parseLinuxUsers(output);
            default:
                return { raw: output };
        }
    }

    // macOS output parsing
    parseMacOutput(output, className) {
        switch (className) {
            case 'cpu':
                return this.parseMacCPU(output);
            case 'memorychip':
                return this.parseMacMemory(output);
            case 'logicaldisk':
                return this.parseMacDisk(output);
            case 'networkadapter':
                return this.parseMacNetwork(output);
            case 'process':
                return this.parseMacProcesses(output);
            case 'service':
                return this.parseMacServices(output);
            case 'useraccount':
                return this.parseMacUsers(output);
            default:
                return { raw: output };
        }
    }

    // Linux CPU parsing
    parseLinuxCPU(output) {
        const lines = output.split('\n');
        const cpuInfo = {};
        
        lines.forEach(line => {
            if (line.includes(':')) {
                const [key, value] = line.split(':').map(s => s.trim());
                cpuInfo[key] = value;
            }
        });
        
        return [{
            name: cpuInfo['Model name'] || cpuInfo['CPU(s)'] || 'Unknown',
            cores: cpuInfo['CPU(s)'] || os.cpus().length,
            architecture: cpuInfo['Architecture'] || os.arch(),
            vendor: cpuInfo['Vendor ID'] || 'Unknown'
        }];
    }

    // Linux Memory parsing
    parseLinuxMemory(output) {
        const lines = output.split('\n');
        const memoryInfo = {};
        
        lines.forEach(line => {
            if (line.includes(':')) {
                const [key, value] = line.split(':').map(s => s.trim());
                memoryInfo[key] = value;
            }
        });
        
        return [{
            total: memoryInfo['MemTotal'] || os.totalmem(),
            free: memoryInfo['MemAvailable'] || memoryInfo['MemFree'],
            used: memoryInfo['MemTotal'] && memoryInfo['MemFree'] ? 
                  parseInt(memoryInfo['MemTotal']) - parseInt(memoryInfo['MemFree']) : 
                  os.totalmem() - os.freemem()
        }];
    }

    // Linux Disk parsing
    parseLinuxDisk(output) {
        const lines = output.split('\n');
        const disks = [];
        
        lines.forEach(line => {
            if (line.includes('/dev/')) {
                const parts = line.split(/\s+/);
                if (parts.length >= 6) {
                    disks.push({
                        device: parts[0],
                        size: parts[1],
                        used: parts[2],
                        available: parts[3],
                        usage: parts[4],
                        mountpoint: parts[5]
                    });
                }
            }
        });
        
        return disks;
    }

    // Linux Network parsing
    parseLinuxNetwork(output) {
        const lines = output.split('\n');
        const adapters = [];
        
        lines.forEach(line => {
            if (line.includes(':')) {
                const parts = line.split(':');
                const name = parts[0].trim();
                if (name && !name.includes('lo')) {
                    adapters.push({
                        name: name,
                        type: 'Ethernet',
                        status: 'Connected'
                    });
                }
            }
        });
        
        return adapters;
    }

    // Linux Processes parsing
    parseLinuxProcesses(output) {
        const lines = output.split('\n');
        const processes = [];
        
        for (let i = 1; i < lines.length; i++) {
            const parts = lines[i].split(/\s+/);
            if (parts.length >= 11) {
                processes.push({
                    pid: parts[1],
                    name: parts[10],
                    cpu: parts[2],
                    memory: parts[3],
                    user: parts[0]
                });
            }
        }
        
        return processes;
    }

    // Linux Services parsing
    parseLinuxServices(output) {
        const lines = output.split('\n');
        const services = [];
        
        lines.forEach(line => {
            if (line.includes('.service')) {
                const parts = line.split(/\s+/);
                if (parts.length >= 2) {
                    services.push({
                        name: parts[0].replace('.service', ''),
                        status: parts[2] || 'Unknown',
                        description: parts.slice(3).join(' ') || 'No description'
                    });
                }
            }
        });
        
        return services;
    }

    // Linux Users parsing
    parseLinuxUsers(output) {
        const lines = output.split('\n');
        const users = [];
        
        lines.forEach(line => {
            if (line.includes(':')) {
                const parts = line.split(':');
                if (parts.length >= 7) {
                    users.push({
                        name: parts[0],
                        uid: parts[2],
                        gid: parts[3],
                        description: parts[4],
                        home: parts[5],
                        shell: parts[6]
                    });
                }
            }
        });
        
        return users;
    }

    // macOS CPU parsing
    parseMacCPU(output) {
        const lines = output.split('\n');
        const cpuInfo = lines[0].trim();
        const coreInfo = lines[1].trim();
        
        return [{
            name: cpuInfo,
            cores: parseInt(coreInfo) || os.cpus().length,
            architecture: os.arch(),
            vendor: 'Apple'
        }];
    }

    // macOS Memory parsing
    parseMacMemory(output) {
        const lines = output.split('\n');
        const memoryInfo = {};
        
        lines.forEach(line => {
            if (line.includes(':')) {
                const [key, value] = line.split(':').map(s => s.trim());
                memoryInfo[key] = value;
            }
        });
        
        return [{
            total: memoryInfo['hw.memsize'] || os.totalmem(),
            free: os.freemem(),
            used: os.totalmem() - os.freemem()
        }];
    }

    // macOS Disk parsing
    parseMacDisk(output) {
        const lines = output.split('\n');
        const disks = [];
        
        lines.forEach(line => {
            if (line.includes('/dev/')) {
                const parts = line.split(/\s+/);
                if (parts.length >= 9) {
                    disks.push({
                        filesystem: parts[0],
                        size: parts[1],
                        used: parts[2],
                        available: parts[3],
                        usage: parts[4],
                        mountpoint: parts[8]
                    });
                }
            }
        });
        
        return disks;
    }

    // macOS Network parsing
    parseMacNetwork(output) {
        const lines = output.split('\n');
        const adapters = [];
        let currentAdapter = {};
        
        lines.forEach(line => {
            if (line.includes('flags=')) {
                if (currentAdapter.name) {
                    adapters.push(currentAdapter);
                }
                currentAdapter = {
                    name: line.split(':')[0].trim(),
                    type: 'Ethernet',
                    status: 'Connected'
                };
            }
        });
        
        if (currentAdapter.name) {
            adapters.push(currentAdapter);
        }
        
        return adapters;
    }

    // macOS Processes parsing
    parseMacProcesses(output) {
        const lines = output.split('\n');
        const processes = [];
        
        for (let i = 1; i < lines.length; i++) {
            const parts = lines[i].split(/\s+/);
            if (parts.length >= 11) {
                processes.push({
                    pid: parts[1],
                    name: parts[10],
                    cpu: parts[2],
                    memory: parts[3],
                    user: parts[0]
                });
            }
        }
        
        return processes;
    }

    // macOS Services parsing
    parseMacServices(output) {
        const lines = output.split('\n');
        const services = [];
        
        lines.forEach(line => {
            if (line.includes('\t')) {
                const parts = line.split('\t');
                if (parts.length >= 2) {
                    services.push({
                        name: parts[0].trim(),
                        status: parts[1].trim(),
                        description: 'macOS Service'
                    });
                }
            }
        });
        
        return services;
    }

    // macOS Users parsing
    parseMacUsers(output) {
        const lines = output.split('\n');
        const users = [];
        
        lines.forEach(line => {
            if (line.trim()) {
                users.push({
                    name: line.trim(),
                    uid: 'Unknown',
                    gid: 'Unknown',
                    description: 'macOS User',
                    home: `/Users/${line.trim()}`,
                    shell: '/bin/bash'
                });
            }
        });
        
        return users;
    }

    // Advanced WMIC operations
    async getSystemSummary() {
        const summary = {};
        
        try {
            summary.computerSystem = await this.wmic('computersystem');
            summary.cpu = await this.wmic('cpu');
            summary.memory = await this.wmic('memorychip');
            summary.disks = await this.wmic('logicaldisk');
            summary.network = await this.wmic('networkadapter');
        } catch (error) {
            console.error('Error getting system summary:', error);
        }
        
        return summary;
    }

    // Process management
    async killProcess(pid) {
        if (this.isWindows) {
            return await execAsync(`taskkill /PID ${pid} /F`);
        } else {
            return await execAsync(`kill -9 ${pid}`);
        }
    }

    // Service management
    async startService(serviceName) {
        if (this.isWindows) {
            return await execAsync(`net start ${serviceName}`);
        } else if (this.isLinux) {
            return await execAsync(`systemctl start ${serviceName}`);
        } else {
            return await execAsync(`launchctl start ${serviceName}`);
        }
    }

    async stopService(serviceName) {
        if (this.isWindows) {
            return await execAsync(`net stop ${serviceName}`);
        } else if (this.isLinux) {
            return await execAsync(`systemctl stop ${serviceName}`);
        } else {
            return await execAsync(`launchctl stop ${serviceName}`);
        }
    }

    // Clear cache
    clearCache() {
        this.cache.clear();
    }

    // Get supported classes
    getSupportedClasses() {
        return Object.keys(this.wmicCommands);
    }

    // Check if class is supported
    isClassSupported(className) {
        return this.wmicCommands.hasOwnProperty(className);
    }

    // Method to check if engine is self-sufficient
    isSelfSufficient() {
        return this.selfSufficient;
    }

    // Method to get dependency information
    getDependencyInfo() {
        return {
            selfSufficient: this.selfSufficient,
            externalDependencies: this.externalDependencies,
            requiredDependencies: this.requiredDependencies,
            hasExternalDependencies: this.externalDependencies.length > 0,
            hasRequiredDependencies: this.requiredDependencies.length > 0
        };
    }
}

// Create and export instance
const universalWMICEngine = new UniversalWMICEngine();

module.exports = universalWMICEngine;
