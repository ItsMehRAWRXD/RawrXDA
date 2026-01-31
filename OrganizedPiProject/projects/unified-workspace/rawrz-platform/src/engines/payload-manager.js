// RawrZ Payload Manager - Comprehensive payload management system
const EventEmitter = require('events');
const fs = require('fs').promises;
const path = require('path');
const crypto = require('crypto');
const { logger } = require('../utils/logger');

class PayloadManager extends EventEmitter {
    constructor() {
        super();
        this.name = 'Payload Manager';
        this.version = '1.0.0';
        this.payloadsDir = path.join(__dirname, '..', '..', 'data', 'payloads');
        this.uploadsDir = path.join(this.payloadsDir, 'uploads');
        this.databaseFile = path.join(this.payloadsDir, 'payload-database.json');
        this.configFile = path.join(this.payloadsDir, 'payload-config.json');
        this.payloads = new Map();
        this.categories = new Map();
        this.versions = new Map();
        this.initialized = false;

        // Payload categories
        this.categories.set('executable', {
            name: 'Executable',
            description: 'Compiled executable files',
            extensions: ['.exe', '.dll', '.so', '.dylib'],
            maxSize: 1024 * 1024 * 1024 // 1GB - WIDE OPEN
        });

        this.categories.set('script', {
            name: 'Script',
            description: 'Script files',
            extensions: ['.py', '.js', '.ps1', '.bat', '.sh'],
            maxSize: 1024 * 1024 * 1024 // 1GB - WIDE OPEN
        });

        this.categories.set('source', {
            name: 'Source Code',
            description: 'Source code files',
            extensions: ['.cpp', '.c', '.cs', '.java'],
            maxSize: 1024 * 1024 * 1024 // 1GB - WIDE OPEN
        });

        this.categories.set('data', {
            name: 'Data',
            description: 'Data files',
            extensions: ['.txt', '.json', '.xml', '.yaml'],
            maxSize: 1024 * 1024 * 1024 // 1GB - WIDE OPEN
        });
    }

    async initialize() {
        if (this.initialized) {
            return true;
        }

        try {
            // Ensure directories exist
            await fs.mkdir(this.payloadsDir, { recursive: true });
            await fs.mkdir(this.uploadsDir, { recursive: true });

            // Load existing data
            await this.loadDatabase();
            await this.loadPayloads();
            await this.initializePayloadSystem();

            this.initialized = true;
            logger.info('Payload Manager initialized successfully');
            return true;
        } catch (error) {
            logger.error('Failed to initialize Payload Manager:', error);
            throw error;
        }
    }

    async loadDatabase() {
        try {
            if (await this.fileExists(this.databaseFile)) {
                const data = await fs.readFile(this.databaseFile, 'utf8');
                const database = JSON.parse(data);
                
                // Restore payloads from database
                for (const payload of database.payloads || []) {
                    this.payloads.set(payload.id, payload);
                }
                
                logger.info(`Loaded ${database.payloads?.length || 0} payloads from database`);
            }
        } catch (error) {
            logger.error('Failed to load payload database:', error);
        }
    }

    async loadPayloads() {
        try {
            const files = await fs.readdir(this.payloadsDir);
            for (const file of files) {
                if (file.endsWith('.json') && file !== 'payload-database.json' && file !== 'payload-config.json') {
                    const filePath = path.join(this.payloadsDir, file);
                    const data = await fs.readFile(filePath, 'utf8');
                    const payload = JSON.parse(data);
                    this.payloads.set(payload.id, payload);
                }
            }
            logger.info(`Loaded ${this.payloads.size} payloads from files`);
        } catch (error) {
            logger.error('Failed to load payloads:', error);
        }
    }

    async initializePayloadSystem() {
        // Initialize payload management systems
        this.payloadSystem = {
            create: this.createPayload.bind(this),
            update: this.updatePayload.bind(this),
            delete: this.deletePayload.bind(this),
            get: this.getPayload.bind(this),
            list: this.listPayloads.bind(this)
        };
    }

