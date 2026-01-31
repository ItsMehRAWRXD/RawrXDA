// RawrZ Built-in Database - No external dependencies required
const fs = require('fs').promises;
const path = require('path');
const crypto = require('crypto');
const { logger } = require('./logger');

class BuiltinDatabase {
    constructor() {
        this.dataPath = path.join(__dirname, '../../data');
        this.dbFile = path.join(this.dataPath, 'rawrz-database.json');
        this.backupFile = path.join(this.dataPath, 'rawrz-backup.json');
        this.data = {
            commands: [],
            operations: [],
            systemStats: {},
            lastBackup: null,
            version: '1.0.0'
        };
        this.autoSaveInterval = null;
        this.enabled = false;
    }

    // Initialize database
    async initialize() {
        try {
            // Create data directory if it doesn't exist
            await fs.mkdir(this.dataPath, { recursive: true });
            
            // Load existing data
            await this.loadData();
            
            // Start auto-save
            this.startAutoSave();
            
            this.enabled = true;
            logger.info('[BUILTIN-DB] Built-in database initialized successfully');
            return true;
        } catch (error) {
            logger.error('[BUILTIN-DB] Failed to initialize:', error);
            return false;
        }
    }

    // Load data from file
    async loadData() {
        try {
            const data = await fs.readFile(this.dbFile, 'utf8');
            this.data = JSON.parse(data);
            logger.info(`[BUILTIN-DB] Loaded ${this.data.commands.length} commands, ${this.data.operations.length} operations`);
        } catch (error) {
            // File doesn't exist or is corrupted, start fresh
            logger.info('[BUILTIN-DB] Starting with fresh database');
            this.data = {
                commands: [],
                operations: [],
                systemStats: {},
                lastBackup: null,
                version: '1.0.0'
            };
        }
    }

    // Save data to file
    async saveData() {
        try {
            this.data.lastBackup = new Date().toISOString();
            await fs.writeFile(this.dbFile, JSON.stringify(this.data, null, 2));
            
            // Also create backup
            await fs.writeFile(this.backupFile, JSON.stringify(this.data, null, 2));
            
            logger.debug('[BUILTIN-DB] Data saved successfully');
        } catch (error) {
            logger.error('[BUILTIN-DB] Failed to save data:', error);
        }
    }

    // Start auto-save every 30 seconds
    startAutoSave() {
        this.autoSaveInterval = setInterval(() => {
            this.saveData();
        }, 30000);
    }

    // Stop auto-save
    stopAutoSave() {
        if (this.autoSaveInterval) {
            clearInterval(this.autoSaveInterval);
            this.autoSaveInterval = null;
        }
    }

    // Log command
    async logCommand(commandData) {
        if (!this.enabled) return null;

        try {
            const command = {
                id: crypto.randomUUID(),
                command: commandData.command,
                args: commandData.args || [],
                userId: commandData.userId,
                channel: commandData.channel,
                response: commandData.response,
                executionTime: commandData.executionTime,
                status: commandData.status || 'success',
                error: commandData.error,
                timestamp: new Date().toISOString(),
                metadata: commandData.metadata || {}
            };

            this.data.commands.push(command);
            
            // Keep only last 1000 commands to prevent file from growing too large
            if (this.data.commands.length > 1000) {
                this.data.commands = this.data.commands.slice(-1000);
            }

            await this.saveData();
            logger.debug(`[BUILTIN-DB] Command logged: ${commandData.command}`);
            return command;
        } catch (error) {
            logger.error('[BUILTIN-DB] Failed to log command:', error);
            return null;
        }
    }

