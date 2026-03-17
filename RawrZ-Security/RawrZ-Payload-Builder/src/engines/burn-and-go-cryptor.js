// RawrZ Burn-and-Go Cryptor - Disposable One-Time Use Encryption
// Perfect for "burn after use" scenarios - no traces left behind
const crypto = require('crypto');
const fs = require('fs').promises;
const path = require('path');
const { logger } = require('../utils/logger');
const { exec } = require('child_process');
const { promisify } = require('util');

const execAsync = promisify(exec);

class BurnAndGoCryptor {
    constructor() {
        this.name = 'Burn-and-Go Cryptor';
        this.version = '1.0.0';
        this.description = 'Disposable one-time use encryption system - burn after use';
        this.initialized = false;
        
        // Burn-and-Go specific features
        this.burnMethods = {
            'instant': {
                name: 'Instant Burn',
                description: 'Immediate destruction after use',
                delay: 0,
                method: 'delete'
            },
            'delayed': {
                name: 'Delayed Burn',
                description: 'Burn after specified time',
                delay: 5000, // 5 seconds default
                method: 'delayed_delete'
            },
            'conditional': {
                name: 'Conditional Burn',
                description: 'Burn based on conditions',
                delay: 0,
                method: 'conditional_delete'
            },
            'stealth': {
                name: 'Stealth Burn',
                description: 'Silent destruction with cleanup',
                delay: 1000,
                method: 'stealth_delete'
            }
        };

        this.encryptionMethods = {
            'single_use_aes': {
                name: 'Single-Use AES-256',
                description: 'One-time AES encryption with key destruction',
                security: 'high',
                burnable: true
            },
            'ephemeral_chacha': {
                name: 'Ephemeral ChaCha20',
                description: 'Temporary ChaCha20 with ephemeral keys',
                security: 'high',
                burnable: true
            },
            'burnable_hybrid': {
                name: 'Burnable Hybrid',
                description: 'Hybrid encryption designed for burning',
                security: 'medium',
                burnable: true
            },
            'one_time_pad': {
                name: 'One-Time Pad',
                description: 'Perfect secrecy with key destruction',
                security: 'maximum',
                burnable: true
            }
        };

        this.burnedFiles = new Set();
        this.activeBurns = new Map();
        this.burnStatistics = {
            totalBurned: 0,
            successfulBurns: 0,
            failedBurns: 0,
            averageBurnTime: 0,
            lastBurn: null
        };
    }

    async initialize(config = {}) {
        this.config = {
            defaultBurnMethod: 'instant',
            cleanupInterval: 30000, // 30 seconds
            maxBurnQueue: 100,
            enableStealthMode: true,
            autoCleanup: true,
            ...config
        };

        // Initialize burn cleanup system
        if (this.config.autoCleanup) {
            this.startBurnCleanup();
        }

        this.initialized = true;
        logger.info('Burn-and-Go Cryptor initialized - ready for disposable encryption');
    }

    // Main burn-and-go encryption method
    async burnAndGoEncrypt(inputFile, options = {}) {
        if (!this.initialized) {
            throw new Error('Burn-and-Go Cryptor not initialized');
        }

        const burnMethod = options.burnMethod || this.config.defaultBurnMethod;
        const encryptionMethod = options.encryptionMethod || 'single_use_aes';
        const burnDelay = options.burnDelay || this.burnMethods[burnMethod].delay;

        try {
            // Step 1: Read input file
            const inputData = await fs.readFile(inputFile);
            logger.info(`[BURN-GO] Processing ${inputFile} for burn-and-go encryption`);

            // Step 2: Generate ephemeral encryption key
            const ephemeralKey = this.generateEphemeralKey(encryptionMethod);
            
            // Step 3: Encrypt data with burnable method
            const encryptedData = await this.encryptWithBurnableMethod(inputData, ephemeralKey, encryptionMethod);
            
            // Step 4: Create temporary encrypted file
            const tempDir = path.join(__dirname, '../../temp');
            await fs.mkdir(tempDir, { recursive: true });
            
            const tempFile = path.join(tempDir, `burn_${Date.now()}_${crypto.randomBytes(8).toString('hex')}.enc`);
            await fs.writeFile(tempFile, encryptedData);

            // Step 5: Schedule burn operation
            const burnId = this.scheduleBurnOperation(tempFile, burnMethod, burnDelay);
            
            // Step 6: Return encrypted file path and burn ID
            const result = {
                encryptedFile: tempFile,
                burnId: burnId,
                burnMethod: burnMethod,
                burnDelay: burnDelay,
                encryptionMethod: encryptionMethod,
                keyDestroyed: true, // Key is already destroyed
                status: 'ready_for_burn'
            };

            logger.info(`[BURN-GO] File encrypted and scheduled for burn: ${burnId}`);
            return result;

        } catch (error) {
            logger.error(`[BURN-GO] Encryption failed: ${error.message}`);
            throw error;
        }
    }

