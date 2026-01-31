// RawrZ Startup Persistence Engine - System startup and persistence management
const crypto = require('crypto');
const fs = require('fs').promises;
const path = require('path');
const { EventEmitter } = require('events');
const { logger } = require('../utils/logger');

class StartupPersistence extends EventEmitter {
    constructor() {
        super();
        this.name = 'Startup Persistence';
        this.version = '1.0.0';
        this.initialized = false;
        this.config = {};
        this.persistenceMethods = new Map();
        this.startupEntries = new Map();
        this.scheduledTasks = new Map();
        this.registryEntries = new Map();
        this.serviceEntries = new Map();
        this.stats = {
            operations: 0,
            errors: 0,
            startTime: Date.now(),
            persistenceMethods: 0,
            startupEntries: 0,
            scheduledTasks: 0
        };
    }

    async initialize(config = {}) {
        try {
            this.config = { ...this.config, ...config };
            
            // Initialize persistence methods
            await this.initializePersistenceMethods();
            
            // Initialize startup management
            await this.initializeStartupManagement();
            
            // Initialize scheduled tasks
            await this.initializeScheduledTasks();
            
            // Initialize registry management
            await this.initializeRegistryManagement();
            
            // Initialize service management
            await this.initializeServiceManagement();

            this.initialized = true;
            logger.info('Startup Persistence initialized successfully');
            this.emit('initialized');
            return { success: true, message: 'Startup Persistence initialized' };
        } catch (error) {
            logger.error('Startup Persistence initialization failed:', error);
            this.stats.errors++;
            throw error;
        }
    }

    async initializePersistenceMethods() {
        // Registry-based persistence
        this.persistenceMethods.set('registry_run', {
            name: 'Registry Run Key',
            description: 'Add entry to Windows Registry Run key',
            platform: 'windows',
            method: 'registry',
            location: 'HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run',
            effectiveness: 85
        });

        this.persistenceMethods.set('registry_runonce', {
            name: 'Registry RunOnce Key',
            description: 'Add entry to Windows Registry RunOnce key',
            platform: 'windows',
            method: 'registry',
            location: 'HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\RunOnce',
            effectiveness: 80
        });

        this.persistenceMethods.set('startup_folder', {
            name: 'Startup Folder',
            description: 'Add shortcut to Windows Startup folder',
            platform: 'windows',
            method: 'file',
            location: '%APPDATA%\\Microsoft\\Windows\\Start Menu\\Programs\\Startup',
            effectiveness: 90
        });

        this.persistenceMethods.set('scheduled_task', {
            name: 'Scheduled Task',
            description: 'Create Windows Scheduled Task',
            platform: 'windows',
            method: 'task',
            location: 'Task Scheduler',
            effectiveness: 95
        });

        this.persistenceMethods.set('service', {
            name: 'Windows Service',
            description: 'Install as Windows Service',
            platform: 'windows',
            method: 'service',
            location: 'Services',
            effectiveness: 98
        });

        this.persistenceMethods.set('wmi_event', {
            name: 'WMI Event Subscription',
            description: 'Create WMI Event Subscription',
            platform: 'windows',
            method: 'wmi',
            location: 'WMI Repository',
            effectiveness: 92
        });

        this.persistenceMethods.set('autostart_plist', {
            name: 'macOS LaunchAgent',
            description: 'Create macOS LaunchAgent plist',
            platform: 'darwin',
            method: 'plist',
            location: '~/Library/LaunchAgents',
            effectiveness: 88
        });

        this.persistenceMethods.set('systemd_service', {
            name: 'Linux Systemd Service',
            description: 'Create Linux systemd service',
            platform: 'linux',
            method: 'systemd',
            location: '/etc/systemd/system',
            effectiveness: 90
        });

        this.persistenceMethods.set('crontab', {
            name: 'Linux Crontab',
            description: 'Add entry to Linux crontab',
            platform: 'linux',
            method: 'cron',
            location: '/etc/crontab',
            effectiveness: 85
        });

        this.stats.persistenceMethods = this.persistenceMethods.size;
        logger.info(`Initialized ${this.persistenceMethods.size} persistence methods`);
    }