    // Log operation
    async logOperation(operationData) {
        if (!this.enabled) return null;

        try {
            const operation = {
                id: crypto.randomUUID(),
                type: operationData.type, // 'encryption', 'stub', 'polymorphic', etc.
                data: operationData.data,
                userId: operationData.userId,
                channel: operationData.channel,
                status: operationData.status || 'success',
                error: operationData.error,
                timestamp: new Date().toISOString(),
                metadata: operationData.metadata || {}
            };

            this.data.operations.push(operation);
            
            // Keep only last 500 operations
            if (this.data.operations.length > 500) {
                this.data.operations = this.data.operations.slice(-500);
            }

            await this.saveData();
            logger.debug(`[BUILTIN-DB] Operation logged: ${operationData.type}`);
            return operation;
        } catch (error) {
            logger.error('[BUILTIN-DB] Failed to log operation:', error);
            return null;
        }
    }

    // Update system stats
    async updateSystemStats(statsData) {
        if (!this.enabled) return null;

        try {
            this.data.systemStats = {
                ...this.data.systemStats,
                ...statsData,
                lastUpdate: new Date().toISOString()
            };

            await this.saveData();
            return this.data.systemStats;
        } catch (error) {
            logger.error('[BUILTIN-DB] Failed to update system stats:', error);
            return null;
        }
    }

    // Get statistics
    getStats() {
        if (!this.enabled) {
            return {
                enabled: false,
                message: 'Built-in database is disabled'
            };
        }

        const now = new Date();
        const last24h = new Date(now.getTime() - 24 * 60 * 60 * 1000);

        const recentCommands = this.data.commands.filter(cmd => 
            new Date(cmd.timestamp) > last24h
        );

        const recentOperations = this.data.operations.filter(op => 
            new Date(op.timestamp) > last24h
        );

        const successfulCommands = this.data.commands.filter(cmd => cmd.status === 'success').length;
        const failedCommands = this.data.commands.filter(cmd => cmd.status === 'error' || cmd.status === 'failed').length;

        return {
            enabled: true,
            type: 'builtin',
            stats: {
                totalCommands: this.data.commands.length,
                totalOperations: this.data.operations.length,
                successfulCommands,
                failedCommands,
                successRate: this.data.commands.length > 0 ? 
                    (successfulCommands / this.data.commands.length * 100).toFixed(1) : 0,
                last24Hours: {
                    commands: recentCommands.length,
                    operations: recentOperations.length
                },
                lastBackup: this.data.lastBackup,
                dataSize: JSON.stringify(this.data).length
            }
        };
    }

    // Get recent commands
    getRecentCommands(limit = 10) {
        if (!this.enabled) return [];
        return this.data.commands.slice(-limit).reverse();
    }

    // Get recent operations
    getRecentOperations(limit = 10) {
        if (!this.enabled) return [];
        return this.data.operations.slice(-limit).reverse();
    }

    // Health check
    healthCheck() {
        if (!this.enabled) {
            return {
                status: 'disabled',
                message: 'Built-in database is disabled'
            };
        }

        try {
            const stats = this.getStats();
            return {
                status: 'healthy',
                message: 'Built-in database is operational',
                stats: stats.stats
            };
        } catch (error) {
            return {
                status: 'error',
                message: `Built-in database error: ${error.message}`
            };
        }
    }

    // Cleanup old data
    async cleanup() {
        if (!this.enabled) return;

        try {
            const cutoffDate = new Date(Date.now() - 7 * 24 * 60 * 60 * 1000); // 7 days ago
            
            this.data.commands = this.data.commands.filter(cmd => 
                new Date(cmd.timestamp) > cutoffDate
            );
            
            this.data.operations = this.data.operations.filter(op => 
                new Date(op.timestamp) > cutoffDate
            );

            await this.saveData();
            logger.info('[BUILTIN-DB] Cleanup completed');
        } catch (error) {
            logger.error('[BUILTIN-DB] Cleanup failed:', error);
        }
    }

    // Shutdown
    async shutdown() {
        this.stopAutoSave();
        await this.saveData();
        logger.info('[BUILTIN-DB] Database shutdown complete');
    }
}

// Create singleton instance
const builtinDatabase = new BuiltinDatabase();

module.exports = { builtinDatabase };
