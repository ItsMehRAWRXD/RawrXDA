// OpenSSL Manager - Handles OpenSSL operations and integration
const crypto = require('crypto');
const { logger } = require('./logger');
const opensslConfig = require('./openssl-config');

class OpenSSLManager {
    constructor() {
        this.initialized = false;
        this.supportedAlgorithms = opensslConfig.getConfig().supportedAlgorithms;
    }

    async initialize() {
        if (this.initialized) {
            return;
        }

        try {
            logger.info('Initializing OpenSSL Manager...');
            
            // Test OpenSSL availability
            await this.testOpenSSL();
            
            this.initialized = true;
            logger.info('OpenSSL Manager initialized successfully');
        } catch (error) {
            logger.error('Failed to initialize OpenSSL Manager:', error.message);
            throw error;
        }
    }

    async testOpenSSL() {
        try {
            // Test basic crypto operations
            const testData = Buffer.from('test data');
            const key = crypto.randomBytes(32);
            const iv = crypto.randomBytes(12);
            
            const cipher = crypto.createCipheriv('aes-256-gcm', key, iv);
            const encrypted = Buffer.concat([cipher.update(testData), cipher.final()]);
            const tag = cipher.getAuthTag();
            
            const decipher = crypto.createDecipheriv('aes-256-gcm', key, iv);
            decipher.setAuthTag(tag);
            const decrypted = Buffer.concat([decipher.update(encrypted), decipher.final()]);
            
            if (!testData.equals(decrypted)) {
                throw new Error('OpenSSL test failed - encryption/decryption mismatch');
            }
            
            logger.debug('OpenSSL test passed');
        } catch (error) {
            logger.error('OpenSSL test failed:', error.message);
            throw error;
        }
    }

    generateKey(algorithm = 'aes-256-gcm') {
        const config = opensslConfig.getAlgorithmConfig(algorithm);
        return crypto.randomBytes(config.keySize);
    }

    generateIV(algorithm = 'aes-256-gcm') {
        const config = opensslConfig.getAlgorithmConfig(algorithm);
        return crypto.randomBytes(config.ivSize);
    }

    encrypt(data, algorithm = 'aes-256-gcm', key = null, iv = null) {
        try {
            if (!key) key = this.generateKey(algorithm);
            if (!iv) iv = this.generateIV(algorithm);
            
            const cipher = crypto.createCipheriv(algorithm, key, iv);
            const encrypted = Buffer.concat([cipher.update(data), cipher.final()]);
            
            const result = {
                encrypted,
                key,
                iv,
                algorithm
            };
            
            // Add auth tag for GCM modes
            if (algorithm.includes('gcm')) {
                result.tag = cipher.getAuthTag();
            }
            
            return result;
        } catch (error) {
            logger.error(`Encryption failed for algorithm ${algorithm}:`, error.message);
            throw error;
        }
    }

    decrypt(encryptedData, key, iv, algorithm = 'aes-256-gcm', tag = null) {
        try {
            const decipher = crypto.createDecipheriv(algorithm, key, iv);
            
            // Set auth tag for GCM modes
            if (tag && algorithm.includes('gcm')) {
                decipher.setAuthTag(tag);
            }
            
            const decrypted = Buffer.concat([decipher.update(encryptedData), decipher.final()]);
            return decrypted;
        } catch (error) {
            logger.error(`Decryption failed for algorithm ${algorithm}:`, error.message);
            throw error;
        }
    }

    getSupportedAlgorithms() {
        return this.supportedAlgorithms;
    }

    isAlgorithmSupported(algorithm) {
        return this.supportedAlgorithms.includes(algorithm);
    }

    getAlgorithmInfo(algorithm) {
        return opensslConfig.getAlgorithmConfig(algorithm);
    }
}

module.exports = OpenSSLManager;