    async initializeStartupManagement() {
        // Startup entry management
        this.startupEntries = new Map();
        logger.info('Startup management initialized');
    }

    async initializeScheduledTasks() {
        // Scheduled task management
        this.scheduledTasks = new Map();
        logger.info('Scheduled task management initialized');
    }

    async initializeRegistryManagement() {
        // Registry entry management
        this.registryEntries = new Map();
        logger.info('Registry management initialized');
    }

    async initializeServiceManagement() {
        // Service management
        this.serviceEntries = new Map();
        logger.info('Service management initialized');
    }

    // Persistence Methods
    async createPersistenceEntry(method, targetPath, options = {}) {
        try {
            const persistenceMethod = this.persistenceMethods.get(method);
            if (!persistenceMethod) {
                throw new Error(`Persistence method ${method} not found`);
            }

            const entryId = crypto.randomUUID();
            const entry = {
                id: entryId,
                method: method,
                targetPath: targetPath,
                options: options,
                created: new Date().toISOString(),
                status: 'active',
                platform: persistenceMethod.platform,
                effectiveness: persistenceMethod.effectiveness
            };

            // Create persistence based on method
            switch (persistenceMethod.method) {
                case 'registry':
                    await this.createRegistryEntry(entry);
                    break;
                case 'file':
                    await this.createFileEntry(entry);
                    break;
                case 'task':
                    await this.createScheduledTask(entry);
                    break;
                case 'service':
                    await this.createServiceEntry(entry);
                    break;
                case 'wmi':
                    await this.createWMIEntry(entry);
                    break;
                case 'plist':
                    await this.createPlistEntry(entry);
                    break;
                case 'systemd':
                    await this.createSystemdEntry(entry);
                    break;
                case 'cron':
                    await this.createCronEntry(entry);
                    break;
                default:
                    throw new Error(`Unsupported persistence method: ${persistenceMethod.method}`);
            }

            this.startupEntries.set(entryId, entry);
            this.stats.startupEntries++;
            this.stats.operations++;

            logger.info(`Persistence entry created: ${method} for ${targetPath}`);
            this.emit('persistenceCreated', entry);
            return entry;
        } catch (error) {
            logger.error(`Failed to create persistence entry: ${method}`, error);
            this.stats.errors++;
            throw error;
        }
    }

    async createRegistryEntry(entry) {
        // Simulate registry entry creation
        const registryEntry = {
            id: crypto.randomUUID(),
            key: entry.options.key || 'RawrZApp',
            value: entry.targetPath,
            type: 'REG_SZ',
            created: new Date().toISOString()
        };

        this.registryEntries.set(registryEntry.id, registryEntry);
        logger.info(`Registry entry created: ${registryEntry.key} = ${registryEntry.value}`);
    }

    async createFileEntry(entry) {
        // Simulate file entry creation (shortcut)
        const fileEntry = {
            id: crypto.randomUUID(),
            name: entry.options.name || 'RawrZApp.lnk',
            target: entry.targetPath,
            created: new Date().toISOString()
        };

        logger.info(`File entry created: ${fileEntry.name} -> ${fileEntry.target}`);
    }

    async createScheduledTask(entry) {
        // Simulate scheduled task creation
        const taskEntry = {
            id: crypto.randomUUID(),
            name: entry.options.name || 'RawrZApp',
            command: entry.targetPath,
            trigger: entry.options.trigger || 'atstartup',
            created: new Date().toISOString()
        };

        this.scheduledTasks.set(taskEntry.id, taskEntry);
        this.stats.scheduledTasks++;
        logger.info(`Scheduled task created: ${taskEntry.name}`);
    }

