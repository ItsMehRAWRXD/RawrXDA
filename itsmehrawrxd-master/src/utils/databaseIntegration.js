// Database Integration - Seamlessly integrates database features with existing functionality
const { builtinDatabase } = require('./builtinDatabase');
const { logger } = require('./logger');
const crypto = require('crypto');

class DatabaseIntegration {
    constructor() {
        this.enabled = false;
        this.connectionStatus = 'disconnected';
        this.stats = {
            totalOperations: 0,
            successfulOperations: 0,
            failedOperations: 0,
            lastOperation: null
        };
    }

    // Initialize database integration
    async initialize() {
        try {
            // Initialize built-in database
            const dbInitialized = await builtinDatabase.initialize();
            if (dbInitialized) {
                this.enabled = true;
                this.connectionStatus = 'connected';
                logger.info('[DB-INTEGRATION] Built-in database integration enabled');
                return true;
            } else {
                this.enabled = false;
                this.connectionStatus = 'disconnected';
                logger.info('[DB-INTEGRATION] Built-in database integration disabled');
                return false;
            }
        } catch (error) {
            this.enabled = false;
            this.connectionStatus = 'error';
            logger.error('[DB-INTEGRATION] Failed to initialize:', error);
            return false;
        }
    }

    // Log encryption operation
    async logEncryptionOperation(operationData) {
        if (!this.enabled) return null;

        try {
            const operation = {
                operationId: crypto.randomUUID(),
                algorithm: operationData.algorithm,
                dataType: operationData.dataType || 'text',
                inputSize: operationData.inputSize || 0,
                outputSize: operationData.outputSize || 0,
                keyHash: operationData.keyHash || crypto.createHash('sha256').update(operationData.key || '').digest('hex'),
                iv: operationData.iv,
                authTag: operationData.authTag,
                compression: operationData.compression || false,
                obfuscation: operationData.obfuscation || false,
                userId: operationData.userId,
                channel: operationData.channel,
                status: operationData.status || 'success',
                error: operationData.error,
                metadata: operationData.metadata || {}
            };

            await dbUtils.saveEncryptionOperation(operation);
            this.stats.totalOperations++;
            if (operation.status === 'success') {
                this.stats.successfulOperations++;
            } else {
                this.stats.failedOperations++;
            }
            this.stats.lastOperation = new Date();

            return operation;
        } catch (error) {
            logger.error('[DB-INTEGRATION] Failed to log encryption operation:', error);
            return null;
        }
    }

    // Log stub generation
    async logStubGeneration(stubData) {
        if (!this.enabled) return null;

        try {
            const stub = {
                stubId: crypto.randomUUID(),
                target: stubData.target,
                stubType: stubData.stubType,
                encryptionMethod: stubData.encryptionMethod,
                outputPath: stubData.outputPath,
                includeAntiDebug: stubData.includeAntiDebug || true,
                includeAntiVM: stubData.includeAntiVM || true,
                includeAntiSandbox: stubData.includeAntiSandbox || true,
                payloadSize: stubData.payloadSize || 0,
                stubSize: stubData.stubSize || 0,
                compilationStatus: stubData.compilationStatus || 'pending',
                compilationError: stubData.compilationError,
                userId: stubData.userId,
                channel: stubData.channel,
                metadata: stubData.metadata || {}
            };

            await dbUtils.saveStubGeneration(stub);
            this.stats.totalOperations++;
            if (stub.compilationStatus === 'success') {
                this.stats.successfulOperations++;
            } else if (stub.compilationStatus === 'failed') {
                this.stats.failedOperations++;
            }

            return stub;
        } catch (error) {
            logger.error('[DB-INTEGRATION] Failed to log stub generation:', error);
            return null;
        }
    }

    // Log polymorphic operation
    async logPolymorphicOperation(polyData) {
        if (!this.enabled) return null;

        try {
            const operation = {
                mutationId: crypto.randomUUID(),
                originalCode: polyData.originalCode,
                mutatedCode: polyData.mutatedCode,
                mutationTypes: polyData.mutationTypes || [],
                intensity: polyData.intensity || 'medium',
                targetArchitecture: polyData.targetArchitecture || 'x64',
                originalSize: polyData.originalSize || 0,
                mutatedSize: polyData.mutatedSize || 0,
                appliedMutations: polyData.appliedMutations || [],
                userId: polyData.userId,
                channel: polyData.channel,
                metadata: polyData.metadata || {}
            };

            await dbUtils.savePolymorphicOperation(operation);
            this.stats.totalOperations++;
            this.stats.successfulOperations++;

            return operation;
        } catch (error) {
            logger.error('[DB-INTEGRATION] Failed to log polymorphic operation:', error);
            return null;
        }
    }

