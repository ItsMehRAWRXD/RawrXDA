// OpenSSL Configuration Utility
const { logger } = require('./logger');

class OpenSSLConfig {
    constructor() {
        this.config = {
            defaultAlgorithm: 'aes-256-gcm',
            keySize: 256,
            ivSize: 12,
            tagSize: 16,
            supportedAlgorithms: [
                'aes-256-gcm',
                'aes-256-cbc',
                'aes-192-gcm',
                'aes-192-cbc',
                'aes-128-gcm',
                'aes-128-cbc',
                'chacha20-poly1305',
                'cam-256-gcm',
                'cam-256-cbc',
                'aria-256-gcm'
            ],
            compression: {
                enabled: true,
                algorithm: 'gzip',
                level: 6
            },
            security: {
                pbkdf2Iterations: 10000,
                saltSize: 16,
                keyDerivation: 'pbkdf2'
            }
        };
    }

    getConfig() {
        return this.config;
    }

    updateConfig(newConfig) {
        this.config = { ...this.config, ...newConfig };
        logger.info('OpenSSL configuration updated');
    }

    getAlgorithmConfig(algorithm) {
        const configs = {
            'aes-256-gcm': { keySize: 32, ivSize: 12, tagSize: 16 },
            'aes-256-cbc': { keySize: 32, ivSize: 16, tagSize: 0 },
            'aes-192-gcm': { keySize: 24, ivSize: 12, tagSize: 16 },
            'aes-192-cbc': { keySize: 24, ivSize: 16, tagSize: 0 },
            'aes-128-gcm': { keySize: 16, ivSize: 12, tagSize: 16 },
            'aes-128-cbc': { keySize: 16, ivSize: 16, tagSize: 0 },
            'chacha20-poly1305': { keySize: 32, ivSize: 12, tagSize: 16 },
            'cam-256-gcm': { keySize: 32, ivSize: 12, tagSize: 16 },
            'cam-256-cbc': { keySize: 32, ivSize: 16, tagSize: 0 },
            'aria-256-gcm': { keySize: 32, ivSize: 12, tagSize: 16 }
        };
        
        return configs[algorithm] || configs['aes-256-gcm'];
    }
}

module.exports = new OpenSSLConfig();
