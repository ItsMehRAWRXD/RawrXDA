// RawrZ File Operations Engine - Comprehensive file system operations
const EventEmitter = require('events');
const fs = require('fs').promises;
const path = require('path');
const crypto = require('crypto');
const { logger } = require('../utils/logger');

class FileOperations extends EventEmitter {
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
    };

    constructor() {
        super();
        this.name = 'FileOperations';
        this.version = '1.0.0';
        this.initialized = false;
        this.operations = new Map();
        this.fileCache = new Map();
        this.operationHistory = [];
        this.maxCacheSize = 1000;
        this.maxHistorySize = 10000;
    }

    async initialize(config = {}) {
        if (this.initialized) {
            return true;
        }

        try {
            logger.info('Initializing File Operations Engine...');
            
            this.maxCacheSize = config.maxCacheSize || this.maxCacheSize;
            this.maxHistorySize = config.maxHistorySize || this.maxHistorySize;
            
            // Initialize file operation types
            this.initializeOperationTypes();
            
            this.initialized = true;
            logger.info('File Operations Engine initialized successfully');
            this.emit('initialized');
            
            return true;
        } catch (error) {
            logger.error('Failed to initialize File Operations Engine:', error);
            throw error;
        }
    }

    initializeOperationTypes() {
        this.operationTypes = {
            'read': {
                name: 'Read File',
                description: 'Read file contents',
                requiresTarget: true,
                returnsData: true
            },
            'write': {
                name: 'Write File',
                description: 'Write data to file',
                requiresTarget: true,
                requiresData: true
            },
            'copy': {
                name: 'Copy File',
                description: 'Copy file to new location',
                requiresTarget: true,
                requiresDestination: true
            },
            'move': {
                name: 'Move File',
                description: 'Move file to new location',
                requiresTarget: true,
                requiresDestination: true
            },
            'delete': {
                name: 'Delete File',
                description: 'Delete file from filesystem',
                requiresTarget: true
            },
            'exists': {
                name: 'Check File Exists',
                description: 'Check if file exists',
                requiresTarget: true,
                returnsData: true
            },
            'stat': {
                name: 'Get File Stats',
                description: 'Get file statistics',
                requiresTarget: true,
                returnsData: true
            },
            'list': {
                name: 'List Directory',
                description: 'List directory contents',
                requiresTarget: true,
                returnsData: true
            },
            'create': {
                name: 'Create Directory',
                description: 'Create directory',
                requiresTarget: true
            },
            'backup': {
                name: 'Backup File',
                description: 'Create backup of file',
                requiresTarget: true,
                returnsData: true
            }
        };
    }

    // Read file contents
    async readFile(filePath, options = {}) {
        const operationId = crypto.randomUUID();
        const startTime = Date.now();
        
        try {
            const { encoding = 'utf8', cache = true } = options;
            
            // Check cache first
            if (cache && this.fileCache.has(filePath)) {
                const cached = this.fileCache.get(filePath);
                if (Date.now() - cached.timestamp < 300000) { // 5 minutes
                    return {
                        success: true,
                        data: cached.data,
                        fromCache: true,
                        operationId,
                        duration: Date.now() - startTime
                    };
                }
            }
            
            const data = await fs.readFile(filePath, encoding);
            
            // Cache the result
            if (cache) {
                this.cacheFile(filePath, data);
            }
            
            const result = {
                success: true,
                data,
                fromCache: false,
                operationId,
                duration: Date.now() - startTime
            };
            
            this.recordOperation('read', filePath, result);
            return result;
            
        } catch (error) {
            const result = {
                success: false,
                error: error.message,
                operationId,
                duration: Date.now() - startTime
            };
            
            this.recordOperation('read', filePath, result);
            throw error;
        }
    }

    // Write file contents
    async writeFile(filePath, data, options = {}) {
        const operationId = crypto.randomUUID();
        const startTime = Date.now();
        
        try {
            const { encoding = 'utf8', createDir = true, backup = false } = options;
            
            // Create directory if needed
            if (createDir) {
                const dir = path.dirname(filePath);
                await fs.mkdir(dir, { recursive: true });
            }
            
            // Create backup if requested
            if (backup) {
                try {
                    await fs.access(filePath);
                    const backupPath = `${filePath}.backup.${Date.now()}`;
                    await fs.copyFile(filePath, backupPath);
                } catch (error) {
                    // File doesn't exist, no backup needed
                }
            }
            
            await fs.writeFile(filePath, data, encoding);
            
            // Update cache
            this.cacheFile(filePath, data);
            
            const result = {
                success: true,
                operationId,
                duration: Date.now() - startTime
            };
            
            this.recordOperation('write', filePath, result);
            return result;
            
        } catch (error) {
            const result = {
                success: false,
                error: error.message,
                operationId,
                duration: Date.now() - startTime
            };
            
            this.recordOperation('write', filePath, result);
            throw error;
        }
    }

    // Copy file
    async copyFile(sourcePath, destPath, options = {}) {
        const operationId = crypto.randomUUID();
        const startTime = Date.now();
        
        try {
            const { createDir = true, overwrite = false } = options;
            
            // Check if destination exists
            if (!overwrite) {
                try {
                    await fs.access(destPath);
                    throw new Error('Destination file already exists');
                } catch (error) {
                    if (error.code !== 'ENOENT') throw error;
                }
            }
            
            // Create destination directory if needed
            if (createDir) {
                const dir = path.dirname(destPath);
                await fs.mkdir(dir, { recursive: true });
            }
            
            await fs.copyFile(sourcePath, destPath);
            
            const result = {
                success: true,
                operationId,
                duration: Date.now() - startTime
            };
            
            this.recordOperation('copy', sourcePath, result, { destPath });
            return result;
            
        } catch (error) {
            const result = {
                success: false,
                error: error.message,
                operationId,
                duration: Date.now() - startTime
            };
            
            this.recordOperation('copy', sourcePath, result, { destPath });
            throw error;
        }
    }

    // Move file
    async moveFile(sourcePath, destPath, options = {}) {
        const operationId = crypto.randomUUID();
        const startTime = Date.now();
        
        try {
            const { createDir = true, overwrite = false } = options;
            
            // Check if destination exists
            if (!overwrite) {
                try {
                    await fs.access(destPath);
                    throw new Error('Destination file already exists');
                } catch (error) {
                    if (error.code !== 'ENOENT') throw error;
                }
            }
            
            // Create destination directory if needed
            if (createDir) {
                const dir = path.dirname(destPath);
                await fs.mkdir(dir, { recursive: true });
            }
            
            await fs.rename(sourcePath, destPath);
            
            // Remove from cache
            this.fileCache.delete(sourcePath);
            
            const result = {
                success: true,
                operationId,
                duration: Date.now() - startTime
            };
            
            this.recordOperation('move', sourcePath, result, { destPath });
            return result;
            
        } catch (error) {
            const result = {
                success: false,
                error: error.message,
                operationId,
                duration: Date.now() - startTime
            };
            
            this.recordOperation('move', sourcePath, result, { destPath });
            throw error;
        }
    }

    // Delete file
    async deleteFile(filePath, options = {}) {
        const operationId = crypto.randomUUID();
        const startTime = Date.now();
        
        try {
            const { backup = false } = options;
            
            // Create backup if requested
            if (backup) {
                try {
                    await fs.access(filePath);
                    const backupPath = `${filePath}.deleted.${Date.now()}`;
                    await fs.copyFile(filePath, backupPath);
                } catch (error) {
                    // File doesn't exist, no backup needed
                }
            }
            
            await fs.unlink(filePath);
            
            // Remove from cache
            this.fileCache.delete(filePath);
            
            const result = {
                success: true,
                operationId,
                duration: Date.now() - startTime
            };
            
            this.recordOperation('delete', filePath, result);
            return result;
            
        } catch (error) {
            const result = {
                success: false,
                error: error.message,
                operationId,
                duration: Date.now() - startTime
            };
            
            this.recordOperation('delete', filePath, result);
            throw error;
        }
    }

    // Check if file exists
    async fileExists(filePath) {
        const operationId = crypto.randomUUID();
        const startTime = Date.now();
        
        try {
            await fs.access(filePath);
            
            const result = {
                success: true,
                exists: true,
                operationId,
                duration: Date.now() - startTime
            };
            
            this.recordOperation('exists', filePath, result);
            return result;
            
        } catch (error) {
            const result = {
                success: true,
                exists: false,
                operationId,
                duration: Date.now() - startTime
            };
            
            this.recordOperation('exists', filePath, result);
            return result;
        }
    }

    // Get file statistics
    async getFileStats(filePath) {
        const operationId = crypto.randomUUID();
        const startTime = Date.now();
        
        try {
            const stats = await fs.stat(filePath);
            
            const result = {
                success: true,
                stats: {
                    size: stats.size,
                    isFile: stats.isFile(),
                    isDirectory: stats.isDirectory(),
                    createdAt: stats.birthtime,
                    modifiedAt: stats.mtime,
                    accessedAt: stats.atime,
                    mode: stats.mode
                },
                operationId,
                duration: Date.now() - startTime
            };
            
            this.recordOperation('stat', filePath, result);
            return result;
            
        } catch (error) {
            const result = {
                success: false,
                error: error.message,
                operationId,
                duration: Date.now() - startTime
            };
            
            this.recordOperation('stat', filePath, result);
            throw error;
        }
    }

    // List directory contents
    async listDirectory(dirPath, options = {}) {
        const operationId = crypto.randomUUID();
        const startTime = Date.now();
        
        try {
            const { recursive = false, includeStats = false } = options;
            
            const items = await fs.readdir(dirPath);
            const result = {
                success: true,
                items: [],
                operationId,
                duration: Date.now() - startTime
            };
            
            for (const item of items) {
                const itemPath = path.join(dirPath, item);
                const itemInfo = {
                    name: item,
                    path: itemPath
                };
                
                if (includeStats) {
                    try {
                        const stats = await fs.stat(itemPath);
                        itemInfo.stats = {
                            size: stats.size,
                            isFile: stats.isFile(),
                            isDirectory: stats.isDirectory(),
                            createdAt: stats.birthtime,
                            modifiedAt: stats.mtime
                        };
                    } catch (error) {
                        itemInfo.stats = null;
                    }
                }
                
                result.items.push(itemInfo);
            }
            
            this.recordOperation('list', dirPath, result);
            return result;
            
        } catch (error) {
            const result = {
                success: false,
                error: error.message,
                operationId,
                duration: Date.now() - startTime
            };
            
            this.recordOperation('list', dirPath, result);
            throw error;
        }
    }

    // Create directory
    async createDirectory(dirPath, options = {}) {
        const operationId = crypto.randomUUID();
        const startTime = Date.now();
        
        try {
            const { recursive = true, mode = 0o755 } = options;
            
            await fs.mkdir(dirPath, { recursive, mode });
            
            const result = {
                success: true,
                operationId,
                duration: Date.now() - startTime
            };
            
            this.recordOperation('create', dirPath, result);
            return result;
            
        } catch (error) {
            const result = {
                success: false,
                error: error.message,
                operationId,
                duration: Date.now() - startTime
            };
            
            this.recordOperation('create', dirPath, result);
            throw error;
        }
    }

    // Backup file
    async backupFile(filePath, options = {}) {
        const operationId = crypto.randomUUID();
        const startTime = Date.now();
        
        try {
            const { backupDir = null, suffix = 'backup' } = options;
            
            let backupPath;
            if (backupDir) {
                const fileName = path.basename(filePath);
                const timestamp = Date.now();
                backupPath = path.join(backupDir, `${fileName}.${suffix}.${timestamp}`);
                await fs.mkdir(backupDir, { recursive: true });
            } else {
                backupPath = `${filePath}.${suffix}.${Date.now()}`;
            }
            
            await fs.copyFile(filePath, backupPath);
            
            const result = {
                success: true,
                backupPath,
                operationId,
                duration: Date.now() - startTime
            };
            
            this.recordOperation('backup', filePath, result, { backupPath });
            return result;
            
        } catch (error) {
            const result = {
                success: false,
                error: error.message,
                operationId,
                duration: Date.now() - startTime
            };
            
            this.recordOperation('backup', filePath, result);
            throw error;
        }
    }

    // Cache management
    cacheFile(filePath, data) {
        if (this.fileCache.size >= this.maxCacheSize) {
            // Remove oldest entry
            const firstKey = this.fileCache.keys().next().value;
            this.fileCache.delete(firstKey);
        }
        
        this.fileCache.set(filePath, {
            data,
            timestamp: Date.now()
        });
    }

    // Operation recording
    recordOperation(type, target, result, metadata = {}) {
        const operation = {
            id: result.operationId,
            type,
            target,
            success: result.success,
            duration: result.duration,
            timestamp: Date.now(),
            metadata
        };
        
        this.operationHistory.push(operation);
        
        // Limit history size
        if (this.operationHistory.length > this.maxHistorySize) {
            this.operationHistory.shift();
        }
        
        this.emit('operation', operation);
    }

    // Get operation history
    getOperationHistory(limit = 100) {
        return this.operationHistory.slice(-limit);
    }

    // Clear cache
    clearCache() {
        this.fileCache.clear();
    }

    // Get cache statistics
    getCacheStats() {
        return {
            size: this.fileCache.size,
            maxSize: this.maxCacheSize,
            entries: Array.from(this.fileCache.keys())
        };
    }

    async getStatus() {
        return {
            name: this.name,
            version: this.version,
            status: this.initialized ? 'active' : 'inactive',
            initialized: this.initialized,
            operationTypes: Object.keys(this.operationTypes).length,
            cacheSize: this.fileCache.size,
            historySize: this.operationHistory.length,
            maxCacheSize: this.maxCacheSize,
            maxHistorySize: this.maxHistorySize
        };
    }
}

module.exports = FileOperations;
