/**
 * RawrZ Botnet Control Panel - Security Configuration
 * Commercial-grade security settings and configurations
 */

const securityConfig = {
    // Authentication & Authorization
    auth: {
        sessionTimeout: 30 * 60 * 1000, // 30 minutes
        maxLoginAttempts: 5,
        lockoutDuration: 15 * 60 * 1000, // 15 minutes
        passwordMinLength: 12,
        requireSpecialChars: true,
        requireNumbers: true,
        requireUppercase: true,
        requireLowercase: true,
        tokenExpiry: 24 * 60 * 60 * 1000, // 24 hours
        refreshTokenExpiry: 7 * 24 * 60 * 60 * 1000 // 7 days
    },

    // Rate Limiting
    rateLimit: {
        windowMs: 15 * 60 * 1000, // 15 minutes
        maxRequests: 100,
        skipSuccessfulRequests: false,
        skipFailedRequests: false,
        keyGenerator: (req) => {
            return req.ip + ':' + req.get('User-Agent');
        }
    },

    // CORS Configuration
    cors: {
        origin: [
            'http://localhost:3000',
            'http://127.0.0.1:3000',
            'https://localhost:3000',
            'https://127.0.0.1:3000'
        ],
        credentials: true,
        methods: ['GET', 'POST', 'PUT', 'DELETE', 'OPTIONS'],
        allowedHeaders: [
            'Content-Type',
            'Authorization',
            'X-Requested-With',
            'X-CSRF-Token',
            'X-Session-ID'
        ],
        exposedHeaders: ['X-Total-Count', 'X-Page-Count'],
        maxAge: 86400 // 24 hours
    },

    // Content Security Policy
    csp: {
        directives: {
            defaultSrc: ["'self'"],
            styleSrc: ["'self'", "'unsafe-inline'"],
            scriptSrc: ["'self'", "'unsafe-inline'"],
            imgSrc: ["'self'", "data:", "https:"],
            connectSrc: ["'self'", "ws:", "wss:"],
            fontSrc: ["'self'"],
            objectSrc: ["'none'"],
            mediaSrc: ["'self'"],
            frameSrc: ["'none'"],
            baseUri: ["'self'"],
            formAction: ["'self'"],
            upgradeInsecureRequests: []
        }
    },

    // Input Validation
    validation: {
        maxStringLength: 10000,
        maxArrayLength: 1000,
        maxObjectDepth: 10,
        allowedFileTypes: ['.txt', '.log', '.json', '.csv'],
        maxFileSize: 10 * 1024 * 1024, // 10MB
        sanitizeHtml: true,
        sanitizeSql: true,
        validateEmail: true,
        validateUrl: true,
        validateIP: true
    },

    // Encryption
    encryption: {
        algorithm: 'aes-256-gcm',
        keyLength: 32,
        ivLength: 16,
        tagLength: 16,
        saltLength: 32,
        iterations: 100000,
        hashAlgorithm: 'sha512'
    },

    // Logging
    logging: {
        level: 'info',
        maxFiles: 10,
        maxSize: '10MB',
        datePattern: 'YYYY-MM-DD',
        auditLogRetention: 90, // days
        errorLogRetention: 30, // days
        accessLogRetention: 7, // days
        sensitiveFields: ['password', 'token', 'key', 'secret', 'private']
    },

    // Session Management
    session: {
        secret: process.env.SESSION_SECRET || 'rawrz-super-secret-key-change-in-production',
        name: 'rawrz.session',
        resave: false,
        saveUninitialized: false,
        rolling: true,
        cookie: {
            secure: process.env.NODE_ENV === 'production',
            httpOnly: true,
            maxAge: 30 * 60 * 1000, // 30 minutes
            sameSite: 'strict'
        }
    },

    // WebSocket Security
    websocket: {
        maxConnections: 10000,
        maxMessageSize: 1024 * 1024, // 1MB
        pingInterval: 30000, // 30 seconds
        pongTimeout: 5000, // 5 seconds
        connectionTimeout: 10000, // 10 seconds
        rateLimit: {
            windowMs: 60000, // 1 minute
            maxMessages: 100
        }
    },

    // Bot Management
    botManagement: {
        maxBotsPerIP: 10,
        botTimeout: 5 * 60 * 1000, // 5 minutes
        commandTimeout: 30 * 1000, // 30 seconds
        maxCommandsPerMinute: 60,
        blacklistIPs: [],
        whitelistIPs: [],
        requireBotAuthentication: true
    },

    // Data Protection
    dataProtection: {
        encryptSensitiveData: true,
        anonymizeLogs: true,
        dataRetention: 30, // days
        backupInterval: 24 * 60 * 60 * 1000, // 24 hours
        compressionEnabled: true,
        deduplicationEnabled: true
    },

    // Monitoring & Alerting
    monitoring: {
        healthCheckInterval: 30000, // 30 seconds
        performanceThreshold: 1000, // ms
        memoryThreshold: 0.8, // 80%
        cpuThreshold: 0.8, // 80%
        diskThreshold: 0.9, // 90%
        alertChannels: ['email', 'telegram', 'webhook'],
        alertCooldown: 5 * 60 * 1000 // 5 minutes
    },

    // API Security
    api: {
        version: 'v1',
        requireApiKey: true,
        apiKeyHeader: 'X-API-Key',
        maxRequestSize: 10 * 1024 * 1024, // 10MB
        timeout: 30000, // 30 seconds
        retryAttempts: 3,
        retryDelay: 1000, // 1 second
        circuitBreaker: {
            threshold: 5,
            timeout: 60000, // 1 minute
            resetTimeout: 30000 // 30 seconds
        }
    },

    // File Upload Security
    fileUpload: {
        maxFileSize: 50 * 1024 * 1024, // 50MB
        allowedMimeTypes: [
            'text/plain',
            'application/json',
            'text/csv',
            'application/octet-stream'
        ],
        scanForMalware: true,
        quarantineSuspicious: true,
        maxFilesPerRequest: 10
    },

    // Database Security
    database: {
        connectionLimit: 10,
        acquireTimeout: 60000, // 1 minute
        timeout: 60000, // 1 minute
        reconnect: true,
        ssl: process.env.NODE_ENV === 'production',
        encrypt: true,
        backupEnabled: true,
        backupInterval: 24 * 60 * 60 * 1000 // 24 hours
    },

    // Network Security
    network: {
        trustedProxies: ['127.0.0.1', '::1'],
        blockTor: true,
        blockVPN: false,
        geoBlocking: false,
        allowedCountries: [],
        blockedCountries: [],
        maxConnectionsPerIP: 100
    },

    // Development vs Production
    environment: {
        isDevelopment: process.env.NODE_ENV !== 'production',
        isProduction: process.env.NODE_ENV === 'production',
        debugMode: process.env.DEBUG === 'true',
        verboseLogging: process.env.VERBOSE === 'true'
    }
};