    // Log file operation
    async logFileOperation(fileData) {
        if (!this.enabled) return null;

        try {
            const operation = {
                operationId: crypto.randomUUID(),
                fileName: fileData.fileName,
                originalPath: fileData.originalPath,
                processedPath: fileData.processedPath,
                operationType: fileData.operationType,
                fileSize: fileData.fileSize || 0,
                fileType: fileData.fileType,
                checksum: fileData.checksum,
                encryption: fileData.encryption,
                status: fileData.status || 'completed',
                error: fileData.error,
                userId: fileData.userId,
                channel: fileData.channel,
                metadata: fileData.metadata || {}
            };

            await dbUtils.saveFileOperation(operation);
            this.stats.totalOperations++;
            if (operation.status === 'completed') {
                this.stats.successfulOperations++;
            } else if (operation.status === 'failed') {
                this.stats.failedOperations++;
            }

            return operation;
        } catch (error) {
            logger.error('[DB-INTEGRATION] Failed to log file operation:', error);
            return null;
        }
    }

    // Log command history
    async logCommandHistory(commandData) {
        if (!this.enabled) return null;

        try {
            const history = {
                commandId: crypto.randomUUID(),
                command: commandData.command,
                args: commandData.args || [],
                userId: commandData.userId,
                channel: commandData.channel,
                response: commandData.response,
                executionTime: commandData.executionTime,
                status: commandData.status || 'success',
                error: commandData.error,
                metadata: commandData.metadata || {}
            };

            const result = await builtinDatabase.logCommand(commandData);
            if (result) {
                this.stats.totalOperations++;
                if (result.status === 'success') {
                    this.stats.successfulOperations++;
                } else {
                    this.stats.failedOperations++;
                }
                this.stats.lastOperation = new Date();
            }
            return result;
        } catch (error) {
            logger.error('[DB-INTEGRATION] Failed to log command history:', error);
            return null;
        }
    }

    // Update system status
    async updateSystemStatus(statusData) {
        if (!this.enabled) return null;

        try {
            const status = {
                statusId: `status_${Date.now()}`,
                health: statusData.health || {
                    status: 'healthy',
                    activeScripts: 0,
                    recentErrors: 0,
                    stuckScripts: 0
                },
                heartbeat: statusData.heartbeat || {
                    monitoring: true,
                    lastHeartbeat: new Date(),
                    overdueScripts: []
                },
                engines: statusData.engines || {
                    totalModules: 17,
                    loadedModules: 17,
                    moduleStatus: 'all_loaded'
                },
                metadata: statusData.metadata || {}
            };

            await dbUtils.saveSystemStatus(status);
            return status;
        } catch (error) {
            logger.error('[DB-INTEGRATION] Failed to update system status:', error);
            return null;
        }
    }

    // Get system statistics
    async getSystemStats() {
        if (!this.enabled) {
            return {
                enabled: false,
                connectionStatus: this.connectionStatus,
                stats: this.stats
            };
        }

        try {
            const dbStats = builtinDatabase.getStats();
            return {
                enabled: true,
                connectionStatus: this.connectionStatus,
                stats: this.stats,
                databaseStats: dbStats.stats
            };
        } catch (error) {
            logger.error('[DB-INTEGRATION] Failed to get system stats:', error);
            return {
                enabled: false,
                connectionStatus: 'error',
                stats: this.stats,
                error: error.message
            };
        }
    }

    // Get recent operations
    async getRecentOperations(userId = null, limit = 10) {
        if (!this.enabled) return [];

        try {
            return builtinDatabase.getRecentCommands(limit);
        } catch (error) {
            logger.error('[DB-INTEGRATION] Failed to get recent operations:', error);
            return [];
        }
    }

    // Health check
    async healthCheck() {
        if (!this.enabled) {
            return {
                status: 'disabled',
                message: 'Database integration is disabled'
            };
        }

        try {
            const health = builtinDatabase.healthCheck();
            return {
                status: health.status,
                message: health.message,
                stats: this.stats
            };
        } catch (error) {
            return {
                status: 'error',
                message: `Database health check failed: ${error.message}`
            };
        }
    }
}

// Create singleton instance
const databaseIntegration = new DatabaseIntegration();

module.exports = { databaseIntegration };