    async createServiceEntry(entry) {
        // Simulate service creation
        const serviceEntry = {
            id: crypto.randomUUID(),
            name: entry.options.name || 'RawrZApp',
            displayName: entry.options.displayName || 'RawrZ Application',
            executable: entry.targetPath,
            startType: entry.options.startType || 'automatic',
            created: new Date().toISOString()
        };

        this.serviceEntries.set(serviceEntry.id, serviceEntry);
        logger.info(`Service created: ${serviceEntry.name}`);
    }

    async createWMIEntry(entry) {
        // Simulate WMI event subscription
        const wmiEntry = {
            id: crypto.randomUUID(),
            name: entry.options.name || 'RawrZApp',
            eventFilter: entry.options.eventFilter || 'Win32_ProcessStartTrace',
            action: entry.targetPath,
            created: new Date().toISOString()
        };

        logger.info(`WMI entry created: ${wmiEntry.name}`);
    }

    async createPlistEntry(entry) {
        // Simulate macOS LaunchAgent plist creation
        const plistEntry = {
            id: crypto.randomUUID(),
            name: entry.options.name || 'com.rawrz.app',
            program: entry.targetPath,
            runAtLoad: true,
            created: new Date().toISOString()
        };

        logger.info(`Plist entry created: ${plistEntry.name}`);
    }

    async createSystemdEntry(entry) {
        // Simulate systemd service creation
        const systemdEntry = {
            id: crypto.randomUUID(),
            name: entry.options.name || 'rawrz-app',
            execStart: entry.targetPath,
            wantedBy: 'multi-user.target',
            created: new Date().toISOString()
        };

        logger.info(`Systemd entry created: ${systemdEntry.name}`);
    }

    async createCronEntry(entry) {
        // Simulate crontab entry creation
        const cronEntry = {
            id: crypto.randomUUID(),
            schedule: entry.options.schedule || '@reboot',
            command: entry.targetPath,
            created: new Date().toISOString()
        };

        logger.info(`Cron entry created: ${cronEntry.schedule} ${cronEntry.command}`);
    }

    // Management Methods
    async removePersistenceEntry(entryId) {
        try {
            const entry = this.startupEntries.get(entryId);
            if (!entry) {
                throw new Error(`Persistence entry ${entryId} not found`);
            }

            // Remove persistence based on method
            const persistenceMethod = this.persistenceMethods.get(entry.method);
            if (persistenceMethod) {
                switch (persistenceMethod.method) {
                    case 'registry':
                        await this.removeRegistryEntry(entryId);
                        break;
                    case 'file':
                        await this.removeFileEntry(entryId);
                        break;
                    case 'task':
                        await this.removeScheduledTask(entryId);
                        break;
                    case 'service':
                        await this.removeServiceEntry(entryId);
                        break;
                    case 'wmi':
                        await this.removeWMIEntry(entryId);
                        break;
                    case 'plist':
                        await this.removePlistEntry(entryId);
                        break;
                    case 'systemd':
                        await this.removeSystemdEntry(entryId);
                        break;
                    case 'cron':
                        await this.removeCronEntry(entryId);
                        break;
                }
            }

            this.startupEntries.delete(entryId);
            this.stats.startupEntries--;
            this.stats.operations++;

            logger.info(`Persistence entry removed: ${entryId}`);
            this.emit('persistenceRemoved', entry);
            return true;
        } catch (error) {
            logger.error(`Failed to remove persistence entry: ${entryId}`, error);
            this.stats.errors++;
            throw error;
        }
    }

    async removeRegistryEntry(entryId) {
        // Simulate registry entry removal
        this.registryEntries.delete(entryId);
        logger.info(`Registry entry removed: ${entryId}`);
    }

    async removeFileEntry(entryId) {
        // Simulate file entry removal
        logger.info(`File entry removed: ${entryId}`);
    }

    async removeScheduledTask(entryId) {
        // Simulate scheduled task removal
        this.scheduledTasks.delete(entryId);
        this.stats.scheduledTasks--;
        logger.info(`Scheduled task removed: ${entryId}`);
    }

    async removeServiceEntry(entryId) {
        // Simulate service removal
        this.serviceEntries.delete(entryId);
        logger.info(`Service removed: ${entryId}`);
    }