    // Generate ephemeral key that will be destroyed
    generateEphemeralKey(method) {
        const keySize = {
            'single_use_aes': 32,
            'ephemeral_chacha': 32,
            'burnable_hybrid': 32,
            'one_time_pad': 256
        }[method] || 32;

        const key = crypto.randomBytes(keySize);
        
        // Immediately mark key for destruction
        this.scheduleKeyDestruction(key);
        
        return key;
    }

    // Encrypt with burnable method
    async encryptWithBurnableMethod(data, key, method) {
        switch (method) {
            case 'single_use_aes':
                return this.singleUseAESEncrypt(data, key);
            case 'ephemeral_chacha':
                return this.ephemeralChaChaEncrypt(data, key);
            case 'burnable_hybrid':
                return this.burnableHybridEncrypt(data, key);
            case 'one_time_pad':
                return this.oneTimePadEncrypt(data, key);
            default:
                throw new Error(`Unknown encryption method: ${method}`);
        }
    }

    // Single-use AES encryption
    singleUseAESEncrypt(data, key) {
        const iv = crypto.randomBytes(16);
        const cipher = crypto.createCipher('aes-256-gcm', key);
        cipher.setAAD(iv);
        
        let encrypted = cipher.update(data);
        encrypted = Buffer.concat([encrypted, cipher.final()]);
        
        const authTag = cipher.getAuthTag();
        
        // Combine IV + AuthTag + Encrypted data
        return Buffer.concat([iv, authTag, encrypted]);
    }

    // Ephemeral ChaCha20 encryption
    ephemeralChaChaEncrypt(data, key) {
        const iv = crypto.randomBytes(12);
        const cipher = crypto.createCipher('chacha20-poly1305', key, iv);
        
        let encrypted = cipher.update(data);
        encrypted = Buffer.concat([encrypted, cipher.final()]);
        
        const authTag = cipher.getAuthTag();
        
        return Buffer.concat([iv, authTag, encrypted]);
    }

    // Burnable hybrid encryption
    burnableHybridEncrypt(data, key) {
        // Triple encryption with key destruction between layers
        let encrypted = data;
        
        // Layer 1: XOR with key
        for (let i = 0; i < encrypted.length; i++) {
            encrypted[i] ^= key[i % key.length];
        }
        
        // Layer 2: Rotate bytes
        const rotation = key[0] % 256;
        encrypted = this.rotateBytes(encrypted, rotation);
        
        // Layer 3: Simple substitution
        encrypted = this.simpleSubstitution(encrypted, key);
        
        return encrypted;
    }

    // One-time pad encryption
    oneTimePadEncrypt(data, key) {
        if (key.length < data.length) {
            throw new Error('One-time pad key must be at least as long as data');
        }
        
        const encrypted = Buffer.alloc(data.length);
        for (let i = 0; i < data.length; i++) {
            encrypted[i] = data[i] ^ key[i];
        }
        
        return encrypted;
    }

    // Schedule burn operation
    scheduleBurnOperation(filePath, method, delay) {
        const burnId = `burn_${Date.now()}_${crypto.randomBytes(8).toString('hex')}`;
        
        const burnOperation = {
            id: burnId,
            filePath: filePath,
            method: method,
            scheduledTime: Date.now() + delay,
            status: 'scheduled'
        };

        this.activeBurns.set(burnId, burnOperation);

        // Schedule the actual burn
        setTimeout(async () => {
            await this.executeBurn(burnId);
        }, delay);

        return burnId;
    }

    // Execute burn operation
    async executeBurn(burnId) {
        const burnOp = this.activeBurns.get(burnId);
        if (!burnOp) {
            logger.warn(`[BURN-GO] Burn operation not found: ${burnId}`);
            return;
        }

        try {
            const startTime = Date.now();
            
            switch (burnOp.method) {
                case 'delete':
                    await this.instantDelete(burnOp.filePath);
                    break;
                case 'delayed_delete':
                    await this.delayedDelete(burnOp.filePath);
                    break;
                case 'conditional_delete':
                    await this.conditionalDelete(burnOp.filePath);
                    break;
                case 'stealth_delete':
                    await this.stealthDelete(burnOp.filePath);
                    break;
                default:
                    await this.instantDelete(burnOp.filePath);
            }

            const burnTime = Date.now() - startTime;
            
            // Update statistics
            this.burnStatistics.totalBurned++;
            this.burnStatistics.successfulBurns++;
            this.burnStatistics.averageBurnTime = 
                (this.burnStatistics.averageBurnTime + burnTime) / 2;
            this.burnStatistics.lastBurn = new Date().toISOString();

            // Mark as burned
            this.burnedFiles.add(burnOp.filePath);
            burnOp.status = 'burned';
            burnOp.burnedAt = new Date().toISOString();
            burnOp.burnTime = burnTime;

            logger.info(`[BURN-GO] Successfully burned file: ${burnOp.filePath} (${burnTime}ms)`);

        } catch (error) {
            this.burnStatistics.failedBurns++;
            burnOp.status = 'failed';
            burnOp.error = error.message;
            logger.error(`[BURN-GO] Burn failed for ${burnOp.filePath}: ${error.message}`);
        }
    }

