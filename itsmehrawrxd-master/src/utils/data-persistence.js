// RawrZ Data Persistence System
const fs = require('fs').promises;
const path = require('path');
const crypto = require('crypto');
const { logger } = require('./logger');

class DataPersistence {
    constructor() {
        this.dataDir = path.join(__dirname, '../../data');
        this.botsFile = path.join(this.dataDir, 'bots.json');
        this.commandsFile = path.join(this.dataDir, 'commands.json');
        this.extractedDataFile = path.join(this.dataDir, 'extracted-data.json');
        this.auditLogFile = path.join(this.dataDir, 'audit-log.json');
        this.initialized = false;
    }

    async initialize() {
        if (this.initialized) return true;

        try {
            // Create data directory if it doesn't exist
            await fs.mkdir(this.dataDir, { recursive: true });

            // Initialize data files if they don't exist
            await this.initializeDataFiles();

            this.initialized = true;
            logger.info('Data persistence system initialized');
            return true;
        } catch (error) {
            logger.error('Failed to initialize data persistence:', error);
            throw error;
        }
    }

    async initializeDataFiles() {
        const files = [
            { path: this.botsFile, defaultData: [] },
            { path: this.commandsFile, defaultData: [] },
            { path: this.extractedDataFile, defaultData: [] },
            { path: this.auditLogFile, defaultData: [] }
        ];

        for (const file of files) {
            try {
                await fs.access(file.path);
            } catch (error) {
                // File doesn't exist, create it with default data
                await fs.writeFile(file.path, JSON.stringify(file.defaultData, null, 2));
            }
        }
    }

    async saveBots(bots) {
        try {
            const data = {
                timestamp: new Date().toISOString(),
                bots: bots,
                count: bots.length
            };
            await fs.writeFile(this.botsFile, JSON.stringify(data, null, 2));
            logger.info(`Saved ${bots.length} bots to persistence`);
        } catch (error) {
            logger.error('Failed to save bots:', error);
            throw error;
        }
    }

    async loadBots() {
        try {
            const data = await fs.readFile(this.botsFile, 'utf8');
            const parsed = JSON.parse(data);
            return parsed.bots || [];
        } catch (error) {
            logger.error('Failed to load bots:', error);
            return [];
        }
    }

    async saveCommand(command) {
        try {
            const commands = await this.loadCommands();
            commands.push({
                ...command,
                id: command.id || crypto.randomUUID(),
                timestamp: command.timestamp || new Date().toISOString()
            });

            // Keep only last 10000 commands
            if (commands.length > 10000) {
                commands.splice(0, commands.length - 10000);
            }

            const data = {
                timestamp: new Date().toISOString(),
                commands: commands,
                count: commands.length
            };
            await fs.writeFile(this.commandsFile, JSON.stringify(data, null, 2));
            logger.info(`Saved command: ${command.action}`);
        } catch (error) {
            logger.error('Failed to save command:', error);
            throw error;
        }
    }

    async loadCommands() {
        try {
            const data = await fs.readFile(this.commandsFile, 'utf8');
            const parsed = JSON.parse(data);
            return parsed.commands || [];
        } catch (error) {
            logger.error('Failed to load commands:', error);
            return [];
        }
    }

    async saveExtractedData(data) {
        try {
            const extractedData = await this.loadExtractedData();
            extractedData.push({
                ...data,
                id: data.id || crypto.randomUUID(),
                timestamp: data.timestamp || new Date().toISOString()
            });

            // Keep only last 50000 entries
            if (extractedData.length > 50000) {
                extractedData.splice(0, extractedData.length - 50000);
            }

            const dataFile = {
                timestamp: new Date().toISOString(),
                extractedData: extractedData,
                count: extractedData.length
            };
            await fs.writeFile(this.extractedDataFile, JSON.stringify(dataFile, null, 2));
            logger.info(`Saved extracted data: ${data.type}`);
        } catch (error) {
            logger.error('Failed to save extracted data:', error);
            throw error;
        }
    }

    async loadExtractedData() {
        try {
            const data = await fs.readFile(this.extractedDataFile, 'utf8');
            const parsed = JSON.parse(data);
            return parsed.extractedData || [];
        } catch (error) {
            logger.error('Failed to load extracted data:', error);
            return [];
        }
    }

    async saveAuditLog(logEntry) {
        try {
            const auditLog = await this.loadAuditLog();
            auditLog.push({
                ...logEntry,
                id: logEntry.id || crypto.randomUUID(),
                timestamp: logEntry.timestamp || new Date().toISOString()
            });

            // Keep only last 10000 audit entries
            if (auditLog.length > 10000) {
                auditLog.splice(0, auditLog.length - 10000);
            }

            const data = {
                timestamp: new Date().toISOString(),
                auditLog: auditLog,
                count: auditLog.length
            };
            await fs.writeFile(this.auditLogFile, JSON.stringify(data, null, 2));
            logger.info(`Saved audit log: ${logEntry.action}`);
        } catch (error) {
            logger.error('Failed to save audit log:', error);
            throw error;
        }
    }

    async loadAuditLog() {
        try {
            const data = await fs.readFile(this.auditLogFile, 'utf8');
            const parsed = JSON.parse(data);
            return parsed.auditLog || [];
        } catch (error) {
            logger.error('Failed to load audit log:', error);
            return [];
        }
    }

    async getStats() {
        try {
            const bots = await this.loadBots();
            const commands = await this.loadCommands();
            const extractedData = await this.loadExtractedData();
            const auditLog = await this.loadAuditLog();

            return {
                bots: {
                    total: bots.length,
                    active: bots.filter(bot => bot.status === 'online').length,
                    offline: bots.filter(bot => bot.status === 'offline').length
                },
                commands: {
                    total: commands.length,
                    recent: commands.slice(-100).length
                },
                extractedData: {
                    total: extractedData.length,
                    byType: this.groupByType(extractedData)
                },
                auditLog: {
                    total: auditLog.length,
                    recent: auditLog.slice(-100).length
                }
            };
        } catch (error) {
            logger.error('Failed to get persistence stats:', error);
            return null;
        }
    }

    groupByType(data) {
        const grouped = {};
        data.forEach(item => {
            const type = item.type || 'unknown';
            grouped[type] = (grouped[type] || 0) + 1;
        });
        return grouped;
    }

    async cleanup() {
        try {
            // Clean up old data files if they get too large
            const stats = await this.getStats();
            if (stats) {
                logger.info('Data persistence cleanup completed', stats);
            }
        } catch (error) {
            logger.error('Failed to cleanup data persistence:', error);
        }
    }
}

module.exports = new DataPersistence();