    async removeWMIEntry(entryId) {
        // Simulate WMI entry removal
        logger.info(`WMI entry removed: ${entryId}`);
    }

    async removePlistEntry(entryId) {
        // Simulate plist entry removal
        logger.info(`Plist entry removed: ${entryId}`);
    }

    async removeSystemdEntry(entryId) {
        // Simulate systemd entry removal
        logger.info(`Systemd entry removed: ${entryId}`);
    }

    async removeCronEntry(entryId) {
        // Simulate cron entry removal
        logger.info(`Cron entry removed: ${entryId}`);
    }

    // Query Methods
    getPersistenceEntries(filter = {}) {
        let entries = Array.from(this.startupEntries.values());

        if (filter.method) {
            entries = entries.filter(entry => entry.method === filter.method);
        }

        if (filter.platform) {
            entries = entries.filter(entry => entry.platform === filter.platform);
        }

        if (filter.status) {
            entries = entries.filter(entry => entry.status === filter.status);
        }

        return entries;
    }

    getPersistenceMethods(platform = null) {
        if (platform) {
            return Array.from(this.persistenceMethods.values()).filter(method => method.platform === platform);
        }
        return Array.from(this.persistenceMethods.values());
    }

    getPersistenceStats() {
        return {
            total: this.startupEntries.size,
            byMethod: {},
            byPlatform: {},
            byStatus: {}
        };
    }

    // Status and Configuration Methods
    async getStatus() {
        return {
            name: this.name,
            version: this.version,
            initialized: this.initialized,
            persistenceMethods: this.persistenceMethods.size,
            startupEntries: this.startupEntries.size,
            scheduledTasks: this.scheduledTasks.size,
            registryEntries: this.registryEntries.size,
            serviceEntries: this.serviceEntries.size,
            stats: this.stats
        };
    }

    async cleanup() {
        try {
            // Cleanup all persistence entries
            for (const [entryId, entry] of this.startupEntries) {
                await this.removePersistenceEntry(entryId);
            }

            this.initialized = false;
            logger.info('Startup Persistence cleanup completed');
            this.emit('cleanup');
            return { success: true, message: 'Startup Persistence cleaned up' };
        } catch (error) {
            logger.error('Startup Persistence cleanup failed:', error);
            throw error;
        }
    }

    // Panel Integration Methods
    async getPanelConfig() {
        return {
            name: this.name,
            version: this.version,
            description: 'Startup Persistence Engine for system startup and persistence management',
            endpoints: this.getAvailableEndpoints(),
            settings: this.getSettings(),
            status: await this.getStatus()
        };
    }

    getAvailableEndpoints() {
        return [
            { method: 'GET', path: '/api/' + this.name + '/status', description: 'Get engine status' },
            { method: 'POST', path: '/api/' + this.name + '/create', description: 'Create persistence entry' },
            { method: 'DELETE', path: '/api/' + this.name + '/remove', description: 'Remove persistence entry' },
            { method: 'GET', path: '/api/' + this.name + '/entries', description: 'List persistence entries' }
        ];
    }

    getSettings() {
        return {
            enabled: true,
            autoStart: false,
            config: {
                supportedPlatforms: ['windows', 'darwin', 'linux'],
                defaultMethod: 'registry_run',
                maxEntries: 100
            }
        };
    }

    // CLI Integration Methods
    async getCLICommands() {
        return [
            {
                command: this.name + ' status',
                description: 'Get engine status',
                action: async () => {
                    return await this.getStatus();
                }
            },
            {
                command: this.name + ' methods',
                description: 'List persistence methods',
                action: async () => {
                    return this.getPersistenceMethods();
                }
            },
            {
                command: this.name + ' entries',
                description: 'List persistence entries',
                action: async () => {
                    return this.getPersistenceEntries();
                }
            },
            {
                command: this.name + ' stats',
                description: 'Get statistics',
                action: async () => {
                    return this.getPersistenceStats();
                }
            }
        ];
    }
}

module.exports = new StartupPersistence();