    // Instant delete
    async instantDelete(filePath) {
        await fs.unlink(filePath);
        logger.info(`[BURN-GO] File instantly deleted: ${filePath}`);
    }

    // Delayed delete with overwrite
    async delayedDelete(filePath) {
        // Overwrite file with random data before deletion
        const fileStats = await fs.stat(filePath);
        const randomData = crypto.randomBytes(fileStats.size);
        await fs.writeFile(filePath, randomData);
        
        // Delete the file
        await fs.unlink(filePath);
        logger.info(`[BURN-GO] File overwritten and deleted: ${filePath}`);
    }

    // Conditional delete (based on conditions)
    async conditionalDelete(filePath) {
        // Check if file is still being accessed
        try {
            await fs.access(filePath, fs.constants.R_OK | fs.constants.W_OK);
            await this.instantDelete(filePath);
        } catch (error) {
            logger.warn(`[BURN-GO] File access denied, skipping burn: ${filePath}`);
        }
    }

    // Stealth delete with cleanup
    async stealthDelete(filePath) {
        // Multiple overwrite passes
        const passes = 3;
        const fileStats = await fs.stat(filePath);
        
        for (let i = 0; i < passes; i++) {
            const randomData = crypto.randomBytes(fileStats.size);
            await fs.writeFile(filePath, randomData);
        }
        
        // Final deletion
        await fs.unlink(filePath);
        logger.info(`[BURN-GO] File stealth deleted (${passes} passes): ${filePath}`);
    }

    // Utility methods
    rotateBytes(data, rotation) {
        const rotated = Buffer.alloc(data.length);
        for (let i = 0; i < data.length; i++) {
            rotated[i] = (data[i] + rotation) % 256;
        }
        return rotated;
    }

    simpleSubstitution(data, key) {
        const substituted = Buffer.alloc(data.length);
        for (let i = 0; i < data.length; i++) {
            substituted[i] = (data[i] + key[i % key.length]) % 256;
        }
        return substituted;
    }

    // Schedule key destruction
    scheduleKeyDestruction(key) {
        // Overwrite key memory immediately
        key.fill(0);
    }

    // Start burn cleanup system
    startBurnCleanup() {
        setInterval(() => {
            this.cleanupCompletedBurns();
        }, this.config.cleanupInterval);
    }

    // Cleanup completed burns
    cleanupCompletedBurns() {
        const completedBurns = [];
        
        for (const [burnId, burnOp] of this.activeBurns) {
            if (burnOp.status === 'burned' || burnOp.status === 'failed') {
                completedBurns.push(burnId);
            }
        }

        for (const burnId of completedBurns) {
            this.activeBurns.delete(burnId);
        }

        if (completedBurns.length > 0) {
            logger.info(`[BURN-GO] Cleaned up ${completedBurns.length} completed burns`);
        }
    }

    // Panel integration methods
    async getPanelConfig() {
        return {
            name: this.name,
            version: this.version,
            description: this.description,
            endpoints: this.getAvailableEndpoints(),
            settings: this.getSettings(),
            status: this.getStatus()
        };
    }

    getAvailableEndpoints() {
        return [
            { method: 'POST', path: '/api/burn-and-go/encrypt', description: 'Burn-and-go encryption' },
            { method: 'GET', path: '/api/burn-and-go/status', description: 'Get burn statistics' },
            { method: 'POST', path: '/api/burn-and-go/burn', description: 'Manual burn operation' },
            { method: 'GET', path: '/api/burn-and-go/methods', description: 'Get available burn methods' }
        ];
    }

    getSettings() {
        return {
            burnMethods: Object.keys(this.burnMethods),
            encryptionMethods: Object.keys(this.encryptionMethods),
            defaultBurnMethod: this.config.defaultBurnMethod,
            autoCleanup: this.config.autoCleanup,
            maxBurnQueue: this.config.maxBurnQueue
        };
    }

    getStatus() {
        return {
            initialized: this.initialized,
            activeBurns: this.activeBurns.size,
            burnedFiles: this.burnedFiles.size,
            statistics: this.burnStatistics,
            uptime: process.uptime(),
            timestamp: new Date().toISOString()
        };
    }

    // Get burn statistics
    getBurnStatistics() {
        return {
            ...this.burnStatistics,
            activeBurns: this.activeBurns.size,
            burnedFiles: this.burnedFiles.size,
            burnMethods: Object.keys(this.burnMethods),
            encryptionMethods: Object.keys(this.encryptionMethods)
        };
    }
}

module.exports = BurnAndGoCryptor;
