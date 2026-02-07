// Security Manager for Secure IDE
import crypto from 'crypto';
import * as fs from 'fs/promises';
import * as path from 'path';

class SecurityManager {
    constructor() {
        this.isInitialized = false;
        this.allowedPaths = new Set();
        this.blockedPaths = new Set();
        this.fileHashes = new Map();
        this.resourceUsage = new Map();
        this.securityViolations = [];
        this.auditLogPath = './security/audit.log';
    }

    async initialize() {
        console.log('Initializing Security Manager...');
        
        try {
            // Create security directory
            await this.createSecurityDirectory();
            
            // Initialize file integrity monitoring
            await this.initializeFileIntegrity();
            
            // Set up resource monitoring
            this.setupResourceMonitoring();
            
            // Load security policies
            await this.loadSecurityPolicies();
            
            this.isInitialized = true;
            console.log('Security Manager initialized successfully');
        } catch (error) {
            console.error('Failed to initialize Security Manager:', error);
            throw error;
        }
    }

    async createSecurityDirectory() {
        const securityDir = './security';
        try {
            await fs.mkdir(securityDir, { recursive: true });
        } catch (error) {
            console.warn('Could not create security directory:', error);
        }
    }

    async initializeFileIntegrity() {
        // Calculate hashes for critical system files
        const criticalFiles = [
            'package.json',
            'src/server/index.js',
            'src/ai/LocalAIEngine.js',
            'src/security/SecurityManager.js'
        ];
        
        for (const file of criticalFiles) {
            try {
                const filePath = path.resolve(file);
                const content = await fs.readFile(filePath, 'utf8');
                const hash = crypto.createHash('sha256').update(content).digest('hex');
                this.fileHashes.set(filePath, hash);
            } catch (error) {
                console.warn(`Could not hash file ${file}:`, error);
            }
        }
    }

    setupResourceMonitoring() {
        // Monitor memory usage
        setInterval(() => {
            const memUsage = process.memoryUsage();
            this.resourceUsage.set('memory', memUsage.heapUsed);
            
            if (memUsage.heapUsed > 1024 * 1024 * 1024) { // 1GB
                this.emitViolation({
                    type: 'resource-usage',
                    message: 'High memory usage detected',
                    severity: 'medium',
                    timestamp: new Date(),
                    details: { memoryUsage: memUsage.heapUsed }
                });
            }
        }, 30000); // Check every 30 seconds
    }

    async loadSecurityPolicies() {
        // Load security policies from configuration
        const defaultPolicies = {
            maxFileSize: 10 * 1024 * 1024, // 10MB
            allowedFileTypes: ['.js', '.ts', '.html', '.css', '.json', '.md', '.txt'],
            maxConcurrentConnections: 10,
            enableSandbox: true,
            allowNetworkAccess: false
        };
        
        this.policies = defaultPolicies;
    }

    // File access security
    async validateFileAccess(filePath, operation) {
        const resolvedPath = path.resolve(filePath);
        
        // Check if path is blocked
        if (this.blockedPaths.has(resolvedPath)) {
            this.emitViolation({
                type: 'file-access',
                message: `Blocked file access attempt: ${operation} on ${resolvedPath}`,
                severity: 'high',
                timestamp: new Date(),
                details: { filePath: resolvedPath, operation }
            });
            return false;
        }
        
        // Check file type restrictions
        const ext = path.extname(resolvedPath).toLowerCase();
        if (this.policies.allowedFileTypes.length > 0 && !this.policies.allowedFileTypes.includes(ext)) {
            this.emitViolation({
                type: 'file-access',
                message: `Disallowed file type access: ${ext}`,
                severity: 'medium',
                timestamp: new Date(),
                details: { filePath: resolvedPath, fileType: ext }
            });
            return false;
        }
        
        // Check file size for write operations
        if (operation === 'write') {
            try {
                const stats = await fs.stat(resolvedPath);
                if (stats.size > this.policies.maxFileSize) {
                    this.emitViolation({
                        type: 'file-access',
                        message: `File size exceeds limit: ${stats.size} bytes`,
                        severity: 'medium',
                        timestamp: new Date(),
                        details: { filePath: resolvedPath, fileSize: stats.size }
                    });
                    return false;
                }
            } catch (error) {
                // File doesn't exist yet, that's okay for write operations
            }
        }
        
        return true;
    }

    // Network access security
    validateNetworkAccess(url) {
        if (!this.policies.allowNetworkAccess) {
            this.emitViolation({
                type: 'network-access',
                message: `Network access blocked: ${url}`,
                severity: 'high',
                timestamp: new Date(),
                details: { url }
            });
            return false;
        }
        
        // Additional URL validation
        try {
            const parsedUrl = new URL(url);
            const allowedDomains = ['localhost', '127.0.0.1'];
            
            if (!allowedDomains.includes(parsedUrl.hostname)) {
                this.emitViolation({
                    type: 'network-access',
                    message: `Unauthorized domain access: ${parsedUrl.hostname}`,
                    severity: 'high',
                    timestamp: new Date(),
                    details: { url, hostname: parsedUrl.hostname }
                });
                return false;
            }
        } catch (error) {
            this.emitViolation({
                type: 'network-access',
                message: `Invalid URL format: ${url}`,
                severity: 'medium',
                timestamp: new Date(),
                details: { url, error: error.message }
            });
            return false;
        }
        
        return true;
    }