    // Payload Management Methods
    async createPayload(payloadData) {
        try {
            const payload = {
                id: crypto.randomUUID(),
                name: payloadData.name || 'Unnamed Payload',
                description: payloadData.description || '',
                category: payloadData.category || 'data',
                type: payloadData.type || 'file',
                size: payloadData.size || 0,
                hash: payloadData.hash || '',
                path: payloadData.path || '',
                metadata: payloadData.metadata || {},
                tags: payloadData.tags || [],
                created: new Date().toISOString(),
                modified: new Date().toISOString(),
                version: '1.0.0',
                status: 'active'
            };

            this.payloads.set(payload.id, payload);
            await this.savePayload(payload);
            
            this.emit('payloadCreated', payload);
            logger.info(`Payload created: ${payload.name} (${payload.id})`);
            return payload;
        } catch (error) {
            logger.error('Failed to create payload:', error);
            throw error;
        }
    }

    async updatePayload(payloadId, updates) {
        try {
            if (!this.payloads.has(payloadId)) {
                throw new Error(`Payload ${payloadId} not found`);
            }

            const payload = this.payloads.get(payloadId);
            const updatedPayload = {
                ...payload,
                ...updates,
                modified: new Date().toISOString()
            };

            this.payloads.set(payloadId, updatedPayload);
            await this.savePayload(updatedPayload);
            
            this.emit('payloadUpdated', updatedPayload);
            logger.info(`Payload updated: ${updatedPayload.name} (${payloadId})`);
            return updatedPayload;
        } catch (error) {
            logger.error('Failed to update payload:', error);
            throw error;
        }
    }

    async deletePayload(payloadId) {
        try {
            if (!this.payloads.has(payloadId)) {
                throw new Error(`Payload ${payloadId} not found`);
            }

            const payload = this.payloads.get(payloadId);
            this.payloads.delete(payloadId);
            
            // Delete payload file if it exists
            if (payload.path && await this.fileExists(payload.path)) {
                await fs.unlink(payload.path);
            }

            // Delete payload metadata file
            const metadataFile = path.join(this.payloadsDir, `${payloadId}.json`);
            if (await this.fileExists(metadataFile)) {
                await fs.unlink(metadataFile);
            }
            
            this.emit('payloadDeleted', payload);
            logger.info(`Payload deleted: ${payload.name} (${payloadId})`);
            return true;
        } catch (error) {
            logger.error('Failed to delete payload:', error);
            throw error;
        }
    }

    getPayload(payloadId) {
        return this.payloads.get(payloadId);
    }

    listPayloads(filter = {}) {
        let payloads = Array.from(this.payloads.values());

        // Apply filters
        if (filter.category) {
            payloads = payloads.filter(p => p.category === filter.category);
        }
        if (filter.status) {
            payloads = payloads.filter(p => p.status === filter.status);
        }
        if (filter.tags && filter.tags.length > 0) {
            payloads = payloads.filter(p => 
                filter.tags.some(tag => p.tags.includes(tag))
            );
        }

        return payloads;
    }

    // File Management
    async savePayload(payload) {
        try {
            const metadataFile = path.join(this.payloadsDir, `${payload.id}.json`);
            await fs.writeFile(metadataFile, JSON.stringify(payload, null, 2));
            
            // Update database
            await this.saveDatabase();
        } catch (error) {
            logger.error('Failed to save payload:', error);
            throw error;
        }
    }

    async saveDatabase() {
        try {
            const database = {
                version: '1.0.0',
                lastUpdated: new Date().toISOString(),
                payloads: Array.from(this.payloads.values())
            };
            
            await fs.writeFile(this.databaseFile, JSON.stringify(database, null, 2));
        } catch (error) {
            logger.error('Failed to save database:', error);
            throw error;
        }
    }

    async fileExists(filePath) {
        try {
            await fs.access(filePath);
            return true;
        } catch {
            return false;
        }
    }

    // Payload Operations
    async uploadPayload(file, metadata = {}) {
        try {
            const payloadId = crypto.randomUUID();
            const fileName = `${payloadId}_${file.originalname}`;
            const filePath = path.join(this.uploadsDir, fileName);
            
            // Save file
            await fs.writeFile(filePath, file.buffer);
            
            // Calculate hash
            const hash = crypto.createHash('sha256').update(file.buffer).digest('hex');
            
            // Determine category
            const extension = path.extname(file.originalname).toLowerCase();
            let category = 'data';
            for (const [catName, catInfo] of this.categories) {
                if (catInfo.extensions.includes(extension)) {
                    category = catName;
                    break;
                }
            }

            // Create payload record
            const payload = await this.createPayload({
                name: file.originalname,
                description: metadata.description || '',
                category: category,
                type: 'file',
                size: file.size,
                hash: hash,
                path: filePath,
                metadata: {
                    originalName: file.originalname,
                    mimeType: file.mimetype,
                    ...metadata
                }
            });

            return payload;
        } catch (error) {
            logger.error('Failed to upload payload:', error);
            throw error;
        }
    }