// Security middleware functions
const securityMiddleware = {
    // Input sanitization
    sanitizeInput: (input) => {
        if (typeof input !== 'string') return input;
        
        return input
            .replace(/[<>\"'&]/g, (match) => {
                const escape = {
                    '<': '&lt;',
                    '>': '&gt;',
                    '"': '&quot;',
                    "'": '&#x27;',
                    '&': '&amp;'
                };
                return escape[match];
            })
            .substring(0, securityConfig.validation.maxStringLength);
    },

    // Validate email
    validateEmail: (email) => {
        const re = /^[^\s@]+@[^\s@]+\.[^\s@]+$/;
        return re.test(email);
    },

    // Validate URL
    validateUrl: (url) => {
        try {
            new URL(url);
            return true;
        } catch {
            return false;
        }
    },

    // Validate IP address
    validateIP: (ip) => {
        const re = /^(?:(?:25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\.){3}(?:25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)$/;
        return re.test(ip);
    },

    // Generate secure token
    generateToken: (length = 32) => {
        const crypto = require('crypto');
        return crypto.randomBytes(length).toString('hex');
    },

    // Hash password
    hashPassword: async (password) => {
        const bcrypt = require('bcryptjs');
        const saltRounds = 12;
        return await bcrypt.hash(password, saltRounds);
    },

    // Verify password
    verifyPassword: async (password, hash) => {
        const bcrypt = require('bcryptjs');
        return await bcrypt.compare(password, hash);
    },

    // Encrypt data
    encryptData: (data, key) => {
        const crypto = require('crypto');
        const algorithm = securityConfig.encryption.algorithm;
        const iv = crypto.randomBytes(securityConfig.encryption.ivLength);
        const cipher = crypto.createCipher(algorithm, key);
        
        let encrypted = cipher.update(JSON.stringify(data), 'utf8', 'hex');
        encrypted += cipher.final('hex');
        
        return {
            encrypted,
            iv: iv.toString('hex')
        };
    },

    // Decrypt data
    decryptData: (encryptedData, key, iv) => {
        const crypto = require('crypto');
        const algorithm = securityConfig.encryption.algorithm;
        const decipher = crypto.createDecipher(algorithm, key);
        
        let decrypted = decipher.update(encryptedData, 'hex', 'utf8');
        decrypted += decipher.final('utf8');
        
        return JSON.parse(decrypted);
    }
};

module.exports = {
    securityConfig,
    securityMiddleware
};