    // Extension execution security
    validateExtensionExecution(extensionPath, permissions) {
        const ext = path.extname(extensionPath).toLowerCase();
        
        if (!['.js', '.ts'].includes(ext)) {
            this.emitViolation({
                type: 'extension-execution',
                message: `Untrusted extension execution: ${extensionPath}`,
                severity: 'critical',
                timestamp: new Date(),
                details: { extensionPath, permissions }
            });
            return false;
        }
        
        // Check for dangerous permissions
        const dangerousPermissions = ['file-system', 'network', 'process'];
        const hasDangerousPermissions = permissions.some(perm => 
            dangerousPermissions.includes(perm)
        );
        
        if (hasDangerousPermissions) {
            this.emitViolation({
                type: 'extension-execution',
                message: `Extension requests dangerous permissions: ${permissions.join(', ')}`,
                severity: 'high',
                timestamp: new Date(),
                details: { extensionPath, permissions }
            });
            return false;
        }
        
        return true;
    }

    // File integrity checking
    async checkFileIntegrity(filePath) {
        const resolvedPath = path.resolve(filePath);
        const storedHash = this.fileHashes.get(resolvedPath);
        
        if (!storedHash) {
            return true; // File not in integrity check list
        }
        
        try {
            const content = await fs.readFile(resolvedPath, 'utf8');
            const currentHash = crypto.createHash('sha256').update(content).digest('hex');
            
            if (currentHash !== storedHash) {
                this.emitViolation({
                    type: 'file-access',
                    message: `File integrity violation: ${resolvedPath}`,
                    severity: 'critical',
                    timestamp: new Date(),
                    details: { 
                        filePath: resolvedPath, 
                        expectedHash: storedHash, 
                        actualHash: currentHash 
                    }
                });
                return false;
            }
            
            return true;
        } catch (error) {
            console.error(`Error checking file integrity for ${resolvedPath}:`, error);
            return false;
        }
    }

    // Add allowed path
    addAllowedPath(path) {
        this.allowedPaths.add(path.resolve(path));
    }

    // Add blocked path
    addBlockedPath(path) {
        this.blockedPaths.add(path.resolve(path));
    }

    // Generate secure token
    generateSecureToken() {
        return crypto.randomBytes(32).toString('hex');
    }

    // Hash sensitive data
    hashSensitiveData(data) {
        return crypto.createHash('sha256').update(data).digest('hex');
    }

    // Encrypt sensitive data
    encryptSensitiveData(data, key) {
        const cipher = crypto.createCipher('aes-256-cbc', key);
        let encrypted = cipher.update(data, 'utf8', 'hex');
        encrypted += cipher.final('hex');
        return encrypted;
    }

    // Decrypt sensitive data
    decryptSensitiveData(encryptedData, key) {
        const decipher = crypto.createDecipher('aes-256-cbc', key);
        let decrypted = decipher.update(encryptedData, 'hex', 'utf8');
        decrypted += decipher.final('utf8');
        return decrypted;
    }

    // Emit security violation
    emitViolation(violation) {
        console.error('Security violation:', violation);
        this.securityViolations.push(violation);
        
        // Log to audit file
        this.logSecurityViolation(violation);
    }

    async logSecurityViolation(violation) {
        try {
            const logEntry = `${new Date().toISOString()} - ${violation.type.toUpperCase()} - ${violation.severity.toUpperCase()} - ${violation.message}\n`;
            await fs.appendFile(this.auditLogPath, logEntry);
        } catch (error) {
            console.error('Failed to log security violation:', error);
        }
    }

    // Get security status
    getSecurityStatus() {
        return {
            isInitialized: this.isInitialized,
            allowedPaths: Array.from(this.allowedPaths),
            blockedPaths: Array.from(this.blockedPaths),
            resourceUsage: Object.fromEntries(this.resourceUsage),
            violations: this.securityViolations.length,
            policies: this.policies
        };
    }

    // Get security violations
    getSecurityViolations(severity = null) {
        if (severity) {
            return this.securityViolations.filter(v => v.severity === severity);
        }
        return this.securityViolations;
    }

    // Clear security violations
    clearSecurityViolations() {
        this.securityViolations = [];
    }

    // Update security policies
    updatePolicies(newPolicies) {
        this.policies = { ...this.policies, ...newPolicies };
    }

    async cleanup() {
        console.log('Cleaning up Security Manager...');
        this.isInitialized = false;
    }
}

export default SecurityManager;