    async downloadPayload(payloadId) {
        try {
            const payload = this.getPayload(payloadId);
            if (!payload) {
                throw new Error(`Payload ${payloadId} not found`);
            }

            if (!await this.fileExists(payload.path)) {
                throw new Error(`Payload file not found: ${payload.path}`);
            }

            const fileBuffer = await fs.readFile(payload.path);
            return {
                payload: payload,
                data: fileBuffer
            };
        } catch (error) {
            logger.error('Failed to download payload:', error);
            throw error;
        }
    }

    // Payload Analysis
    async analyzePayload(payloadId) {
        try {
            const payload = this.getPayload(payloadId);
            if (!payload) {
                throw new Error(`Payload ${payloadId} not found`);
            }

            const analysis = {
                id: crypto.randomUUID(),
                payloadId: payloadId,
                timestamp: new Date().toISOString(),
                results: {
                    fileType: payload.metadata.mimeType || 'unknown',
                    size: payload.size,
                    hash: payload.hash,
                    category: payload.category,
                    riskLevel: this.assessRiskLevel(payload),
                    features: this.detectFeatures(payload)
                }
            };

            return analysis;
        } catch (error) {
            logger.error('Failed to analyze payload:', error);
            throw error;
        }
    }

    assessRiskLevel(payload) {
        // Simple risk assessment based on category and size
        if (payload.category === 'executable') {
            return payload.size > 100 * 1024 * 1024 ? 'high' : 'medium';
        } else if (payload.category === 'script') {
            return 'medium';
        } else {
            return 'low';
        }
    }

    detectFeatures(payload) {
        // Simple feature detection
        const features = [];
        
        if (payload.category === 'executable') {
            features.push('executable');
        }
        if (payload.category === 'script') {
            features.push('script');
        }
        if (payload.size > 1024 * 1024) {
            features.push('large_file');
        }
        
        return features;
    }

    // Statistics and Monitoring
    getStats() {
        const stats = {
            total: this.payloads.size,
            byCategory: {},
            byStatus: {},
            totalSize: 0
        };

        for (const payload of this.payloads.values()) {
            // Count by category
            stats.byCategory[payload.category] = (stats.byCategory[payload.category] || 0) + 1;
            
            // Count by status
            stats.byStatus[payload.status] = (stats.byStatus[payload.status] || 0) + 1;
            
            // Total size
            stats.totalSize += payload.size || 0;
        }

        return stats;
    }

    // Status and Configuration Methods
    getStatus() {
        return {
            name: this.name,
            version: this.version,
            initialized: this.initialized,
            payloads: this.payloads.size,
            categories: this.categories.size,
            directories: {
                payloads: this.payloadsDir,
                uploads: this.uploadsDir
            }
        };
    }

    // Panel Integration Methods
    async getPanelConfig() {
        return {
            name: this.name,
            version: this.version,
            description: 'Payload Manager for comprehensive payload management',
            endpoints: this.getAvailableEndpoints(),
            settings: this.getSettings(),
            status: this.getStatus()
        };
    }

    getAvailableEndpoints() {
        return [
            { method: 'GET', path: '/api/' + this.name + '/status', description: 'Get engine status' },
            { method: 'POST', path: '/api/' + this.name + '/create', description: 'Create payload' },
            { method: 'GET', path: '/api/' + this.name + '/list', description: 'List payloads' },
            { method: 'POST', path: '/api/' + this.name + '/upload', description: 'Upload payload' }
        ];
    }

    getSettings() {
        return {
            enabled: true,
            autoStart: false,
            config: {
                maxFileSize: 50 * 1024 * 1024,
                allowedCategories: Array.from(this.categories.keys())
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
                    return this.getStatus();
                }
            },
            {
                command: this.name + ' list',
                description: 'List all payloads',
                action: async () => {
                    return this.listPayloads();
                }
            },
            {
                command: this.name + ' stats',
                description: 'Get statistics',
                action: async () => {
                    return this.getStats();
                }
            },
            {
                command: this.name + ' config',
                description: 'Get engine configuration',
                action: async () => {
                    return this.getSettings();
                }
            }
        ];
    }
}

module.exports = new PayloadManager();
